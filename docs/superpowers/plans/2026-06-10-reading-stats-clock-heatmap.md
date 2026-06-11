# Reading Stats, Heatmap, Calendar Clock & Home Covers — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring InkPoint's reading-stats experience to parity with the reference screens: a calendar **Reading Heatmap**, a richer **Reading Stats** screen (daily goal, goal streak, books started/finished, 7-day bar chart, annual total), per-book **cover + progress %** on the home screen, and the **current date** in the status bar — all backed by a real calendar-date source (DS3231 on X3, NTP-seeded system clock on X4).

**Architecture:** A calendar-date layer extends `HalClock` to expose `getDate()` / `hasValidDate()` (X3 reads the DS3231 date registers; X4 reads the ESP32 system clock once it has been NTP-synced). The pure, host-tested `ReadingStatsAggregator` gains a compact **per-day reading-minutes log** plus `booksStarted`, with goal-streak computed from that log. The reader stamps each ended session into today's bucket when a valid date is available. Two screens (upgraded `ReadingStatsActivity`, new `ReadingHeatmapActivity`) and the status bar / home read from the store. Persistence is JSON at `/.inkpoint/reading_stats.json` (version bumped).

**Tech Stack:** C++20 (no exceptions/RTTI), ESP-IDF/Arduino-ESP32, PlatformIO, GoogleTest (host unit tests under `test/`), DS3231 over I2C, ESP-IDF SNTP, GfxRenderer/UITheme, ArduinoJson.

**Hardware reality (read first):**
- **X3** has a DS3231 RTC → real date always available once set. `HalClock::isAvailable()` is already true on X3.
- **X4 has no RTC.** Its only date source is the ESP32 internal RTC seeded by **NTP over Wi-Fi**. That date **survives deep sleep and `ESP.restart()`** (RTC power domain) but **resets on full power loss** (battery dead/removed) and **drifts** (internal RC oscillator). So on X4 the date is "valid" only after a sync and until power is cut; every date-dependent feature must degrade gracefully to "—"/hidden when no valid date is available. This is an accepted tradeoff (the user chose the NTP-seeded approach).

**Verification model:** The pure engine + date math are unit-tested on the host with GoogleTest (`cmake -S test -B test/build && cmake --build test/build && ctest --test-dir test/build`). UI, hardware (DS3231/SNTP), and persistence-on-device are human-tester scope. Build the firmware with `~/.platformio/penv/bin/pio`.

---

## Phases (execute in order; each is independently shippable)

1. **Calendar-date layer** — `HalClock` date API for X3 + X4, `DateProvider` wrapper, NTP date sync enabled on X4.
2. **Per-day reading engine** — `DayBucket` log, accessors, goal-streak, `booksStarted` (pure, host-TDD).
3. **Persistence + reader wiring** — JSON format bump, `JsonSettingsIO`, `dailyGoalMinutes` setting, reader stamps sessions into today.
4. **Status-bar date** — render `DD/MM/YYYY` next to the battery when a valid date exists.
5. **Reading Stats screen** — tile grid + 7-day bar chart + annual total; keep the per-book list behind "More Details".
6. **Reading Heatmap screen** — month calendar shaded by reading minutes + summary tiles + month navigation; wire into the stats screen.
7. **Home recent-books covers + %** — replace the single Continue-Reading tile with a row of cover thumbnails carrying progress bars and percentages.

---

## File Structure

- **Modify** `lib/hal/HalClock.h` / `lib/hal/HalClock.cpp` — add `getDate()`, `hasValidDate()`, X4 system-clock date path.
- **Create** `lib/ReadingStats/DateProvider.h` — thin helper turning `HalClock` into `{valid, year, month, day, dayOfYear}` (pure struct + one device-side fill function).
- **Modify** `lib/ReadingStats/ReadingStats.h` / `ReadingStats.cpp` — `DayBucket`, per-day log, `booksStarted`, accessors, `computeGoalStreak`.
- **Modify** `lib/ReadingStats/StatsFormat.h` / `StatsFormat.cpp` — formatting helpers (e.g. `formatHm`, `formatDate`) used by tests + UI.
- **Modify** `src/ReadingStatsStore.h` / `src/ReadingStatsStore.cpp` — expose new accessors + record-day-minutes.
- **Modify** `src/JsonSettingsIO.cpp` (reading-stats load/save) — persist `version`, `days`, `booksStarted`.
- **Modify** `src/InkPointSettings.h` / `src/SettingsList.h` — `dailyGoalMinutes`.
- **Modify** `src/activities/reader/EpubReaderActivity.cpp` — stamp session minutes + reading day on session end; `booksStarted` on first session.
- **Modify** `src/components/themes/BaseTheme.cpp` (and `lyra/LyraTheme.cpp` if it overrides the header) — date in the status bar.
- **Modify** `src/activities/home/ReadingStatsActivity.{h,cpp}` — upgraded tile/chart layout.
- **Create** `src/activities/home/ReadingHeatmapActivity.{h,cpp}` — the heatmap screen.
- **Modify** `src/activities/ActivityManager.{h,cpp}` — `goToReadingHeatmap()`.
- **Modify** `src/activities/home/HomeActivity.{h,cpp}` — recent-books cover row with progress %.
- **Modify** `lib/I18n/translations/english.yaml` (+ others) — new strings.
- **Test** `test/reading_stats/ReadingStatsTest.cpp` — new engine cases.
- **Docs** `docs/file-formats.md` — bump reading_stats.json version note.

---

## Phase 1 — Calendar-date layer

Goal: one API that answers "what is today's date, and is it trustworthy?" on both X3 and X4.

### Task 1.1: HalClock date API — declaration

**Files:**
- Modify: `lib/hal/HalClock.h`

- [ ] **Step 1: Add the date API to the class**

In `lib/hal/HalClock.h`, inside `class HalClock`'s public section (after `getTime(...)`), add:

```cpp
  // Current local calendar date. Works on both hardware variants:
  //   X3: read from the DS3231 date registers.
  //   X4: read from the ESP32 system clock, valid only after an NTP sync this
  //       power cycle (survives sleep/restart, lost on power loss).
  // utcOffsetQuarterHoursBiased matches getTime()/formatTime() (48 = UTC+0).
  // Returns false (and leaves outputs untouched) when no trustworthy date exists.
  bool getDate(int16_t& year, uint8_t& month, uint8_t& day, uint16_t& dayOfYear,
               uint8_t utcOffsetQuarterHoursBiased = 48) const;

  // Convenience: true when getDate() would succeed right now.
  bool hasValidDate(uint8_t utcOffsetQuarterHoursBiased = 48) const;
```

