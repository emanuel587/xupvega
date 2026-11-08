#include "codec.hpp"
#include <mbedtls/des.h>
#include <mbedtls/base64.h>
#include <mbedtls/md5.h>
#include <mbedtls/aes.h>
#include <algorithm>
#include <vector>
#include <array>
#include <mbedtls/version.h>

namespace xs {
// mbedTLS 3.x removed mbedtls_md5_ret(); mbedtls_md5() is its replacement.
int md5_digest(const unsigned char* input,size_t length,unsigned char output[16]){
#if defined(MBEDTLS_VERSION_MAJOR) && MBEDTLS_VERSION_MAJOR>=3
    return mbedtls_md5(input,length,output);
#else
    return mbedtls_md5_ret(input,length,output);
#endif
}
std::string derive_device_serial(const std::string& token,const std::string& salt) {
    if(token.empty()||salt.empty())return {};
    const std::string data=token+salt;unsigned char digest[16];
    if(md5_digest(reinterpret_cast<const unsigned char*>(data.data()),data.size(),digest))return {};
    std::string result;const char* hex="0123456789abcdef";
    for(auto c:digest){result+=hex[c>>4];result+=hex[c&15];}return result;
}
std::string account_type(const std::string& username) {
    if(username.find('@')!=std::string::npos)return "2";
    if(!username.empty()&&std::all_of(username.begin(),username.end(),[](unsigned char c){return c>='0'&&c<='9';}))return "1";
    return "6";
}
bool prepare_password(const std::string& password,const std::string& salt,const std::string& aes_key,std::string& prepared) {
    prepared.clear();if(password.empty()||password.size()>128||salt.empty()||aes_key.size()!=16)return false;
    // The APK's password form accepts ASCII letters, digits and underscore.
    // Its MD5 helper truncates Java chars to bytes; ASCII is identical in UTF-8.
    if(std::any_of(password.begin(),password.end(),[](unsigned char c){return c>127;}))return false;
    const std::string md5_input=password+salt;unsigned char digest[16];
    if(md5_digest(reinterpret_cast<const unsigned char*>(md5_input.data()),md5_input.size(),digest))return false;
    const char* hex="0123456789abcdef";
    for(auto c:digest){prepared+=hex[c>>4];prepared+=hex[c&15];}
    std::vector<unsigned char> bytes(password.begin(),password.end());
    const auto padding=16-bytes.size()%16;bytes.insert(bytes.end(),padding,static_cast<unsigned char>(padding));
    mbedtls_aes_context aes;mbedtls_aes_init(&aes);
    int rc=mbedtls_aes_setkey_enc(&aes,reinterpret_cast<const unsigned char*>(aes_key.data()),128);
    if(!rc)for(size_t i=0;i<bytes.size();i+=16){rc=mbedtls_aes_crypt_ecb(&aes,MBEDTLS_AES_ENCRYPT,bytes.data()+i,bytes.data()+i);if(rc)break;}
    mbedtls_aes_free(&aes);if(rc){prepared.clear();return false;}
    std::vector<unsigned char> encoded(((bytes.size()+2)/3)*4+1);size_t count=0;
    if(mbedtls_base64_encode(encoded.data(),encoded.size(),&count,bytes.data(),bytes.size())){prepared.clear();return false;}
    // Android Base64.DEFAULT inserts LF every 76 characters; outer trim removes
    // its final LF but preserves any internal line breaks.
    for(size_t i=0;i<count;++i){if(i&&i%76==0)prepared+='\n';prepared+=static_cast<char>(encoded[i]);}
    return true;
}
static int hex_digit(unsigned char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static std::vector<unsigned char> android_base64(const std::string& input) {
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string clean;for(char c:input)if(c!='\r'&&c!='\n')clean+=c;
    std::vector<unsigned char> out;
    for(size_t i=0;i+1<clean.size();i+=4) {
        int v[4]={-1,-1,-1,-1};
        for(size_t j=0;j<4&&i+j<clean.size();++j) {
            const auto p=alphabet.find(clean[i+j]);v[j]=p==std::string::npos?-1:static_cast<int>(p);
        }
        // Convert to unsigned before shifting; Java masks these low bits.
        out.push_back(((static_cast<unsigned>(v[0])<<2)&252)|((static_cast<unsigned>(v[1])>>4)&3));
        if(i+2<clean.size()&&clean[i+2]!='=')out.push_back(((static_cast<unsigned>(v[1])<<4)&240)|((static_cast<unsigned>(v[2])>>2)&15));
        if(i+3<clean.size()&&clean[i+3]!='=')out.push_back(((static_cast<unsigned>(v[2])<<6)&192)|(v[3]&63));
    }
    return out;
}
static bool crypt(std::vector<unsigned char>& bytes,const std::string& key,bool encrypt) {
    auto decoded=android_base64(key);if(decoded.size()<24)return false;
    mbedtls_des3_context ctx;mbedtls_des3_init(&ctx);
    int rc=encrypt?mbedtls_des3_set3key_enc(&ctx,decoded.data()):mbedtls_des3_set3key_dec(&ctx,decoded.data());
    if(rc==0)for(size_t i=0;i<bytes.size();i+=8) {
        rc=mbedtls_des3_crypt_ecb(&ctx,bytes.data()+i,bytes.data()+i);if(rc)break;
    }
    mbedtls_des3_free(&ctx);return rc==0;
}
bool encode_request(const std::string& plain,const std::string& key,std::string& encoded) {
    encoded.clear();if(plain.size()>8*1024*1024)return false;
    std::vector<unsigned char> bytes(plain.begin(),plain.end());
    const unsigned pad=8-bytes.size()%8;bytes.insert(bytes.end(),pad,static_cast<unsigned char>(pad));
    if(!crypt(bytes,key,true))return false;
    std::vector<unsigned char> base64(((bytes.size()+2)/3)*4+1);size_t written=0;
    if(mbedtls_base64_encode(base64.data(),base64.size(),&written,bytes.data(),bytes.size()))return false;
    const char* digits="0123456789abcdef";encoded.reserve(written*2);
    for(size_t i=0;i<written;++i){encoded+=digits[base64[i]>>4];encoded+=digits[base64[i]&15];}
    return true;
}
bool decode_response_data(const std::string& encoded,const std::string& key,std::string& plain) {
    plain.clear();if(encoded.empty()||encoded.size()%2||encoded.size()>32*1024*1024)return false;
    std::string base64;base64.reserve(encoded.size()/2);
    for(size_t i=0;i<encoded.size();i+=2) {
        int a=hex_digit(encoded[i]),b=hex_digit(encoded[i+1]);if(a<0||b<0)return false;
        base64+=static_cast<char>((a<<4)|b);
    }
    // Ciphertext itself is strict base64. The permissive decoder is needed only
    // for Android's historical key decoding behavior.
    std::vector<unsigned char> bytes(base64.size());size_t size=0;
    if(mbedtls_base64_decode(bytes.data(),bytes.size(),&size,
       reinterpret_cast<const unsigned char*>(base64.data()),base64.size()))return false;
    bytes.resize(size);if(bytes.empty()||bytes.size()%8)return false;
    if(!crypt(bytes,key,false))return false;
    const auto pad=bytes.back();if(pad==0||pad>8||pad>bytes.size())return false;
    for(size_t i=bytes.size()-pad;i<bytes.size();++i)if(bytes[i]!=pad)return false;
    plain.assign(bytes.begin(),bytes.end()-pad);return true;
}
}
