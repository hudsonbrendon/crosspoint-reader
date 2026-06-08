#pragma once
#include <Print.h>
#include <expat.h>

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

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);
  static const char* findAttribute(const XML_Char** atts, const char* name);
  // Returns the part after the last ':' so namespaced tags (atom:title) match bare names.
  static const char* localName(const XML_Char* name);

  XML_Parser parser = nullptr;
  std::vector<RssEntry> entries;
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
