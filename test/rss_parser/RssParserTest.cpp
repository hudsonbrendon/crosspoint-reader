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
