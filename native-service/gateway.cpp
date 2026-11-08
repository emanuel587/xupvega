// Xuper TV NF - servicio local integrado en la aplicación de Vega OS.
// Requiere service.json y device_state.json del usuario; no incluye cuentas ni claves.
// No usar expuesto a Internet ni como servidor público. Linux/macOS POSIX.
#include "service.hpp"
#include "catalog.hpp"
#include <curl/curl.h>
#define JSON_NOEXCEPTION 1
#include "json.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <random>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cctype>

using Json=nlohmann::json;
namespace {
std::shared_ptr<xs::ServiceSession> session;
std::mutex service_lock, catalog_lock;
std::unordered_map<std::string,xs::Entry> registry;

std::string secret,config_path,public_origin;
struct MediaTicket {xs::SourceResult source;std::chrono::steady_clock::time_point created;};
std::unordered_map<std::string,MediaTicket> tickets;
std::unordered_map<std::string,std::string> artwork_urls;
std::string artwork_ref(const std::string& url){
    if(url.empty()||(url.rfind("https://",0)!=0&&url.rfind("http://",0)!=0)||!xs::valid_source(url))return url;
    uint64_t hash=14695981039346656037ULL;
    for(unsigned char c:url){hash^=c;hash*=1099511628211ULL;}
    char key[32];std::snprintf(key,sizeof(key),"%016llx",static_cast<unsigned long long>(hash));
    std::lock_guard<std::mutex> lock(catalog_lock);
    artwork_urls[key]=url;
    return public_origin+"/art/"+key;
}

std::string random_hex(size_t bytes=24){
    static std::mutex rng_mutex; static std::random_device rd;
    static std::mt19937_64 engine((static_cast<uint64_t>(rd())<<32)^rd());
    std::lock_guard<std::mutex> lock(rng_mutex);
    const char* hex="0123456789abcdef";std::string s;
    for(size_t i=0;i<bytes;i++){auto b=static_cast<unsigned>(engine()&255);s+=hex[b>>4];s+=hex[b&15];}return s;
}
std::string word(const Json& j,const char* key,const std::string& d=""){
    if(!j.is_object()||!j.contains(key))return d;
    auto x=j[key];if(x.is_string())return x.get<std::string>();return d;
}
std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
std::string url_decode(const std::string& s){std::string out;for(size_t i=0;i<s.size();i++){
    if(s[i]=='+')out+=' ';
    else if(s[i]=='%'&&i+2<s.size()){char h[3]={s[i+1],s[i+2],0};char* end;long b=std::strtol(h,&end,16);if(*end==0){out+=static_cast<char>(b);i+=2;}else out+=s[i];}
    else out+=s[i];}return out;}
std::map<std::string,std::string> query_params(const std::string& path){
    std::map<std::string,std::string> q;auto idx=path.find('?');if(idx==std::string::npos)return q;
    std::stringstream in(path.substr(idx+1));std::string part;
    while(std::getline(in,part,'&')){auto eq=part.find('=');if(eq!=std::string::npos)q[url_decode(part.substr(0,eq))]=url_decode(part.substr(eq+1));}
    return q;
}
std::string param(const std::map<std::string,std::string>& q,const std::string& k){auto i=q.find(k);return i==q.end()?"":i->second;}
bool sendall(int socket,const std::string& v){
    size_t written=0;while(written<v.size()){auto n=send(socket,v.data()+written,v.size()-written,MSG_NOSIGNAL);if(n<=0)return false;written+=n;}return true;
}
void reply(int fd,int status,const Json& body){
    const std::string data=body.dump();
    const std::string reason=status==200?"OK":status==400?"Bad Request":status==401?"Unauthorized":status==404?"Not Found":status==503?"Service Unavailable":"Bad Gateway";
    sendall(fd,"HTTP/1.1 "+std::to_string(status)+" "+reason+"\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Length: "+std::to_string(data.size())+"\r\nConnection: close\r\n\r\n"+data);
}
std::string remember(const xs::Entry& e){
    // Identificadores estables entre reinicios para conservar favoritos, sin publicar la URL.
    uint64_t hash=14695981039346656037ULL;
    for(unsigned char c:std::to_string(static_cast<int>(e.kind))+":"+e.channel_code+":"+
                         e.content_id+":"+e.series_id+":"+e.program_type+":"+
                         std::to_string(e.column_id)+":"+e.title+":"+
                         std::to_string(e.page)+":"+e.query){hash^=c;hash*=1099511628211ULL;}
    char key[24];std::snprintf(key,sizeof(key),"i%016llx",static_cast<unsigned long long>(hash));
    std::string id=key;
    std::lock_guard<std::mutex> lock(catalog_lock);
    registry[id]=e;
    if(registry.size()>30000)registry.clear();
    return id;
}
Json serialize(const xs::Catalog& catalog){
    Json entries=Json::array();
    for(const auto& e:catalog.entries){
        std::string kind="media";
        switch(e.kind){
        case xs::EntryKind::Category:kind="category";break;
        case xs::EntryKind::Channel:kind="channel";break;
        case xs::EntryKind::Section:kind="section";break;
        case xs::EntryKind::VodCategory:kind="vod-category";break;
        case xs::EntryKind::Asset:kind="asset";break;
        case xs::EntryKind::Episode:kind="episode";break;
        case xs::EntryKind::VodPage:kind="vod-page";break;
        case xs::EntryKind::SearchPage:kind="search-page";break;
        default:break;
        }
        Json v={{"id",remember(e)},{"kind",kind},{"title",e.title},{"artwork",artwork_ref(e.artwork)},
          {"description",e.description},{"program_type",e.program_type},{"column_id",e.column_id}};
        entries.push_back(std::move(v));
    }
    return {{"title",catalog.title},{"description",catalog.description},{"artwork",artwork_ref(catalog.artwork)},
      {"rating",catalog.rating},{"release_date",catalog.release_date},{"country",catalog.country},
      {"genres",catalog.genres},{"director",catalog.director},{"actors",catalog.actors},
      {"platform",catalog.platform},{"entries",entries}};
}
Json result(const xs::BrowseResult& r){return {{"ok",r.ok},{"status",r.status},{"catalog",serialize(r.catalog)}};}
// HTTP image cache bridge: WebView 4 blocks unlisted HTTP image domains.
// Only images previously returned by the authorized service are fetchable here.
struct ArtworkBuffer{std::string bytes;bool too_large=false;std::string content_type="image/jpeg";};
size_t art_write(char* buf,size_t size,size_t n,void* user){
    auto* b=static_cast<ArtworkBuffer*>(user);size_t count=size*n;
    if(b->bytes.size()+count>5*1024*1024){b->too_large=true;return 0;}
    b->bytes.append(buf,count);return count;
}
size_t art_header(char* buf,size_t size,size_t n,void* user){
    auto* b=static_cast<ArtworkBuffer*>(user);std::string line(buf,size*n);
    if(lower(line).rfind("content-type:",0)==0){auto content=line.substr(13);
        auto start=content.find_first_not_of(" \t");if(start!=std::string::npos)content.erase(0,start);
        while(!content.empty()&&(content.back()=='\r'||content.back()=='\n'||content.back()==' '))content.pop_back();
        if((content.rfind("image/",0)==0)&&content.find_first_of("\r\n")==std::string::npos)b->content_type=content;
    }
    return size*n;
}
void stream_art(int fd,const std::string& url){
    CURL* curl=curl_easy_init();if(!curl){reply(fd,502,{{"error","Error al descargar portada"}});return;}
    ArtworkBuffer out;
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(curl,CURLOPT_MAXREDIRS,3L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");
#else
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS);
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
#ifdef XUPER_VEGA
    curl_easy_setopt(curl,CURLOPT_CAINFO,"/pkg/assets/cacert.pem");
#endif
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,7L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,18L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,art_write);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
    curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,art_header);curl_easy_setopt(curl,CURLOPT_HEADERDATA,&out);
    auto rc=curl_easy_perform(curl);long code=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
    curl_easy_cleanup(curl);
    if(rc!=CURLE_OK||code!=200||out.too_large||out.bytes.empty()){
        reply(fd,502,{{"error","Portada no disponible"}});return;
    }
    sendall(fd,"HTTP/1.1 200 OK\r\nContent-Type: "+out.content_type+
        "\r\nCache-Control: public, max-age=86400\r\nContent-Length: "+
        std::to_string(out.bytes.size())+"\r\nConnection: close\r\n\r\n");
    sendall(fd,out.bytes);
}

