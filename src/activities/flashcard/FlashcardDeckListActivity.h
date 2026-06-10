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

  // Row layout: 0 = "Import CSV"; 1 = "Flashcard Settings"; 2..(2+deckCount-1) = decks
  static constexpr int EXTRA_ROWS = 2;
  static constexpr int ROW_IMPORT = 0;
  static constexpr int ROW_SETTINGS = 1;
  static constexpr int FIRST_DECK_ROW = 2;
};
