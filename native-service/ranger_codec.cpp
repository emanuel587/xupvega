#include "ranger_codec.hpp"
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstdio>
namespace xs {
namespace {
constexpr size_t max_cipher=65536;
const std::string alphabet="jWB7YtC3n9iXbEkUcJl1VxF4STpQoOIaRmh2M-efAgLwPqGr6uyD5vNsdH_Kz0Z8";
const std::string standard="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string hex(const unsigned char* bytes,size_t count){
    const char* digits="0123456789abcdef";std::string result;result.reserve(count*2);
    for(size_t i=0;i<count;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;
}
uint32_t rot32(uint32_t x,unsigned n){return (x<<n)|(x>>(32-n));}
uint64_t rot64(uint64_t x,unsigned n){return (x<<n)|(x>>(64-n));}
uint64_t little(const unsigned char* data,size_t count){uint64_t result=0;for(size_t i=0;i<count;++i)result|=uint64_t(data[i])<<(i*8);return result;}
std::array<unsigned char,16> modified_md5(const std::string& data){
    static constexpr uint32_t table[64]={
    0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
    0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
    0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
    0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
    0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
    0x289b7ec6u,0xeaa127fau,0xd46f3085u,0x04881d05u,0xd9d4d039u,0xe6bd99e5u,0x1fa27cf8u,0xc4ac5665u,
    0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffecc47du,0x85845dd1u,
    0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2da7d2bbu,0xeb86d391u
    };
    static constexpr unsigned shifts[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    static constexpr unsigned first[16]={10,11,12,13,14,15,6,7,8,9,0,1,2,3,4,5};
    std::vector<unsigned char> bytes(data.begin(),data.end());const uint64_t bits=uint64_t(bytes.size())*8;
    bytes.push_back(128);while(bytes.size()%64!=56)bytes.push_back(0);
    for(unsigned i=0;i<8;++i)bytes.push_back(static_cast<unsigned char>(bits>>(8*i)));
    uint32_t state[4]={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u};
    for(size_t block=0;block<bytes.size();block+=64){
        uint32_t words[16];for(unsigned i=0;i<16;++i)words[i]=static_cast<uint32_t>(little(bytes.data()+block+i*4,4));
        uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
        for(unsigned i=0;i<64;++i){uint32_t f;unsigned g;
            if(i<16){f=(b&c)|(~b&d);g=first[i];}
            else if(i<32){f=(d&b)|(~d&c);g=(5*i+1)%16;}
            else if(i<48){f=b^c^d;g=(3*i+5)%16;}
            else{f=c^(b|~d);g=(7*i)%16;}
            const uint32_t next=b+rot32(a+f+table[i]+words[g],shifts[i]);a=d;d=c;c=b;b=next;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
    }
    std::array<unsigned char,16> out{};
    for(unsigned i=0;i<16;++i)out[i]=static_cast<unsigned char>(state[i/4]>>(8*(i%4)));
    return out;
}
uint64_t mix64(uint64_t x){x^=x>>33;x*=0xff51afd7ed558ccdULL;x^=x>>33;x*=0xc4ceb9fe1a85ec53ULL;return x^(x>>33);}
std::array<unsigned char,16> murmur(const std::string& data){
    const auto* bytes=reinterpret_cast<const unsigned char*>(data.data());
    constexpr uint64_t c1=0x87c37b91114253d5ULL,c2=0x4cf5ad432745937fULL;
    uint64_t h1=1597135542,h2=h1;
    for(size_t i=0;i+16<=data.size();i+=16){uint64_t k1=little(bytes+i,8),k2=little(bytes+i+8,8);
        h1^=rot64(k1*c1,31)*c2;h1=rot64(h1,27);h1=(h1+h2)*5+0x52dce729;
        h2^=rot64(k2*c2,33)*c1;h2=rot64(h2,31);h2=(h2+h1)*5+0x38495ab5;
    }
    const size_t rest=data.size()%16;const auto* tail=bytes+(data.size()/16)*16;
    if(rest>8)h2^=rot64(little(tail+8,rest-8)*c2,33)*c1;
    if(rest)h1^=rot64(little(tail,std::min(size_t(8),rest))*c1,31)*c2;
    h1^=data.size();h2^=data.size();h1+=h2;h2+=h1;h1=mix64(h1);h2=mix64(h2);h1+=h2;h2+=h1;
    std::array<unsigned char,16> out{};for(unsigned i=0;i<8;++i){out[i]=static_cast<unsigned char>(h1>>(i*8));out[i+8]=static_cast<unsigned char>(h2>>(i*8));}return out;
}
bool unhex(const std::string& value,std::array<unsigned char,16>& out){
    if(value.size()!=32)return false;
    const std::string digits="0123456789abcdef";
    for(size_t i=0;i<16;++i){const auto a=digits.find(value[i*2]),b=digits.find(value[i*2+1]);if(a==std::string::npos||b==std::string::npos)return false;out[i]=static_cast<unsigned char>((a<<4)|b);}return true;
}
std::string route(RangerMode mode){return mode==RangerMode::Live?"/slb/v9/live":"/slb/v11/vod";}
bool material(const std::string& seed,RangerMode mode,bool response,std::array<unsigned char,16>& key,std::array<unsigned char,16>& iv){
    const auto key_hex=ranger_key_hex(seed,mode);
    return unhex(key_hex,key)&&unhex(ranger_iv_hex(key_hex,(response?"resp:":"req:")+route(mode)),iv);
}
std::string encode64(const std::vector<unsigned char>& bytes){
    std::vector<unsigned char> encoded(((bytes.size()+2)/3)*4+1);size_t count=0;
    if(mbedtls_base64_encode(encoded.data(),encoded.size(),&count,bytes.data(),bytes.size()))return {};
    std::string result;result.reserve(count);
    for(size_t i=0;i<count;++i){const auto index=standard.find(static_cast<char>(encoded[i]));if(index!=std::string::npos)result+=alphabet[index];}return result;
}
bool decode64(const std::string& wire,std::vector<unsigned char>& bytes){
    if(wire.empty()||wire.size()>((max_cipher+2)/3)*4+4)return false;
    std::string canonical=wire;
    // Native decoder 0x6790ac accepts '='-terminated responses. Its encoder
    // emits unpadded requests, so response framing must not use encoder equality verbatim.
    if(canonical.find('=')!=std::string::npos)while(!canonical.empty()&&(canonical.back()=='\r'||canonical.back()=='\n'))canonical.pop_back();
    size_t padding=0;while(!canonical.empty()&&canonical.back()=='='){canonical.pop_back();++padding;}
    if(padding&&(padding>2||padding!=(4-canonical.size()%4)%4))return false;
    if(canonical.empty()||canonical.size()%4==1)return false;
    std::string encoded;encoded.reserve(canonical.size()+3);
    for(char c:canonical){const auto index=alphabet.find(c);if(index==std::string::npos)return false;encoded+=standard[index];}
    while(encoded.size()%4)encoded+='=';
    bytes.resize((encoded.size()/4)*3);size_t count=0;
    if(mbedtls_base64_decode(bytes.data(),bytes.size(),&count,reinterpret_cast<const unsigned char*>(encoded.data()),encoded.size()))return false;
    bytes.resize(count);return count&&count<=max_cipher&&count%16==0&&encode64(bytes)==canonical;
}
}
std::string ranger_request_id(uint32_t user,uint64_t android_id,uint32_t seconds,uint32_t nonce){
    // Native 0x674b20: Murmur3 x86_32(seed=42), network-order fields,
    // then a nonce-rotated alphabet. This is a request identifier, not a login token.
    const unsigned prefix=0x40|(nonce&63);char source[64];
    std::snprintf(source,sizeof(source),"%02x%08x%016llx%08x%08x",prefix,user,
                  static_cast<unsigned long long>(android_id),seconds,nonce);
    uint32_t hash=42;const auto* bytes=reinterpret_cast<const unsigned char*>(source);
    for(unsigned i=0;i<40;i+=4){uint32_t k=static_cast<uint32_t>(little(bytes+i,4));
        k=rot32(k*0xcc9e2d51u,15)*0x1b873593u;hash=rot32(hash^k,13)*5+0xe6546b64u;}
    uint32_t tail=static_cast<uint32_t>(little(bytes+40,2));
    hash^=rot32(tail*0xcc9e2d51u,15)*0x1b873593u;hash^=42;
    hash^=hash>>16;hash*=0x85ebca6bu;hash^=hash>>13;hash*=0xc2b2ae35u;hash^=hash>>16;
    std::vector<unsigned char> payload;
    for(uint32_t field:{hash,user,static_cast<uint32_t>(android_id>>32),static_cast<uint32_t>(android_id),seconds,nonce})
        for(int shift=24;shift>=0;shift-=8)payload.push_back(static_cast<unsigned char>(field>>shift));
    const std::string base="B8oOvNYtn9RPijWXbcJqGyD5Eklmh21V_efAgLC3wxF4STpQr6usdHM-Kz0IaUZ7";
    const size_t offset=nonce&63;const std::string rotated=base.substr(offset)+base.substr(0,offset);
    unsigned char encoded[33];size_t length=0;
    if(mbedtls_base64_encode(encoded,sizeof(encoded),&length,payload.data(),payload.size())||length!=32)return {};
    char start[3];std::snprintf(start,sizeof(start),"%02x",prefix);std::string result=start;
    for(size_t i=0;i<length;++i)result+=rotated[standard.find(static_cast<char>(encoded[i]))];
    return result;
}
std::string ranger_key_hex(const std::string& seed,RangerMode mode){
    if(seed.size()>4096)return {};
    const auto digest=modified_md5(seed+(mode==RangerMode::Live?"93DDB237F6944625B0A1914C5ED4A15F":"9B4F4CE2168E11EF8DCE000C297524BB"));
    return hex(digest.data(),digest.size());
}
std::string ranger_live_content_auth(const std::string& resource,int32_t instance,uint64_t moment_ms){
    if(resource.empty()||resource.size()>8192||std::any_of(resource.begin(),resource.end(),[](unsigned char c){return c<32||c==127;}))return {};
    // Native BuildReq(0x46de48) signs Uri::resource(), not the SLB authorization.
    // Native signer 0x6752e0 hashes the unescaped suffix following the first token=.
    const auto token=resource.find("token=");if(token==std::string::npos||token+6==resource.size())return resource;
    const std::string extra="&sign2_method=sign_o3&instance="+std::to_string(instance)+"&start_moment="+std::to_string(moment_ms);
    static constexpr unsigned char salt[]={0x73,0x61,0x6c,0x74,0x33,0x33,0x33,0x33,0x3d,0x34,0x98,0x0d,0x0a,0x15,0x32,0xc9,0xc3,0x82,0x17,0x08,0xc0};
    const auto digest=modified_md5(resource.substr(token)+extra+std::string(reinterpret_cast<const char*>(salt),sizeof(salt)));
    return resource+extra+"&sign2="+hex(digest.data(),digest.size());
}
std::string ranger_slb_live_path(const std::string& code,const std::string& format){
    // Original 0x480314 chooses suffix, 0x4804f4 builds /live/ + Media.name + suffix.
    if(code.empty()||code.size()>512||code=="."||code==".."||std::any_of(code.begin(),code.end(),[](unsigned char c){
        return c<=32||c>=127||c=='/'||c=='\\'||c=='?'||c=='#'||c=='"';}))return {};
    return "/live/"+code+(format=="mp4"?".mp4":".ts");
}
std::string ranger_iv_hex(const std::string& key,const std::string& resource){
    std::array<unsigned char,16> decoded{};if(!unhex(key,decoded)||resource.size()>4096)return {};
    const auto digest=murmur("http://encrypt.io?scheme=aes-cbc-128-o2&key="+key+"&resource="+resource);
    return hex(digest.data(),digest.size());
}
bool ranger_encode(const std::string& seed,RangerMode mode,const std::string& plain,bool response,std::string& wire){
    wire.clear();if(plain.size()>=max_cipher)return false;
    std::array<unsigned char,16> key{},iv{};if(!material(seed,mode,response,key,iv))return false;
    std::vector<unsigned char> bytes(plain.begin(),plain.end());const size_t padding=16-bytes.size()%16;bytes.insert(bytes.end(),padding,static_cast<unsigned char>(padding));
    std::vector<unsigned char> encrypted(bytes.size());mbedtls_aes_context aes;mbedtls_aes_init(&aes);
    int rc=mbedtls_aes_setkey_enc(&aes,key.data(),128);
    if(!rc)rc=mbedtls_aes_crypt_cbc(&aes,MBEDTLS_AES_ENCRYPT,bytes.size(),iv.data(),bytes.data(),encrypted.data());
    mbedtls_aes_free(&aes);if(rc)return false;wire=encode64(encrypted);return !wire.empty();
}
bool ranger_decode(const std::string& seed,RangerMode mode,const std::string& wire,bool response,std::string& plain){
    plain.clear();std::vector<unsigned char> bytes;if(!decode64(wire,bytes))return false;
    std::array<unsigned char,16> key{},iv{};if(!material(seed,mode,response,key,iv))return false;
    std::vector<unsigned char> decrypted(bytes.size());mbedtls_aes_context aes;mbedtls_aes_init(&aes);
    int rc=mbedtls_aes_setkey_dec(&aes,key.data(),128);
    if(!rc)rc=mbedtls_aes_crypt_cbc(&aes,MBEDTLS_AES_DECRYPT,bytes.size(),iv.data(),bytes.data(),decrypted.data());
    mbedtls_aes_free(&aes);if(rc)return false;
    const size_t padding=decrypted.back();if(!padding||padding>16||padding>decrypted.size())return false;
    if(!std::all_of(decrypted.end()-padding,decrypted.end(),[&](unsigned char c){return c==padding;}))return false;
    plain.assign(reinterpret_cast<const char*>(decrypted.data()),decrypted.size()-padding);return true;
}
}
