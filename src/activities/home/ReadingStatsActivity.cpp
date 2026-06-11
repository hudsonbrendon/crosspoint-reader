#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <StatsFormat.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "HalClock.h"
#include "MappedInputManager.h"
#include "ReadingStatsDetailActivity.h"
#include "ReadingStatsStore.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Vertical space reserved above the list for the totals block (tiles + chart + annual).
// Large enough that the list's selection highlight clears all new sections.
constexpr int TOTALS_BLOCK_HEIGHT = 310;

// Walk back `back` days from (year, doy). Handles year boundaries.
void minusDays(int16_t& year, uint16_t& doy, uint16_t back) {
  int days = static_cast<int>(doy) - static_cast<int>(back);
  while (days < 0) {
    year -= 1;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    days += leap ? 366 : 365;
  }
  doy = static_cast<uint16_t>(days);
}
}  // namespace

void ReadingStatsActivity::loadStats() {
  books = READING_STATS.books();  // copy snapshot
  std::sort(books.begin(), books.end(), [](const reading_stats::BookStats& a, const reading_stats::BookStats& b) {
    return a.totalReadingMs > b.totalReadingMs;
  });
  totalPages = READING_STATS.totalPagesRead();
  totalMs = READING_STATS.totalReadingMs();
  currentStreak = READING_STATS.currentStreak();
  longestStreak = READING_STATS.longestStreak();
  booksFinished = READING_STATS.booksFinished();
  // A valid wall-clock day has been recorded iff lastReadYear >= 0. On X4 (no
  // RTC, Phase 1) this is always false, so streak rows render "—".
  haveStreakClock = READING_STATS.lastReadYear() >= 0;
}

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  goalMs = static_cast<uint32_t>(SETTINGS.dailyGoalMinutes) * 60000u;
  uint8_t mo = 0, d = 0;
  uint16_t doy = 0;
  int16_t y = 0;
  haveDate = halClock.getDate(y, mo, d, doy, SETTINGS.clockUtcOffsetQ);
  today_y = y;
  today_doy = doy;
  if (haveDate) goal = READING_STATS.goalStreak(today_y, today_doy, goalMs);
  loadStats();
  selectorIndex = 0;
  requestUpdate();
}

void ReadingStatsActivity::onExit() {
  Activity::onExit();
  books.clear();
}

