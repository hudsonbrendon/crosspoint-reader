#include "RssBrowserActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <Txt.h>
#include <WiFi.h>

#include <algorithm>

#include "HtmlToText.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "RssFeedCache.h"
#include "RssParser.h"
#include "SilentRestart.h"
#include "activities/ActivityManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/reader/TxtReaderActivity.h"
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
  // If we were opened while Confirm was held (selecting the feed in the list),
  // ignore its release so an instantly-cached feed doesn't auto-open item 0.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
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
      if (lockNextConfirmRelease) {
        lockNextConfirmRelease = false;  // swallow the entry release; don't open
      } else if (!items.empty()) {
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
    // Offline banner: drawn at y=40 (UI_10 font, ~20px tall). listTop must
    // clear the banner text so the first row highlight doesn't overlap it.
    constexpr int LIST_TOP_ONLINE = 30;
    constexpr int LIST_TOP_OFFLINE = 75;
    constexpr int BUTTON_HINTS_HEIGHT = 40;
    constexpr int LIST_BOTTOM_MARGIN = 5;

    if (offline) {
      renderer.drawCenteredText(UI_10_FONT_ID, 40, tr(STR_RSS_OFFLINE_CACHED));
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    if (items.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_RSS_NO_ITEMS));
    } else {
      const int listTop = offline ? LIST_TOP_OFFLINE : LIST_TOP_ONLINE;
      const int listHeight = pageHeight - listTop - BUTTON_HINTS_HEIGHT - LIST_BOTTOM_MARGIN;

      GUI.drawList(renderer, Rect{0, listTop, pageWidth, listHeight},
                   static_cast<int>(items.size()), selectorIndex,
                   [this](int index) {
                     std::string text = items[index].title;
                     if (!items[index].date.empty()) text += " (" + items[index].date + ")";
                     return text;
                   },
                   nullptr,
                   [](int /*index*/) { return UIIcon::Library; });
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

  const std::string& articleTitle = items[selectorIndex].title;

  // Strip the cached HTML to readable plain text NOW (no active TLS here, only
  // this one item in memory) and write it to a slug-named file so the reader's
  // status bar and Recents show the article title instead of "reading".
  std::string text;
  {
    const String html = Storage.readFile(htmlPath.c_str());
    text = htmlToText(std::string(html.c_str()));
  }
  const std::string articlePath =
      RssFeedCache::feedDir(feedUrl) + "/article_" + RssFeedCache::rssSlug(articleTitle) + ".txt";
  if (!Storage.writeFile(articlePath.c_str(), String(text.c_str()))) {
    LOG_ERR("RSS", "Failed to prepare reading file: %s", articlePath.c_str());
    return;
  }

  // Add to Recents with proper metadata (article title + feed name) BEFORE
  // opening the reader. The reader is transient so it won't add its own entry.
  RECENT_BOOKS.addBook(articlePath, articleTitle, feedName, "");

  // Open the article as a transient reader PUSHED on top of this browser. Back
  // then returns to the article list (this browser resumes with its list intact),
  // not Home. No reboot here — the browser stays alive and its onExit() does the
  // WiFi teardown + heap-clearing reboot only when the user fully leaves the feed.
  auto txt = makeUniqueNoThrow<Txt>(articlePath.c_str(), "/.inkpoint");
  if (!txt || !txt->load()) {
    LOG_ERR("RSS", "Failed to open article %s", articlePath.c_str());
    return;
  }
  auto reader = makeUniqueNoThrow<TxtReaderActivity>(renderer, mappedInput, std::move(txt), /*transient=*/true,
                                                     /*displayTitle=*/articleTitle);
  if (!reader) {
    LOG_ERR("RSS", "OOM creating article reader");
    return;
  }
  activityManager.pushActivity(std::move(reader));
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
