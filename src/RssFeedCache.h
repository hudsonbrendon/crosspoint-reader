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

// --- Streaming API (memory-safe for full-text feeds) ---
// Ensure the feed's cache directory exists. Call once before writeItemText().
bool ensureFeedDir(const std::string& feedUrl);
// Write one item's already-stripped plaintext to item_<index>.txt.
bool writeItemText(const std::string& feedUrl, size_t index, const std::string& text);
// Write index.json from a list of items (only title + date are used).
bool writeIndex(const std::string& feedUrl, const std::vector<RssEntry>& items);

// Read the cached item list (title + date) for offline browsing. Empty if none cached.
std::vector<RssEntry> readIndex(const std::string& feedUrl);

// Path to a cached item's content file (item_<index>.txt holds the raw capped HTML).
std::string itemTextPath(const std::string& feedUrl, size_t index);

// Path to the reusable "currently reading" plaintext file for this feed
// (reading.txt). openSelectedItem() writes the stripped article here and opens it.
std::string readingTextPath(const std::string& feedUrl);

// Build a filesystem-safe slug from an article title.
// Lowercases ASCII, replaces runs of non-alphanumeric chars with '-',
// trims leading/trailing '-', caps at 48 chars. Falls back to "article".
std::string rssSlug(const std::string& title);

// True if a cache exists for this feed.
bool hasCache(const std::string& feedUrl);
}  // namespace RssFeedCache