// Stream HTTP from original CDN through local companion, including mandatory Content-Auth headers.
// No redirects, to avoid disclosing credentials to a different host.
struct MediaStream{int socket;bool headers_sent=false;bool chunked=false;long status=200;
    std::string content_type="application/octet-stream",length,range,ranges;
};
size_t media_header(char* buffer,size_t size,size_t n,void* user){
    auto* s=static_cast<MediaStream*>(user);std::string h(buffer,size*n);
    if(h.rfind("HTTP/",0)==0){std::istringstream line(h);std::string proto;line>>proto>>s->status;return size*n;}
    auto colon=h.find(':');if(colon!=std::string::npos){auto name=lower(h.substr(0,colon));auto val=h.substr(colon+1);
        auto start=val.find_first_not_of(" \t");if(start!=std::string::npos)val.erase(0,start);
        while(!val.empty()&&(val.back()=='\r'||val.back()=='\n'||val.back()==' '))val.pop_back();
        if(name=="content-type")s->content_type=val;
        else if(name=="content-length")s->length=val;
        else if(name=="content-range")s->range=val;
        else if(name=="accept-ranges")s->ranges=val;
    }
    if(h=="\r\n"||h=="\n"){
        if(s->status!=200&&s->status!=206){return size*n;}
        s->chunked=s->length.empty();
        std::string out="HTTP/1.1 "+std::to_string(s->status)+(s->status==206?" Partial Content":" OK")+"\r\n";
        out+="Content-Type: "+s->content_type+"\r\nCache-Control: no-store\r\nConnection: close\r\n";
        if(!s->range.empty())out+="Content-Range: "+s->range+"\r\n";
        if(!s->ranges.empty())out+="Accept-Ranges: "+s->ranges+"\r\n";
        if(s->chunked)out+="Transfer-Encoding: chunked\r\n";
        else out+="Content-Length: "+s->length+"\r\n";
        sendall(s->socket,out+"\r\n");s->headers_sent=true;
    }
    return size*n;
}
size_t media_write(char* buffer,size_t size,size_t n,void* user){
    auto* s=static_cast<MediaStream*>(user);const size_t count=size*n;if(!s->headers_sent)return count;
    if(s->chunked){char len[32];std::snprintf(len,sizeof(len),"%zx\r\n",count);if(!sendall(s->socket,len))return 0;}
    if(!sendall(s->socket,std::string(buffer,count)))return 0;
    if(s->chunked&&!sendall(s->socket,"\r\n"))return 0;
    return count;
}
void stream_media(int fd,const xs::SourceResult& source,const std::string& range){
    CURL* curl=curl_easy_init();if(!curl){reply(fd,502,{{"error","No se pudo iniciar el reproductor local"}});return;}
    MediaStream stream{fd};
    if(source.source.find(".ts")!=std::string::npos)stream.content_type="video/mp2t";
    else if(source.source.find(".mp4")!=std::string::npos)stream.content_type="video/mp4";
    curl_slist* headers=nullptr;
    for(const auto& item:source.headers)if(item.first.find_first_of("\r\n:")==std::string::npos&&item.second.find_first_of("\r\n")==std::string::npos)
        headers=curl_slist_append(headers,(item.first+": "+item.second).c_str());
    if(!range.empty()&&range.rfind("bytes=",0)==0&&range.size()<100)headers=curl_slist_append(headers,("Range: "+range).c_str());
    curl_easy_setopt(curl,CURLOPT_URL,source.source.c_str());
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
#ifdef XUPER_VEGA
    curl_easy_setopt(curl,CURLOPT_CAINFO,"/pkg/assets/cacert.pem");
#endif
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,100L);curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,media_header);curl_easy_setopt(curl,CURLOPT_HEADERDATA,&stream);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,media_write);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&stream);
    const auto rc=curl_easy_perform(curl);
    if(!stream.headers_sent)reply(fd,502,{{"error","CDN rechazó el video; consulta registro de reproducción."}});
    else if(stream.chunked)sendall(fd,"0\r\n\r\n");
    if(rc!=CURLE_OK)std::cerr<<"media: error de transporte "<<static_cast<int>(rc)<<"\n";
    curl_slist_free_all(headers);curl_easy_cleanup(curl);
}
void client(int fd){
    struct Close{int fd;~Close(){close(fd);}} done{fd};
    std::string request;char block[2048];
    while(request.find("\r\n\r\n")==std::string::npos&&request.size()<16000){auto n=recv(fd,block,sizeof(block),0);if(n<=0)return;request.append(block,n);}
    std::istringstream data(request);std::string method,path,protocol;data>>method>>path>>protocol;
    if(method!="GET"||path.size()>5000){reply(fd,400,{{"error","Solicitud no válida"}});return;}
    std::string line,api_key,range;
    std::getline(data,line); // consumir resto de la linea GET antes de analizar headers
    while(std::getline(data,line)){
        if(line=="\r"||line.empty())break;
        const auto colon=line.find(':');if(colon==std::string::npos)continue;
        const auto name=lower(line.substr(0,colon));auto value=line.substr(colon+1);
        while(!value.empty()&&(value.back()=='\r'||value.back()=='\n'))value.pop_back();
        auto start=value.find_first_not_of(" \t");if(start!=std::string::npos)value.erase(0,start);
        if(name=="x-xuper-key")api_key=value;
        if(name=="range")range=value;
    }
    // Media tickets are short lived and unguessable; API endpoints require the pairing token.
    auto resource=path.substr(0,path.find('?'));
    if(resource.rfind("/art/",0)==0){
        std::string original;
        {
            std::lock_guard<std::mutex> g(catalog_lock);
            auto i=artwork_urls.find(resource.substr(5));
            if(i!=artwork_urls.end())original=i->second;
        }
        if(original.empty()){reply(fd,404,{{"error","Portada no encontrada"}});return;}
        stream_art(fd,original);return;
    }
    if(resource.rfind("/media/",0)==0){
        const auto ticket=resource.substr(7);xs::SourceResult source;
        {
            std::lock_guard<std::mutex> guard(catalog_lock);
            auto it=tickets.find(ticket);
            if(it==tickets.end()||std::chrono::steady_clock::now()-it->second.created>std::chrono::minutes(8)){
                reply(fd,404,{{"error","Enlace de reproducción caducado"}});return;}
            source=it->second.source;
        }
        stream_media(fd,source,range);return;
    }
    if(secret.empty()||api_key.size()!=secret.size()){
        reply(fd,401,{{"error","Clave local requerida"}});return;}
    unsigned difference=0;for(size_t i=0;i<secret.size();i++)difference|=static_cast<unsigned>(secret[i]^api_key[i]);
    if(difference){reply(fd,401,{{"error","Clave local incorrecta"}});return;}
    const auto args=query_params(path);
    if(resource=="/api/health"){
        reply(fd,200,{{"ok",true},{"service","Servicio integrado Xuper TV NF"},{"authenticated",static_cast<bool>(session)}});return;}
    if(resource=="/api/start"){
        std::lock_guard<std::mutex> g(service_lock);
        auto active=xs::service_bootstrap(config_path);
        if(!active.authenticated||!active.session){reply(fd,503,{{"ok",false},{"status",active.status}});return;}
        session=active.session;
        auto front=xs::service_home_assets(session,active.catalog);
        reply(fd,200,{{"ok",true},{"status",active.status},
            {"root",serialize(active.catalog)},{"featured",serialize(front.catalog)}});return;
    }
    if(!session){reply(fd,503,{{"ok",false},{"status","Primero inicia el servicio"}});return;}
    if(resource=="/api/search"){
        auto term=param(args,"q");if(term.size()>120){reply(fd,400,{{"error","Búsqueda demasiado larga"}});return;}
        std::lock_guard<std::mutex> g(service_lock);
        auto found=xs::service_search_all(session,term);
        reply(fd,200,result(found));return;
    }
    const auto id=param(args,"id");xs::Entry entry;
    {
        std::lock_guard<std::mutex> g(catalog_lock);
        auto i=registry.find(id);if(i==registry.end()){reply(fd,404,{{"error","Elemento no encontrado; vuelve a Inicio"}});return;}
        entry=i->second;
    }
    if(resource=="/api/open"){
        std::lock_guard<std::mutex> g(service_lock);
        auto opened=entry.kind==xs::EntryKind::Category?xs::service_channels(session,entry.column_id):xs::service_browse(session,entry);
        reply(fd,200,result(opened));return;
    }
    if(resource=="/api/play"){
        int variant=0;try{variant=std::stoi(param(args,"variant"));}catch(...){}
        if(variant<0||variant>20){reply(fd,400,{{"error","Variante no válida"}});return;}
        xs::SourceResult media;
        {
            std::lock_guard<std::mutex> g(service_lock);
            media=xs::service_source(session,entry,variant);
        }
        if(!media.ok){reply(fd,200,{{"ok",false},{"status",media.status}});return;}
        const auto suffix=media.source.find(".mp4")!=std::string::npos?".mp4":
            media.source.find(".m3u8")!=std::string::npos?".m3u8":
            media.source.find(".mpd")!=std::string::npos?".mpd":".ts";
        const auto ticket=random_hex()+suffix;{
            std::lock_guard<std::mutex> guard(catalog_lock);
            tickets[ticket]={media,std::chrono::steady_clock::now()};
            if(tickets.size()>800)tickets.clear();
        }
        // Always proxy: the native Vega player does not support origin-specific auth headers.
        reply(fd,200,{{"ok",true},{"status",media.status},
          {"url",public_origin+"/media/"+ticket},{"variants",media.variants}});return;
    }
    reply(fd,404,{{"error","Ruta desconocida"}});
}
} // namespace

