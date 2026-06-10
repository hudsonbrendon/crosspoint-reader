#include "FlashcardSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "InkPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Row 0 = New cards per session, Row 1 = Max reviews per session
const StrId rowNames[FlashcardSettingsActivity::ITEM_COUNT] = {
    StrId::STR_FLASHCARD_NEW_PER_DAY,
    StrId::STR_FLASHCARD_MAX_REVIEW,
};
}  // namespace

void FlashcardSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void FlashcardSettingsActivity::onExit() { Activity::onExit(); }

void FlashcardSettingsActivity::adjustValue(int delta) {
  // Steps match SettingsList.h: flashcardNewPerDay step=5, flashcardMaxReviewPerDay step=25
  if (selectedIndex == 0) {
    const int cur = static_cast<int>(SETTINGS.flashcardNewPerDay);
    const int next = cur + delta * 5;
    SETTINGS.flashcardNewPerDay =
        static_cast<uint8_t>(std::max(static_cast<int>(InkPointSettings::FLASHCARD_NEW_PER_DAY_MIN),
                                      std::min(static_cast<int>(InkPointSettings::FLASHCARD_NEW_PER_DAY_MAX), next)));
  } else {
    const int cur = static_cast<int>(SETTINGS.flashcardMaxReviewPerDay);
    const int next = cur + delta * 25;
    SETTINGS.flashcardMaxReviewPerDay =
        static_cast<uint8_t>(std::max(static_cast<int>(InkPointSettings::FLASHCARD_MAX_REVIEW_MIN),
                                      std::min(static_cast<int>(InkPointSettings::FLASHCARD_MAX_REVIEW_MAX), next)));
  }
  SETTINGS.saveToFile();
}

void FlashcardSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Left / Confirm decrements; Right increments — mirrors SettingsActivity VALUE handling
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    adjustValue(-1);
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    adjustValue(+1);
    requestUpdate();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, ITEM_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, ITEM_COUNT);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, ITEM_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, ITEM_COUNT);
    requestUpdate();
  });
}

void FlashcardSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FLASHCARD));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, ITEM_COUNT, selectedIndex,
      [](int index) { return std::string(I18N.get(rowNames[index])); }, nullptr, nullptr,
      [](int index) -> std::string {
        char buf[16];
        if (index == 0) {
          snprintf(buf, sizeof(buf), "%d", static_cast<int>(SETTINGS.flashcardNewPerDay));
        } else {
          snprintf(buf, sizeof(buf), "%d", static_cast<int>(SETTINGS.flashcardMaxReviewPerDay));
        }
        return buf;
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
