#pragma once
#include <string>
#include <vector>

#include "RssParser.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class RssBrowserActivity final : public Activity {
 public:
  RssBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string feedUrl,
                     std::string feedName);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { CHECK_WIFI, WIFI_SELECTION, FETCHING, BROWSING, ERROR };

  void fetchFeed();
  void loadFromCache();
  void openSelectedItem();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  bool preventAutoSleep() override { return true; }

  std::string feedUrl;
  std::string feedName;
  State state = State::CHECK_WIFI;
  bool offline = false;
  bool wifiWasConnected = false;  // true when we brought WiFi up, so onExit can silentRestart
  std::vector<RssEntry> items;
  int selectorIndex = 0;
  ButtonNavigator buttonNavigator;
  std::string errorMessage;
};
