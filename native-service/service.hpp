#pragma once
#include "catalog.hpp"
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <functional>
namespace xs {
void service_cancel_pending();
void service_reset_cancel();
struct ServiceSession;
struct LoginAttempt {
    bool authenticated=false;
    Catalog catalog;
    std::string status;
    std::shared_ptr<ServiceSession> session;
};
struct BrowseResult {bool ok=false;Catalog catalog;std::string status;};
struct SubtitleSource {std::string language,url,md5;};
struct SourceResult {bool ok=false;std::string source;std::string status;std::vector<std::pair<std::string,std::string>> headers{};std::vector<std::string> variants{};std::vector<SubtitleSource> subtitles{};};
bool service_subtitle(const std::shared_ptr<ServiceSession>& session,const SubtitleSource& subtitle,std::string& data,std::string& error);
// Tokens remain in RAM and never enter a catalogue entry or diagnostic log.
LoginAttempt service_login(const std::string& config_path,const std::string& username,const std::string& password);
LoginAttempt service_bootstrap(const std::string& config_path);
BrowseResult service_root(const std::shared_ptr<ServiceSession>& session);
BrowseResult service_home_assets(const std::shared_ptr<ServiceSession>& session,const Catalog& root);
BrowseResult service_channels(const std::shared_ptr<ServiceSession>& session,int column_id);
BrowseResult service_browse(const std::shared_ptr<ServiceSession>& session,const Entry& entry);
BrowseResult service_search(const std::shared_ptr<ServiceSession>& session,const std::string& query,int page=1);
BrowseResult service_search_all(const std::shared_ptr<ServiceSession>& session,const std::string& query,const std::function<bool(size_t,size_t)>& progress={});
SourceResult service_source(const std::shared_ptr<ServiceSession>& session,const Entry& entry,int variant=0);
}
