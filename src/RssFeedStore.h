#pragma once
#include <string>
#include <vector>

struct RssFeed {
  std::string name;  // display name (derived from feed <title> or the URL host)
  std::string url;
};

// Singleton storing subscribed RSS/Atom feeds at /.inkpoint/rss/feeds.json.
class RssFeedStore {
 private:
  static RssFeedStore instance;
  std::vector<RssFeed> feeds;
  static constexpr size_t MAX_FEEDS = 16;
  RssFeedStore() = default;

 public:
  RssFeedStore(const RssFeedStore&) = delete;
  RssFeedStore& operator=(const RssFeedStore&) = delete;
  static RssFeedStore& getInstance() { return instance; }

  bool saveToFile() const;
  bool loadFromFile();

  bool addFeed(const RssFeed& feed);
  bool removeFeed(size_t index);

  const std::vector<RssFeed>& getFeeds() const { return feeds; }
  const RssFeed* getFeed(size_t index) const;
  size_t getCount() const { return feeds.size(); }
  bool hasFeeds() const { return !feeds.empty(); }
};

#define RSS_STORE RssFeedStore::getInstance()
