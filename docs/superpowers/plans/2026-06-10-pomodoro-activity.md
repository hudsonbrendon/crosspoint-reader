# Pomodoro Activity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a standalone fixed-cadence Pomodoro timer Activity (25/5/15) reachable from the Home menu, E-Ink-safe and within the project's memory discipline.

**Architecture:** A new `PomodoroActivity` (heap-allocated, deleted on exit) mirrors `VirtualPetActivity`. The countdown advances inside `loop()` via `millis()` — no FreeRTOS task, no SPIFFS, no dynamic buffers. The screen re-renders at most once per minute. `preventAutoSleep()` keeps the device awake only while a phase is counting down. Wired into Home + `ActivityManager` exactly like the Virtual Pet tile.

**Tech Stack:** C++20 (no exceptions/RTTI), ESP-IDF/Arduino-ESP32, PlatformIO, GfxRenderer/UITheme, MappedInputManager, I18n YAML generator.

**Verification note:** This firmware has no host unit-test harness; "tests" here are compile + static-analysis + targeted code inspection. Each task ends by building. Device behaviour is human-tester scope (see spec). Build with the local penv pio: `~/.platformio/penv/bin/pio` (Homebrew pio lacks littlefs).

---

## File Structure

- **Create** `src/activities/tools/PomodoroActivity.h` — Activity declaration, `State` enum, members.
- **Create** `src/activities/tools/PomodoroActivity.cpp` — lifecycle, timer logic, render, input.
- **Create** `src/components/icons/clock.h` — 32×32 clock icon bitmap (generated).
- **Modify** `src/components/themes/BaseTheme.h` — add `Clock` to `UIIcon` enum.
- **Modify** `src/components/themes/lyra/LyraTheme.cpp` — include `clock.h`, add `Clock` case to 32px `iconForName`.
- **Modify** `src/activities/ActivityManager.h` — add `POMODORO_MENU` to `HomeMenuItem`, declare `goToPomodoro()`.
- **Modify** `src/activities/ActivityManager.cpp` — define `goToPomodoro()`.
- **Modify** `src/activities/home/HomeActivity.h` — add `POMODORO_MENU` to `menuItemToIndex`/`indexToMenuItem`, declare `onPomodoroOpen()`.
- **Modify** `src/activities/home/HomeActivity.cpp` — bump count, add tile (label + icon), Confirm dispatch case, `onPomodoroOpen()`.
- **Modify** `lib/I18n/translations/english.yaml` — add `STR_POMODORO_*` keys.
- **Modify** `lib/I18n/translations/portuguese.yaml` (+ other langs as time allows) — translations.
- **Regenerate** I18n headers via `scripts/gen_i18n.py` (output gitignored).

---

## Task 1: Clock icon

**Files:**
- Create: `src/components/icons/clock.h`

- [ ] **Step 1: Generate the icon header**

The existing icons are 32×32, 1bpp, MSB-first, `shape=0/black, background=1/transparent` (128 bytes). Generate deterministically rather than hand-authoring hex. Run this from the repo root:

```bash
python3 - <<'PY'
import math
N=32
buf=[[1]*N for _ in range(N)]  # 1 = background
cx=cy=15.5
def plot(x,y):
    xi,yi=int(round(x)),int(round(y))
    if 0<=xi<N and 0<=yi<N: buf[yi][xi]=0
# outer circle ring (r=14), 2px thick
for r in (14,13):
    for a in range(0,360,3):
        rad=math.radians(a); plot(cx+r*math.cos(rad), cy+r*math.sin(rad))
# 12 o'clock hand (up) and 4 o'clock-ish hand
for t in range(0,9):
    plot(cx, cy-t)              # minute hand up
for t in range(0,7):
    plot(cx+t*0.7, cy+t*0.7)   # hour hand toward lower-right
# center dot
for dx in (-1,0,1):
    for dy in (-1,0,1):
        plot(cx+dx,cy+dy)
# pack to bytes MSB-first
out=[]
for row in buf:
    for byte_i in range(N//8):
        b=0
        for bit in range(8):
            b=(b<<1)|row[byte_i*8+bit]
        out.append(b)
hexs=", ".join(f"0x{v:02X}" for v in out)
# wrap 12 per line
import textwrap
body=""
vals=[f"0x{v:02X}" for v in out]
for i in range(0,len(vals),12):
    body+="    "+", ".join(vals[i:i+12])+(",\n" if i+12<len(vals) else "};\n")
hdr='#pragma once\n#include <cstdint>\n\n// size: 32x32  (auto-generated; shape=0/black, background=1/transparent)\nstatic const uint8_t ClockIcon[] = {\n'+body
open("src/components/icons/clock.h","w").write(hdr)
print(hdr)
PY
```