void ReadingStatsActivity::loop() {
  const int pageItems =
      UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true, TOTALS_BLOCK_HEIGHT);

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    activityManager.goToReadingHeatmap();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!books.empty() && selectorIndex < books.size()) {
      startActivityForResult(std::make_unique<ReadingStatsDetailActivity>(renderer, mappedInput, books[selectorIndex],
                                                                          totalPages, totalMs),
                             [](const ActivityResult&) {});
    }
    return;
  }

  const int listSize = static_cast<int>(books.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STATS_TITLE2));

  const int headerBottom = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  // -----------------------------------------------------------------------
  // Tile grid: 2 columns × 4 rows
  // -----------------------------------------------------------------------
  const int tileMargin = metrics.contentSidePadding;
  const int tileGap = 8;
  const int tileW = (pageWidth - tileMargin * 2 - tileGap) / 2;
  const int tileH = 42;
  const int labelH = renderer.getLineHeight(UI_10_FONT_ID);
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);

  // Helper lambda: draw one tile at column col (0 or 1), row row (0-based)
  // label is the small top text, value is the larger bottom text.
  auto drawTile = [&](int col, int row, const char* label, const char* value) {
    const int tx = tileMargin + col * (tileW + tileGap);
    const int ty = headerBottom + row * (tileH + tileGap);
    renderer.drawRect(tx, ty, tileW, tileH);
    // Label (small, top of tile)
    const int lx = tx + 6;
    const int ly = ty + 5;
    renderer.drawText(UI_10_FONT_ID, lx, ly, label);
    // Value (slightly larger, below label)
    const int vy = ly + labelH + 2;
    renderer.drawText(UI_12_FONT_ID, lx, vy, value);
  };

  char buf1[32], buf2[32], buf3[64], buf4[32], buf5[32], buf6[32], buf7[32], buf8[32];

  // Tile 1: Goal Streak
  if (haveDate) {
    std::snprintf(buf1, sizeof(buf1), "%u", (unsigned)goal.current);
  } else {
    std::snprintf(buf1, sizeof(buf1), "—");
  }
  drawTile(0, 0, tr(STR_STATS_GOAL_STREAK), buf1);

  // Tile 2: Max Goal Streak
  if (haveDate) {
    std::snprintf(buf2, sizeof(buf2), "%u", (unsigned)goal.max);
  } else {
    std::snprintf(buf2, sizeof(buf2), "—");
  }
  drawTile(1, 0, tr(STR_STATS_MAX_GOAL_STREAK), buf2);

  // Tile 3: Daily Goal (today ms / goal ms)
  {
    const uint32_t todayMs = haveDate ? READING_STATS.msForDay(today_y, today_doy) : 0u;
    char todayBuf[16], goalBuf[16];
    reading_stats::formatHm(todayBuf, sizeof(todayBuf), todayMs);
    reading_stats::formatHm(goalBuf, sizeof(goalBuf), goalMs);
    std::snprintf(buf3, sizeof(buf3), "%s / %s", todayBuf, goalBuf);
  }
  drawTile(0, 1, tr(STR_STATS_DAILY_GOAL), buf3);

  // Tile 4: Reading Time (total)
  reading_stats::formatHm(buf4, sizeof(buf4), READING_STATS.totalReadingMs());
  drawTile(1, 1, tr(STR_STATS_READING_TIME), buf4);

  // Tile 5: Books Finished
  std::snprintf(buf5, sizeof(buf5), "%u", (unsigned)READING_STATS.booksFinished());
  drawTile(0, 2, tr(STR_STATS_BOOKS_FINISHED), buf5);

  // Tile 6: Books Started
  std::snprintf(buf6, sizeof(buf6), "%u", (unsigned)READING_STATS.booksStarted());
  drawTile(1, 2, tr(STR_STATS_BOOKS_STARTED), buf6);

  // Tile 7: 7D window
  if (haveDate) {
    reading_stats::formatHm(buf7, sizeof(buf7), READING_STATS.windowMs(today_y, today_doy, 7));
  } else {
    std::snprintf(buf7, sizeof(buf7), "—");
  }
  drawTile(0, 3, tr(STR_STATS_7D), buf7);

  // Tile 8: 30D window
  if (haveDate) {
    reading_stats::formatHm(buf8, sizeof(buf8), READING_STATS.windowMs(today_y, today_doy, 30));
  } else {
    std::snprintf(buf8, sizeof(buf8), "—");
  }
  drawTile(1, 3, tr(STR_STATS_30D), buf8);

  // -----------------------------------------------------------------------
  // 7-day bar chart section
  // -----------------------------------------------------------------------
  const int tilesBottom = headerBottom + 4 * (tileH + tileGap);
  const int chartLabelY = tilesBottom + 4;
  renderer.drawText(UI_10_FONT_ID, tileMargin, chartLabelY, tr(STR_STATS_DAILY_LAST7));

  const int chartTop = chartLabelY + labelH + 4;
  const int chartBottom = chartTop + 60;  // 60px tall bars max
  const int chartHeight = chartBottom - chartTop;

  if (!haveDate) {
    renderer.drawText(UI_10_FONT_ID, tileMargin, chartTop, tr(STR_STATS_NO_CLOCK));
  } else {
    // Collect the 7 day values (index 0 = 6 days ago, index 6 = today)
    uint32_t dayMs[7];
    uint32_t maxMs = 1u;  // avoid divide-by-zero
    for (int k = 6; k >= 0; --k) {
      int16_t by = today_y;
      uint16_t bd = today_doy;
      minusDays(by, bd, static_cast<uint16_t>(k));
      const uint32_t ms = READING_STATS.msForDay(by, bd);
      dayMs[6 - k] = ms;
      if (ms > maxMs) maxMs = ms;
    }

    // Bar width and spacing derived from available width
    const int chartWidth = pageWidth - tileMargin * 2;
    const int barW = (chartWidth - 6 * 4) / 7;  // 4px gap between bars
    const int barGap = (chartWidth - barW * 7) / 6;

    for (int i = 0; i < 7; ++i) {
      const int barX = tileMargin + i * (barW + barGap);
      const uint32_t ms = dayMs[i];
      const int barH =
          (ms > 0u) ? static_cast<int>(static_cast<uint64_t>(ms) * static_cast<uint64_t>(chartHeight) / maxMs) : 0;
      if (barH > 0) {
        renderer.fillRect(barX, chartBottom - barH, barW, barH, true);
      } else {
        // Draw a 1px baseline for days with no reading
        renderer.fillRect(barX, chartBottom - 1, barW, 1, true);
      }
      // Minutes label below bar
      const uint32_t mins = ms / 60000u;
      char minBuf[8];
      std::snprintf(minBuf, sizeof(minBuf), "%um", (unsigned)mins);
      const int lw = renderer.getTextWidth(UI_10_FONT_ID, minBuf);
      const int lx = barX + (barW - lw) / 2;
      renderer.drawText(UI_10_FONT_ID, lx, chartBottom + 2, minBuf);
    }
  }

  // -----------------------------------------------------------------------
  // Annual reading line
  // -----------------------------------------------------------------------
  const int annualY = chartBottom + labelH + 8;
  {
    const uint32_t annual = haveDate ? READING_STATS.annualMs(today_y) : 0u;
    char annualMs[16];
    reading_stats::formatHm(annualMs, sizeof(annualMs), annual);
    char annualLine[64];
    if (haveDate) {
      std::snprintf(annualLine, sizeof(annualLine), "%s (%d): %s", tr(STR_STATS_ANNUAL), (int)today_y, annualMs);
    } else {
      std::snprintf(annualLine, sizeof(annualLine), "%s: %s", tr(STR_STATS_ANNUAL), annualMs);
    }
    renderer.drawText(UI_10_FONT_ID, tileMargin, annualY, annualLine);
  }

  // -----------------------------------------------------------------------
  // Per-book list (below all new sections)
  // -----------------------------------------------------------------------
  const int contentTop = headerBottom + TOTALS_BLOCK_HEIGHT;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (books.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_READING_STATS));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(books.size()),
        static_cast<int>(selectorIndex),
        [this](int index) { return reading_stats::pathToDisplayName(books[index].bookPath); },
        [this](int index) {
          return std::to_string(books[index].pagesRead) + " " + tr(STR_READING_STATS_PAGES) + " - " +
                 reading_stats::formatDurationMs(books[index].totalReadingMs);
        },
        [](int) -> UIIcon { return Book; },
        [this](int index) {
          const auto& b = books[index];
          const uint32_t pph =
              b.totalReadingMs
                  ? static_cast<uint32_t>(static_cast<uint64_t>(b.pagesRead) * 3600000ULL / b.totalReadingMs)
                  : 0u;
          return std::to_string(pph) + tr(STR_READING_STATS_PER_HOUR);
        });
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_HOME), tr(STR_STATS_MORE_DETAILS), tr(STR_HEATMAP_VIEW), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  (void)valueH;  // suppress unused warning if compiler doesn't inline lambda
}
