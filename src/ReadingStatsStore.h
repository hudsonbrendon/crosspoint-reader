#pragma once
#include <ReadingStats.h>

#include <string>
#include <vector>

class ReadingStatsStore;
namespace JsonSettingsIO {
bool saveReadingStats(const ReadingStatsStore& store, const char* path);
bool loadReadingStats(ReadingStatsStore& store, const char* json);
}  // namespace JsonSettingsIO

// Singleton wrapper around the pure ReadingStatsAggregator that adds SD-card
// persistence. Mirrors RecentBooksStore. Reading data is stored at
// /.inkpoint/reading_stats.json.
class ReadingStatsStore {
  static ReadingStatsStore instance;
  reading_stats::ReadingStatsAggregator aggregator;

 public:
  // Max per-book entries persisted/loaded. Bounds memory on large libraries;
  // save and load use the same cap so a reboot never silently drops entries.
  static constexpr size_t kMaxBooks = 500;

  static ReadingStatsStore& getInstance() { return instance; }

  // --- Session lifecycle (called from the reader) ---
  void beginSession(const std::string& bookPath, uint32_t nowMs) { aggregator.beginSession(bookPath, nowMs); }
  void recordPageTurn(uint32_t nowMs, bool forward) { aggregator.recordPageTurn(nowMs, forward); }
  // Ends the active session and persists. Best-effort: a failed save is logged.
  // Returns the ms banked by this session (0 if no session was active).
  uint32_t endSession(uint32_t nowMs);

  // --- Persistence ---
  bool saveToFile() const;
  bool loadFromFile();

  // --- Accessors used by JsonSettingsIO and (later) the stats screen ---
  const std::vector<reading_stats::BookStats>& books() const { return aggregator.books(); }
  const reading_stats::BookStats* statsFor(const std::string& path) const { return aggregator.statsFor(path); }
  uint32_t totalPagesRead() const { return aggregator.totalPagesRead(); }
  uint32_t totalReadingMs() const { return aggregator.totalReadingMs(); }
  uint32_t pagesPerHour(const std::string& path) const { return aggregator.pagesPerHour(path); }

  uint16_t currentStreak() const { return aggregator.currentStreak(); }
  uint16_t longestStreak() const { return aggregator.longestStreak(); }
  uint16_t booksFinished() const { return aggregator.booksFinished(); }
  int16_t lastReadYear() const { return aggregator.lastReadYear(); }
  int16_t lastReadDayOfYear() const { return aggregator.lastReadDayOfYear(); }

  // Called by the reader when a book is finished (read to the last page).
  void incrementBooksFinished() { aggregator.incrementBooksFinished(); }

  // Stamp today's reading day into the streak (valid wall-clock only).
  void recordReadingDay(int16_t year, int16_t dayOfYear) { aggregator.recordReadingDay(year, dayOfYear); }

  // --- Per-day log accessors ---
  void recordReadingMs(int16_t year, uint16_t dayOfYear, uint32_t ms) { aggregator.recordReadingMs(year, dayOfYear, ms); }
  uint32_t msForDay(int16_t y, uint16_t doy) const { return aggregator.msForDay(y, doy); }
  uint32_t daysRead() const { return aggregator.daysRead(); }
  uint32_t bestDayMs() const { return aggregator.bestDayMs(); }
  uint32_t annualMs(int16_t y) const { return aggregator.annualMs(y); }
  uint32_t windowMs(int16_t y, uint16_t doy, uint16_t n) const { return aggregator.windowMs(y, doy, n); }
  const std::vector<reading_stats::DayBucket>& days() const { return aggregator.days(); }
  uint16_t booksStarted() const { return aggregator.booksStarted(); }
  reading_stats::GoalStreak goalStreak(int16_t y, uint16_t doy, uint32_t goalMs) const {
    return aggregator.computeGoalStreak(y, doy, goalMs);
  }

 private:
  ReadingStatsStore() = default;

  // Only JsonSettingsIO may replace the in-memory stats wholesale (used on load).
  void loadBooks(std::vector<reading_stats::BookStats> books) { aggregator.load(std::move(books)); }

  // Only JsonSettingsIO may restore persisted lifetime counters (used on load).
  void setLifetimeCounters(uint16_t current, uint16_t longest, uint16_t finished, int16_t lastYear, int16_t lastDay) {
    aggregator.setLifetimeCounters(current, longest, finished, lastYear, lastDay);
  }

  // Only JsonSettingsIO may restore per-day log and booksStarted on load.
  void loadDays(std::vector<reading_stats::DayBucket> d) { aggregator.loadDays(std::move(d)); }
  void setBooksStarted(uint16_t n) { aggregator.setBooksStarted(n); }

  friend bool JsonSettingsIO::loadReadingStats(ReadingStatsStore& store, const char* json);
};

// Helper macro mirroring RECENT_BOOKS.
#define READING_STATS ReadingStatsStore::getInstance()