Expected: `src/components/icons/clock.h` created, 128 bytes of data, recognizable clock when rendered.

- [ ] **Step 2: Sanity-check the file**

Run: `head -5 src/components/icons/clock.h`
Expected: `#pragma once`, the `// size: 32x32` comment, `static const uint8_t ClockIcon[] = {`.

- [ ] **Step 3: Commit**

```bash
git add src/components/icons/clock.h
git commit -m "feat(icons): add 32x32 clock icon for Pomodoro tile"
```

---

## Task 2: Register Clock in the icon system

**Files:**
- Modify: `src/components/themes/BaseTheme.h` (UIIcon enum, ~line 100-117)
- Modify: `src/components/themes/lyra/LyraTheme.cpp` (includes ~line 19-27, `iconForName` 32px switch ~line 64-88)

- [ ] **Step 1: Add `Clock` to the UIIcon enum**

In `src/components/themes/BaseTheme.h`, change the end of the enum:

```cpp
  Stats,
  Paw,
  Cards,
  Clock
};
```

- [ ] **Step 2: Include the icon in LyraTheme**

In `src/components/themes/lyra/LyraTheme.cpp`, add alongside the other icon includes (keep alphabetical-ish grouping near `cards.h`):

```cpp
#include "components/icons/clock.h"
```

- [ ] **Step 3: Map `Clock` in the 32px switch**

In `iconForName`, inside the `size == 32` switch, add before `default:`:

```cpp
      case UIIcon::Cards:
        return CardsIcon;
      case UIIcon::Clock:
        return ClockIcon;
      default:
        return nullptr;
```

