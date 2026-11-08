#include "gateway_native.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
static bool readable(const std::string& path){struct stat st{};return stat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_size>2&&st.st_size<65536;}
static bool copy_once(const std::string& source,const std::string& dest){
    if(readable(dest))return true; // never overwrite account state after app updates
    if(!readable(source))return false;
    std::ifstream in(source,std::ios::binary);std::ofstream out(dest,std::ios::binary|std::ios::trunc);
    if(!in||!out)return false;
    out<<in.rdbuf();out.flush();
    if(!out.good()){std::remove(dest.c_str());return false;}
    chmod(dest.c_str(),0600);
    return true;
}
}
std::string xuper_launch_on_device(const std::string& key,int port){
    // App /pkg is immutable, /data persists across updates. Credentials never
    // traverse the LAN and are only read by the native process on Fire TV.
    if(mkdir("/data/xuper-nf",0700)!=0&&errno!=EEXIST)
        return "ERROR: Vega OS denegó el directorio privado";
    const std::string home="/data/xuper-nf/";
    if(!copy_once("/pkg/assets/private/service.json",home+"service.json"))
        return "ERROR: copia tu service.json de Windows antes de compilar";
    if(!copy_once("/pkg/assets/private/device_state.json",home+"device_state.json"))
        return "ERROR: copia tu device_state.json de Windows antes de compilar";
    return xuper_start_local_gateway(home+"service.json",key,port);
}
