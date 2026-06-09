#pragma once
#include <Print.h>
#include <expat.h>

#include <functional>
#include <string>
#include <vector>

// One feed item (RSS 2.0 <item> or Atom <entry>).
struct RssEntry {
  std::string title;
  std::string link;         // RSS <link> text or Atom <link rel="alternate" href>
  std::string date;         // RSS <pubDate> or Atom <updated> (raw string)
  std::string contentHtml;  // <content:encoded> | <description> (RSS) / <content> | <summary> (Atom)
};

// Streaming Expat parser for RSS 2.0 and Atom feeds.
// Usage: feed bytes via write(); call flush(); read getEntries().
class RssParser final : public Print {
 public:
  RssParser();
  ~RssParser();
  RssParser(const RssParser&) = delete;
  RssParser& operator=(const RssParser&) = delete;

  size_t write(uint8_t) override;
  size_t write(const uint8_t*, size_t) override;
  void flush() override;

  bool error() const { return errorOccured; }
  operator bool() { return !errorOccured; }

  const std::vector<RssEntry>& getEntries() const& { return entries; }
  std::vector<RssEntry> getEntries() && { return std::move(entries); }
  void clear();

  // Streaming sink: the memory-safe path for full-text feeds. When set, the big
  // <content:encoded>/<content> field is delivered chunk-by-chunk to onContent
  // (the consumer writes straight to SD) and is NEVER buffered in RAM — crucial
  // because free heap during a live TLS/HTTPS fetch is only tens of KB on the
  // 380KB device. onBegin fires at <item>/<entry> start; onEnd fires at close
  // with the lightweight metadata (title/date) and, when no content streamed,
  // the small description fallback in meta.contentHtml. Without a sink, items
  // accumulate in `entries` (used by the unit tests via getEntries()).
  using ContentSink = std::function<void(const char* data, size_t len)>;
  using ItemBoundary = std::function<void()>;
  using ItemComplete = std::function<void(const RssEntry& meta, bool wroteContent)>;
  void setStreamingSink(ItemBoundary onBegin, ContentSink onContent, ItemComplete onEnd) {
    streamBegin = std::move(onBegin);
    streamContent = std::move(onContent);
    streamEnd = std::move(onEnd);
    streaming = true;
  }

  // Hard cap on bytes buffered for any single SMALL field (title/date/
  // description). The field buffer is reserved to this size ONCE (constructor)
  // so appends never reallocate — std::string capacity-doubling would otherwise
  // spike and OOM under TLS heap pressure. The big content field is streamed,
  // not buffered, so it is not bounded here (the consumer caps the file).
  static constexpr size_t MAX_FIELD_BYTES = 8 * 1024;

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);
  static const char* findAttribute(const XML_Char** atts, const char* name);
  // Returns the part after the last ':' so namespaced tags (atom:title) match bare names.
  static const char* localName(const XML_Char* name);

  XML_Parser parser = nullptr;
  std::vector<RssEntry> entries;
  bool streaming = false;
  bool wroteContent = false;  // did the content field stream any bytes this item
  ItemBoundary streamBegin;
  ContentSink streamContent;
  ItemComplete streamEnd;
  RssEntry current;
  std::string descriptionHtml;  // holds <description>/<summary> until we know if <content:encoded> exists
  std::string text;             // accumulator for the element currently being read

  bool inItem = false;   // inside <item> or <entry>
  bool inTitle = false;
  bool inLink = false;       // RSS <link> (text content)
  bool inDate = false;
  bool inDescription = false;
  bool inContentEncoded = false;  // content:encoded OR atom <content>
  bool errorOccured = false;
};
