#include "FlashcardDoneActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

FlashcardDoneActivity::FlashcardDoneActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("FlashcardDone", renderer, mappedInput) {}

void FlashcardDoneActivity::onEnter() {
  Activity::onEnter();
  requestUpdate(true);
}

void FlashcardDoneActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void FlashcardDoneActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageHeight = renderer.getScreenHeight();
  const int y = pageHeight / 2 - renderer.getLineHeight(NOTOSERIF_16_FONT_ID) / 2;
  renderer.drawCenteredText(NOTOSERIF_16_FONT_ID, y, tr(STR_FLASHCARD_DONE_MSG));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
