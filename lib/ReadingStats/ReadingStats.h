#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace reading_stats {

// Per-book accumulated reading statistics. Time is wall-clock-free: it is
// derived from monotonic millis() deltas, so it works identically on X3 and
// X4 (neither exposes a reliable calendar date).
struct BookStats {
  std::string bookPath;         // identity key (epub->getPath())
  uint32_t pagesRead = 0;       // forward page turns only
  uint32_t totalReadingMs = 0;  // summed, with each gap capped (see kMaxPageMs)
  uint32_t sessionCount = 0;    // number of ended reading sessions

  bool operator==(const BookStats& o) const = default;
};

// Pure calendar-day streak update. Mirrors the reference branch logic exactly.
// Given today's (curYear, curDay-of-year) and the persisted last-read date
// (lastYear/lastDay, both -1 when never read), updates current/longest streaks
// in place and advances lastYear/lastDay. No time() call — host-testable.
//   - Same (year, day) as last read: no-op (already counted today).
//   - Consecutive: same year & curDay == lastDay+1, OR Dec31->Jan1 rollover
//     (curYear == lastYear+1 && curDay == 0 && lastDay in {364, 365}).
//   - Otherwise: reset current streak to 1.
// longest is bumped to current whenever current exceeds it.
void updateStreak(int16_t curYear, int16_t curDay, int16_t& lastYear, int16_t& lastDay, uint16_t& current,
                  uint16_t& longest);

// Pure aggregation engine. No Arduino/SD dependencies, so it is unit-tested on
// the host. Persistence lives in ReadingStatsStore (src/), mirroring
// RecentBooksStore. All timestamps are millis() values (monotonic, ms).
class ReadingStatsAggregator {
 public:
  // The time attributed to a single page turn is capped at this value, so a
  // device left on an open page does not inflate reading time. A gap longer
  // than this still counts, but only up to the cap.
  static constexpr uint32_t kMaxPageMs = 300000;  // 5 minutes

  // Replace all per-book stats with previously persisted data.
  void load(std::vector<BookStats> books);

  // Snapshot for persistence.
  const std::vector<BookStats>& books() const { return books_; }

  // Begin a reading session for a book. If a session is already active it is
  // ended first (banked at nowMs) before the new one starts.
  void beginSession(const std::string& bookPath, uint32_t nowMs);

  // Record a page turn within the active session. `forward` true counts the
  // page as read; either direction banks elapsed time. No-op without a session.
  void recordPageTurn(uint32_t nowMs, bool forward);

  // End the active session, banking the final delta and counting the session.
  // No-op if no session is active.
  void endSession(uint32_t nowMs);

  // Aggregate accessors.
  const BookStats* statsFor(const std::string& bookPath) const;
  uint32_t totalPagesRead() const;
  uint32_t totalReadingMs() const;

  // Reading speed for one book in pages per hour (0 if no time recorded).
  uint32_t pagesPerHour(const std::string& bookPath) const;

  // --- Lifetime counters ---
  uint16_t currentStreak() const { return currentStreak_; }
  uint16_t longestStreak() const { return longestStreak_; }
  uint16_t booksFinished() const { return booksFinished_; }
  int16_t lastReadYear() const { return lastReadYear_; }
  int16_t lastReadDayOfYear() const { return lastReadDayOfYear_; }

  // Increment the finished-books counter (called when a book is read to the end).
  void incrementBooksFinished() {
    if (booksFinished_ < UINT16_MAX) booksFinished_++;
  }

  // Stamp today's reading day into the streak. Only call with a valid wall-clock.
  void recordReadingDay(int16_t year, int16_t dayOfYear) {
    updateStreak(year, dayOfYear, lastReadYear_, lastReadDayOfYear_, currentStreak_, longestStreak_);
  }

  // Restore persisted lifetime counters (used by JsonSettingsIO on load).
  void setLifetimeCounters(uint16_t current, uint16_t longest, uint16_t finished, int16_t lastYear, int16_t lastDay) {
    currentStreak_ = current;
    longestStreak_ = longest;
    booksFinished_ = finished;
    lastReadYear_ = lastYear;
    lastReadDayOfYear_ = lastDay;
  }

 private:
  // Wrap-safe, capped delta from lastEventMs_ to nowMs.
  uint32_t cappedDelta(uint32_t nowMs) const;

  std::vector<BookStats> books_;
  bool sessionActive_ = false;
  std::optional<std::size_t> activeIndex_;  // set to the active session's index in books_
  uint32_t lastEventMs_ = 0;

  // Lifetime counters (persisted alongside per-book stats).
  uint16_t currentStreak_ = 0;
  uint16_t longestStreak_ = 0;
  uint16_t booksFinished_ = 0;
  int16_t lastReadYear_ = -1;       // -1 = never recorded a valid wall-clock day
  int16_t lastReadDayOfYear_ = -1;  // 0..365
};

}  // namespace reading_stats
