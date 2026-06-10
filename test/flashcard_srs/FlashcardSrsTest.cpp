#include <gtest/gtest.h>
#include "FlashcardSrs.h"

// -------------------------------------------------------------------------
// New-card branch
// -------------------------------------------------------------------------

TEST(FlashcardSrs, NewCard_Hard_Interval1) {
    SrsState s{};
    auto r = FlashcardSrs::review(s, SrsRating::Hard, 100);
    EXPECT_EQ(r.interval, 1);
    EXPECT_EQ(r.dueDate, 101u);
    EXPECT_EQ(r.ease, 250u - 15u);  // 235
}

TEST(FlashcardSrs, NewCard_Good_Interval1) {
    SrsState s{};
    auto r = FlashcardSrs::review(s, SrsRating::Good, 100);
    EXPECT_EQ(r.interval, 1);
    EXPECT_EQ(r.dueDate, 101u);
    EXPECT_EQ(r.ease, 250u);  // unchanged
}

TEST(FlashcardSrs, NewCard_Easy_Interval4) {
    SrsState s{};
    auto r = FlashcardSrs::review(s, SrsRating::Easy, 100);
    EXPECT_EQ(r.interval, 4);
    EXPECT_EQ(r.dueDate, 104u);
    EXPECT_EQ(r.ease, 265u);  // 250 + 15
}

TEST(FlashcardSrs, NewCard_Again_Resets) {
    SrsState s{};
    auto r = FlashcardSrs::review(s, SrsRating::Again, 100);
    EXPECT_EQ(r.interval, 0);
    EXPECT_EQ(r.dueDate, 100u);  // today
    EXPECT_EQ(r.ease, 230u);     // 250 - 20
}

// -------------------------------------------------------------------------
// Graduated-card branch
// -------------------------------------------------------------------------

TEST(FlashcardSrs, Graduated_Good_GrowsByEase) {
    // interval=10, ease=250: Good -> interval = (uint16_t)(10 * 250 / 100.0f) = 25
    SrsState s{10, 250, 50};
    auto r = FlashcardSrs::review(s, SrsRating::Good, 60);
    EXPECT_EQ(r.interval, 25u);
    EXPECT_EQ(r.dueDate, 85u);   // 60 + 25
    EXPECT_EQ(r.ease, 250u);
}

TEST(FlashcardSrs, Graduated_Easy_GrowsByEaseTimes1_3) {
    // interval=10, ease=250: Easy -> interval = (uint16_t)(10 * 250 / 100.0f * 1.3f) = 32
    SrsState s{10, 250, 50};
    auto r = FlashcardSrs::review(s, SrsRating::Easy, 60);
    EXPECT_EQ(r.interval, 32u);
    EXPECT_EQ(r.ease, 265u);     // 250 + 15
    EXPECT_EQ(r.dueDate, 92u);   // 60 + 32
}

TEST(FlashcardSrs, Graduated_Hard_GrowsBy1_2) {
    // interval=10, ease=250: Hard -> interval = (uint16_t)(10 * 1.2f) = 12
    SrsState s{10, 250, 50};
    auto r = FlashcardSrs::review(s, SrsRating::Hard, 60);
    EXPECT_EQ(r.interval, 12u);
    EXPECT_EQ(r.ease, 235u);     // 250 - 15
    EXPECT_EQ(r.dueDate, 72u);   // 60 + 12
}

// -------------------------------------------------------------------------
// Ease clamping
// -------------------------------------------------------------------------

TEST(FlashcardSrs, EaseClampLow) {
    // Start near the floor; Again and Hard must never drop ease below 130.
    SrsState s{5, 135, 10};
    for (int i = 0; i < 20; ++i) {
        s = FlashcardSrs::review(s, SrsRating::Again, (uint32_t)i * 2);
        EXPECT_GE(s.ease, FlashcardSrs::EASE_MIN);
        s = FlashcardSrs::review(s, SrsRating::Hard, (uint32_t)i * 2 + 1);
        EXPECT_GE(s.ease, FlashcardSrs::EASE_MIN);
    }
}

TEST(FlashcardSrs, EaseClampHigh) {
    // Start near the ceiling; Easy must never raise ease above 400.
    SrsState s{5, 395, 10};
    for (int i = 0; i < 20; ++i) {
        s = FlashcardSrs::review(s, SrsRating::Easy, (uint32_t)s.dueDate);
        EXPECT_LE(s.ease, FlashcardSrs::EASE_MAX);
    }
}

// -------------------------------------------------------------------------
// Preview consistency
// -------------------------------------------------------------------------

TEST(FlashcardSrs, PreviewMatchesReview) {
    SrsState s{7, 250, 5};
    constexpr uint32_t today = 10;
    for (auto r : {SrsRating::Again, SrsRating::Hard, SrsRating::Good, SrsRating::Easy}) {
        EXPECT_EQ(FlashcardSrs::previewInterval(s, r),
                  FlashcardSrs::review(s, r, today).interval);
    }
}