- [ ] **Step 4: Build**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -20`
Expected: SUCCESS, 0 errors. (Clock icon now resolvable; not yet used.)

- [ ] **Step 5: Commit**

```bash
git add src/components/themes/BaseTheme.h src/components/themes/lyra/LyraTheme.cpp
git commit -m "feat(theme): wire Clock icon into UIIcon + Lyra iconForName"
```

---

## Task 3: i18n strings

**Files:**
- Modify: `lib/I18n/translations/english.yaml`
- Modify: `lib/I18n/translations/portuguese.yaml`

- [ ] **Step 1: Add English keys**

In `lib/I18n/translations/english.yaml`, after the Virtual Pet / Flashcard block (anywhere in the `STR_*` body), add:

```yaml
STR_POMODORO: "Pomodoro"
STR_POMODORO_READY: "Ready"
STR_POMODORO_FOCUS: "Focus"
STR_POMODORO_BREAK: "Break"
STR_POMODORO_LONG_BREAK: "Long Break"
STR_POMODORO_PAUSED: "Paused"
STR_POMODORO_FOCUS_NOW: "Focus!"
STR_POMODORO_BREAK_NOW: "Break time!"
STR_POMODORO_MIN_FMT: "%u min"
STR_POMODORO_START: "Start"
STR_POMODORO_PAUSE: "Pause"
STR_POMODORO_RESET: "Reset"
STR_POMODORO_SKIP: "Skip"
```

- [ ] **Step 2: Add Portuguese translations**

In `lib/I18n/translations/portuguese.yaml`, add the matching keys:

```yaml
STR_POMODORO: "Pomodoro"
STR_POMODORO_READY: "Pronto"
STR_POMODORO_FOCUS: "Foco"
STR_POMODORO_BREAK: "Pausa"
STR_POMODORO_LONG_BREAK: "Pausa Longa"
STR_POMODORO_PAUSED: "Pausado"
STR_POMODORO_FOCUS_NOW: "Foco!"
STR_POMODORO_BREAK_NOW: "Hora da pausa!"
STR_POMODORO_MIN_FMT: "%u min"
STR_POMODORO_START: "Iniciar"
STR_POMODORO_PAUSE: "Pausar"
STR_POMODORO_RESET: "Reiniciar"
STR_POMODORO_SKIP: "Pular"
```

(Other languages fall back to English automatically; translate later as a follow-up — see the i18n translation-debt memory.)

- [ ] **Step 3: Regenerate the i18n headers**

Run: `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`
Expected: regenerates `I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` (all gitignored); no error; new `StrId::STR_POMODORO*` enumerators exist.

- [ ] **Step 4: Verify the keys generated**

Run: `grep -c "STR_POMODORO" lib/I18n/I18nKeys.h`
Expected: `13` (one enumerator per new key).

- [ ] **Step 5: Commit (source YAML only)**

```bash
git add lib/I18n/translations/english.yaml lib/I18n/translations/portuguese.yaml
git commit -m "i18n: add Pomodoro strings (en, pt)"
```

Do NOT `git add` the generated `I18nKeys.h` / `I18nStrings.h` / `I18nStrings.cpp` — they are gitignored.

---

## Task 4: PomodoroActivity header

**Files:**
- Create: `src/activities/tools/PomodoroActivity.h`

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <cstdint>

#include "I18nKeys.h"
#include "activities/Activity.h"

class PomodoroActivity final : public Activity {
 public:
  PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // Keep the device awake only while a phase is actively counting down.
  bool preventAutoSleep() override {
    return state == State::Focus || state == State::Break || state == State::LongBreak;
  }

 private:
  enum class State : uint8_t { Idle, Focus, Break, LongBreak, Paused };

  // Fixed classic Pomodoro cadence (milliseconds), flash-resident.
  static constexpr uint32_t FOCUS_MS = 25u * 60u * 1000u;
  static constexpr uint32_t BREAK_MS = 5u * 60u * 1000u;
  static constexpr uint32_t LONG_BREAK_MS = 15u * 60u * 1000u;
  static constexpr uint8_t CYCLES_BEFORE_LONG = 4;

  static uint32_t durationFor(State phase);
  void startPhase(State phase);  // sets phaseEndMs, requests render
  void advancePhase();           // phase complete -> next phase (+flash)
  void togglePause();            // Confirm
  void resetPhase();             // Left: restart current phase
  void skipPhase();              // Right: jump to next phase
  void flashTransition(StrId msg);
  uint32_t remainingMs() const;  // 0 when not running
  uint16_t remainingMin() const;

  StrId phaseLabel() const;

  State state = State::Idle;
  State pausedFrom = State::Idle;     // phase to resume into
  uint32_t phaseEndMs = 0;            // millis() target for current phase end
  uint32_t pausedRemainingMs = 0;     // captured at pause
  uint8_t focusCount = 0;             // completed Focus phases in current set (0..4)
  uint16_t lastRenderedMin = 0xFFFF;  // dirty-check to gate per-minute refresh

  bool lockNextConfirmRelease = false;  // swallow Confirm carried from Home
};
```

- [ ] **Step 2: Commit**

```bash
git add src/activities/tools/PomodoroActivity.h
git commit -m "feat(pomodoro): add PomodoroActivity header"
```

---

## Task 5: PomodoroActivity timer logic + lifecycle

**Files:**
- Create: `src/activities/tools/PomodoroActivity.cpp`

- [ ] **Step 1: Write the lifecycle + timing implementation (render added in next task)**

Create `src/activities/tools/PomodoroActivity.cpp` with everything EXCEPT `render()` (added Task 6). Use `millis()` from Arduino and `vTaskDelay` from FreeRTOS:

