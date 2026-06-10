#include "FlashcardReviewActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <string>

#include "FlashcardDoneActivity.h"
#include "FlashcardSession.h"
#include "FlashcardSrs.h"
#include "InkPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

FlashcardReviewActivity::FlashcardReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 std::string deckPath)
    : Activity("FlashcardReview", renderer, mappedInput), deckPath(std::move(deckPath)) {}

void FlashcardReviewActivity::onEnter() {
  Activity::onEnter();
  FLASHCARD_SESSION.load();

  if (!deck.loadFromCsv(deckPath)) {
    LOG_ERR("FCREV", "Failed to load deck: %s", deckPath.c_str());
    finish();
    return;
  }

  queue =
      deck.buildReviewQueue(FLASHCARD_SESSION.today(), SETTINGS.flashcardNewPerDay, SETTINGS.flashcardMaxReviewPerDay);
  queuePos = 0;
  state = State::Front;
  reviewedSinceSave = 0;
  deferredSave = false;

  // If Confirm is still held from the deck list, swallow the release.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);

  if (queue.empty()) {
    activityManager.pushActivity(std::make_unique<FlashcardDoneActivity>(renderer, mappedInput));
    finish();
    return;
  }

  requestUpdate();
}

void FlashcardReviewActivity::onExit() {
  flushSaveIfDeferred();
  Activity::onExit();
}

void FlashcardReviewActivity::flushSaveIfDeferred() {
  if (deferredSave) {
    deck.saveToCsv();
    deferredSave = false;
    reviewedSinceSave = 0;
  }
}

void FlashcardReviewActivity::checkExhausted() {
  if (queuePos >= queue.size()) {
    flushSaveIfDeferred();
    activityManager.pushActivity(std::make_unique<FlashcardDoneActivity>(renderer, mappedInput));
    finish();
  }
}

void FlashcardReviewActivity::applyRating(SrsRating rating) {
  if (queuePos >= queue.size()) return;

  const size_t cardIdx = queue[queuePos];
  const SrsState next = FlashcardSrs::review(deck.getCards()[cardIdx].srs, rating, FLASHCARD_SESSION.today());
  deck.updateCard(cardIdx, next);

  ++queuePos;
  ++reviewedSinceSave;

  if (reviewedSinceSave >= 5) {
    deferredSave = true;
    // Flush in loop() to avoid SD I/O inside the button handler
  }

  state = State::Front;
}

void FlashcardReviewActivity::loop() {
  // Flush batched saves from loop() (keeps SD I/O out of render/button callbacks)
  if (deferredSave && reviewedSinceSave >= 5) {
    flushSaveIfDeferred();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (state == State::Back) {
      // Back from answer side returns to question side
      state = State::Front;
      requestUpdate();
    } else {
      flushSaveIfDeferred();
      finish();
    }
    return;
  }

  if (state == State::Front) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (lockNextConfirmRelease) {
        lockNextConfirmRelease = false;
        return;
      }
      state = State::Back;
      requestUpdate();
    }
  } else {
    // Back state: 4 rating buttons
    // Logical button mapping: Left=Again, Down/Up navigation reused as Hard/Easy,
    // Right=Good (approachable for 4-button device), Confirm=Easy
    // Final mapping per plan: Back/Left/Right/Confirm → Again/Hard/Good/Easy
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      applyRating(SrsRating::Again);
      checkExhausted();
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      applyRating(SrsRating::Hard);
      checkExhausted();
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      applyRating(SrsRating::Good);
      checkExhausted();
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      applyRating(SrsRating::Easy);
      checkExhausted();
      requestUpdate();
      return;
    }
  }
}

// Choose the largest font that fits the text within maxWidth / maxLines.
// Tries 18→16→14pt Noto Serif in that order.
static int chooseFontId(const GfxRenderer& renderer, const char* text, int maxWidth, int maxLines) {
  static const int fontIds[] = {NOTOSERIF_18_FONT_ID, NOTOSERIF_16_FONT_ID, NOTOSERIF_14_FONT_ID};
  for (int fid : fontIds) {
    auto lines = renderer.wrappedText(fid, text, maxWidth, maxLines);
    // Fits if wrapping produces <= maxLines lines (overflow is truncated)
    if (!lines.empty()) return fid;
  }
  return NOTOSERIF_14_FONT_ID;
}

