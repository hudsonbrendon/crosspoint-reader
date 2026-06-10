# Pomodoro Activity — Design Spec

**Date:** 2026-06-10
**Status:** Approved (design), pending implementation plan
**Target:** Xteink X4 (ESP32-C3), InkPoint Reader

## Summary

Add a standalone Pomodoro timer as an isolated Activity, accessible from the
Home menu — same lifecycle and wiring pattern as `VirtualPetActivity`. Fixed
classic Pomodoro cadence (25/5/15), no user-configurable settings, no SPIFFS
persistence, no FreeRTOS task, no large heap allocation, no cache impact.

## Goals

- One-tap Pomodoro from Home.
- E-Ink-safe rendering (no per-second refresh).
- Survive the global auto-sleep timeout while a session is running.
- Stay within project memory discipline: zero dynamic buffers, RAM state only.

## Non-Goals (YAGNI)

- No configurable durations / settings entry.
- No persistence of session across reboot or activity exit.
- No reader integration (timer does not run during EPUB reading).
- No sound/vibration (X4 has no buzzer or vibration motor).
- No statistics / history logging.

## Architecture

New `PomodoroActivity` (`final : public Activity`) in
`src/activities/tools/`, mirroring `VirtualPetActivity`:

- Files: `src/activities/tools/PomodoroActivity.{h,cpp}`.
- Heap-allocated by `ActivityManager`, deleted on exit (standard lifecycle).
- Lifecycle methods: `onEnter()`, `onExit()`, `loop()`, `render(RenderLock&&)`.
- Uses `ButtonNavigator` / `MappedInputManager` for logical button input.
- **No FreeRTOS task** — the timer advances inside `loop()` using `millis()`.
- **No SPIFFS write** — all state lives in RAM and is discarded on `onExit()`.

### Header sketch

```cpp
#pragma once

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

  void startPhase(State phase);   // sets phaseEndMs, requests render
  void advancePhase();            // FOCUS -> BREAK/LONG_BREAK -> FOCUS
  void togglePause();
  void resetPhase();              // Left: restart current phase
  void skipPhase();               // Right: jump to next phase
  void flashTransition(StrId msg);
  uint32_t remainingMs() const;

  State state = State::Idle;
  State pausedFrom = State::Idle;     // phase to resume into
  uint32_t phaseEndMs = 0;           // millis() target for current phase end
  uint32_t pausedRemainingMs = 0;    // captured at pause
  uint8_t focusCount = 0;            // completed FOCUS phases this set (0..4)
  uint16_t lastRenderedMin = 0xFFFF; // dirty-check to gate refresh

  bool lockNextConfirmRelease = false;  // swallow Confirm carried from Home
};
```

## Timing model

- Compile-time constants (`static constexpr`, flash-resident):
  - `FOCUS_MS = 25u * 60u * 1000u`
  - `BREAK_MS = 5u * 60u * 1000u`
  - `LONG_BREAK_MS = 15u * 60u * 1000u`
  - `CYCLES_BEFORE_LONG = 4`
- `startPhase(p)`: `phaseEndMs = millis() + durationFor(p)`.
- `remainingMs()`: `phaseEndMs - millis()` (running) or `pausedRemainingMs` (paused).
  Guard against wrap: if `millis() >= phaseEndMs`, remaining is 0.
- Pause: `pausedRemainingMs = remainingMs(); pausedFrom = state; state = Paused;`
- Resume: `state = pausedFrom; phaseEndMs = millis() + pausedRemainingMs;`
- `millis()` is monotonic and unsigned — subtraction handles the ~49.7-day
  rollover correctly for our minute-scale intervals. No RTC (X4 has none).

### Phase progression

```
Idle --Confirm--> Focus
Focus  complete --> (focusCount++; if focusCount % 4 == 0) LongBreak else Break
Break / LongBreak complete --> Focus
LongBreak complete --> reset focusCount to 0, back to Focus
```

`focusCount` wraps the ● indicator: completed focus phases in the current set
of 4 shown as filled dots (e.g. `●●○○`).

## Rendering (E-Ink safe)

E-Ink full refresh is 1–2s; per-second updates are forbidden. Render only when
something visible changes:

- Set `lastRenderedMin` dirty-check. In `loop()`, compute current
  `remainingMin = ceil(remainingMs / 60000)`. Re-render only when
  `remainingMin != lastRenderedMin`, on phase change, or on button input.
- Screen contents (all via `GUI` / `UITheme`, orientation-aware, `tr()` strings):
  - Phase label — `tr(STR_POMODORO_FOCUS)` / `STR_POMODORO_BREAK` /
    `STR_POMODORO_LONG_BREAK` / `STR_POMODORO_PAUSED` / `STR_POMODORO_READY`.
  - Big remaining minutes: `"NN min"` (or `tr(STR_POMODORO_MIN_FMT)` with `%u`).
  - Progress bar — fraction `1 - remainingMs/phaseDuration`, updated once per
    minute (stepped, not smooth).
  - Cycle dots — filled/empty for `focusCount` within the current set of 4.
  - Button hints row.