```cpp
#include "PomodoroActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

PomodoroActivity::PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Pomodoro", renderer, mappedInput) {}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  state = State::Idle;
  pausedFrom = State::Idle;
  focusCount = 0;
  phaseEndMs = 0;
  pausedRemainingMs = 0;
  lastRenderedMin = 0xFFFF;
  // Swallow the Confirm release carried over from the Home menu so it does not
  // immediately start a session. Mirrors VirtualPetActivity.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate();
}

void PomodoroActivity::onExit() {
  // RAM-only state; nothing to persist or free.
  Activity::onExit();
}

uint32_t PomodoroActivity::durationFor(State phase) {
  switch (phase) {
    case State::Focus:
      return FOCUS_MS;
    case State::Break:
      return BREAK_MS;
    case State::LongBreak:
      return LONG_BREAK_MS;
    default:
      return 0;
  }
}

uint32_t PomodoroActivity::remainingMs() const {
  if (state == State::Paused) return pausedRemainingMs;
  if (state == State::Idle) return 0;
  const uint32_t now = millis();
  // millis() is monotonic unsigned; this subtraction is rollover-safe.
  return (now >= phaseEndMs) ? 0u : (phaseEndMs - now);
}

uint16_t PomodoroActivity::remainingMin() const {
  const uint32_t ms = remainingMs();
  return static_cast<uint16_t>((ms + 59999u) / 60000u);  // ceil to whole minutes
}

StrId PomodoroActivity::phaseLabel() const {
  switch (state) {
    case State::Focus:
      return STR_POMODORO_FOCUS;
    case State::Break:
      return STR_POMODORO_BREAK;
    case State::LongBreak:
      return STR_POMODORO_LONG_BREAK;
    case State::Paused:
      return STR_POMODORO_PAUSED;
    default:
      return STR_POMODORO_READY;
  }
}

void PomodoroActivity::startPhase(State phase) {
  state = phase;
  phaseEndMs = millis() + durationFor(phase);
  lastRenderedMin = 0xFFFF;  // force redraw
  requestUpdate();
}

void PomodoroActivity::advancePhase() {
  // Called when the current running phase hits zero.
  State next;
  StrId msg;
  if (state == State::Focus) {
    ++focusCount;
    if (focusCount >= CYCLES_BEFORE_LONG) {
      focusCount = 0;
      next = State::LongBreak;
    } else {
      next = State::Break;
    }
    msg = STR_POMODORO_BREAK_NOW;
  } else {
    // Break or LongBreak finished -> back to Focus.
    next = State::Focus;
    msg = STR_POMODORO_FOCUS_NOW;
  }
  flashTransition(msg);
  startPhase(next);
}

void PomodoroActivity::togglePause() {
  if (state == State::Idle) {
    startPhase(State::Focus);
    return;
  }
  if (state == State::Paused) {
    state = pausedFrom;
    phaseEndMs = millis() + pausedRemainingMs;
    lastRenderedMin = 0xFFFF;
    requestUpdate();
    return;
  }
  // Running -> pause.
  pausedRemainingMs = remainingMs();
  pausedFrom = state;
  state = State::Paused;
  requestUpdate();
}

void PomodoroActivity::resetPhase() {
  // Restart the current (or paused) phase from full duration.
  State target = (state == State::Paused) ? pausedFrom : state;
  if (target == State::Idle) return;
  startPhase(target);
}

void PomodoroActivity::skipPhase() {
  // Jump immediately to the next phase, as if the current one completed.
  if (state == State::Idle) {
    startPhase(State::Focus);
    return;
  }
  if (state == State::Paused) state = pausedFrom;  // resume context, then advance
  advancePhase();
}

void PomodoroActivity::loop() {
  // Exit.
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Confirm: start / pause / resume (swallow the carried-over release once).
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
    } else {
      togglePause();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    resetPhase();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    skipPhase();
    return;
  }

  // Timer tick: only meaningful while a phase is running.
  if (state == State::Focus || state == State::Break || state == State::LongBreak) {
    if (remainingMs() == 0) {
      advancePhase();
      return;
    }
    const uint16_t mins = remainingMin();
    if (mins != lastRenderedMin) {
      requestUpdate();  // render() refreshes lastRenderedMin
    }
  }
}
```