void FlashcardReviewActivity::renderFront() {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FLASHCARD));

  if (queuePos >= queue.size()) {
    renderer.displayBuffer();
    return;
  }

  const FlashcardCard& card = deck.getCards()[queue[queuePos]];

  // Progress counter e.g. "3 / 10"
  char progressBuf[32];
  snprintf(progressBuf, sizeof(progressBuf), "%zu / %zu", queuePos + 1, queue.size());
  renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding + metrics.headerHeight + 4, progressBuf);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 20;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2 - 20;
  const int margin = 16;
  const int maxWidth = pageWidth - margin * 2;
  const int maxLines = 6;

  const std::string mainText = card.frontMain();
  const std::string hintText = card.frontHint();

  const int fontId = chooseFontId(renderer, mainText.c_str(), maxWidth, maxLines);
  const int lineH = renderer.getLineHeight(fontId);

  auto lines = renderer.wrappedText(fontId, mainText.c_str(), maxWidth, maxLines);

  int totalH = static_cast<int>(lines.size()) * lineH;
  if (!hintText.empty()) {
    totalH += renderer.getLineHeight(UI_10_FONT_ID) + 8;
  }

  int y = contentTop + (contentHeight - totalH) / 2;
  if (y < contentTop) y = contentTop;

  for (const auto& line : lines) {
    renderer.drawCenteredText(fontId, y, line.c_str());
    y += lineH;
  }

  if (!hintText.empty()) {
    y += 8;
    renderer.drawCenteredText(UI_10_FONT_ID, y, hintText.c_str());
  }

  // Button hints: Confirm = Show Answer
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_FLASHCARD_SHOW), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void FlashcardReviewActivity::renderBack() {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FLASHCARD));

  if (queuePos >= queue.size()) {
    renderer.displayBuffer();
    return;
  }

  const FlashcardCard& card = deck.getCards()[queue[queuePos]];

  // Progress counter
  char progressBuf[32];
  snprintf(progressBuf, sizeof(progressBuf), "%zu / %zu", queuePos + 1, queue.size());
  renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding + metrics.headerHeight + 4, progressBuf);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 20;
  const int margin = 16;
  const int maxWidth = pageWidth - margin * 2;
  const int maxLines = 5;

  const std::string backText = card.backContent;
  const int fontId = chooseFontId(renderer, backText.c_str(), maxWidth, maxLines);
  const int lineH = renderer.getLineHeight(fontId);

  auto lines = renderer.wrappedText(fontId, backText.c_str(), maxWidth, maxLines);

  // Reserve space for 4 rating buttons at the bottom
  const int buttonAreaHeight = renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing * 3;
  const int contentHeight = pageHeight - contentTop - buttonAreaHeight;

  int totalH = static_cast<int>(lines.size()) * lineH;
  int y = contentTop + (contentHeight - totalH) / 2;
  if (y < contentTop) y = contentTop;

  for (const auto& line : lines) {
    renderer.drawCenteredText(fontId, y, line.c_str());
    y += lineH;
  }

  // 4 rating buttons with +N interval previews
  // Layout: Again | Hard | Good | Easy across the bottom button hints area
  SrsState curState = card.srs;
  char againBuf[24], hardBuf[24], goodBuf[24], easyBuf[24];
  uint16_t intAgain = FlashcardSrs::previewInterval(curState, SrsRating::Again);
  uint16_t intHard = FlashcardSrs::previewInterval(curState, SrsRating::Hard);
  uint16_t intGood = FlashcardSrs::previewInterval(curState, SrsRating::Good);
  uint16_t intEasy = FlashcardSrs::previewInterval(curState, SrsRating::Easy);

  if (intAgain == 0) {
    snprintf(againBuf, sizeof(againBuf), "%s", tr(STR_FLASHCARD_AGAIN));
  } else {
    snprintf(againBuf, sizeof(againBuf), "%s +%u", tr(STR_FLASHCARD_AGAIN), (unsigned)intAgain);
  }
  snprintf(hardBuf, sizeof(hardBuf), "%s +%u", tr(STR_FLASHCARD_HARD), (unsigned)intHard);
  snprintf(goodBuf, sizeof(goodBuf), "%s +%u", tr(STR_FLASHCARD_GOOD), (unsigned)intGood);
  snprintf(easyBuf, sizeof(easyBuf), "%s +%u", tr(STR_FLASHCARD_EASY), (unsigned)intEasy);

  // btn1=Back=Again, btn2=Down=Hard, btn3=Right=Good, btn4=Confirm=Easy
  const auto labels = mappedInput.mapLabels(againBuf, easyBuf, hardBuf, goodBuf);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void FlashcardReviewActivity::render(RenderLock&&) {
  if (state == State::Front) {
    renderFront();
  } else {
    renderBack();
  }
}
