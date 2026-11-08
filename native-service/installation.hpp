#pragma once
#define JSON_NOEXCEPTION 1
#include "json.hpp"
#include "service_embedded.hpp"
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <functional>
#include <string>
#include <sys/stat.h>
namespace xs {
using InstallationJson=nlohmann::json;
inline bool read_installation_json(const std::string& path,InstallationJson& out){
    std::ifstream f(path,std::ios::binary|std::ios::ate);const auto n=f.tellg();if(n<0||n>65536)return false;
    f.seekg(0);std::string data(static_cast<size_t>(n),'\0');f.read(data.data(),n);if(!f)return false;
    out=InstallationJson::parse(data,nullptr,false);return out.is_object();
}
// A common template has no installation/account identity. Existing installations
// always keep their configuration; a missing config with activation state is not
// replaced by a new identity. Generation happens only before the first API call.
inline bool load_installation(const std::string& path,InstallationJson& config,std::string& error,
                              const std::function<bool(unsigned char*,size_t)>& entropy){
    struct stat st{};
    if(stat(path.c_str(),&st)==0){if(read_installation_json(path,config))return true;error="Configuracion existente invalida; restaura service.json.";return false;}
    if(errno!=ENOENT){error="No se pudo leer la configuracion de esta instalacion.";return false;}
    const auto slash=path.find_last_of("/\\");const std::string folder=slash==std::string::npos?"":path.substr(0,slash+1);
    if(stat((folder+"device_state.json").c_str(),&st)==0){error="Conserva la activacion: restaura tu service.json anterior.";return false;}
    if(errno!=ENOENT){error="No se pudo comprobar el estado de activacion.";return false;}
    if(!read_installation_json(folder+"service.template.json",config)){error="Falta service.template.json junto al NRO.";return false;}
    for(const char* key:{"installation_serial","device_id","userId","userToken","sn","password","username"})if(config.contains(key)){
        error="La plantilla publica contiene datos de instalacion; usa el paquete oficial.";return false;
    }
    auto nonempty=[&](const char* key){auto i=config.find(key);return i!=config.end()&&i->is_string()&&!i->get_ref<const std::string&>().empty();};
    if(!config.contains("portal_hosts")||!config["portal_hosts"].is_array()||config["portal_hosts"].empty()||!nonempty("key_base64")||!nonempty("password_salt")||!nonempty("app_id")){
        error="Plantilla de conexion incompleta.";return false;
    }
    unsigned char random[22]{};if(!entropy(random,sizeof(random))){error="No se pudo crear la identidad de esta instalacion.";return false;}
    const char* hex="0123456789abcdef";std::string serial;
    for(size_t i=0;i<16;++i){serial+=hex[random[i]>>4];serial+=hex[random[i]&15];}
    random[16]=(random[16]&0xfe)|0x02;std::string device;
    for(size_t i=16;i<22;++i){if(i>16)device+=':';device+=hex[random[i]>>4];device+=hex[random[i]&15];}
    config["installation_serial"]=serial;config["device_id"]=device;
    const auto temp=path+".tmp";std::ofstream f(temp,std::ios::binary|std::ios::trunc);const auto data=config.dump(2)+"\n";f.write(data.data(),data.size());f.close();
    if(!f.good()||std::rename(temp.c_str(),path.c_str())!=0){error="No se pudo guardar service.json en la SD.";return false;}
    return true;
}
}
