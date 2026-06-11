#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>

#include <cstring>
#include <functional>
#include <vector>

#include "InkPointSettings.h"
#include "InkPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Read spineIndex from progress.bin and spineCount from the book.bin header.
// Returns a chapter-level reading progress percent (0-100). Returns 0 when no
// cache files are present (book not yet opened) or on any read error.
// This is intentionally lightweight — it opens two tiny files (~6 + 9 bytes)
// and does no EPUB zip parsing, so it is safe to call per-book on the home screen.
int HomeActivity::loadBookProgressPercent(const std::string& cachePath) {
  if (cachePath.empty()) return 0;

  // --- 1. Read spineIndex from progress.bin ---
  // Format: {spineIndex(u16 LE), pageNum(u16 LE), pageCount(u16 LE)}
  const std::string progressPath = cachePath + "/progress.bin";
  uint16_t spineIndex = 0;
  {
    HalFile f;
    if (!Storage.openFileForRead("HOME", progressPath, f)) return 0;
    uint8_t data[4];
    const int n = f.read(data, 4);
    if (n < 2) return 0;
    spineIndex = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  }
  if (spineIndex == 0) return 0;

  // --- 2. Read spineCount from book.bin header ---
  // Format: {version(u8), lutOffset(u32 LE), spineCount(u16 LE), tocCount(u16 LE), ...}
  const std::string bookBinPath = cachePath + "/book.bin";
  uint16_t spineCount = 0;
  {
    HalFile f;
    if (!Storage.openFileForRead("HOME", bookBinPath, f)) return 0;
    uint8_t header[7];  // version(1) + lutOffset(4) + spineCount(2)
    const int n = f.read(header, 7);
    if (n < 7) return 0;
    spineCount = static_cast<uint16_t>(header[5]) | (static_cast<uint16_t>(header[6]) << 8);
  }
  if (spineCount == 0) return 0;

  // Chapter-level approximation: spineIndex / spineCount * 100
  const int pct = static_cast<int>(spineIndex) * 100 / static_cast<int>(spineCount);
  return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
}

int HomeActivity::getMenuItemCount() const {
  int count =
      9;  // File Browser, Recents, File transfer, RSS, Virtual Pet, Reading Stats, Flashcards, Pomodoro, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.inkpoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.inkpoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  // Load reading progress percent for each book from its cache dir.
  // cachePath is the directory portion of coverBmpPath (strip filename after last '/').
  // Falls back to derivation from book.path when coverBmpPath is empty.
  for (RecentBook& book : recentBooks) {
    std::string cachePath;
    if (!book.coverBmpPath.empty()) {
      const size_t slashPos = book.coverBmpPath.rfind('/');
      if (slashPos != std::string::npos) {
        cachePath = book.coverBmpPath.substr(0, slashPos);
      }
    } else if (FsHelpers::hasEpubExtension(book.path)) {
      // Derive cache path from filepath hash, same formula as Epub constructor
      cachePath = "/.inkpoint/epub_" + std::to_string(std::hash<std::string>{}(book.path));
    }
    book.progressPercent = loadBookProgressPercent(cachePath);
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  const int menuCount = getMenuItemCount();

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
    } else {
      const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
      switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
        case HomeMenuItem::FILE_BROWSER:
          onFileBrowserOpen();
          break;
        case HomeMenuItem::RECENTS:
          onRecentsOpen();
          break;
        case HomeMenuItem::OPDS_BROWSER:
          onOpdsBrowserOpen();
          break;
        case HomeMenuItem::FILE_TRANSFER:
          onFileTransferOpen();
          break;
        case HomeMenuItem::SETTINGS_MENU:
          onSettingsOpen();
          break;
        case HomeMenuItem::READING_STATS_MENU:
          onReadingStatsOpen();
          break;
        case HomeMenuItem::RSS_BROWSER:
          onRssOpen();
          break;
        case HomeMenuItem::VIRTUAL_PET:
          onVirtualPetOpen();
          break;
        case HomeMenuItem::FLASHCARD_MENU:
          onFlashcardOpen();
          break;
        case HomeMenuItem::POMODORO_MENU:
          onPomodoroOpen();
          break;
        default:
          break;
      }
    }
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding},
                 metrics.homeContinueReadingInMenu && !recentBooks.empty() ? recentBooks[0].title.c_str() : nullptr);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = metrics.homeTopPadding;
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),
                                        tr(STR_RSS_TITLE),    tr(STR_VIRTUAL_PET),       tr(STR_READING_STATS_TITLE),
                                        tr(STR_FLASHCARD),    tr(STR_POMODORO),          tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Library, Paw, Stats, Cards, Clock, Settings};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Library);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  const int menuY = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  int menuHeight = pageHeight - menuY - metrics.buttonHintsHeight - metrics.verticalSpacing;
  if (menuHeight < 0) menuHeight = 0;  // guard for very short/oriented screens
  GUI.drawButtonMenu(
      renderer, Rect{0, menuY, pageWidth, menuHeight}, static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }

void HomeActivity::onReadingStatsOpen() { activityManager.goToReadingStats(); }

void HomeActivity::onRssOpen() { activityManager.goToRssFeeds(); }

void HomeActivity::onFlashcardOpen() { activityManager.goToFlashcards(); }

void HomeActivity::onVirtualPetOpen() { activityManager.goToVirtualPet(); }

void HomeActivity::onPomodoroOpen() { activityManager.goToPomodoro(); }
