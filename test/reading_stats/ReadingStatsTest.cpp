#include <gtest/gtest.h>

#include "ReadingStats.h"
#include "StatsFormat.h"

using reading_stats::BookStats;
using reading_stats::ReadingStatsAggregator;

TEST(ReadingStats, EmptyAggregatorHasNoData) {
  ReadingStatsAggregator agg;
  EXPECT_EQ(agg.totalPagesRead(), 0u);
  EXPECT_EQ(agg.totalReadingMs(), 0u);
  EXPECT_EQ(agg.statsFor("/books/missing.epub"), nullptr);
  EXPECT_EQ(agg.pagesPerHour("/books/missing.epub"), 0u);
  EXPECT_TRUE(agg.books().empty());
}

TEST(ReadingStats, BanksTimeAndCountsForwardPages) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 1000);
  agg.recordPageTurn(3000, true);  // +2000 ms, +1 page
  agg.recordPageTurn(8000, true);  // +5000 ms, +1 page
  agg.endSession(9000);            // +1000 ms

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->pagesRead, 2u);
  EXPECT_EQ(s->totalReadingMs, 8000u);  // 2000 + 5000 + 1000
  EXPECT_EQ(s->sessionCount, 1u);
}

TEST(ReadingStats, BackwardTurnBanksTimeButNotPages) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  agg.recordPageTurn(1000, true);   // +1000 ms, +1 page
  agg.recordPageTurn(2000, false);  // +1000 ms, no page
  agg.endSession(2000);             // +0 ms

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->pagesRead, 1u);
  EXPECT_EQ(s->totalReadingMs, 2000u);
}

TEST(ReadingStats, PageTurnWithoutSessionIsIgnored) {
  ReadingStatsAggregator agg;
  agg.recordPageTurn(1000, true);
  agg.endSession(2000);
  EXPECT_EQ(agg.totalPagesRead(), 0u);
  EXPECT_EQ(agg.totalReadingMs(), 0u);
}

TEST(ReadingStats, LongGapIsCappedAtMaxPageMs) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  // Gap of 1 hour on one page: only kMaxPageMs (5 min) should be counted.
  agg.recordPageTurn(3600000, true);
  agg.endSession(3600000);

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->totalReadingMs, ReadingStatsAggregator::kMaxPageMs);
}

TEST(ReadingStats, MillisWrapCountsAsZeroDelta) {
  ReadingStatsAggregator agg;
  // Start near the uint32 millis() ceiling, then wrap past zero.
  agg.beginSession("/books/a.epub", 0xFFFFFF00u);
  agg.recordPageTurn(0x00000100u, true);  // nowMs < lastEventMs_ -> 0 ms
  agg.endSession(0x00000100u);

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->pagesRead, 1u);
  EXPECT_EQ(s->totalReadingMs, 0u);
}

TEST(ReadingStats, TracksTwoBooksIndependently) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  agg.recordPageTurn(1000, true);
  agg.endSession(1000);

  agg.beginSession("/books/b.epub", 5000);
  agg.recordPageTurn(7000, true);
  agg.recordPageTurn(9000, true);
  agg.endSession(9000);

  EXPECT_EQ(agg.statsFor("/books/a.epub")->pagesRead, 1u);
  EXPECT_EQ(agg.statsFor("/books/b.epub")->pagesRead, 2u);
  EXPECT_EQ(agg.totalPagesRead(), 3u);
}

TEST(ReadingStats, BeginSessionEndsThePriorSession) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  agg.recordPageTurn(1000, true);  // book a: +1000 ms, +1 page
  // Switch to b without an explicit endSession; a is banked at nowMs (2000).
  agg.beginSession("/books/b.epub", 2000);
  agg.recordPageTurn(3000, true);  // book b: +1000 ms, +1 page
  agg.endSession(3000);

  const BookStats* a = agg.statsFor("/books/a.epub");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->totalReadingMs, 2000u);  // 1000 (to first turn) + 1000 (banked on switch)
  EXPECT_EQ(a->sessionCount, 1u);       // switching ended a's session

  const BookStats* b = agg.statsFor("/books/b.epub");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->totalReadingMs, 1000u);
  EXPECT_EQ(b->sessionCount, 1u);
}

