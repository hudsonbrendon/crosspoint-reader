#pragma once
#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class RssFeedListActivity final : public Activity {
 public:
  RssFeedListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void onAddFeed();
  void onManageOnWeb();
  void onSelectFeed(size_t index);
  void onDeleteFeed(size_t index);
  static std::string hostOf(const std::string& url);

  int selectedIndex = 0;  // 0..feedCount-1 = feeds; feedCount = "Add Feed"; feedCount+1 = "Manage on web"
  ButtonNavigator buttonNavigator;

  // Swallow the Confirm release carried over from selecting this screen in the
  // parent menu, so it doesn't immediately open/delete the first feed.
  bool lockNextConfirmRelease = false;

  // Set once the hold-to-delete dialog has fired for the current press so it
  // doesn't fire twice, and so the eventual Confirm release doesn't also open
  // the feed. Reset on the next Confirm press.
  bool longPressFired = false;

  int getItemCount() const;
};
