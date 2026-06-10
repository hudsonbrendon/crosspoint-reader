#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "FlashcardDeck.h"
#include "activities/Activity.h"

class FlashcardReviewActivity final : public Activity {
 public:
  FlashcardReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string deckPath);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { Front, Back };

  void renderFront();
  void renderBack();
  void applyRating(SrsRating rating);
  void flushSaveIfDeferred();
  void checkExhausted();

  std::string deckPath;
  FlashcardDeck deck;
  std::vector<size_t> queue;  // indices into deck.getCards()
  size_t queuePos = 0;
  State state = State::Front;

  // Batched save: flush every 5 ratings and on exit.
  int reviewedSinceSave = 0;
  bool deferredSave = false;

  // Swallow Confirm held from entering this screen via the deck list.
  bool lockNextConfirmRelease = false;
};
