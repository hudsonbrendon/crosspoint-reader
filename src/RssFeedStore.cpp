#include "RssFeedStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include "RssJsonIO.h"

RssFeedStore RssFeedStore::instance;

namespace {
constexpr char RSS_DIR[] = "/.inkpoint/rss";
constexpr char FEEDS_JSON[] = "/.inkpoint/rss/feeds.json";
}  // namespace

bool RssFeedStore::saveToFile() const {
  Storage.ensureDirectoryExists(RSS_DIR);
  return Storage.writeFile(FEEDS_JSON, String(RssJsonIO::serializeFeeds(feeds).c_str()));
}

bool RssFeedStore::loadFromFile() {
  if (!Storage.exists(FEEDS_JSON)) return false;
  String json = Storage.readFile(FEEDS_JSON);
  if (json.isEmpty()) return false;
  feeds.clear();
  return RssJsonIO::deserializeFeeds(json.c_str(), feeds);
}

bool RssFeedStore::addFeed(const RssFeed& feed) {
  if (feeds.size() >= MAX_FEEDS) {
    LOG_DBG("RSS", "Feed limit %zu reached", MAX_FEEDS);
    return false;
  }
  feeds.push_back(feed);
  return saveToFile();
}

bool RssFeedStore::removeFeed(size_t index) {
  if (index >= feeds.size()) return false;
  feeds.erase(feeds.begin() + static_cast<ptrdiff_t>(index));
  return saveToFile();
}

const RssFeed* RssFeedStore::getFeed(size_t index) const {
  if (index >= feeds.size()) return nullptr;
  return &feeds[index];
}
