#include <gtest/gtest.h>

#include "RssParser.h"

namespace {
const char* kRss2 =
    "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel>"
    "<title>My Feed</title>"
    "<item><title>First Post</title><link>https://ex.com/1</link>"
    "<pubDate>Mon, 08 Jun 2026 10:00:00 GMT</pubDate>"
    "<description>&lt;p&gt;Hello &amp;amp; welcome&lt;/p&gt;</description></item>"
    "<item><title>Second</title><link>https://ex.com/2</link>"
    "<content:encoded><![CDATA[<p>Full <b>body</b></p>]]></content:encoded></item>"
    "</channel></rss>";

const char* kAtom =
    "<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\">"
    "<title>Atom Feed</title>"
    "<entry><title>Atom One</title>"
    "<link href=\"https://ex.com/a1\" rel=\"alternate\"/>"
    "<updated>2026-06-08T10:00:00Z</updated>"
    "<content type=\"html\">&lt;p&gt;Atom body&lt;/p&gt;</content></entry>"
    "</feed>";
}  // namespace

// Capture the streaming-sink output for assertions.
struct StreamCapture {
  struct Item {
    std::string title;
    std::string date;
    std::string content;  // streamed <content> chunks (empty if none)
    std::string fallback;  // meta.contentHtml when no content streamed
    bool wroteContent = false;
  };
  std::vector<Item> items;
  Item cur;
  void attach(RssParser& p) {
    p.setStreamingSink([this] { cur = Item{}; },
                       [this](const char* d, size_t n) { cur.content.append(d, n); },
                       [this](const RssEntry& meta, bool wrote) {
                         cur.title = meta.title;
                         cur.date = meta.date;
                         cur.wroteContent = wrote;
                         if (!wrote) cur.fallback = meta.contentHtml;
                         items.push_back(cur);
                       });
  }
};

TEST(RssParser, StreamingDeliversContentAndFallback) {
  RssParser p;
  StreamCapture cap;
  cap.attach(p);
  p.write(reinterpret_cast<const uint8_t*>(kRss2), strlen(kRss2));
  p.flush();
  ASSERT_FALSE(p.error());
  // Streaming bypasses getEntries().
  EXPECT_TRUE(p.getEntries().empty());
  ASSERT_EQ(cap.items.size(), 2u);
  // Item 0: only <description> -> no streamed content, description is the fallback body.
  EXPECT_EQ(cap.items[0].title, "First Post");
  EXPECT_FALSE(cap.items[0].wroteContent);
  EXPECT_EQ(cap.items[0].fallback, "<p>Hello &amp; welcome</p>");
  // Item 1: <content:encoded> streamed to the sink.
  EXPECT_EQ(cap.items[1].title, "Second");
  EXPECT_TRUE(cap.items[1].wroteContent);
  EXPECT_EQ(cap.items[1].content, "<p>Full <b>body</b></p>");
}

TEST(RssParser, StreamingAtomContent) {
  RssParser p;
  StreamCapture cap;
  cap.attach(p);
  p.write(reinterpret_cast<const uint8_t*>(kAtom), strlen(kAtom));
  p.flush();
  ASSERT_FALSE(p.error());
  ASSERT_EQ(cap.items.size(), 1u);
  EXPECT_EQ(cap.items[0].title, "Atom One");
  EXPECT_TRUE(cap.items[0].wroteContent);
  EXPECT_EQ(cap.items[0].content, "<p>Atom body</p>");
}

TEST(RssParser, ParsesRss2Items) {
  RssParser p;
  p.write(reinterpret_cast<const uint8_t*>(kRss2), strlen(kRss2));
  p.flush();
  ASSERT_FALSE(p.error());
  const auto& items = p.getEntries();
  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].title, "First Post");
  EXPECT_EQ(items[0].link, "https://ex.com/1");
  EXPECT_EQ(items[0].date, "Mon, 08 Jun 2026 10:00:00 GMT");
  // description content (entities resolved by Expat into the text)
  EXPECT_EQ(items[0].contentHtml, "<p>Hello &amp; welcome</p>");
  // content:encoded preferred over description when both present
  EXPECT_EQ(items[1].contentHtml, "<p>Full <b>body</b></p>");
}

TEST(RssParser, ParsesAtomEntries) {
  RssParser p;
  p.write(reinterpret_cast<const uint8_t*>(kAtom), strlen(kAtom));
  p.flush();
  ASSERT_FALSE(p.error());
  const auto& items = p.getEntries();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].title, "Atom One");
  EXPECT_EQ(items[0].link, "https://ex.com/a1");
  EXPECT_EQ(items[0].date, "2026-06-08T10:00:00Z");
  EXPECT_EQ(items[0].contentHtml, "<p>Atom body</p>");
}

TEST(RssParser, ChannelTitleNotAnItem) {
  RssParser p;
  p.write(reinterpret_cast<const uint8_t*>(kRss2), strlen(kRss2));
  p.flush();
  // "My Feed" channel title must not become an item.
  for (const auto& it : p.getEntries()) EXPECT_NE(it.title, "My Feed");
}