// La app ejecuta este servicio DENTRO del Fire TV. No escucha en LAN:
// se limita a 127.0.0.1 y termina junto al proceso Vega.
namespace {
std::mutex gateway_start_lock;
bool gateway_started=false;
}
std::string xuper_start_local_gateway(const std::string& config,const std::string& key,int port){
    std::lock_guard<std::mutex> guard(gateway_start_lock);
    if(gateway_started)return public_origin;
    if(port<1024||port>65535||key.size()<32||key.size()>128)return "ERROR: puerto o clave local inválidos";
    std::ifstream f(config);
    if(!f)return "ERROR: falta la configuración privada en /data";
    f.close();
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return "ERROR: no se pudo iniciar la biblioteca HTTP";
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)return "ERROR: el sistema no permitió abrir el socket local";
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0){close(fd);return "ERROR: no se pudo iniciar el puerto local";}
    if(listen(fd,16)!=0){close(fd);return "ERROR: no se pudo activar el servicio local";}
    config_path=config;
    secret=key;
    public_origin="http://127.0.0.1:"+std::to_string(port);
    gateway_started=true;
    std::thread([fd]{
        while(true){
            int peer=accept(fd,nullptr,nullptr);
            if(peer<0)continue;
            std::thread([peer]{
                try{client(peer);}catch(const std::exception&){/* sin exponer identificadores */}
            }).detach();
        }
    }).detach();
    return public_origin;
}