- [ ] **Step 2: Build (expect a link/compile error for missing `render` — that is fine, it is declared and added next; if the compiler errors on the missing definition, proceed to Task 6 before building)**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -20`
Expected: may fail only due to undefined `PomodoroActivity::render` / `flashTransition`. That is expected; Task 6 adds both. Do not commit yet.

---

## Task 6: PomodoroActivity render + transition flash

**Files:**
- Modify: `src/activities/tools/PomodoroActivity.cpp`

- [ ] **Step 1: Append `flashTransition` and `render` to the .cpp**

Add at the end of `src/activities/tools/PomodoroActivity.cpp`:

```cpp
void PomodoroActivity::flashTransition(StrId msg) {
  // E-Ink has no buzzer/vibration. Draw a bold full-screen inverted frame with
  // the message centered, hold briefly, then return so render() draws the new
  // phase. One inverted full refresh IS the "flash" on slow E-Ink.
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.fillRect(0, 0, pageWidth, pageHeight, true);  // solid black background
  // tr() only accepts a literal enumerator name; for a StrId variable use I18N.get().
  const char* text = I18N.get(msg);
  const int tw = renderer.getTextWidth(FONT_UI_LARGE, text);
  const int tx = (pageWidth - tw) / 2;
  const int ty = (pageHeight - renderer.getLineHeight(FONT_UI_LARGE)) / 2;
  renderer.drawText(FONT_UI_LARGE, tx, ty, text, false);  // white text on black
  renderer.displayBuffer();

  vTaskDelay(pdMS_TO_TICKS(1500));
}

void PomodoroActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_POMODORO));

  // Phase label.
  const int labelY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  renderer.drawCenteredText(UI_12_FONT_ID, labelY, I18N.get(phaseLabel()));  // StrId var -> I18N.get, not tr()

  // Big remaining minutes (or a prompt when idle).
  char timeBuf[32];
  if (state == State::Idle) {
    std::snprintf(timeBuf, sizeof(timeBuf), "%s", tr(STR_POMODORO_FOCUS_NOW));
  } else {
    std::snprintf(timeBuf, sizeof(timeBuf), tr(STR_POMODORO_MIN_FMT), (unsigned)remainingMin());
  }
  const int timeY = labelY + renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing * 2;
  renderer.drawCenteredText(FONT_UI_LARGE, timeY, timeBuf);

  // Progress bar (stepped, once per minute). Fraction of the current phase done.
  const int barY = timeY + renderer.getLineHeight(FONT_UI_LARGE) + metrics.verticalSpacing * 2;
  const int barH = 14;
  const int barX = metrics.contentSidePadding;
  const int barW = pageWidth - metrics.contentSidePadding * 2;
  renderer.drawRect(barX, barY, barW, barH);
  if (state == State::Focus || state == State::Break || state == State::LongBreak || state == State::Paused) {
    const State activePhase = (state == State::Paused) ? pausedFrom : state;
    const uint32_t total = durationFor(activePhase);
    if (total > 0) {
      const uint32_t done = (total > remainingMs()) ? (total - remainingMs()) : 0u;
      const int fillW = static_cast<int>((static_cast<uint64_t>(barW - 4) * done) / total);
      if (fillW > 0) renderer.fillRect(barX + 2, barY + 2, fillW, barH - 4, true);
    }
  }

  // Cycle dots: filled for completed Focus phases in the current set of 4.
  char dots[CYCLES_BEFORE_LONG * 4 + 1];
  int p = 0;
  for (int i = 0; i < CYCLES_BEFORE_LONG; ++i) {
    const char* glyph = (i < focusCount) ? "#" : ".";
    p += std::snprintf(dots + p, sizeof(dots) - p, "%s ", glyph);
  }
  const int dotsY = barY + barH + metrics.verticalSpacing * 2;
  renderer.drawCenteredText(UI_12_FONT_ID, dotsY, dots);

  // Button hints.
  const bool startLabel = (state == State::Paused || state == State::Idle);
  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), I18N.get(startLabel ? StrId::STR_POMODORO_START : StrId::STR_POMODORO_PAUSE),
      tr(STR_POMODORO_RESET), tr(STR_POMODORO_SKIP));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  // Mark the rendered minute so loop() does not request another refresh until it changes.
  lastRenderedMin = remainingMin();

  (void)pageHeight;
}
```

- [ ] **Step 2: Verify font IDs exist**

Run: `grep -nE "UI_12_FONT_ID|FONT_UI_LARGE" src/fontIds.h`
Expected: both defined. If `FONT_UI_LARGE` is absent, substitute the largest available UI font id from `src/fontIds.h` (e.g. an 18pt UI/serif id) consistently in both `render` and `flashTransition`. If `UI_12_FONT_ID` is absent, use `UI_10_FONT_ID` (used by VirtualPet).

- [ ] **Step 3: Verify `mapLabels` arity + `drawCenteredText`/`fillRect` signatures**

Run: `grep -nE "mapLabels|drawCenteredText|fillRect|getTextWidth|getLineHeight" lib/GfxRenderer/GfxRenderer.h src/MappedInputManager.h`
Expected: `mapLabels(btn1,btn2,btn3,btn4)`, `drawCenteredText(fontId, y, text)`, `fillRect(x,y,w,h,bool)` confirmed (these match VirtualPetActivity usage). Adjust call sites if signatures differ.

- [ ] **Step 4: Build**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -25`
Expected: SUCCESS, 0 errors/warnings. (Activity compiles; not yet reachable from Home.)

