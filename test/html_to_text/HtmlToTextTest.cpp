#include <gtest/gtest.h>

#include "HtmlToText.h"

TEST(HtmlToText, StripsTagsAndDecodesEntities) {
  EXPECT_EQ(htmlToText("<p>Hello <b>world</b> &amp; friends</p>"), "Hello world & friends");
}
TEST(HtmlToText, ParagraphsBecomeBlankLines) {
  EXPECT_EQ(htmlToText("<p>One</p><p>Two</p>"), "One\n\nTwo");
}
TEST(HtmlToText, BreaksBecomeNewlines) {
  EXPECT_EQ(htmlToText("a<br>b<br/>c"), "a\nb\nc");
}
TEST(HtmlToText, DropsScriptAndStyle) {
  EXPECT_EQ(htmlToText("keep<script>var x=1;</script><style>.a{}</style>end"), "keepend");
}
TEST(HtmlToText, CollapsesWhitespace) {
  EXPECT_EQ(htmlToText("a   \n\t  b"), "a b");
}
TEST(HtmlToText, NumericEntities) {
  EXPECT_EQ(htmlToText("caf&#233; &#xe9;"), "café é");
}
