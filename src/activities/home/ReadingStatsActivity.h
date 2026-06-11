#pragma once
#include <ReadingStats.h>

#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Top-level screen showing reading stats: tile grid, 7-day bar chart,
// annual total, and a scrollable per-book list. Read-only; Back returns Home.
class ReadingStatsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  std::vector<reading_stats::BookStats> books;  // snapshot, sorted by time desc
  uint32_t totalPages = 0;
  uint32_t totalMs = 0;
  uint16_t currentStreak = 0;
  uint16_t longestStreak = 0;
  uint16_t booksFinished = 0;
  bool haveStreakClock = false;  // false on X4 until Phase 2 -> render "—"

  // New date/goal members
  bool haveDate = false;
  int16_t today_y = 0;
  uint16_t today_doy = 0;
  uint32_t goalMs = 0;
  reading_stats::GoalStreak goal;

  void loadStats();

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
