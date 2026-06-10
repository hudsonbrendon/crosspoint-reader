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