TEST(ReadingStats, ReopeningSameBookAccumulates) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  agg.recordPageTurn(1000, true);
  agg.endSession(1000);

  agg.beginSession("/books/a.epub", 2000);
  agg.recordPageTurn(3000, true);
  agg.endSession(3000);

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->pagesRead, 2u);
  EXPECT_EQ(s->totalReadingMs, 2000u);
  EXPECT_EQ(s->sessionCount, 2u);
  EXPECT_EQ(agg.books().size(), 1u);  // not duplicated
}

TEST(ReadingStats, PagesPerHourComputesSpeed) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  // 30 pages over exactly 1 hour -> 30 pages/hour. Use forward turns and a
  // final endSession that adds no extra time (each gap <= kMaxPageMs).
  uint32_t t = 0;
  for (int i = 0; i < 30; ++i) {
    t += 120000;  // 2 min per page (under the 5 min cap)
    agg.recordPageTurn(t, true);
  }
  agg.endSession(t);

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->pagesRead, 30u);
  EXPECT_EQ(s->totalReadingMs, 3600000u);  // 30 * 120000
  EXPECT_EQ(agg.pagesPerHour("/books/a.epub"), 30u);
}

TEST(ReadingStats, PagesPerHourIsZeroWithoutTime) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 1000);
  agg.recordPageTurn(1000, true);  // zero elapsed
  agg.endSession(1000);
  EXPECT_EQ(agg.pagesPerHour("/books/a.epub"), 0u);
}

TEST(ReadingStats, LoadReplacesContentsAndResetsSession) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/stale.epub", 0);  // active session that load() must drop

  std::vector<BookStats> persisted;
  persisted.push_back(BookStats{"/books/a.epub", 10, 600000, 3});
  persisted.push_back(BookStats{"/books/b.epub", 5, 300000, 1});
  agg.load(persisted);

  EXPECT_EQ(agg.books().size(), 2u);
  EXPECT_EQ(agg.totalPagesRead(), 15u);
  EXPECT_EQ(agg.statsFor("/books/a.epub")->sessionCount, 3u);
  EXPECT_EQ(agg.statsFor("/books/stale.epub"), nullptr);

  // After load the prior session is gone, so a stray page turn is ignored
  // until a new beginSession.
  agg.recordPageTurn(1000, true);
  EXPECT_EQ(agg.totalPagesRead(), 15u);
}

TEST(ReadingStats, PerBookTimeSaturatesAtUint32Max) {
  ReadingStatsAggregator agg;
  // Seed a book already near the uint32 ceiling, then add more time via a turn.
  std::vector<BookStats> seed;
  seed.push_back(BookStats{"/books/a.epub", 1, UINT32_MAX - 1000u, 1});
  agg.load(seed);

  agg.beginSession("/books/a.epub", 0);
  agg.recordPageTurn(200000, true);  // +200000 ms would overflow -> must saturate
  agg.endSession(200000);

  const BookStats* s = agg.statsFor("/books/a.epub");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->totalReadingMs, UINT32_MAX);
}

using reading_stats::avgMsPerPage;
using reading_stats::avgMsPerSession;
using reading_stats::formatDurationMs;
using reading_stats::pagesPerHour;
using reading_stats::pathToDisplayName;

TEST(StatsFormat, FormatsDurationUnderOneHourAsMinutesSeconds) {
  EXPECT_EQ(formatDurationMs(0), "0m 0s");
  EXPECT_EQ(formatDurationMs(37606), "0m 37s");
  EXPECT_EQ(formatDurationMs(668754), "11m 8s");
  EXPECT_EQ(formatDurationMs(59999), "0m 59s");
}

TEST(StatsFormat, FormatsDurationOverOneHourAsHoursMinutes) {
  EXPECT_EQ(formatDurationMs(3600000), "1h 0m");
  EXPECT_EQ(formatDurationMs(3600000 + 125000), "1h 2m");
  EXPECT_EQ(formatDurationMs(36000000), "10h 0m");
}

TEST(StatsFormat, DerivesDisplayNameFromPath) {
  EXPECT_EQ(pathToDisplayName("/books/great-gatsby.epub"), "great-gatsby");
  EXPECT_EQ(pathToDisplayName("/a/b/c.txt"), "c");
  EXPECT_EQ(pathToDisplayName("plath-bell-jar.epub"), "plath-bell-jar");
  EXPECT_EQ(pathToDisplayName("noextension"), "noextension");
  EXPECT_EQ(pathToDisplayName("/dir/file.name.epub"), "file.name");
}