- [ ] **Step 5: Commit**

```bash
git add src/activities/tools/PomodoroActivity.cpp
git commit -m "feat(pomodoro): implement timer logic, render, and transition flash"
```

---

## Task 7: ActivityManager wiring

**Files:**
- Modify: `src/activities/ActivityManager.h` (HomeMenuItem enum ~line 19-30; method decls ~line 100-105)
- Modify: `src/activities/ActivityManager.cpp` (~line 199-207)

- [ ] **Step 1: Add the menu item enumerator**

In `src/activities/ActivityManager.h`, extend `HomeMenuItem`:

```cpp
enum class HomeMenuItem {
  NONE,
  FILE_BROWSER,
  RECENTS,
  OPDS_BROWSER,
  RSS_BROWSER,
  FILE_TRANSFER,
  SETTINGS_MENU,
  READING_STATS_MENU,
  FLASHCARD_MENU,
  VIRTUAL_PET,
  POMODORO_MENU
};
```

- [ ] **Step 2: Declare `goToPomodoro()`**

Near `goToVirtualPet();` / `goToFlashcards();` declarations:

```cpp
void goToVirtualPet();
void goToPomodoro();
```

- [ ] **Step 3: Define `goToPomodoro()` + include**

In `src/activities/ActivityManager.cpp`, add the include near the other tool-activity includes (find where `VirtualPetActivity.h` is included and add beside it):

```cpp
#include "activities/tools/PomodoroActivity.h"
```

Then add the method next to `goToVirtualPet`:

```cpp
void ActivityManager::goToPomodoro() {
  replaceActivity(std::make_unique<PomodoroActivity>(renderer, mappedInput));
}
```

- [ ] **Step 4: Build**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -20`
Expected: SUCCESS. (Reachable via manager; Home tile next.)

- [ ] **Step 5: Commit**

```bash
git add src/activities/ActivityManager.h src/activities/ActivityManager.cpp
git commit -m "feat(pomodoro): add goToPomodoro to ActivityManager"
```

---

## Task 8: Home menu tile

**Files:**
- Modify: `src/activities/home/HomeActivity.h` (`menuItemToIndex` ~line 36-54, `indexToMenuItem` ~line 61-72, method decls ~line 80)
- Modify: `src/activities/home/HomeActivity.cpp` (count ~line 22-24, menu build ~line 250-256, Confirm dispatch ~line 187-218, handler ~line 303)

- [ ] **Step 1: Bump the menu item count**

In `HomeActivity.cpp` `getMenuItemCount()`:

```cpp
  int count = 9;  // File Browser, Recents, File transfer, RSS, Virtual Pet, Reading Stats, Flashcards, Pomodoro, Settings
```

- [ ] **Step 2: Add Pomodoro to the menu item + icon vectors**

In `HomeActivity::render`, extend the two vectors (insert Pomodoro right after Flashcards, before Settings — this order MUST match the index helpers in Steps 4-5):

```cpp
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),
                                        tr(STR_RSS_TITLE),    tr(STR_VIRTUAL_PET),       tr(STR_READING_STATS_TITLE),
                                        tr(STR_FLASHCARD),    tr(STR_POMODORO),          tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Library, Paw, Stats, Cards, Clock, Settings};
```

- [ ] **Step 3: Add the Confirm dispatch case**

In `HomeActivity::loop`, in the `switch (indexToMenuItem(...))`, add before `default:`:

```cpp
        case HomeMenuItem::FLASHCARD_MENU:
          onFlashcardOpen();
          break;
        case HomeMenuItem::POMODORO_MENU:
          onPomodoroOpen();
          break;
        default:
          break;
