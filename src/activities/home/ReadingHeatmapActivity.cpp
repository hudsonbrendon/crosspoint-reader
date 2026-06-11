#include "ReadingHeatmapActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <StatsFormat.h>

#include <cstdio>

#include "HalClock.h"
#include "MappedInputManager.h"
#include "ReadingStatsStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// Calendar helpers (file-scope statics, all in flash)
// ---------------------------------------------------------------------------

// 0-based day-of-year for the 1st of `month` (leap-aware).
static uint16_t firstDoyOfMonth(uint8_t month, int16_t year) {
  static const int cum[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int d = cum[month];
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (leap && month > 2) d += 1;
  return static_cast<uint16_t>(d);
}

static uint8_t daysInMonth(uint8_t month, int16_t year) {
  static const uint8_t dm[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return dm[month];
}

// Day-of-week (0=Sun) for (year, month, day) via Sakamoto's algorithm.
static int dowSun0(int16_t y, uint8_t m, uint8_t d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int yy = (m < 3) ? y - 1 : y;
  return (yy + yy / 4 - yy / 100 + yy / 400 + t[m - 1] + d) % 7;
}

// Reading-minutes -> 0..5 shade level.
static uint8_t shadeLevel(uint32_t ms) {
  const uint32_t m = ms / 60000u;
  if (m >= 240) return 5;
  if (m >= 120) return 4;
  if (m >= 60) return 3;
  if (m >= 30) return 2;
  if (m >= 15) return 1;
  return 0;
}

// Short month names (English, flash-resident).
static const char* const kMonthAbbr[13] = {"",    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// ---------------------------------------------------------------------------
// Activity implementation
// ---------------------------------------------------------------------------

ReadingHeatmapActivity::ReadingHeatmapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("ReadingHeatmap", renderer, mappedInput) {}

void ReadingHeatmapActivity::onEnter() {
  Activity::onEnter();
  goalMs = static_cast<uint32_t>(SETTINGS.dailyGoalMinutes) * 60000u;

  uint8_t mo = 0, d = 0;
  uint16_t doy = 0;
  int16_t y = 0;
  haveDate = halClock.getDate(y, mo, d, doy, SETTINGS.clockUtcOffsetQ);
  if (haveDate) {
    viewYear = y;
    viewMonth = mo;
    today_y = y;
    today_doy = doy;
  } else {
    // Fallback: use the most recent day bucket year if available, else 2025.
    const auto& days = READING_STATS.days();
    if (!days.empty()) {
      viewYear = days.back().year;
    } else {
      viewYear = 2025;
    }
    viewMonth = 1;
  }
  requestUpdate();
}

void ReadingHeatmapActivity::onExit() { Activity::onExit(); }

void ReadingHeatmapActivity::prevMonth() {
  if (viewMonth == 1) {
    viewMonth = 12;
    viewYear -= 1;
  } else {
    viewMonth -= 1;
  }
  requestUpdate();
}

void ReadingHeatmapActivity::nextMonth() {
  if (viewMonth == 12) {
    viewMonth = 1;
    viewYear += 1;
  } else {
    viewMonth += 1;
  }
  requestUpdate();
}

void ReadingHeatmapActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    prevMonth();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    nextMonth();
    return;
  }
}

void ReadingHeatmapActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  // -- Header --
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HEATMAP_TITLE));

  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  // -- Month/year title row --
  char monthTitle[32];
  std::snprintf(monthTitle, sizeof(monthTitle), "%s %d", kMonthAbbr[viewMonth], (int)viewYear);
  renderer.drawCenteredText(UI_12_FONT_ID, y, monthTitle);
  y += renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;

  // -----------------------------------------------------------------------
  // Summary tiles: 2 columns x 2 rows (Month Total, Days Read, Best Day, Goal Streak)
  // -----------------------------------------------------------------------
  const int tileGap = 6;
  const int tileW = (contentW - tileGap) / 2;
  const int tileH = 38;
  const int labelH = renderer.getLineHeight(UI_10_FONT_ID);

  // Accumulate month stats.
  uint8_t numDays = daysInMonth(viewMonth, viewYear);
  uint16_t baseDoy = firstDoyOfMonth(viewMonth, viewYear);

  uint32_t monthTotalMs = 0;
  uint32_t monthBestMs = 0;
  uint16_t monthDaysRead = 0;
  for (uint8_t day = 1; day <= numDays; ++day) {
    const uint16_t doy = static_cast<uint16_t>(baseDoy + (day - 1));
    const uint32_t ms = READING_STATS.msForDay(viewYear, doy);
    monthTotalMs += ms;
    if (ms > monthBestMs) monthBestMs = ms;
    if (ms > 0) ++monthDaysRead;
  }

  // Goal streak: only meaningful when we have a valid date.
  reading_stats::GoalStreak gs;
  if (haveDate) {
    gs = READING_STATS.goalStreak(today_y, today_doy, goalMs);
  }

  auto drawSummaryTile = [&](int col, int row, const char* label, const char* value) {
    const int tx = contentX + col * (tileW + tileGap);
    const int ty = y + row * (tileH + tileGap);
    renderer.drawRect(tx, ty, tileW, tileH);
    renderer.drawText(UI_10_FONT_ID, tx + 5, ty + 4, label);
    renderer.drawText(UI_12_FONT_ID, tx + 5, ty + 4 + labelH + 2, value);
  };

  char buf1[20], buf2[20], buf3[20], buf4[20];
  reading_stats::formatHm(buf1, sizeof(buf1), monthTotalMs);
  std::snprintf(buf2, sizeof(buf2), "%u", (unsigned)monthDaysRead);
  reading_stats::formatHm(buf3, sizeof(buf3), monthBestMs);
  if (haveDate) {
    std::snprintf(buf4, sizeof(buf4), "%u", (unsigned)gs.current);
  } else {
    std::snprintf(buf4, sizeof(buf4), "—");
  }

  drawSummaryTile(0, 0, tr(STR_HEATMAP_MONTH_TOTAL), buf1);
  drawSummaryTile(1, 0, tr(STR_HEATMAP_DAYS_READ), buf2);
  drawSummaryTile(0, 1, tr(STR_HEATMAP_BEST_DAY), buf3);
  drawSummaryTile(1, 1, tr(STR_STATS_GOAL_STREAK), buf4);

  y += 2 * (tileH + tileGap) + metrics.verticalSpacing;

  // -----------------------------------------------------------------------
  // Calendar grid
  // -----------------------------------------------------------------------
  // 7 columns (Sun..Sat). Compute cell size from available width.
  const int cellGap = 2;
  const int cellW = (contentW - 6 * cellGap) / 7;
  const int cellH = cellW;  // square cells

  // Day-of-week header row
  static const char* const kDow[7] = {"S", "M", "T", "W", "T", "F", "S"};
  const int dowLabelH = renderer.getLineHeight(UI_10_FONT_ID);
  for (int col = 0; col < 7; ++col) {
    const int cx = contentX + col * (cellW + cellGap);
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, kDow[col]);
    renderer.drawText(UI_10_FONT_ID, cx + (cellW - tw) / 2, y, kDow[col]);
  }
  y += dowLabelH + 2;

  // First cell column offset.
  const int startCol = dowSun0(viewYear, viewMonth, 1);
  int col = startCol;
  int row = 0;

  const int dayFontId = UI_10_FONT_ID;
  const int dayFontH = renderer.getLineHeight(dayFontId);

  // Shade patterns for levels 1..4: we use fillRectDither with Color enum values.
  // Color: Clear=0x00, White=0x01, LightGray=0x05, DarkGray=0x0A, Black=0x10
  static constexpr Color kShadeColors[6] = {Color::White,     Color::White,    Color::LightGray,
                                            Color::LightGray, Color::DarkGray, Color::Black};
  // Level 0 = empty (just outline). Levels 1-5 fill with increasing darkness.
  // We use fillRectDither for all fills; level 1 uses White (essentially transparent dither).

  for (uint8_t day = 1; day <= numDays; ++day) {
    const int cx = contentX + col * (cellW + cellGap);
    const int cy = y + row * (cellH + cellGap);

    const uint16_t doy = static_cast<uint16_t>(baseDoy + (day - 1));
    const uint32_t ms = READING_STATS.msForDay(viewYear, doy);
    const uint8_t shade = shadeLevel(ms);

    // Draw outline.
    renderer.drawRect(cx, cy, cellW, cellH);

    // Fill interior based on shade level.
    if (shade >= 1) {
      const int innerX = cx + 1;
      const int innerY = cy + 1;
      const int innerW = cellW - 2;
      const int innerH = cellH - 2;
      if (shade == 5) {
        renderer.fillRect(innerX, innerY, innerW, innerH, true);
      } else {
        renderer.fillRectDither(innerX, innerY, innerW, innerH, kShadeColors[shade]);
      }
    }

    // Day number (white on black for shade 5, black otherwise).
    char dayBuf[4];
    std::snprintf(dayBuf, sizeof(dayBuf), "%u", (unsigned)day);
    const int tw = renderer.getTextWidth(dayFontId, dayBuf);
    const int tx = cx + (cellW - tw) / 2;
    const int ty = cy + (cellH - dayFontH) / 2;
    renderer.drawText(dayFontId, tx, ty, dayBuf, shade < 5);  // false = white text on shade 5

    // Goal check mark: a small "v" glyph in the bottom-right of the cell if
    // the day's reading met or exceeded the daily goal.
    if (goalMs > 0 && ms >= goalMs) {
      // Draw a tiny tick using two short lines (down-right then up-right).
      // Position in lower-right quadrant of cell.
      const int tickX = cx + cellW - 5;
      const int tickY = cy + cellH - 4;
      renderer.drawLine(tickX - 2, tickY - 2, tickX - 1, tickY, shade < 5);
      renderer.drawLine(tickX - 1, tickY, tickX + 1, tickY - 3, shade < 5);
    }

    col += 1;
    if (col == 7) {
      col = 0;
      row += 1;
    }
  }

  // Advance y past calendar rows.
  const int calendarRows = (startCol + numDays + 6) / 7;
  y += calendarRows * (cellH + cellGap) + metrics.verticalSpacing;

  // -----------------------------------------------------------------------
  // Legend row: 0 | 15m | 30m | 60m | 120m | 240m+
  // -----------------------------------------------------------------------
  const int legendItemW = (contentW - 5 * cellGap) / 6;
  static const char* const kLegendLabels[6] = {"0", "15m", "30m", "1h", "2h", "4h+"};

  // Only draw if it fits on screen.
  const int legendBoxSize = 8;
  const int legendY = y;
  const int legendLabelY = legendY + legendBoxSize + 2;

  if (legendLabelY + dowLabelH <= pageHeight - metrics.buttonHintsHeight) {
    for (int i = 0; i < 6; ++i) {
      const int lx = contentX + i * (legendItemW + cellGap);
      // Shade box.
      renderer.drawRect(lx, legendY, legendBoxSize, legendBoxSize);
      if (i >= 1) {
        if (i == 5) {
          renderer.fillRect(lx + 1, legendY + 1, legendBoxSize - 2, legendBoxSize - 2, true);
        } else {
          renderer.fillRectDither(lx + 1, legendY + 1, legendBoxSize - 2, legendBoxSize - 2, kShadeColors[i]);
        }
      }
      // Label below box.
      renderer.drawText(UI_10_FONT_ID, lx, legendLabelY, kLegendLabels[i]);
    }
  }

  // -- Button hints: Back | (none) | < Prev | Next > --
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), nullptr, tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  (void)pageHeight;
}
