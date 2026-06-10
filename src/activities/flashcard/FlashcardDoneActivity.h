#pragma once
#include "activities/Activity.h"

// Shown when the review queue is exhausted for this session.
// Back or Confirm returns to the deck list.
class FlashcardDoneActivity final : public Activity {
 public:
  FlashcardDoneActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