- Use `renderer.getScreenWidth()/getScreenHeight()` and
  `getOrientedViewableTRBL()` — no hardcoded 800/480.

## Phase-transition alert

On any phase boundary (focus↔break), before drawing the new phase:

- `flashTransition(msg)`: full-screen invert 2–3× to draw the eye (the only
  attention signal available on E-Ink), then a large centered message:
  `tr(STR_POMODORO_BREAK_NOW)` ("Break! 5 min") or
  `tr(STR_POMODORO_FOCUS_NOW)` ("Focus!").
- No buzzer / LED dependency (not assuming X4 has a controllable status LED).

## Sleep handling

The global auto-sleep timeout (`SETTINGS.sleepTimeoutMinutes`, default 10) would
trigger mid-focus (25 min). `PomodoroActivity` overrides
`preventAutoSleep()` to return `true` while `state` is Focus/Break/LongBreak,
and `false` when Idle or Paused (allow normal sleep). Pattern matches
`KOReaderSyncActivity` and `OtaUpdateActivity`.

## Button mapping (logical, via MappedInputManager)

| Button  | Action                                                            |
|---------|-------------------------------------------------------------------|
| Confirm | Idle → start Focus; Running → pause; Paused → resume               |
| Left    | Reset current phase (restart its countdown from full duration)    |
| Right   | Skip to next phase (advance immediately)                          |
| Back    | Exit activity; if a phase is running, require a confirm/back-press |

`lockNextConfirmRelease` swallows the Confirm release carried over from the Home
menu, mirroring `VirtualPetActivity` / `RssFeedListActivity`.

## Home + ActivityManager wiring

- `HomeActivity.cpp`: bump `int count = 8;` → `9`; update the inline comment list;
  add the Pomodoro tile with a distinct icon (clock / tomato), and
  `onPomodoroOpen()` → `activityManager.goToPomodoro()`.
- `ActivityManager.{h,cpp}`: add
  ```cpp
  void goToPomodoro();
  // ...
  void ActivityManager::goToPomodoro() {
    replaceActivity(std::make_unique<PomodoroActivity>(renderer, mappedInput));
  }
  ```
- Re-highlight mapping on `goHome` if the home grid tracks last-opened tile
  (mirror the recent `goToVirtualPet` re-highlight change).

## i18n

Add new `STR_POMODORO_*` keys to `lib/I18n/translations/english.yaml`
(reference language), then translate across the other language YAMLs (English
fallback covers any gaps). Run
`python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`.
Commit **source YAML only** — the three generated files are gitignored.

Proposed keys: `STR_POMODORO` (menu label), `STR_POMODORO_FOCUS`,
`STR_POMODORO_BREAK`, `STR_POMODORO_LONG_BREAK`, `STR_POMODORO_PAUSED`,
`STR_POMODORO_READY`, `STR_POMODORO_FOCUS_NOW`, `STR_POMODORO_BREAK_NOW`,
`STR_POMODORO_MIN_FMT`, plus button-hint strings as needed.

## Memory & resource justification

- **No heap allocation** beyond the Activity object itself (one small `new` by
  `ActivityManager` via `make_unique`, standard for all activities).
- **No buffers**: state is a handful of scalars (≈20 bytes of members). Well
  under the 256-byte stack guidance; nothing on the heap.
- **No SPIFFS writes**: avoids flash-wear; no settings persistence needed.
- **No cache touch**: `.inkpoint/` untouched; no `book.bin`/`section.bin`
  version bump.
- **No new build flag**.

## Out of scope / explicitly rejected

- Configurable durations (rejected: adds settings UI + SPIFFS persistence for
  marginal value on a fixed-cadence technique).
- MM:SS live seconds via partial refresh (rejected: E-Ink ghosting risk,
  uncertain clean partial-refresh support on X4 SDK).
- Session persistence across exit (rejected: YAGNI; a Pomodoro is a focused
  in-the-moment session).

## Verification (post-implementation)

AI-verifiable:
1. `pio run -t clean && pio run` — 0 errors/warnings.
2. `pio check` clean; `clang-format` applied.
3. i18n generator runs; no missing English keys.
4. Switch/case coverage for all `State` values; no hardcoded screen dims.

Human-tester scope:
5. On device: start focus, confirm timer counts down at 1×/min refresh.
6. Leave idle through the 10-min sleep window during focus — device must NOT
   sleep; pause it and confirm it sleeps normally.
7. Phase transition flash fires and message is legible.
8. All 4 orientations render within viewable area.
9. `ESP.getFreeHeap()` stable across enter/exit (no leak).
