#include "catalog.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>

namespace xs {
static std::string trim(std::string s) {
    const auto a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
bool valid_source(const std::string& s) {
    if(s.empty() || s.size()>8192) return false;
    for(unsigned char ch:s) if(ch<32 || ch==127 || ch=='"' || ch=='\\') return false;
#if defined(XUPERSWITCH_TEST) && !defined(__SWITCH__)
    if(s[0]=='/')return true;
#endif
    if(s.rfind("sdmc:/",0)==0) return s.size()>6 && s.find("/../")==std::string::npos;
    const auto prefix=s.rfind("https://",0)==0?8u:s.rfind("http://",0)==0?7u:0u;
    if(!prefix) return false;
    const auto end=s.find_first_of("/?#",prefix);
    const auto host=s.substr(prefix,end==std::string::npos?end:end-prefix);
    return !host.empty() && host.find('@')==std::string::npos &&
           std::none_of(s.begin(),s.end(),[](unsigned char c){return std::isspace(c)!=0;});
}
std::string artwork_key(const std::string& url){
    if(url.empty())return {};
    uint64_t h=14695981039346656037ULL;for(unsigned char c:url){h^=c;h*=1099511628211ULL;}
    char out[17];std::snprintf(out,sizeof(out),"%016llx",static_cast<unsigned long long>(h));return out;
}
std::string search_key(const std::string& text){
    std::string out;
    for(size_t i=0;i<text.size();++i){unsigned char c=text[i];
        if(c==0xc3&&i+1<text.size()){
            const auto d=static_cast<unsigned char>(text[++i]);
            if(d==0x81||d==0xa1||d==0x80||d==0xa0||d==0x84||d==0xa4)out+='a';
            else if(d==0x89||d==0xa9||d==0x88||d==0xa8)out+='e';
            else if(d==0x8d||d==0xad)out+='i';else if(d==0x93||d==0xb3||d==0x96||d==0xb6)out+='o';
            else if(d==0x9a||d==0xba||d==0x9c||d==0xbc)out+='u';else if(d==0x91||d==0xb1)out+='n';
        }else if(c<128&&std::isalnum(c))out+=static_cast<char>(std::tolower(c));
    }return out;
}
Catalog read_m3u(std::istream& input) {
    Catalog out; std::string line,title; bool unsupported=false; size_t number=0;
    while(std::getline(input,line)) {
        ++number;
        if(number==1 && line.rfind("\xEF\xBB\xBF",0)==0) line.erase(0,3);
        line=trim(line); if(line.empty()) continue;
        if(line.rfind("#EXTINF:",0)==0) {
            // The title separator can occur after commas inside quoted attributes.
            bool quoted=false; size_t comma=std::string::npos;
            for(size_t i=8;i<line.size();++i) {
                if(line[i]=='"') quoted=!quoted;
                else if(line[i]==',' && !quoted) {comma=i;break;}
            }
            title=comma==std::string::npos?"":trim(line.substr(comma+1));
            unsupported=false; continue;
        }
        if(line[0]=='#') {
            if(line.rfind("#EXTVLCOPT:",0)==0 || line.rfind("#KODIPROP:",0)==0) unsupported=true;
            continue;
        }
        if(unsupported || line.find('|')!=std::string::npos) {
            out.warnings.push_back("Entrada con opciones adicionales no admitidas, linea "+std::to_string(number));
        } else if(!valid_source(line)) {
            out.warnings.push_back("Origen no admitido, linea "+std::to_string(number));
        } else if(out.entries.size()>=10000) {
            out.warnings.push_back("Limite de 10000 entradas alcanzado"); break;
        } else {
            if(title.empty()) title="Video "+std::to_string(out.entries.size()+1);
            // Keep text out of terminal escape sequences without changing URLs.
            for(char& ch:title) if(static_cast<unsigned char>(ch)<32 || ch==127) ch=' ';
            if(title.size()>180) title.resize(180);
            out.entries.push_back({title,line});
        }
        title.clear(); unsupported=false;
    }
    return out;
}
}
