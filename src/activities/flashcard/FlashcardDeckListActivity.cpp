#include "FlashcardDeckListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "FlashcardDeck.h"
#include "FlashcardReviewActivity.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/home/FileBrowserActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

FlashcardDeckListActivity::FlashcardDeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("FlashcardDeckList", renderer, mappedInput) {}

int FlashcardDeckListActivity::getItemCount() const {
  // deck rows + "Import CSV" + "Flashcard Settings"
  return static_cast<int>(decks.size()) + EXTRA_ROWS;
}

void FlashcardDeckListActivity::reloadDecks() {
  decks = FlashcardDeck::listDecks();
  selectedIndex = 0;
  requestUpdate();
}

void FlashcardDeckListActivity::onEnter() {
  Activity::onEnter();
  reloadDecks();
  // If Confirm is still held from selecting this screen in the parent menu,
  // swallow its release so it doesn't act on the first row.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
}

void FlashcardDeckListActivity::onExit() {
  Activity::onExit();
  decks.clear();
}

void FlashcardDeckListActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }

    const int deckCount = static_cast<int>(decks.size());
    const int importRow = deckCount;
    const int settingsRow = deckCount + 1;

    if (selectedIndex < deckCount) {
      onOpenDeck(static_cast<size_t>(selectedIndex));
    } else if (selectedIndex == importRow) {
      onImportCsv();
    } else if (selectedIndex == settingsRow) {
      onOpenSettings();
    }
    return;
  }

  const int itemCount = getItemCount();
  if (itemCount > 0) {
    buttonNavigator.onNext([this, itemCount] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
      requestUpdate();
    });
    buttonNavigator.onPrevious([this, itemCount] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
      requestUpdate();
    });
  }
}

void FlashcardDeckListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FLASHCARD));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int itemCount = getItemCount();
  const int deckCount = static_cast<int>(decks.size());

  if (deckCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 15, tr(STR_FLASHCARD_NO_DECKS));
  }

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount, selectedIndex,
      [this, deckCount](int index) -> std::string {
        if (index < deckCount) {
          // v1: show deck name only; New/Due/Total stats deferred until deck open to avoid
          // loading every deck on entry (potentially many decks, each a full CSV read).
          return decks[static_cast<size_t>(index)].name;
        }
        if (index == deckCount) {
          return std::string(I18n::getInstance().get(StrId::STR_FLASHCARD_IMPORT));
        }
        return std::string(I18n::getInstance().get(StrId::STR_FLASHCARD_SETTINGS));
      },
      [](int) { return std::string(""); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void FlashcardDeckListActivity::onImportCsv() {
  startActivityForResult(
      std::make_unique<FileBrowserActivity>(renderer, mappedInput, "/", FileBrowserActivity::Mode::PickCsv),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          if (const auto* fp = std::get_if<FilePathResult>(&result.data)) {
            FlashcardDeck::importCsv(fp->path);
          }
        }
        reloadDecks();
      });
}

void FlashcardDeckListActivity::onOpenSettings() { activityManager.goToSettings(); }

void FlashcardDeckListActivity::onOpenDeck(size_t index) {
  if (index >= decks.size()) return;
  activityManager.pushActivity(std::make_unique<FlashcardReviewActivity>(renderer, mappedInput, decks[index].path));
}
