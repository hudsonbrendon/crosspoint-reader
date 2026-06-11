#include "Lyra3CoversTheme.h"

#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"
#include "fontIds.h"

// Internal constants
namespace {
constexpr int hPaddingInSelection = 8;
constexpr int cornerRadius = 6;
constexpr int progressBarHeight = 4;   // thin solid bar
constexpr int progressBarMarginTop = 5;
constexpr int progressTextMarginTop = 3;
}  // namespace

void Lyra3CoversTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                           bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const int tileWidth = (rect.width - 2 * Lyra3CoversMetrics::values.contentSidePadding) / 3;
  const int tileY = rect.y;
  const bool hasContinueReading = !recentBooks.empty();

  // Draw book card regardless, fill with message based on `hasContinueReading`
  // Draw cover image as background if available (inside the box)
  // Only load from SD on first render, then use stored buffer
  if (hasContinueReading) {
    if (!coverRendered) {
      for (int i = 0;
           i < std::min(static_cast<int>(recentBooks.size()), Lyra3CoversMetrics::values.homeRecentBooksCount); i++) {
        std::string coverPath = recentBooks[i].coverBmpPath;
        bool hasCover = true;
        int tileX = Lyra3CoversMetrics::values.contentSidePadding + tileWidth * i;
        if (coverPath.empty()) {
          hasCover = false;
        } else {
          const std::string coverBmpPath =
              UITheme::getCoverThumbPath(coverPath, Lyra3CoversMetrics::values.homeCoverHeight);

          // First time: load cover from SD and render
          HalFile file;
          if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
            Bitmap bitmap(file);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
              float coverHeight = static_cast<float>(bitmap.getHeight());
              float coverWidth = static_cast<float>(bitmap.getWidth());
              float ratio = coverWidth / coverHeight;
              const float tileRatio = static_cast<float>(tileWidth - 2 * hPaddingInSelection) /
                                      static_cast<float>(Lyra3CoversMetrics::values.homeCoverHeight);
              float cropX = 1.0f - (tileRatio / ratio);

              renderer.drawBitmap(bitmap, tileX + hPaddingInSelection, tileY + hPaddingInSelection,
                                  tileWidth - 2 * hPaddingInSelection, Lyra3CoversMetrics::values.homeCoverHeight,
                                  cropX);
            } else {
              hasCover = false;
            }
            file.close();
          }
        }
        // Draw either way
        renderer.drawRect(tileX + hPaddingInSelection, tileY + hPaddingInSelection, tileWidth - 2 * hPaddingInSelection,
                          Lyra3CoversMetrics::values.homeCoverHeight, true);

        if (!hasCover) {
          // Render empty cover
          renderer.fillRect(tileX + hPaddingInSelection,
                            tileY + hPaddingInSelection + (Lyra3CoversMetrics::values.homeCoverHeight / 3),
                            tileWidth - 2 * hPaddingInSelection, 2 * Lyra3CoversMetrics::values.homeCoverHeight / 3,
                            true);
          renderer.drawIcon(CoverIcon, tileX + hPaddingInSelection + 24, tileY + hPaddingInSelection + 24, 32, 32);
        }
      }

      coverBufferStored = storeCoverBuffer();
      coverRendered = coverBufferStored;  // Only consider it rendered if we successfully stored the buffer
    }

    for (int i = 0; i < std::min(static_cast<int>(recentBooks.size()), Lyra3CoversMetrics::values.homeRecentBooksCount);
         i++) {
      bool bookSelected = (selectorIndex == i);

      int tileX = Lyra3CoversMetrics::values.contentSidePadding + tileWidth * i;

      const int maxLineWidth = tileWidth - 2 * hPaddingInSelection;

      auto titleLines = renderer.wrappedText(SMALL_FONT_ID, recentBooks[i].title.c_str(), maxLineWidth, 2);

      const int titleLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
      const int dynamicBlockHeight = static_cast<int>(titleLines.size()) * titleLineHeight;
      // Total footer box: title + progress bar + percent text
      const int progressBarTotalHeight = progressBarMarginTop + progressBarHeight + progressTextMarginTop + titleLineHeight;
      const int dynamicTitleBoxHeight = dynamicBlockHeight + hPaddingInSelection + 5 + progressBarTotalHeight;

      if (bookSelected) {
        // Draw selection box spanning cover + title + progress area
        renderer.fillRoundedRect(tileX, tileY, tileWidth, hPaddingInSelection, cornerRadius, true, true, false, false,
                                 Color::LightGray);
        renderer.fillRectDither(tileX, tileY + hPaddingInSelection, hPaddingInSelection,
                                Lyra3CoversMetrics::values.homeCoverHeight, Color::LightGray);
        renderer.fillRectDither(tileX + tileWidth - hPaddingInSelection, tileY + hPaddingInSelection,
                                hPaddingInSelection, Lyra3CoversMetrics::values.homeCoverHeight, Color::LightGray);
        renderer.fillRoundedRect(tileX, tileY + Lyra3CoversMetrics::values.homeCoverHeight + hPaddingInSelection,
                                 tileWidth, dynamicTitleBoxHeight, cornerRadius, false, false, true, true,
                                 Color::LightGray);
      }

      int currentY = tileY + Lyra3CoversMetrics::values.homeCoverHeight + hPaddingInSelection + 5;
      for (const auto& line : titleLines) {
        renderer.drawText(SMALL_FONT_ID, tileX + hPaddingInSelection, currentY, line.c_str(), true);
        currentY += titleLineHeight;
      }

      // Progress bar + percentage text beneath the title
      const int pct = recentBooks[i].progressPercent;
      currentY += progressBarMarginTop;

      // Outline of the progress bar
      const int barX = tileX + hPaddingInSelection;
      const int barW = tileWidth - 2 * hPaddingInSelection;
      renderer.drawRect(barX, currentY, barW, progressBarHeight);

      // Filled portion (guard against pct == 0 to avoid drawing a 0-width fill)
      if (pct > 0) {
        const int fillW = pct * (barW - 2) / 100;
        if (fillW > 0) {
          renderer.fillRect(barX + 1, currentY + 1, fillW, progressBarHeight - 2);
        }
      }
      currentY += progressBarHeight + progressTextMarginTop;

      // Percentage text, right-aligned within the tile
      char pctBuf[8];
      snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
      const int pctTextW = renderer.getTextWidth(SMALL_FONT_ID, pctBuf);
      const int pctTextX = tileX + tileWidth - hPaddingInSelection - pctTextW;
      renderer.drawText(SMALL_FONT_ID, pctTextX, currentY, pctBuf, true);
    }
  } else {
    drawEmptyRecents(renderer, rect);
  }
}
