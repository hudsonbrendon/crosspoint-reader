#include "RssBrowserActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>

#include "HtmlToText.h"
#include "InkPointState.h"
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
// Cap the items kept/cached per feed — bounds RAM (metadata vector) and SD files.
constexpr size_t MAX_FEED_ITEMS = 40;
// Cap the body bytes cached per item. The body is streamed to SD (so the fetch
// itself is unbounded-safe), but openSelectedItem() reads + strips one item in
// RAM, so keep that within budget. ~24KB HTML ≈ a long article.
constexpr size_t ITEM_CONTENT_CAP = 24 * 1024;
}  // namespace

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

  // Stream each item's <content> straight to its SD file as it is parsed — the
  // article body is NEVER held in RAM. Free heap during the TLS/HTTPS fetch is
  // only tens of KB on the 380KB device, so any large std::string here OOMs
  // (confirmed by repeated crash backtraces). Only small metadata stays in RAM;
  // htmlToText runs later in openSelectedItem(), off the network path.
  RssFeedCache::ensureFeedDir(feedUrl);
  items.clear();

  HalFile itemFile;     // (re)opened per item by the begin callback
  size_t itemBytes = 0;  // bytes written to the current item, for the per-item cap
  RssParser parser;
  parser.setStreamingSink(
      /*onBegin=*/
      [this, &itemFile, &itemBytes]() {
        if (items.size() >= MAX_FEED_ITEMS) return;
        Storage.openFileForWrite("RSS", RssFeedCache::itemTextPath(feedUrl, items.size()), itemFile);
        itemBytes = 0;
      },
      /*onContent=*/
      [&itemFile, &itemBytes](const char* data, size_t len) {
        if (!itemFile || itemBytes >= ITEM_CONTENT_CAP) return;
        const size_t n = std::min(len, ITEM_CONTENT_CAP - itemBytes);
        itemFile.write(reinterpret_cast<const uint8_t*>(data), n);
        itemBytes += n;
      },
      /*onEnd=*/
      [this, &itemFile](const RssEntry& meta, bool wroteContent) {
        if (items.size() >= MAX_FEED_ITEMS) {
          if (itemFile) itemFile.close();
          return;
        }
        if (!wroteContent && itemFile && !meta.contentHtml.empty()) {
          // No <content> streamed — the small description is the body.
          itemFile.write(reinterpret_cast<const uint8_t*>(meta.contentHtml.data()), meta.contentHtml.size());
        }
        if (itemFile) itemFile.close();
        RssEntry m;  // keep metadata only — the body lives in the cache file
        m.title = meta.title;
        m.date = meta.date;
        items.push_back(std::move(m));
      });

  const bool ok = HttpDownloader::fetchUrl(feedUrl, [&parser](const uint8_t* data, size_t len) {
    parser.write(data, len);
    return true;
  });
  parser.flush();

  if (!ok || parser.error()) {
    // Network/parse failure mid-stream: prefer a complete previous cache if any.
    if (items.empty() && RssFeedCache::hasCache(feedUrl)) {
      loadFromCache();
      return;
    }
    if (items.empty()) {
      errorMessage = tr(STR_RSS_FETCH_FAILED);
      state = State::ERROR;
      requestUpdate();
      return;
    }
    // else: we streamed at least some items before the error — show them.
  }

  if (items.empty()) {
    errorMessage = tr(STR_RSS_NO_ITEMS);
    state = State::ERROR;
    requestUpdate();
    return;
  }

  RssFeedCache::writeIndex(feedUrl, items);
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
  const std::string htmlPath = RssFeedCache::itemTextPath(feedUrl, static_cast<size_t>(selectorIndex));
  if (!Storage.exists(htmlPath.c_str())) {
    LOG_ERR("RSS", "Cached article missing: %s", htmlPath.c_str());
    return;
  }

  // Strip the cached HTML to readable plain text NOW (no active TLS here, only
  // this one item in memory) and write it to a reusable reading file that the
  // TXT reader opens. Doing this off the network path is what keeps us within
  // the heap budget.
  std::string text;
  {
    const String html = Storage.readFile(htmlPath.c_str());
    text = htmlToText(std::string(html.c_str()));
  }
  const std::string readingPath = RssFeedCache::readingTextPath(feedUrl);
  if (!Storage.writeFile(readingPath.c_str(), String(text.c_str()))) {
    LOG_ERR("RSS", "Failed to prepare reading file: %s", readingPath.c_str());
    return;
  }

  if (wifiWasConnected && WiFi.getMode() != WIFI_MODE_NULL) {
    // This session brought WiFi up, so we must clear the LWIP heap fragmentation
    // on the way out (same reason onExit() reboots). Navigating directly would
    // trigger onExit()'s silentRestart() and reboot us to Home, losing the
    // article. Instead, reboot straight into the reader showing this article.
    // goToReader() dispatches by extension, so the .txt opens in the TXT reader.
    APP_STATE.openEpubPath = readingPath;
    APP_STATE.saveToFile();
    WiFi.disconnect(false);
    silentRestartToReader();
    return;
  }

  // Pure offline browse (no WiFi session this time) — navigate directly, no reboot.
  activityManager.goToTxtReader(readingPath);
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
