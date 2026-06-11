#pragma once
#include <functional>
#include <vector>

#include "./FileBrowserActivity.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool hasOpdsServers = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  size_t coverBufferSize = 0;      // Bytes allocated to coverBuffer
  // Logical rect last passed to drawRecentBookCover. The cover snapshot only
  // needs to cover this region, not the entire framebuffer, so we cache the
  // tile instead of all 48 KB. Set in render() before the call.
  int coverRectX = 0;
  int coverRectY = 0;
  int coverRectW = 0;
  int coverRectH = 0;
  std::vector<RecentBook> recentBooks;
  const HomeMenuItem initialMenuItem;

  // Convert HomeMenuItem to menu index (used in onEnter)
  // Order: FILE_BROWSER, RECENTS, [OPDS], FILE_TRANSFER, RSS_BROWSER, VIRTUAL_PET, READING_STATS_MENU, FLASHCARD_MENU,
  // SETTINGS_MENU
  static int menuItemToIndex(HomeMenuItem item, bool hasOpdsUrl) {
    int i = 0;
    if (item == HomeMenuItem::FILE_BROWSER) return i;
    ++i;
    if (item == HomeMenuItem::RECENTS) return i;
    ++i;
    if (item == HomeMenuItem::OPDS_BROWSER) return hasOpdsUrl ? i : 0;
    if (hasOpdsUrl) ++i;
    if (item == HomeMenuItem::FILE_TRANSFER) return i;
    ++i;
    if (item == HomeMenuItem::RSS_BROWSER) return i;
    ++i;
    if (item == HomeMenuItem::VIRTUAL_PET) return i;
    ++i;
    if (item == HomeMenuItem::READING_STATS_MENU) return i;
    ++i;
    if (item == HomeMenuItem::FLASHCARD_MENU) return i;
    ++i;
    if (item == HomeMenuItem::POMODORO_MENU) return i;
    ++i;
    if (item == HomeMenuItem::SETTINGS_MENU) return i;
    return 0;
  }

  // Convert menu index to HomeMenuItem (used in loop)
  // Order: FILE_BROWSER, RECENTS, [OPDS], FILE_TRANSFER, RSS_BROWSER, VIRTUAL_PET, READING_STATS_MENU, FLASHCARD_MENU,
  // SETTINGS_MENU
  static HomeMenuItem indexToMenuItem(int idx, bool hasOpdsUrl) {
    int i = 0;
    if (idx == i++) return HomeMenuItem::FILE_BROWSER;
    if (idx == i++) return HomeMenuItem::RECENTS;
    if (hasOpdsUrl && idx == i++) return HomeMenuItem::OPDS_BROWSER;
    if (idx == i++) return HomeMenuItem::FILE_TRANSFER;
    if (idx == i++) return HomeMenuItem::RSS_BROWSER;
    if (idx == i++) return HomeMenuItem::VIRTUAL_PET;
    if (idx == i++) return HomeMenuItem::READING_STATS_MENU;
    if (idx == i++) return HomeMenuItem::FLASHCARD_MENU;
    if (idx == i++) return HomeMenuItem::POMODORO_MENU;
    if (idx == i) return HomeMenuItem::SETTINGS_MENU;
    return HomeMenuItem::NONE;
  }
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onRecentsOpen();
  void onSettingsOpen();
  void onFileTransferOpen();
  void onOpdsBrowserOpen();
  void onReadingStatsOpen();
  void onRssOpen();
  void onFlashcardOpen();
  void onVirtualPetOpen();
  void onPomodoroOpen();

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);
  // Read progress.bin + book.bin header from an epub cache dir to compute a
  // chapter-level progress percent (0-100). Returns 0 when cache is absent.
  static int loadBookProgressPercent(const std::string& cachePath);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE)
      : Activity("Home", renderer, mappedInput), initialMenuItem(initialMenuItemValue) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
