#pragma once
#include <string>
#include <vector>

#include "FlashcardDeck.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class FlashcardDeckListActivity final : public Activity {
 public:
  FlashcardDeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void reloadDecks();
  void onImportCsv();
  void onOpenSettings();
  void onOpenDeck(size_t index);

  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;

  std::vector<DeckListEntry> decks;

  // Swallow the Confirm release carried over from selecting this screen in the
  // parent menu, so it doesn't immediately open/delete the first deck.
  bool lockNextConfirmRelease = false;

  int getItemCount() const;

  // Row layout: 0..deckCount-1 = decks; deckCount = "Import CSV"; deckCount+1 = "Flashcard Settings"
  static constexpr int EXTRA_ROWS = 2;
};
