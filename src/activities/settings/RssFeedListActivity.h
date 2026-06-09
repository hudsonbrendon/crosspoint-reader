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

  int getItemCount() const;
};
