#include "RssFeedCache.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <functional>

#include "HtmlToText.h"
#include "RssJsonIO.h"
#include "RssParser.h"

namespace {
constexpr char RSS_DIR[] = "/.inkpoint/rss";

std::string hashUrl(const std::string& url) {
  // Same hashing approach the EPUB cache uses for paths: std::hash -> hex.
  char buf[17];
  snprintf(buf, sizeof(buf), "%016zx", std::hash<std::string>{}(url));
  return std::string(buf);
}
}  // namespace

std::string RssFeedCache::feedDir(const std::string& feedUrl) {
  return std::string(RSS_DIR) + "/feed_" + hashUrl(feedUrl);
}

std::string RssFeedCache::itemTextPath(const std::string& feedUrl, size_t index) {
  return feedDir(feedUrl) + "/item_" + std::to_string(index) + ".txt";
}

bool RssFeedCache::hasCache(const std::string& feedUrl) {
  return Storage.exists((feedDir(feedUrl) + "/index.json").c_str());
}

bool RssFeedCache::writeFeed(const std::string& feedUrl, const std::vector<RssEntry>& items) {
  const std::string dir = feedDir(feedUrl);
  Storage.ensureDirectoryExists(RSS_DIR);
  Storage.ensureDirectoryExists(dir.c_str());

  for (size_t i = 0; i < items.size(); ++i) {
    const std::string body = htmlToText(items[i].contentHtml);
    const std::string path = dir + "/item_" + std::to_string(i) + ".txt";
    if (!Storage.writeFile(path.c_str(), String(body.c_str()))) {
      LOG_ERR("RSS", "Failed to write %s", path.c_str());
      return false;
    }
  }
  const std::string index = RssJsonIO::serializeIndex(items);
  return Storage.writeFile((dir + "/index.json").c_str(), String(index.c_str()));
}

std::vector<RssEntry> RssFeedCache::readIndex(const std::string& feedUrl) {
  std::vector<RssEntry> out;
  const std::string path = feedDir(feedUrl) + "/index.json";
  if (!Storage.exists(path.c_str())) return out;
  String json = Storage.readFile(path.c_str());
  if (!json.isEmpty()) RssJsonIO::deserializeIndex(json.c_str(), out);
  return out;
}