- [ ] **Step 2: Commit**

```bash
git add lib/hal/HalClock.h
git commit -m "feat(clock): declare HalClock date API (X3 + X4)"
```

### Task 1.2: HalClock date API — implementation

**Files:**
- Modify: `lib/hal/HalClock.cpp`

- [ ] **Step 1: Add the DS3231 date registers + implementation**

The DS3231 exposes date at registers `0x04` (date/day-of-month), `0x05` (month, bit7 = century), `0x06` (year 00-99). Add near the existing register usage in `lib/hal/HalClock.cpp` and implement both paths. `bcdToDec` already exists in this file.

```cpp
#include <sys/time.h>  // add with the other includes if not present

// DS3231 date registers (BCD): 0x04 day-of-month, 0x05 month(+century bit7), 0x06 year(00-99)
static constexpr uint8_t DS3231_DATE_REG = 0x04;

// Days before the start of each month for a non-leap year (index 1..12).
static int daysBeforeMonth(uint8_t month, int16_t year) {
  static const int cum[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int d = cum[month];
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (leap && month > 2) d += 1;
  return d;  // 0-based day-of-year for the 1st of `month`
}

bool HalClock::getDate(int16_t& year, uint8_t& month, uint8_t& day, uint16_t& dayOfYear,
                       uint8_t utcOffsetQuarterHoursBiased) const {
  if (_available) {
    // X3 path: read date straight from the DS3231 (already local time on the chip).
    Wire.beginTransmission(I2C_ADDR_DS3231);
    Wire.write(DS3231_DATE_REG);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(I2C_ADDR_DS3231, (uint8_t)3);
    if (Wire.available() < 3) return false;
    const uint8_t d = bcdToDec(Wire.read());
    const uint8_t mo = bcdToDec(Wire.read() & 0x1F);  // mask century bit
    const uint8_t yy = bcdToDec(Wire.read());
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
    year = 2000 + yy;
    month = mo;
    day = d;
    dayOfYear = static_cast<uint16_t>(daysBeforeMonth(mo, year) + (d - 1));
    return true;
  }

  // X4 path: system clock, only trustworthy after a sync this power cycle.
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // ~2023-11; unset/garbage clock
  const long offsetSec = (static_cast<long>(utcOffsetQuarterHoursBiased) - 48) * 15 * 60;
  time_t local = now + offsetSec;
  struct tm t;
  gmtime_r(&local, &t);
  if (t.tm_year + 1900 < 2023) return false;
  year = static_cast<int16_t>(t.tm_year + 1900);
  month = static_cast<uint8_t>(t.tm_mon + 1);
  day = static_cast<uint8_t>(t.tm_mday);
  dayOfYear = static_cast<uint16_t>(t.tm_yday);  // 0..365
  return true;
}

bool HalClock::hasValidDate(uint8_t utcOffsetQuarterHoursBiased) const {
  int16_t y;
  uint8_t mo, d;
  uint16_t doy;
  return getDate(y, mo, d, doy, utcOffsetQuarterHoursBiased);
}
```

