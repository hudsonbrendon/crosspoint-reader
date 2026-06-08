#pragma once
#include <string>
#include <vector>

struct RssEntry;

// Offline cache for one feed's items, keyed by a hash of the feed URL.
namespace RssFeedCache {
// feed_<hash> directory path under /.inkpoint/rss/ for a feed URL.
std::string feedDir(const std::string& feedUrl);

// Write index.json + item_<n>.txt (htmlToText of each item's content). Returns false on I/O error.
bool writeFeed(const std::string& feedUrl, const std::vector<RssEntry>& items);

// Read the cached item list (title + date) for offline browsing. Empty if none cached.
std::vector<RssEntry> readIndex(const std::string& feedUrl);

// Path to a cached item's plaintext file (item_<index>.txt). Caller launches the TXT reader on it.
std::string itemTextPath(const std::string& feedUrl, size_t index);

// True if a cache exists for this feed.
bool hasCache(const std::string& feedUrl);
}  // namespace RssFeedCache