TEST(StatsCompute, PagesPerHour) {
  EXPECT_EQ(pagesPerHour(0, 0), 0u);
  EXPECT_EQ(pagesPerHour(5, 0), 0u);
  EXPECT_EQ(pagesPerHour(30, 3600000), 30u);
  EXPECT_EQ(pagesPerHour(29, 858475), 121u);
}

TEST(StatsCompute, AveragesGuardZero) {
  EXPECT_EQ(avgMsPerPage(0, 0), 0u);
  EXPECT_EQ(avgMsPerPage(60000, 0), 0u);
  EXPECT_EQ(avgMsPerPage(60000, 4), 15000u);
  EXPECT_EQ(avgMsPerSession(0, 0), 0u);
  EXPECT_EQ(avgMsPerSession(60000, 0), 0u);
  EXPECT_EQ(avgMsPerSession(90000, 3), 30000u);
}

// ---- Streak math (pure updateStreak) ----
using reading_stats::updateStreak;

TEST(Streak, FirstEverDayStartsAtOne) {
  int16_t lastYear = -1, lastDay = -1;
  uint16_t current = 0, longest = 0;
  updateStreak(2026, 100, lastYear, lastDay, current, longest);
  EXPECT_EQ(current, 1u);
  EXPECT_EQ(longest, 1u);
  EXPECT_EQ(lastYear, 2026);
  EXPECT_EQ(lastDay, 100);
}

TEST(Streak, ConsecutiveDayIncrements) {
  int16_t lastYear = 2026, lastDay = 100;
  uint16_t current = 1, longest = 1;
  updateStreak(2026, 101, lastYear, lastDay, current, longest);
  EXPECT_EQ(current, 2u);
  EXPECT_EQ(longest, 2u);
}

TEST(Streak, SameDayIsNoOp) {
  int16_t lastYear = 2026, lastDay = 100;
  uint16_t current = 5, longest = 7;
  updateStreak(2026, 100, lastYear, lastDay, current, longest);
  EXPECT_EQ(current, 5u);   // unchanged
  EXPECT_EQ(longest, 7u);   // unchanged
  EXPECT_EQ(lastDay, 100);  // unchanged
}

TEST(Streak, GapResetsToOne) {
  int16_t lastYear = 2026, lastDay = 100;
  uint16_t current = 9, longest = 9;
  updateStreak(2026, 105, lastYear, lastDay, current, longest);  // 5-day gap
  EXPECT_EQ(current, 1u);
  EXPECT_EQ(longest, 9u);  // longest preserved
}

TEST(Streak, YearBoundaryDec31ToJan1Continues) {
  // Non-leap year 2025 has 365 days, yday 0..364; Dec 31 = yday 364.
  int16_t lastYear = 2025, lastDay = 364;
  uint16_t current = 3, longest = 3;
  updateStreak(2026, 0, lastYear, lastDay, current, longest);  // Jan 1 2026
  EXPECT_EQ(current, 4u);
  EXPECT_EQ(longest, 4u);
}

TEST(Streak, LeapYearDec31ToJan1Continues) {
  // Leap year 2024 has 366 days, yday 0..365; Dec 31 = yday 365.
  int16_t lastYear = 2024, lastDay = 365;
  uint16_t current = 2, longest = 2;
  updateStreak(2025, 0, lastYear, lastDay, current, longest);  // Jan 1 2025
  EXPECT_EQ(current, 3u);
  EXPECT_EQ(longest, 3u);
}

TEST(Streak, YearJumpWithMidYearDaysResets) {
  // New year but not a Dec31->Jan1 rollover (e.g. opened mid-year) -> reset.
  int16_t lastYear = 2025, lastDay = 200;
  uint16_t current = 8, longest = 8;
  updateStreak(2026, 10, lastYear, lastDay, current, longest);
  EXPECT_EQ(current, 1u);
  EXPECT_EQ(longest, 8u);
}

TEST(Streak, LongestTracksHistoricalMax) {
  int16_t lastYear = 2026, lastDay = 10;
  uint16_t current = 4, longest = 6;
  updateStreak(2026, 11, lastYear, lastDay, current, longest);  // current 4->5, still < longest
  EXPECT_EQ(current, 5u);
  EXPECT_EQ(longest, 6u);
}

// ---- Books finished counter ----
TEST(BooksFinished, IncrementsAndSaturates) {
  ReadingStatsAggregator agg;
  EXPECT_EQ(agg.booksFinished(), 0u);
  agg.incrementBooksFinished();
  agg.incrementBooksFinished();
  EXPECT_EQ(agg.booksFinished(), 2u);
}

