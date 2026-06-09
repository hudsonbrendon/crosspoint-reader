#include <gtest/gtest.h>

// Mirrors the predicate in ParsedText::resolveFirstLineIndent after the Phase-1 change.
// Returns true when the first-line CSS text-indent should be applied.
static bool shouldApplyCssIndent(bool isFirstLine, bool textIndentDefined, int textIndent,
                                 bool extraParagraphSpacing, bool forceParagraphIndents, bool isNaturalAlign) {
  return isFirstLine && textIndentDefined &&
         (textIndent < 0 || !extraParagraphSpacing || forceParagraphIndents) && isNaturalAlign;
}

// Mirrors the early-return guard in ParsedText::applyParagraphIndent after the Phase-1 change.
// Returns true when the em-space fallback indent path runs (i.e. the guard does NOT early-return),
// given words is non-empty.
static bool emSpaceFallbackRuns(bool extraParagraphSpacing, bool forceParagraphIndents, bool wordsEmpty) {
  if ((extraParagraphSpacing && !forceParagraphIndents) || wordsEmpty) return false;
  return true;
}

TEST(IndentDecision, CssIndentUnchangedWhenSpacingOff) {
  // extraParagraphSpacing off: indent applies for natural-aligned defined indent regardless of force flag.
  EXPECT_TRUE(shouldApplyCssIndent(true, true, 20, false, false, true));
  EXPECT_TRUE(shouldApplyCssIndent(true, true, 20, false, true, true));
}

TEST(IndentDecision, CssIndentSuppressedWhenSpacingOnAndNotForced) {
  EXPECT_FALSE(shouldApplyCssIndent(true, true, 20, true, false, true));
}

TEST(IndentDecision, CssIndentForcedWhenSpacingOnAndForced) {
  EXPECT_TRUE(shouldApplyCssIndent(true, true, 20, true, true, true));
}

TEST(IndentDecision, NegativeIndentAlwaysApplies) {
  // A negative (hanging) indent applies even with spacing on and force off.
  EXPECT_TRUE(shouldApplyCssIndent(true, true, -10, true, false, true));
}

TEST(IndentDecision, NonNaturalAlignNeverIndents) {
  EXPECT_FALSE(shouldApplyCssIndent(true, true, 20, false, true, false));
}

TEST(IndentDecision, NotFirstLineNeverIndents) {
  EXPECT_FALSE(shouldApplyCssIndent(false, true, 20, false, true, true));
}

TEST(IndentDecision, EmSpaceFallbackUnchangedWhenSpacingOff) {
  EXPECT_TRUE(emSpaceFallbackRuns(false, false, false));
}

TEST(IndentDecision, EmSpaceFallbackSuppressedWhenSpacingOnNotForced) {
  EXPECT_FALSE(emSpaceFallbackRuns(true, false, false));
}

TEST(IndentDecision, EmSpaceFallbackRunsWhenSpacingOnAndForced) {
  EXPECT_TRUE(emSpaceFallbackRuns(true, true, false));
}

TEST(IndentDecision, EmptyWordsNeverRunsFallback) {
  EXPECT_FALSE(emSpaceFallbackRuns(false, false, true));
  EXPECT_FALSE(emSpaceFallbackRuns(true, true, true));
}