```

- [ ] **Step 4: Update `menuItemToIndex`**

In `HomeActivity.h`, insert the Pomodoro step between `FLASHCARD_MENU` and `SETTINGS_MENU`:

```cpp
    if (item == HomeMenuItem::FLASHCARD_MENU) return i;
    ++i;
    if (item == HomeMenuItem::POMODORO_MENU) return i;
    ++i;
    if (item == HomeMenuItem::SETTINGS_MENU) return i;
    return 0;
```

- [ ] **Step 5: Update `indexToMenuItem`**

In `HomeActivity.h`, mirror the order:

```cpp
    if (idx == i++) return HomeMenuItem::FLASHCARD_MENU;
    if (idx == i++) return HomeMenuItem::POMODORO_MENU;
    if (idx == i) return HomeMenuItem::SETTINGS_MENU;
    return HomeMenuItem::NONE;
```

- [ ] **Step 6: Declare + define the handler**

In `HomeActivity.h`, beside `onVirtualPetOpen();`:

```cpp
  void onVirtualPetOpen();
  void onPomodoroOpen();
```

In `HomeActivity.cpp`, beside `onVirtualPetOpen`:

```cpp
void HomeActivity::onVirtualPetOpen() { activityManager.goToVirtualPet(); }

void HomeActivity::onPomodoroOpen() { activityManager.goToPomodoro(); }
```

- [ ] **Step 7: Build**

Run: `~/.platformio/penv/bin/pio run -e default 2>&1 | tail -25`
Expected: SUCCESS, 0 errors/warnings. Pomodoro tile now reachable from Home.

- [ ] **Step 8: Commit**

```bash
git add src/activities/home/HomeActivity.h src/activities/home/HomeActivity.cpp
git commit -m "feat(home): add Pomodoro tile with clock icon"
```

---

## Task 9: Quality pass

**Files:** all touched.

- [ ] **Step 1: Format**

Run: `find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i`
Then: `git diff --stat`
Expected: only whitespace adjustments, if any.

- [ ] **Step 2: Static analysis**

Run: `~/.platformio/penv/bin/pio check 2>&1 | tail -30`
Expected: no new high-severity findings in `PomodoroActivity` / touched files.

- [ ] **Step 3: Clean build**

Run: `~/.platformio/penv/bin/pio run -e default -t clean && ~/.platformio/penv/bin/pio run -e default 2>&1 | tail -15`
Expected: SUCCESS, 0 errors/warnings.

- [ ] **Step 4: Verify no generated/ignored files staged**

Run: `git status --short`
Expected: clean (or only intended source files). Confirm `lib/I18n/I18nKeys.h`, `I18nStrings.*`, `.pio/` are NOT staged.

- [ ] **Step 5: Commit any formatting**

```bash
git add -u
git commit -m "chore(pomodoro): clang-format + static-analysis sweep" || echo "nothing to commit"
```

---

## Self-Review Notes (already reconciled)

- **Spec coverage:** standalone activity (Task 4-6) ✓; fixed 25/5/15 + long break every 4 (`advancePhase`, Task 5) ✓; minutes + progress bar 1×/min (`render`, gated by `lastRenderedMin`, Task 6) ✓; full-screen flash alert (`flashTransition`, Task 6) ✓; `preventAutoSleep` while running (header, Task 4) ✓; buttons Confirm/Left/Right/Back (`loop`, Task 5) ✓; Home + manager wiring (Tasks 7-8) ✓; i18n (Task 3) ✓; no cache/build-flag/heap impact ✓.
- **Type consistency:** `State` enum, `durationFor`, `remainingMs`/`remainingMin`, `phaseLabel`, `flashTransition(StrId)` consistent header↔cpp. `StrId` is the `tr()` enum type from `I18nKeys.h` (matches `tr(STR_*)` usage); referenced in the header as `StrId` — included via `I18nKeys.h`.
- **Order invariant:** menu vector order (Task 8 Step 2) == `menuItemToIndex`/`indexToMenuItem` order (Steps 4-5). Pomodoro sits between Flashcards and Settings in all three.
- **Known fallbacks documented:** font ids (Task 6 Step 2), renderer signatures (Task 6 Step 3) — verify-and-adapt steps included rather than assumed.