- [ ] **Step 2: Build the firmware**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -6`
Expected: SUCCESS, 0 errors.

- [ ] **Step 3: Commit**

```bash
git add lib/hal/HalClock.cpp
git commit -m "feat(clock): implement getDate/hasValidDate for X3 (DS3231) and X4 (system clock)"
```

### Task 1.3: Allow NTP date sync on X4

The X4 needs `syncFromNTP()` to set the **system clock** even though there is no DS3231 to write. `syncFromNTP()` already calls SNTP (it sets the system time before writing the RTC). Make `begin()` not disable the SNTP path on X4, and make the clock-sync entry point reachable on X4.

**Files:**
- Modify: `lib/hal/HalClock.cpp` (`syncFromNTP`)
- Modify: `src/activities/network/WifiSelectionActivity.cpp:256` (the auto-sync gate)

- [ ] **Step 1: Confirm `syncFromNTP` sets system time on both variants**

Read `lib/hal/HalClock.cpp`'s `syncFromNTP()`. It must call `configTime(...)` / wait for SNTP, which sets the system clock regardless of hardware, and only the **`writeTimeToRTC`** part is DS3231-specific. Make `syncFromNTP` skip the DS3231 write on X4 but still return true if the system clock was set:

```cpp
bool HalClock::syncFromNTP() {
  if (WiFi.status() != WL_CONNECTED) return false;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  // getLocalTime blocks up to the timeout waiting for the first SNTP response.
  if (!getLocalTime(&t, 5000)) {
    LOG_ERR("CLK", "NTP sync timed out");
    return false;
  }
  if (_available) {
    // X3: persist into the DS3231 so the date survives power loss.
    writeTimeToRTC(static_cast<uint8_t>(t.tm_hour), static_cast<uint8_t>(t.tm_min),
                   static_cast<uint8_t>(t.tm_sec));
    // (date registers are written by the existing DS3231 write path if present)
  }
  LOG_INF("CLK", "NTP sync OK (%04d-%02d-%02d)", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return true;
}
```

Keep the existing DS3231 write code if it already writes date registers; only the guard (`if (_available)`) and the early system-clock acquisition are required. If the existing `syncFromNTP` differs, adapt it minimally so that on X4 it sets the system clock and returns true, and on X3 it also writes the DS3231.

- [ ] **Step 2: Let the auto clock-sync run on X4**

In `src/activities/network/WifiSelectionActivity.cpp` around line 256 the auto-sync is gated on `halClock.isAvailable()` (X3 only). Change it so the device also syncs on X4 (where `isAvailable()` is false) when not yet synced this session:

```cpp
    // Sync the clock opportunistically while Wi-Fi is up. On X3 this seeds the
    // DS3231; on X4 it seeds the system clock for the session (date features).
    if (!SETTINGS.clockHasBeenSynced) {
      if (halClock.syncFromNTP()) {
        SETTINGS.clockHasBeenSynced = 1;
        SETTINGS.saveToFile();
      }
    }
```

(Replace the previous `halClock.isAvailable() && !SETTINGS.clockHasBeenSynced` condition. Note: on X4, `clockHasBeenSynced` should be treated as "synced this power cycle" — Phase 3 Task adds a power-cycle reset; for now it gates the WiFi auto-sync.)

- [ ] **Step 3: Build**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -6`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add lib/hal/HalClock.cpp src/activities/network/WifiSelectionActivity.cpp
git commit -m "feat(clock): seed the system clock via NTP on X4 (date features)"
```

### Task 1.4: Reset the X4 "synced" flag on cold boot

On X4 the system clock is lost on power loss, so `clockHasBeenSynced` must not persist a stale "synced" across a power cycle. On every cold boot, if the system clock is unset, clear the flag so the next Wi-Fi connection re-syncs.

**Files:**
- Modify: `src/main.cpp` (in `setup()`, after `SETTINGS.loadFromFile()` and `halClock.begin()`)

- [ ] **Step 1: Clear the flag when the clock is actually unset**

In `src/main.cpp`'s `setup()`, after settings load and `halClock.begin()`, add:

```cpp
  // On X4 the NTP-seeded system clock is lost on power loss. If it is unset now,
  // forget any previous "synced" flag so Wi-Fi will re-seed it. (X3 keeps the
  // DS3231 date across power loss, so its flag is left as-is.)
  if (!halClock.isAvailable() && time(nullptr) < 1700000000 && SETTINGS.clockHasBeenSynced) {
    SETTINGS.clockHasBeenSynced = 0;
    SETTINGS.saveToFile();
  }
```

- [ ] **Step 2: Build + commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -4
git add src/main.cpp
git commit -m "fix(clock): re-arm X4 NTP sync after power loss"
```

---

## Phase 2 — Per-day reading engine (host-TDD)

Goal: the pure aggregator stores a per-day reading-minutes log and `booksStarted`, and computes goal streaks. All host-testable.

### Task 2.1: DayBucket store + record/query

**Files:**
- Modify: `lib/ReadingStats/ReadingStats.h`
- Modify: `lib/ReadingStats/ReadingStats.cpp`
- Test: `test/reading_stats/ReadingStatsTest.cpp`

- [ ] **Step 1: Write failing tests for the per-day log**

Append to `test/reading_stats/ReadingStatsTest.cpp`:

```cpp
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
  // last-7 window ending on (2026, 11) covers days 5..11
  EXPECT_EQ(agg.windowMs(2026, 11, 7), 1200000u);
  // last-30 window ending (2026,11) reaches back into 2025 day 364
  EXPECT_EQ(agg.windowMs(2026, 11, 30), 1800000u);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake -S test -B test/build >/dev/null && cmake --build test/build --target ReadingStatsTest 2>&1 | tail -15`
Expected: FAIL — `recordReadingMs` / `msForDay` / `daysRead` / `bestDayMs` / `annualMs` / `windowMs` not declared.

- [ ] **Step 3: Add the DayBucket struct + members to the header**

In `lib/ReadingStats/ReadingStats.h`, add inside `namespace reading_stats` before the aggregator, and new members/methods to `ReadingStatsAggregator`:

```cpp
// One calendar day of accumulated reading time. Compact for SD persistence.
struct DayBucket {
  int16_t year = 0;
  uint16_t dayOfYear = 0;  // 0..365
  uint32_t ms = 0;
  bool operator==(const DayBucket& o) const = default;
};
```

Add to the aggregator's public section:

```cpp
  // --- Per-day reading log ---
  // Add reading time to a specific calendar day (creates the day if new).
  void recordReadingMs(int16_t year, uint16_t dayOfYear, uint32_t ms);
  // Total ms recorded for one day (0 if none).
  uint32_t msForDay(int16_t year, uint16_t dayOfYear) const;
  // Number of distinct days with any reading time.
  uint32_t daysRead() const { return static_cast<uint32_t>(days_.size()); }
  // Largest single-day total (0 if no days).
  uint32_t bestDayMs() const;
  // Total ms in a calendar year.
  uint32_t annualMs(int16_t year) const;
  // Total ms in the `count`-day window ending on (year, dayOfYear), inclusive.
  uint32_t windowMs(int16_t year, uint16_t dayOfYear, uint16_t count) const;
  // Read-only access for charts/heatmap and persistence.
  const std::vector<DayBucket>& days() const { return days_; }
  void loadDays(std::vector<DayBucket> days) { days_ = std::move(days); }
```

And a private member:

```cpp
  std::vector<DayBucket> days_;  // unsorted; one entry per (year, dayOfYear)
```

- [ ] **Step 4: Implement in the .cpp**

Add to `lib/ReadingStats/ReadingStats.cpp`. To convert (year, dayOfYear) into a comparable ordinal for window math, use a helper:

```cpp
namespace {
// A strictly increasing ordinal across years for window comparisons.
// 366 slots per year is enough since dayOfYear is 0..365.
int64_t dayOrdinal(int16_t year, uint16_t dayOfYear) {
  return static_cast<int64_t>(year) * 366 + dayOfYear;
}
}  // namespace

void ReadingStatsAggregator::recordReadingMs(int16_t year, uint16_t dayOfYear, uint32_t ms) {
  if (ms == 0) return;
  for (auto& d : days_) {
    if (d.year == year && d.dayOfYear == dayOfYear) {
      d.ms += ms;
      return;
    }
  }
  days_.push_back(DayBucket{year, dayOfYear, ms});
}

uint32_t ReadingStatsAggregator::msForDay(int16_t year, uint16_t dayOfYear) const {
  for (const auto& d : days_)
    if (d.year == year && d.dayOfYear == dayOfYear) return d.ms;
  return 0;
}

uint32_t ReadingStatsAggregator::bestDayMs() const {
  uint32_t best = 0;
  for (const auto& d : days_) best = (d.ms > best) ? d.ms : best;
  return best;
}

uint32_t ReadingStatsAggregator::annualMs(int16_t year) const {
  uint32_t sum = 0;
  for (const auto& d : days_)
    if (d.year == year) sum += d.ms;
  return sum;
}

uint32_t ReadingStatsAggregator::windowMs(int16_t year, uint16_t dayOfYear, uint16_t count) const {
  const int64_t end = dayOrdinal(year, dayOfYear);
  const int64_t start = end - (static_cast<int64_t>(count) - 1);
  uint32_t sum = 0;
  for (const auto& d : days_) {
    const int64_t o = dayOrdinal(d.year, d.dayOfYear);
    if (o >= start && o <= end) sum += d.ms;
  }
  return sum;
}
```

- [ ] **Step 5: Run tests to verify pass**

Run: `cmake --build test/build --target ReadingStatsTest 2>&1 | tail -4 && ctest --test-dir test/build -R ReadingStats 2>&1 | tail -6`
Expected: all ReadingStats tests PASS.

- [ ] **Step 6: Commit**

```bash
git add lib/ReadingStats/ReadingStats.h lib/ReadingStats/ReadingStats.cpp test/reading_stats/ReadingStatsTest.cpp
git commit -m "feat(stats): per-day reading-minutes log with window/annual sums"
```

### Task 2.2: Goal streak from the per-day log

**Files:**
- Modify: `lib/ReadingStats/ReadingStats.h` / `.cpp`
- Test: `test/reading_stats/ReadingStatsTest.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
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
  EXPECT_EQ(gs.max, 2u);      // best run is 2 (100-101 and 103-104)
}

TEST(ReadingStatsGoal, GoalStreakZeroWhenTodayMissed) {
  ReadingStatsAggregator agg;
  agg.recordReadingMs(2026, 100, 5000000);
  auto gs = agg.computeGoalStreak(2026, 101, 3600000);  // today (101) has nothing
  EXPECT_EQ(gs.current, 0u);
  EXPECT_EQ(gs.max, 1u);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build test/build --target ReadingStatsTest 2>&1 | tail -8`
Expected: FAIL — `computeGoalStreak` / `GoalStreak` undefined.

- [ ] **Step 3: Implement**

In `ReadingStats.h` (in the namespace, before the aggregator):

```cpp
struct GoalStreak {
  uint16_t current = 0;
  uint16_t max = 0;
};
```

Add to the aggregator public section:

```cpp
  // Consecutive days (ending today) whose reading time met `goalMs`, plus the
  // longest such run ever. A day with no record counts as missed.
  GoalStreak computeGoalStreak(int16_t todayYear, uint16_t todayDayOfYear, uint32_t goalMs) const;
```

In `ReadingStats.cpp`:

```cpp
GoalStreak ReadingStatsAggregator::computeGoalStreak(int16_t todayYear, uint16_t todayDayOfYear,
                                                     uint32_t goalMs) const {
  if (goalMs == 0) return {};
  // Collect met-day ordinals into a sorted set via a temporary vector.
  std::vector<int64_t> met;
  met.reserve(days_.size());
  for (const auto& d : days_)
    if (d.ms >= goalMs) met.push_back(dayOrdinal(d.year, d.dayOfYear));
  std::sort(met.begin(), met.end());

  GoalStreak gs;
  // Longest run of consecutive ordinals.
  uint16_t run = 0;
  int64_t prev = INT64_MIN;
  for (int64_t o : met) {
    run = (o == prev + 1) ? static_cast<uint16_t>(run + 1) : 1;
    if (run > gs.max) gs.max = run;
    prev = o;
  }
  // Current run must end today: walk back from today while met.
  const int64_t today = dayOrdinal(todayYear, todayDayOfYear);
  int64_t cur = today;
  while (std::binary_search(met.begin(), met.end(), cur)) {
    gs.current++;
    cur -= 1;
  }
  return gs;
}
```

Ensure `<algorithm>` is included in `ReadingStats.cpp`.

- [ ] **Step 4: Run tests**

Run: `cmake --build test/build --target ReadingStatsTest 2>&1 | tail -3 && ctest --test-dir test/build -R ReadingStats 2>&1 | tail -5`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/ReadingStats/ReadingStats.h lib/ReadingStats/ReadingStats.cpp test/reading_stats/ReadingStatsTest.cpp
git commit -m "feat(stats): goal-streak computed from the per-day log"
```

### Task 2.3: booksStarted counter

**Files:**
- Modify: `lib/ReadingStats/ReadingStats.h` / `.cpp`
- Test: `test/reading_stats/ReadingStatsTest.cpp`

- [ ] **Step 1: Failing test**

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build test/build --target ReadingStatsTest 2>&1 | tail -6`
Expected: FAIL — `booksStarted()` undefined.

- [ ] **Step 3: Implement**

`booksStarted` is simply the number of distinct books that have any stats, which equals `books_.size()` after the first session of each. Add an accessor and a persisted counter so it cannot drift if entries are capped. In `ReadingStats.h`:

```cpp
  uint16_t booksStarted() const { return booksStarted_; }
  void setBooksStarted(uint16_t n) { booksStarted_ = n; }
```

Private member: `uint16_t booksStarted_ = 0;`

In `ReadingStats.cpp`, inside `beginSession`, when a brand-new book index is created (the path was not present before), increment `booksStarted_`. Locate the spot in `beginSession` where a new `BookStats` is appended to `books_` and add:

```cpp
    if (booksStarted_ < UINT16_MAX) booksStarted_++;
```

immediately after the new book is pushed. (If `beginSession` reuses an existing index, do not increment.)

- [ ] **Step 4: Run tests + commit**

```bash
cmake --build test/build --target ReadingStatsTest 2>&1 | tail -3 && ctest --test-dir test/build -R ReadingStats 2>&1 | tail -4
git add lib/ReadingStats/ReadingStats.h lib/ReadingStats/ReadingStats.cpp test/reading_stats/ReadingStatsTest.cpp
git commit -m "feat(stats): track booksStarted (distinct books opened)"
```

### Task 2.4: Formatting helpers

**Files:**
- Modify: `lib/ReadingStats/StatsFormat.h` / `.cpp`
- Test: `test/reading_stats/ReadingStatsTest.cpp`

- [ ] **Step 1: Failing tests**

```cpp
#include "StatsFormat.h"
TEST(StatsFormat, FormatsHoursAndMinutes) {
  char buf[16];
  reading_stats::formatHm(buf, sizeof(buf), 3600000 + 18 * 60000);  // 1h 18m
  EXPECT_STREQ(buf, "1h 18m");
  reading_stats::formatHm(buf, sizeof(buf), 7 * 60000);  // 7m
  EXPECT_STREQ(buf, "7m");
  reading_stats::formatHm(buf, sizeof(buf), 66 * 3600000 + 18 * 60000);  // 66h 18m
  EXPECT_STREQ(buf, "66h 18m");
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build test/build --target ReadingStatsTest 2>&1 | tail -6`
Expected: FAIL — `formatHm` undefined (or already declared but unimplemented).

- [ ] **Step 3: Implement**

In `lib/ReadingStats/StatsFormat.h` (declare in `namespace reading_stats`):

```cpp
// Format a millisecond duration as "Hh Mm" (or "Mm" when under an hour).
// Writes into buf (>=16 bytes). Always null-terminates.
void formatHm(char* buf, size_t bufSize, uint32_t ms);
```

In `StatsFormat.cpp`:

```cpp
#include "StatsFormat.h"
#include <cstdio>
namespace reading_stats {
void formatHm(char* buf, size_t bufSize, uint32_t ms) {
  const uint32_t totalMin = ms / 60000;
  const uint32_t h = totalMin / 60;
  const uint32_t m = totalMin % 60;
  if (h > 0)
    std::snprintf(buf, bufSize, "%uh %um", static_cast<unsigned>(h), static_cast<unsigned>(m));
  else
    std::snprintf(buf, bufSize, "%um", static_cast<unsigned>(m));
}
}  // namespace reading_stats
```

(If `StatsFormat.h`/`.cpp` already exist with other helpers, add `formatHm` alongside them; the CMake target already compiles `StatsFormat.cpp`.)

- [ ] **Step 4: Run tests + commit**

```bash
cmake --build test/build --target ReadingStatsTest 2>&1 | tail -3 && ctest --test-dir test/build -R "ReadingStats|StatsFormat" 2>&1 | tail -4
git add lib/ReadingStats/StatsFormat.h lib/ReadingStats/StatsFormat.cpp test/reading_stats/ReadingStatsTest.cpp
git commit -m "feat(stats): formatHm duration helper"
```

---

## Phase 3 — Persistence + reader wiring + daily goal setting

### Task 3.1: Persist days + booksStarted (JSON format bump)

**Files:**
- Modify: `src/ReadingStatsStore.h` — expose `recordReadingMs`, `days()`, `booksStarted`, `computeGoalStreak`, `loadDays`, `setBooksStarted`.
- Modify: `src/JsonSettingsIO.cpp` — read/write `version`, `days`, `booksStarted`.
- Modify: `docs/file-formats.md` — note the new version.

- [ ] **Step 1: Forward the new engine API through the store**

In `src/ReadingStatsStore.h`, add public pass-throughs mirroring the existing ones:

```cpp
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
```

And in the private section add to the `JsonSettingsIO::loadReadingStats` friend's reach:

```cpp
  void loadDays(std::vector<reading_stats::DayBucket> d) { aggregator.loadDays(std::move(d)); }
  void setBooksStarted(uint16_t n) { aggregator.setBooksStarted(n); }
```

- [ ] **Step 2: Read the current JSON save/load in JsonSettingsIO**

Run: `grep -n "reading_stats\|ReadingStats\|booksFinished\|currentStreak\|\"books\"\|version" src/JsonSettingsIO.cpp | head -30`
Read the existing `saveReadingStats` / `loadReadingStats` so the additions match the document shape and the ArduinoJson idiom in use.

- [ ] **Step 3: Add `version`, `days`, `booksStarted` to save**

In `saveReadingStats`, add a top-level `version` and a `days` array, plus `booksStarted`. Cap `days` to the most recent 400 entries to bound size. Example additions (adapt to the existing doc variable, here assumed `doc`):

```cpp
  doc["version"] = 2;
  doc["booksStarted"] = store.booksStarted();
  JsonArray days = doc["days"].to<JsonArray>();
  // Persist newest-first, capped, so the file never grows unbounded.
  const auto& src = store.days();
  size_t kept = 0;
  for (auto it = src.rbegin(); it != src.rend() && kept < 400; ++it, ++kept) {
    JsonObject o = days.add<JsonObject>();
    o["y"] = it->year;
    o["d"] = it->dayOfYear;
    o["ms"] = it->ms;
  }
```

- [ ] **Step 4: Add `days` + `booksStarted` to load**

In `loadReadingStats`, after restoring the existing fields, restore the new ones (default-safe for old v1 files that lack them):

```cpp
  store.setBooksStarted(doc["booksStarted"] | 0);
  std::vector<reading_stats::DayBucket> days;
  for (JsonObject o : doc["days"].as<JsonArray>()) {
    reading_stats::DayBucket b;
    b.year = o["y"] | 0;
    b.dayOfYear = o["d"] | 0;
    b.ms = o["ms"] | 0u;
    if (b.year != 0) days.push_back(b);
  }
  store.loadDays(std::move(days));
```

- [ ] **Step 5: Document the format bump**

In `docs/file-formats.md`, add a line under the reading-stats section: `reading_stats.json: version 2 — adds days[] (per-day reading minutes) and booksStarted. v1 files load with empty days.`

- [ ] **Step 6: Build + commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -5
git add src/ReadingStatsStore.h src/JsonSettingsIO.cpp docs/file-formats.md
git commit -m "feat(stats): persist per-day log + booksStarted (reading_stats.json v2)"
```

### Task 3.2: dailyGoalMinutes setting

**Files:**
- Modify: `src/InkPointSettings.h`
- Modify: `src/SettingsList.h`
- Modify: `lib/I18n/translations/english.yaml`

- [ ] **Step 1: Add the field**

In `src/InkPointSettings.h`, near the other reading settings, add:

```cpp
  // Daily reading goal in minutes (used by Reading Stats / Heatmap goal streak).
  uint16_t dailyGoalMinutes = 60;
```

- [ ] **Step 2: Add a settings-list entry**

In `src/SettingsList.h`, follow the existing numeric-setting pattern (look at how `sleepTimeoutMinutes` is wired at `src/SettingsList.h:187`) to add a `dailyGoalMinutes` entry under the reading category with a sensible range (e.g. 5–600, step 5) and a new string id `STR_DAILY_GOAL`.

- [ ] **Step 3: Add the string**

In `lib/I18n/translations/english.yaml`, add `STR_DAILY_GOAL: "Daily goal (min)"`. Run `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`.

- [ ] **Step 4: Build + commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -4
git add src/InkPointSettings.h src/SettingsList.h lib/I18n/translations/english.yaml
git commit -m "feat(stats): dailyGoalMinutes setting"
```

### Task 3.3: Stamp sessions into today (reader wiring)

**Files:**
- Modify: `src/activities/reader/EpubReaderActivity.cpp` (the Phase-2 hook at ~line 197 and the session-end path)

- [ ] **Step 1: Read the existing session lifecycle in the reader**

Run: `grep -n "READING_STATS\|beginSession\|endSession\|recordPageTurn\|recordReadingDay\|booksFinished\|Phase 2" src/activities/reader/EpubReaderActivity.cpp`
Read those sites to see where a session ends and where the per-page time is banked.

- [ ] **Step 2: On session end, attribute the session's minutes to today**

The aggregator already banks per-book ms. To get the **session's** ms, capture `totalReadingMs()` before/after, or read the active book's delta. The simplest correct approach: have the store expose the ms banked by the most recent `endSession`. Add to `ReadingStatsStore` a tiny helper that records the day at end time. In `src/ReadingStatsStore.cpp`'s `endSession`, after the aggregator ends the session and you know the session ms (compute as `totalReadingMs()` delta captured around `endSession`, or expose `lastSessionMs()` from the aggregator — add `uint32_t lastSessionMs_` set in `endSession`), call into a date-aware wrapper. Concretely, add to the aggregator an out-param:

In `ReadingStats.h` change `endSession`:

```cpp
  // Returns the ms banked by this session (0 if none active).
  uint32_t endSession(uint32_t nowMs);
```

In `ReadingStats.cpp`, make `endSession` return the delta it banked. Then in `EpubReaderActivity` where `READING_STATS.endSession(millis())` is called, wrap it:

```cpp
    const uint32_t sessionMs = READING_STATS.endSession(millis());
    int16_t y; uint8_t mo, d; uint16_t doy;
    if (sessionMs > 0 && halClock.getDate(y, mo, d, doy, SETTINGS.clockUtcOffsetQ)) {
      READING_STATS.recordReadingMs(y, doy, sessionMs);
      READING_STATS.recordReadingDay(y, doy);  // enables the reading-day streak on X3/X4-when-synced
    }
```

(`ReadingStatsStore::endSession` must forward the aggregator's return value; update its signature to `uint32_t endSession(uint32_t nowMs)` and `return aggregator.endSession(nowMs);` plus its existing save.)

- [ ] **Step 3: Build + commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -5
git add lib/ReadingStats/ReadingStats.h lib/ReadingStats/ReadingStats.cpp src/ReadingStatsStore.h src/ReadingStatsStore.cpp src/activities/reader/EpubReaderActivity.cpp
git commit -m "feat(stats): stamp ended reading sessions into today's bucket"
```

- [ ] **Step 4: Update the host test for the new endSession return**

Adjust existing tests that call `endSession` (they ignore the return, which is fine) and add:

```cpp
TEST(ReadingStats, EndSessionReturnsBankedMs) {
  ReadingStatsAggregator agg;
  agg.beginSession("/b.epub", 1000);
  agg.recordPageTurn(3000, true);
  EXPECT_EQ(agg.endSession(4000), 3000u);  // 2000 + 1000
}
```

Run: `cmake --build test/build --target ReadingStatsTest 2>&1 | tail -3 && ctest --test-dir test/build -R ReadingStats 2>&1 | tail -4`
Expected: PASS. Then commit the test.

---

## Phase 4 — Status-bar date

### Task 4.1: Show the date next to the battery

**Files:**
- Modify: `src/components/themes/BaseTheme.cpp` (the header/battery draw, ~lines 79–110)
- Modify: `lib/I18n` if a format string is needed (date is numeric `DD/MM/YYYY`, no translation needed)

- [ ] **Step 1: Read the header battery layout**

Run: `sed -n '79,140p' src/components/themes/BaseTheme.cpp` and find where `drawBatteryRight` is called from `drawHeader`, plus where the optional status-bar clock is drawn (around `SETTINGS.statusBarClock` near line 797).

- [ ] **Step 2: Draw the date when available**

In the header draw, when `halClock.hasValidDate(SETTINGS.clockUtcOffsetQ)` is true, render `DD/MM/YYYY` immediately to the left of the battery percentage using `SMALL_FONT_ID`, mirroring how the existing status-bar clock positions itself to the left of the battery (see the `statusBarClock` block). Build the string:

```cpp
  int16_t y; uint8_t mo, d; uint16_t doy;
  if (halClock.getDate(y, mo, d, doy, SETTINGS.clockUtcOffsetQ)) {
    char dateBuf[12];
    std::snprintf(dateBuf, sizeof(dateBuf), "%02u/%02u/%04u", (unsigned)d, (unsigned)mo, (unsigned)y);
    const int dateW = renderer.getTextWidth(SMALL_FONT_ID, dateBuf);
    // place to the left of the battery icon/percentage (reuse the same x math as
    // the statusBarClock block, subtracting dateW + a small gap)
    renderer.drawText(SMALL_FONT_ID, /* computed x */, textY, dateBuf);
  }
```

Use the exact x/y the existing clock block computes (copy its positioning and substitute `dateBuf`/`dateW`). Include `<cstdio>` if needed.

- [ ] **Step 3: Build + verify on host build, commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -5
git add src/components/themes/BaseTheme.cpp
git commit -m "feat(ui): show current date in the status bar when a clock is available"
```

- [ ] **Step 4: If LyraTheme overrides the header, mirror the change**

Run: `grep -n "drawHeader\|statusBarClock\|drawBatteryRight" src/components/themes/lyra/LyraTheme.cpp`. If Lyra draws its own header, apply the same date block there and commit.

---

## Phase 5 — Reading Stats screen (tile grid + chart)

### Task 5.1: Upgrade ReadingStatsActivity layout

**Files:**
- Modify: `src/activities/home/ReadingStatsActivity.h` / `.cpp`
- Modify: `lib/I18n/translations/english.yaml`

**Context:** mirror the Activity render pattern used by `VirtualPetActivity` / `PomodoroActivity` (clearScreen → GUI.drawHeader → draw → GUI.drawButtonHints → displayBuffer). All data comes from `READING_STATS` (the new accessors). The current screen already loads a per-book list; keep it reachable as "More Details".

- [ ] **Step 1: Add i18n strings**

Add to `english.yaml` and regenerate:

```yaml
STR_STATS_TITLE2: "Reading Stats"
STR_STATS_GOAL_STREAK: "Goal Streak"
STR_STATS_MAX_GOAL_STREAK: "Max Goal Streak"
STR_STATS_DAILY_GOAL: "Daily Goal"
STR_STATS_READING_TIME: "Reading Time"
STR_STATS_BOOKS_FINISHED: "Books Finished"
STR_STATS_BOOKS_STARTED: "Books Started"
STR_STATS_7D: "7D"
STR_STATS_30D: "30D"
STR_STATS_DAILY_LAST7: "Daily Reading (Last 7 days)"
STR_STATS_ANNUAL: "Annual Reading"
STR_STATS_MORE_DETAILS: "More Details"
STR_STATS_HEATMAP: "Reading Heatmap"
STR_STATS_NO_CLOCK: "Set the clock (Wi-Fi) to track daily goals"
```

Run `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`.

- [ ] **Step 2: Compute today's date once in onEnter**

In `ReadingStatsActivity.h` add members:

```cpp
  bool haveDate = false;
  int16_t today_y = 0; uint16_t today_doy = 0;
  reading_stats::GoalStreak goal;
  uint32_t goalMs = 0;
```

In `onEnter()` (after `Activity::onEnter()`), fill them:

```cpp
  goalMs = static_cast<uint32_t>(SETTINGS.dailyGoalMinutes) * 60000u;
  uint8_t mo, d; uint16_t doy; int16_t y;
  haveDate = halClock.getDate(y, mo, d, doy, SETTINGS.clockUtcOffsetQ);
  today_y = y; today_doy = doy;
  if (haveDate) goal = READING_STATS.goalStreak(today_y, today_doy, goalMs);
  loadStats();  // existing per-book snapshot
  requestUpdate();
```

- [ ] **Step 3: Render the tile grid + 7-day bar chart**

Replace the totals portion of `render()` with a 2-column tile grid drawn via `renderer.drawRect`/`drawText`/`drawCenteredText` (reuse the `.spec`-like look). Tiles, in order, using `reading_stats::formatHm` for durations:

1. Goal Streak — `haveDate ? std::to_string(goal.current) : "—"`
2. Max Goal Streak — `haveDate ? std::to_string(goal.max) : "—"`
3. Daily Goal — `formatHm(today's ms) + " / " + formatHm(goalMs)` (today's ms = `haveDate ? READING_STATS.msForDay(today_y, today_doy) : 0`), with a check glyph when met
4. Reading Time — `formatHm(READING_STATS.totalReadingMs())`
5. Books Finished — `READING_STATS.booksFinished()`
6. Books Started — `READING_STATS.booksStarted()`
7. 7D — `haveDate ? formatHm(READING_STATS.windowMs(today_y, today_doy, 7)) : "—"`
8. 30D — `haveDate ? formatHm(READING_STATS.windowMs(today_y, today_doy, 30)) : "—"`

Below the tiles, draw a **"Daily Reading (Last 7 days)"** bar chart: 7 bars; bar i height ∝ `READING_STATS.msForDay` for the day `i` days before today (compute each day's (year, dayOfYear) by walking back, handling year rollover via the ordinal→date inverse below). Label each bar with minutes and the date `DD/MM`. When `!haveDate`, draw `tr(STR_STATS_NO_CLOCK)` instead of the chart.

For walking back N days from (year, doy), use this inverse helper (add it privately in the .cpp):

```cpp
// Move (year, dayOfYear) back by `back` days, handling year boundaries.
static void minusDays(int16_t& year, uint16_t& doy, uint16_t back) {
  int days = static_cast<int>(doy) - back;
  while (days < 0) {
    year -= 1;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    days += leap ? 366 : 365;
  }
  doy = static_cast<uint16_t>(days);
}
```

Draw the annual total line: `tr(STR_STATS_ANNUAL) + " (" + year + "): " + formatHm(READING_STATS.annualMs(today_y))`.

- [ ] **Step 4: Wire navigation**

Keep Back = exit. Add two actions in `loop()` via `MappedInputManager`: Confirm on a "More Details" hint opens the existing per-book list (or scrolls to it); Right (or a dedicated hint) opens the heatmap (Phase 6 adds `goToReadingHeatmap()` — call it once it exists). Use the established `mappedInput.wasReleased(Button::...)` pattern from `PomodoroActivity`.

- [ ] **Step 5: Build + commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -6
git add src/activities/home/ReadingStatsActivity.h src/activities/home/ReadingStatsActivity.cpp lib/I18n/translations/english.yaml
git commit -m "feat(stats): tile-grid Reading Stats screen with 7-day chart and annual total"
```

---

## Phase 6 — Reading Heatmap screen

### Task 6.1: ReadingHeatmapActivity

**Files:**
- Create: `src/activities/home/ReadingHeatmapActivity.h` / `.cpp`
- Modify: `src/activities/ActivityManager.h` / `.cpp` (`goToReadingHeatmap`)
- Modify: `src/activities/home/ReadingStatsActivity.cpp` (open the heatmap)
- Modify: `lib/I18n/translations/english.yaml`

**Context:** new Activity mirroring `PomodoroActivity` structure. Shows a month calendar; each day cell is shaded by `READING_STATS.msForDay(year, doy)` against the legend thresholds (15/30/60/120/240 min) and marked with a check when the day met `dailyGoalMinutes`. Summary tiles: Month Total, Days Read, Best Day, Goal Streak. Left/Right change the month. Requires a valid date for "today"; the displayed month starts at today's month and can page backward.

- [ ] **Step 1: i18n strings**

```yaml
STR_HEATMAP_TITLE: "Reading Heatmap"
STR_HEATMAP_MONTH_TOTAL: "Month Total"
STR_HEATMAP_DAYS_READ: "Days Read"
STR_HEATMAP_BEST_DAY: "Best Day"
```

Regenerate i18n.

- [ ] **Step 2: Header + class**

`ReadingHeatmapActivity.h`:

```cpp
#pragma once
#include <cstdint>
#include "activities/Activity.h"

class ReadingHeatmapActivity final : public Activity {
 public:
  ReadingHeatmapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int16_t viewYear = 0;   // month currently displayed
  uint8_t viewMonth = 1;  // 1..12
  bool haveDate = false;
  uint32_t goalMs = 0;
  void prevMonth();
  void nextMonth();
};
```

- [ ] **Step 3: Implement the calendar math + render**

`ReadingHeatmapActivity.cpp` — compute, for the displayed month, each day's dayOfYear via `daysBeforeMonth` (duplicate the small helper locally or expose it from `ReadingStats`), then `READING_STATS.msForDay(viewYear, doy)`. Shade cells by thresholds:

```cpp
// Returns a 0..5 shade level for a day's reading ms.
static uint8_t shadeLevel(uint32_t ms) {
  const uint32_t m = ms / 60000;  // minutes
  if (m >= 240) return 5;
  if (m >= 120) return 4;
  if (m >= 60) return 3;
  if (m >= 30) return 2;
  if (m >= 15) return 1;
  return 0;
}
```

Render: header (`tr(STR_HEATMAP_TITLE)`), the four summary tiles (Month Total = sum of the month's days, Days Read = count of month days with ms>0, Best Day = max day ms + its `formatHm`, Goal Streak = `READING_STATS.goalStreak(today,...).current` when `haveDate`), the 7-column calendar grid (fill each cell with `renderer.fillRect` using a dither/checker for shade levels since e-ink is monochrome — use `renderer.fillRectDither(x,y,w,h,Color)` where available, else stipple by drawing dots proportional to level), a check glyph when `msForDay >= goalMs`, and the legend row. Button hints: Back, Open (today), Left, Right.

For the monochrome shading on e-ink, map shade level 0..5 to increasing dot density using `renderer.fillRectDither` (seen in `GfxRenderer.h`); level 5 = solid `fillRect`. Keep cell size derived from `renderer.getScreenWidth()` (no hardcoded dims).

`loop()`: `Left` → `prevMonth()`, `Right` → `nextMonth()`, `Back` → `finish()`. `prevMonth`/`nextMonth` adjust `viewMonth`/`viewYear` with rollover and `requestUpdate()`.

`onEnter()`: set `goalMs`, read today's date into `viewYear/viewMonth` (fallback to a fixed recent month label if `!haveDate`, but disable navigation past data).

- [ ] **Step 4: ActivityManager + open from stats**

In `ActivityManager.h` declare `void goToReadingHeatmap();`; in `.cpp` add `void ActivityManager::goToReadingHeatmap() { replaceActivity(std::make_unique<ReadingHeatmapActivity>(renderer, mappedInput)); }` and include the header. In `ReadingStatsActivity.cpp`, wire the "Reading Heatmap"/Right action to `activityManager.goToReadingHeatmap()`.

- [ ] **Step 5: Build + commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -6
git add src/activities/home/ReadingHeatmapActivity.h src/activities/home/ReadingHeatmapActivity.cpp src/activities/ActivityManager.h src/activities/ActivityManager.cpp src/activities/home/ReadingStatsActivity.cpp lib/I18n/translations/english.yaml
git commit -m "feat(stats): Reading Heatmap calendar screen"
```

---

## Phase 7 — Home recent-books covers + progress %

### Task 7.1: Cover row with progress bars

**Files:**
- Modify: `src/activities/home/HomeActivity.cpp` (the `render()` recent-books / Continue-Reading area and `loadRecentCovers`)
- Modify: `src/RecentBooksStore.h` if a progress percent is not already available

- [ ] **Step 1: Confirm where progress % comes from**

Run: `grep -n "progress\|percent\|coverBmpPath\|getCoverThumbPath\|drawRecentBookCover" src/RecentBooksStore.h src/activities/home/HomeActivity.cpp src/components/themes/BaseTheme.cpp | head -30`
Read how the existing single Continue-Reading cover is drawn and where the reading percentage is read (per-book `progress.bin` gives chapter/page → percent; the recents store or Epub metadata may already expose it). If no percent is readily available, compute it from the book's cached `progress.bin` via the existing reader/cache helper used elsewhere.

- [ ] **Step 2: Draw a row of up to 3 covers with progress bars**

In `HomeActivity::render()`, where the single cover tile is drawn, render up to three recent books side by side: each as a cover thumbnail (reuse `UITheme::getCoverThumbPath` / the existing cover-draw path), with a thin progress bar beneath (`renderer.drawRect` outline + `renderer.fillRect` to `percent`), the percentage text, and the (truncated) title below. Keep it behind the same `metrics.homeContinueReadingInMenu` / recents-available guards already used, and keep memory in budget (the existing cover-buffer logic already bounds the snapshot region — extend the rect to cover the row, or draw covers without the snapshot optimization if simpler and still within RAM).

- [ ] **Step 3: Build + verify, commit**

```bash
~/.platformio/penv/bin/pio run -e default 2>&1 | tail -6
git add src/activities/home/HomeActivity.cpp src/RecentBooksStore.h
git commit -m "feat(home): recent-books row with cover thumbnails and progress %"
```

---

## Phase 8 — i18n sweep + quality

### Task 8.1: Translate new strings + final checks

- [ ] **Step 1: Translate the new STR_* keys** to the other 23 languages (follow the established i18n process: edit each `lib/I18n/translations/<lang>.yaml`, then `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`). English fallback covers anything missed.
- [ ] **Step 2: Format** — `find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i`
- [ ] **Step 3: Clean build** — `~/.platformio/penv/bin/pio run -e default -t clean && ~/.platformio/penv/bin/pio run -e default 2>&1 | tail -8` (0 errors/warnings)
- [ ] **Step 4: Host tests** — `cmake --build test/build && ctest --test-dir test/build 2>&1 | tail -15` (all PASS)
- [ ] **Step 5: Confirm no gitignored/generated files staged** — `git status --short` (no `I18n*.generated`, `.pio/`, `test/build/`)
- [ ] **Step 6: Commit**

```bash
git add -u
git commit -m "i18n+chore: translate reading-stats strings; format + test sweep"
```

---

## Human-tester verification (on device — required; cannot be automated here)

1. **X3:** date shows in the status bar; opening a book and reading a few minutes increments today's bucket; Reading Stats tiles and the 7-day chart populate; the heatmap shades the right day; goal streak increments after meeting the daily goal two days running.
2. **X4 (no clock):** before any Wi-Fi sync, date-dependent tiles show "—" and the status bar omits the date; connect Wi-Fi, confirm the date appears and stats start attributing to the correct day; power-cycle the device and confirm the date disappears until the next sync (expected).
3. **Persistence:** read across a reboot and confirm `days[]` survives (check `/.inkpoint/reading_stats.json` on the SD card shows `version: 2` and day entries).
4. **Home:** recent books show covers with progress bars and percentages.
5. **Heap:** `ESP.getFreeHeap()` stays > 50 KB across the new screens; no leak entering/leaving the heatmap.

---

## Self-Review Notes

- **Spec coverage:** Reading Heatmap (Phase 6) ✓; richer Reading Stats with daily goal / goal streak / books started / 7-day chart / annual (Phases 2,3,5) ✓; home covers + per-book % (Phase 7) ✓; current date next to battery (Phase 4) ✓; "both" clock strategy — X3 DS3231 + X4 NTP-seeded (Phase 1) ✓.
- **Hard dependency made explicit:** every date-dependent feature degrades to "—"/hidden when `hasValidDate()` is false (Phases 4–6), so the X4-without-sync case never shows wrong data.
- **Type consistency:** `recordReadingMs`, `msForDay`, `daysRead`, `bestDayMs`, `annualMs`, `windowMs`, `computeGoalStreak`/`GoalStreak`, `booksStarted`, `formatHm`, `DayBucket`, `getDate`/`hasValidDate` are declared in Phase 1–2 and reused consistently by the store (Phase 3), screens (Phases 5–6), and status bar (Phase 4). `endSession` returns `uint32_t` (Phase 3.3) and the store/reader/tests are updated together.
- **Persistence safety:** `reading_stats.json` bumped to v2; v1 files load with empty `days` (default-safe), `days[]` capped at 400 entries.
- **Known UI caveat:** the UI phases (4–7) are device-verified (no host UI harness — matches the project's testing model) and build on confirmed APIs (`renderer.*`, `GUI.drawHeader/drawButtonHints`, `mappedInput.*`, the Activity lifecycle, `READING_STATS.*`). Each starts by reading the existing pattern (VirtualPet/Pomodoro/BaseTheme/Home) before implementing.
