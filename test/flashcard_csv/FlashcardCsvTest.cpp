#include <gtest/gtest.h>

#include "FlashcardCsv.h"

// -------------------------------------------------------------------------
// csvEscapeField
// -------------------------------------------------------------------------

TEST(FlashcardCsv, EscapeField_NoSpecialChars_Unchanged) { EXPECT_EQ(csvEscapeField("hello"), "hello"); }

TEST(FlashcardCsv, EscapeField_WithComma_GetsQuoted) {
  // A field containing a comma must be wrapped in double quotes.
  EXPECT_EQ(csvEscapeField("hello,world"), "\"hello,world\"");
}

TEST(FlashcardCsv, EscapeField_WithEmbeddedQuote_DoublesQuote) {
  // RFC-4180: embedded " becomes "" inside outer quotes.
  EXPECT_EQ(csvEscapeField("say \"hi\""), "\"say \"\"hi\"\"\"");
}

TEST(FlashcardCsv, EscapeField_WithNewlineToken_GetsQuoted) {
  // Fields containing the /n storage token must be quoted.
  std::string in = "line1/nline2";
  std::string out = csvEscapeField(in);
  EXPECT_EQ(out.front(), '"');
  EXPECT_EQ(out.back(), '"');
  // The /n token must be preserved verbatim inside the quotes.
  EXPECT_NE(out.find("/n"), std::string::npos);
}

// -------------------------------------------------------------------------
// csvUnquoteField
// -------------------------------------------------------------------------

TEST(FlashcardCsv, UnquoteField_DoubledQuote_Collapsed) {
  // Input between outer quotes: a""b  =>  a"b
  const char raw[] = "a\"\"b";
  std::string result = csvUnquoteField(raw, 4);
  EXPECT_EQ(result, "a\"b");
}

TEST(FlashcardCsv, UnquoteField_NoEscapes_Passthrough) {
  const char raw[] = "hello";
  std::string result = csvUnquoteField(raw, 5);
  EXPECT_EQ(result, "hello");
}

// -------------------------------------------------------------------------
// strReplaceAll (round-trip /n <-> \n)
// -------------------------------------------------------------------------

TEST(FlashcardCsv, ReplaceAll_NewlineTokenRoundTrip) {
  // Simulate encode: \n -> /n (for storage)
  std::string stored = "line1\nline2";
  strReplaceAll(stored, "\n", "/n");
  EXPECT_EQ(stored, "line1/nline2");

  // Simulate decode: /n -> \n (on load)
  strReplaceAll(stored, "/n", "\n");
  EXPECT_EQ(stored, "line1\nline2");
}

TEST(FlashcardCsv, ReplaceAll_EmbeddedNewlineStoredAsSlashN) {
  // A field with an embedded newline is stored with /n token and round-trips cleanly.
  std::string original = "first\nsecond\nthird";
  std::string encoded = original;
  strReplaceAll(encoded, "\n", "/n");
  EXPECT_EQ(encoded, "first/nsecond/nthird");

  std::string decoded = encoded;
  strReplaceAll(decoded, "/n", "\n");
  EXPECT_EQ(decoded, original);
}

// -------------------------------------------------------------------------
// tokenizeCsvLine
// -------------------------------------------------------------------------

TEST(FlashcardCsv, Tokenize_QuotedFieldWithComma_StaysOneField) {
  // "hello,world",foo  =>  2 fields: ["hello,world", "foo"]
  const char line[] = "\"hello,world\",foo";
  std::string fields[4];
  size_t n = tokenizeCsvLine(line, sizeof(line) - 1, fields, 4);
  EXPECT_EQ(n, 2u);
  EXPECT_EQ(fields[0], "hello,world");
  EXPECT_EQ(fields[1], "foo");
}

TEST(FlashcardCsv, Tokenize_UnquotedFields_Basic) {
  const char line[] = "1,front,back";
  std::string fields[6];
  size_t n = tokenizeCsvLine(line, sizeof(line) - 1, fields, 6);
  EXPECT_EQ(n, 3u);
  EXPECT_EQ(fields[0], "1");
  EXPECT_EQ(fields[1], "front");
  EXPECT_EQ(fields[2], "back");
}

TEST(FlashcardCsv, Tokenize_QuotedFieldWithDoubledQuote) {
  // "a""b"  =>  field value: a"b
  const char line[] = "\"a\"\"b\"";
  std::string fields[2];
  size_t n = tokenizeCsvLine(line, sizeof(line) - 1, fields, 2);
  EXPECT_EQ(n, 1u);
  EXPECT_EQ(fields[0], "a\"b");
}

// -------------------------------------------------------------------------
// Front hint split (backslash markers)
// -------------------------------------------------------------------------
// These tests exercise the parsing convention used by FlashcardCard::frontMain()
// and frontHint(). The helpers themselves are in FlashcardDeck, but the /n-token
// round-trip and CSV tokenisation that surround them are pure CSV concerns.
// We verify the stored token format the CSV layer produces for a hint field.

TEST(FlashcardCsv, FrontHintTokenFormat_NoEscape) {
  // The hint marker uses backslashes: "main \ hint \"
  // When stored in CSV without commas/quotes it passes through unescaped.
  std::string raw = "main \\ hint \\";
  std::string escaped = csvEscapeField(raw);
  // No comma, no quote, no /n: should be stored as-is (no outer quotes).
  EXPECT_EQ(escaped, raw);
}
