#include "RssFeedListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "RssFeedStore.h"
#include "activities/ActivityManager.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

RssFeedListActivity::RssFeedListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("RssFeedList", renderer, mappedInput) {}

int RssFeedListActivity::getItemCount() const {
  // Feed rows + "Add Feed" row + "Manage on web" row
  return static_cast<int>(RSS_STORE.getCount()) + 2;
}

std::string RssFeedListActivity::hostOf(const std::string& url) {
  size_t s = url.find("://");
  s = (s == std::string::npos) ? 0 : s + 3;
  size_t e = url.find('/', s);
  return url.substr(s, (e == std::string::npos ? url.size() : e) - s);
}

void RssFeedListActivity::onEnter() {
  Activity::onEnter();
  RSS_STORE.loadFromFile();
  selectedIndex = 0;
  requestUpdate();
}

void RssFeedListActivity::onExit() { Activity::onExit(); }

void RssFeedListActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const int feedCount = static_cast<int>(RSS_STORE.getCount());
    if (selectedIndex < feedCount) {
      onSelectFeed(static_cast<size_t>(selectedIndex));
    } else if (selectedIndex == feedCount) {
      onAddFeed();
    } else {
      onManageOnWeb();
    }
    return;
  }

  // Right button on a feed row = delete
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    const int feedCount = static_cast<int>(RSS_STORE.getCount());
    if (selectedIndex < feedCount) {
      onDeleteFeed(static_cast<size_t>(selectedIndex));
    }
    return;
  }

  const int itemCount = getItemCount();
  if (itemCount > 0) {
    buttonNavigator.onNext([this, itemCount] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
      requestUpdate();
    });

    buttonNavigator.onPrevious([this, itemCount] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
      requestUpdate();
    });
  }
}

void RssFeedListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_RSS_FEEDS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int itemCount = getItemCount();
  const int feedCount = static_cast<int>(RSS_STORE.getCount());

  if (feedCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 15, tr(STR_RSS_NO_FEEDS));
  }

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount, selectedIndex,
      [feedCount](int index) {
        if (index < feedCount) {
          const auto* feed = RSS_STORE.getFeed(static_cast<size_t>(index));
          if (feed) return feed->name.empty() ? feed->url : feed->name;
        }
        if (index == feedCount) {
          return std::string(I18n::getInstance().get(StrId::STR_RSS_ADD_FEED));
        }
        return std::string(I18n::getInstance().get(StrId::STR_RSS_MANAGE_WEB));
      },
      [feedCount](int index) {
        if (index < feedCount) {
          const auto* feed = RSS_STORE.getFeed(static_cast<size_t>(index));
          if (feed && !feed->name.empty()) return feed->url;
        }
        return std::string("");
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void RssFeedListActivity::onAddFeed() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_RSS_ENTER_URL), "", 256,
                                             InputType::Url),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& text = std::get<KeyboardResult>(result.data).text;
          if (!text.empty()) {
            RssFeed feed;
            feed.url = text;
            feed.name = hostOf(text);
            RSS_STORE.addFeed(feed);
          }
        }
        RSS_STORE.loadFromFile();
        selectedIndex = 0;
        requestUpdate();
      });
}

void RssFeedListActivity::onSelectFeed(size_t index) {
  const RssFeed* f = RSS_STORE.getFeed(index);
  if (f) activityManager.goToRssBrowser(f->url, f->name);
}

void RssFeedListActivity::onDeleteFeed(size_t index) {
  const RssFeed* f = RSS_STORE.getFeed(index);
  if (!f) return;

  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_RSS_DELETE_FEED), f->name),
      [this, index](const ActivityResult& result) {
        if (!result.isCancelled) {
          RSS_STORE.removeFeed(index);
          if (selectedIndex >= getItemCount()) {
            selectedIndex = getItemCount() - 1;
          }
        }
        requestUpdate();
      });
}

void RssFeedListActivity::onManageOnWeb() { activityManager.goToFileTransfer(); }
