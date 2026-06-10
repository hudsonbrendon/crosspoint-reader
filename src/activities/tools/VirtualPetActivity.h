#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class VirtualPetActivity final : public Activity {
 public:
  VirtualPetActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Action rows, in display order.
  enum class Action : int { Feed = 0, Snack = 1, Medicine = 2, Exercise = 3, Clean = 4, Pet = 5, Count = 6 };

  void runAction(Action action);
  const char* actionLabel(int index) const;

  int selectedIndex = 0;  // 0..Action::Count-1
  ButtonNavigator buttonNavigator;

  // Swallow the Confirm release carried over from the Home menu so it doesn't
  // immediately run the first action. Mirrors RssFeedListActivity.
  bool lockNextConfirmRelease = false;
};
