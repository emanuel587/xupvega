#pragma once
#include <string>
#include <cstdint>
namespace xs {
enum class RangerMode { Live, Vod };
std::string ranger_request_id(uint32_t user,uint64_t android_id,uint32_t seconds,uint32_t nonce);
std::string ranger_live_content_auth(const std::string& resource,int32_t instance,uint64_t moment_ms);
std::string ranger_slb_live_path(const std::string& code,const std::string& format);
// Reimplemented transport primitives, checked against the APK's ARM64 code.
// This module alone does not resolve or play media; it performs no network I/O.
std::string ranger_key_hex(const std::string& communication_key,RangerMode mode);
std::string ranger_iv_hex(const std::string& key_hex,const std::string& resource);
bool ranger_encode(const std::string& communication_key,RangerMode mode,
                   const std::string& plain,bool response,std::string& wire);
bool ranger_decode(const std::string& communication_key,RangerMode mode,
                   const std::string& wire,bool response,std::string& plain);
}
