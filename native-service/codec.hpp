#pragma once
#include <string>
#include <cstddef>
namespace xs {
// Android protocol: 3DES-ECB/PKCS5, base64, then hexadecimal ASCII.
// key_base64 is decoded as in the APK; DESedeKeySpec uses its first 24 bytes.
bool encode_request(const std::string& plain,const std::string& key_base64,std::string& encoded);
bool decode_response_data(const std::string& encoded,const std::string& key_base64,std::string& plain);
bool prepare_password(const std::string& password,const std::string& salt,const std::string& aes_key,std::string& prepared);
std::string account_type(const std::string& username);
std::string derive_device_serial(const std::string& token,const std::string& salt);
// MD5 over a buffer; works with mbedTLS 2.x and 3.x. Returns 0 on success.
int md5_digest(const unsigned char* input,size_t length,unsigned char output[16]);
}
