#pragma once
#include <string>
#include <vector>

struct RssFeed;
struct RssEntry;

namespace RssJsonIO {
// feeds.json: [{"name":...,"url":...}, ...]
std::string serializeFeeds(const std::vector<RssFeed>& feeds);
bool deserializeFeeds(const char* json, std::vector<RssFeed>& out);

// per-feed index.json: [{"title":...,"date":...}, ...] (one object per cached item;
// the body text lives in item_<n>.txt). `link` is not needed after caching.
std::string serializeIndex(const std::vector<RssEntry>& items);
bool deserializeIndex(const char* json, std::vector<RssEntry>& out);
}  // namespace RssJsonIO