TEST(BooksFinished, RecordReadingDayDrivesStreakAccessors) {
  ReadingStatsAggregator agg;
  EXPECT_EQ(agg.currentStreak(), 0u);
  agg.recordReadingDay(2026, 50);
  EXPECT_EQ(agg.currentStreak(), 1u);
  EXPECT_EQ(agg.longestStreak(), 1u);
  agg.recordReadingDay(2026, 51);
  EXPECT_EQ(agg.currentStreak(), 2u);
  EXPECT_EQ(agg.longestStreak(), 2u);
  agg.recordReadingDay(2026, 51);  // same day -> no-op
  EXPECT_EQ(agg.currentStreak(), 2u);
}

TEST(BooksFinished, SetLifetimeCountersRoundTrips) {
  ReadingStatsAggregator agg;
  agg.setLifetimeCounters(3, 9, 12, 2026, 100);
  EXPECT_EQ(agg.currentStreak(), 3u);
  EXPECT_EQ(agg.longestStreak(), 9u);
  EXPECT_EQ(agg.booksFinished(), 12u);
  EXPECT_EQ(agg.lastReadYear(), 2026);
  EXPECT_EQ(agg.lastReadDayOfYear(), 100);
}

TEST(ReadingStatsDays, RecordsAndSumsPerDay) {
  ReadingStatsAggregator agg;
  agg.recordReadingMs(2026, 150, 600000);   // 10 min
  agg.recordReadingMs(2026, 150, 300000);   // +5 min same day
  agg.recordReadingMs(2026, 151, 1200000);  // 20 min next day
  EXPECT_EQ(agg.msForDay(2026, 150), 900000u);
  EXPECT_EQ(agg.msForDay(2026, 151), 1200000u);
  EXPECT_EQ(agg.msForDay(2026, 999), 0u);
  EXPECT_EQ(agg.daysRead(), 2u);
  EXPECT_EQ(agg.bestDayMs(), 1200000u);
}

TEST(ReadingStatsDays, AnnualAndWindowSums) {
  ReadingStatsAggregator agg;
  agg.recordReadingMs(2026, 10, 600000);
  agg.recordReadingMs(2026, 11, 600000);
  agg.recordReadingMs(2025, 364, 600000);
  EXPECT_EQ(agg.annualMs(2026), 1200000u);
  EXPECT_EQ(agg.annualMs(2025), 600000u);
  EXPECT_EQ(agg.windowMs(2026, 11, 7), 1200000u);   // days 5..11 of 2026
  EXPECT_EQ(agg.windowMs(2026, 11, 30), 1800000u);  // reaches back into 2025 day 364
}

TEST(ReadingStatsGoal, GoalStreakCountsConsecutiveMetDays) {
  ReadingStatsAggregator agg;
  const uint32_t goalMs = 3600000;  // 60 min
  agg.recordReadingMs(2026, 100, 4000000);  // met
  agg.recordReadingMs(2026, 101, 3600000);  // met
  agg.recordReadingMs(2026, 102, 1000000);  // missed
  agg.recordReadingMs(2026, 103, 5000000);  // met
  agg.recordReadingMs(2026, 104, 5000000);  // met (today)
  auto gs = agg.computeGoalStreak(2026, 104, goalMs);
  EXPECT_EQ(gs.current, 2u);  // days 103,104
  EXPECT_EQ(gs.max, 2u);      // best run is 2
}

TEST(ReadingStatsGoal, GoalStreakZeroWhenTodayMissed) {
  ReadingStatsAggregator agg;
  agg.recordReadingMs(2026, 100, 5000000);
  auto gs = agg.computeGoalStreak(2026, 101, 3600000);  // today (101) has nothing
  EXPECT_EQ(gs.current, 0u);
  EXPECT_EQ(gs.max, 1u);
}

TEST(ReadingStatsBooks, BooksStartedCountsDistinctBooks) {
  ReadingStatsAggregator agg;
  agg.beginSession("/books/a.epub", 0);
  agg.endSession(1000);
  agg.beginSession("/books/a.epub", 2000);  // same book again
  agg.endSession(3000);
  agg.beginSession("/books/b.epub", 4000);  // new book
  agg.endSession(5000);
  EXPECT_EQ(agg.booksStarted(), 2u);
}
