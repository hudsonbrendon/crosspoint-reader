#include "PomodoroActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Largest UI-inserted font for the big remaining-minutes readout and the flash.
constexpr int BIG_FONT_ID = NOTOSANS_18_FONT_ID;
}  // namespace

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
      return StrId::STR_POMODORO_FOCUS;
    case State::Break:
      return StrId::STR_POMODORO_BREAK;
    case State::LongBreak:
      return StrId::STR_POMODORO_LONG_BREAK;
    case State::Paused:
      return StrId::STR_POMODORO_PAUSED;
    default:
      return StrId::STR_POMODORO_READY;
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
    msg = StrId::STR_POMODORO_BREAK_NOW;
  } else {
    // Break or LongBreak finished -> back to Focus.
    next = State::Focus;
    msg = StrId::STR_POMODORO_FOCUS_NOW;
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

void PomodoroActivity::flashTransition(StrId msg) {
  // E-Ink has no buzzer/vibration. Draw a bold full-screen inverted frame with
  // the message centered, hold briefly, then return so render() draws the new
  // phase. One inverted full refresh IS the "flash" on slow E-Ink.
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.fillRect(0, 0, pageWidth, pageHeight, true);  // solid black background
  const char* text = I18N.get(msg);                      // StrId var -> I18N.get, not tr()
  const int tw = renderer.getTextWidth(BIG_FONT_ID, text);
  const int tx = (pageWidth - tw) / 2;
  const int ty = (pageHeight - renderer.getLineHeight(BIG_FONT_ID)) / 2;
  renderer.drawText(BIG_FONT_ID, tx, ty, text, false);  // white text on black
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
  renderer.drawCenteredText(BIG_FONT_ID, timeY, timeBuf);

  // Progress bar (stepped, once per minute). Fraction of the current phase done.
  const int barY = timeY + renderer.getLineHeight(BIG_FONT_ID) + metrics.verticalSpacing * 2;
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
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), I18N.get(startLabel ? StrId::STR_POMODORO_START : StrId::STR_POMODORO_PAUSE),
                            tr(STR_POMODORO_RESET), tr(STR_POMODORO_SKIP));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  // Mark the rendered minute so loop() does not request another refresh until it changes.
  lastRenderedMin = remainingMin();

  (void)pageHeight;
}
