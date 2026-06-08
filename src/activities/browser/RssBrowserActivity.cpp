#include "RssBrowserActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "RssFeedCache.h"
#include "RssParser.h"
#include "SilentRestart.h"
#include "activities/ActivityManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {
constexpr int PAGE_ITEMS = 23;
}

RssBrowserActivity::RssBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string feedUrl,
                                       std::string feedName)
    : Activity("RssBrowser", renderer, mappedInput),
      feedUrl(std::move(feedUrl)),
      feedName(std::move(feedName)) {}

void RssBrowserActivity::onEnter() {
  Activity::onEnter();

  state = State::CHECK_WIFI;
  items.clear();
  selectorIndex = 0;
  offline = false;
  wifiWasConnected = false;
  errorMessage.clear();
  requestUpdate();

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    wifiWasConnected = true;
    fetchFeed();
  } else if (RssFeedCache::hasCache(feedUrl)) {
    loadFromCache();
  } else {
    launchWifiSelection();
  }
}

void RssBrowserActivity::onExit() {
  Activity::onExit();
  items.clear();

  if (wifiWasConnected && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void RssBrowserActivity::loop() {
  if (state == State::WIFI_SELECTION || state == State::FETCHING) {
    return;
  }

  if (state == State::CHECK_WIFI) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  if (state == State::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  if (state == State::BROWSING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!items.empty()) {
        openSelectedItem();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }

    if (!items.empty()) {
      buttonNavigator.onNextRelease([this] {
        selectorIndex = ButtonNavigator::nextIndex(selectorIndex, static_cast<int>(items.size()));
        requestUpdate();
      });
      buttonNavigator.onPreviousRelease([this] {
        selectorIndex = ButtonNavigator::previousIndex(selectorIndex, static_cast<int>(items.size()));
        requestUpdate();
      });
      buttonNavigator.onNextContinuous([this] {
        selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, static_cast<int>(items.size()), PAGE_ITEMS);
        requestUpdate();
      });
      buttonNavigator.onPreviousContinuous([this] {
        selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, static_cast<int>(items.size()), PAGE_ITEMS);
        requestUpdate();
      });
    }
  }
}

void RssBrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const char* headerTitle = feedName.empty() ? tr(STR_RSS_TITLE) : feedName.c_str();
  renderer.drawCenteredText(UI_12_FONT_ID, 15, headerTitle, true, EpdFontFamily::BOLD);

  if (state == State::CHECK_WIFI || state == State::FETCHING || state == State::WIFI_SELECTION) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_RSS_FETCHING));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == State::BROWSING) {
    if (offline) {
      renderer.drawCenteredText(UI_10_FONT_ID, 40, tr(STR_RSS_OFFLINE_CACHED));
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    if (items.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_RSS_NO_ITEMS));
    } else {
      const int listTop = offline ? 60 : 40;
      const auto pageStartIndex = selectorIndex / PAGE_ITEMS * PAGE_ITEMS;
      renderer.fillRect(0, listTop + (selectorIndex % PAGE_ITEMS) * 30 - 2, pageWidth - 1, 30);

      for (int i = pageStartIndex; i < static_cast<int>(items.size()) && i < pageStartIndex + PAGE_ITEMS; i++) {
        const auto& item = items[i];
        std::string displayText = item.title;
        if (!item.date.empty()) displayText += " (" + item.date + ")";
        auto label = renderer.truncatedText(UI_10_FONT_ID, displayText.c_str(), pageWidth - 40);
        renderer.drawText(UI_10_FONT_ID, 20, listTop + (i % PAGE_ITEMS) * 30, label.c_str(),
                          i != selectorIndex);
      }
    }
    renderer.displayBuffer();
  }
}

void RssBrowserActivity::fetchFeed() {
  state = State::FETCHING;
  requestUpdate(true);

  RssParser parser;
  const bool ok = HttpDownloader::fetchUrl(feedUrl, [&parser](const uint8_t* data, size_t len) {
    parser.write(data, len);
    return true;
  });
  parser.flush();
  if (!ok || parser.error()) {
    if (RssFeedCache::hasCache(feedUrl)) {
      loadFromCache();
      return;
    }
    errorMessage = tr(STR_RSS_FETCH_FAILED);
    state = State::ERROR;
    requestUpdate();
    return;
  }

  items = std::move(parser).getEntries();
  if (items.empty()) {
    errorMessage = tr(STR_RSS_NO_ITEMS);
    state = State::ERROR;
    requestUpdate();
    return;
  }

  if (!RssFeedCache::writeFeed(feedUrl, items)) {
    LOG_ERR("RSS", "Cache write failed (continuing live)");
  }
  selectorIndex = 0;
  offline = false;
  state = State::BROWSING;
  requestUpdate();
}

void RssBrowserActivity::loadFromCache() {
  items = RssFeedCache::readIndex(feedUrl);
  offline = true;
  selectorIndex = 0;
  if (items.empty()) {
    errorMessage = tr(STR_RSS_NO_ITEMS);
    state = State::ERROR;
  } else {
    state = State::BROWSING;
  }
  requestUpdate();
}

void RssBrowserActivity::openSelectedItem() {
  if (selectorIndex < 0 || static_cast<size_t>(selectorIndex) >= items.size()) return;
  const std::string path = RssFeedCache::itemTextPath(feedUrl, static_cast<size_t>(selectorIndex));
  activityManager.goToTxtReader(path);
}

void RssBrowserActivity::launchWifiSelection() {
  state = State::WIFI_SELECTION;
  requestUpdate();

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void RssBrowserActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    wifiWasConnected = true;
    fetchFeed();
  } else {
    state = State::ERROR;
    errorMessage = tr(STR_RSS_FETCH_FAILED);
    requestUpdate();
  }
}
