#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Focused sub-screen for the two flashcard scheduling settings:
//   - New cards per session  (flashcardNewPerDay)
//   - Max reviews per session (flashcardMaxReviewPerDay)
class FlashcardSettingsActivity final : public Activity {
 public:
  static constexpr int ITEM_COUNT = 2;
  explicit FlashcardSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FlashcardSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;

  int selectedIndex = 0;

  void cycleValue();
};
