#include "NetworkModeSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct ModeInfo {
  NetworkMode mode;
  StrId label;
  StrId desc;
  UIIcon icon;
};
constexpr ModeInfo kJoin{NetworkMode::JOIN_NETWORK, StrId::STR_JOIN_NETWORK, StrId::STR_JOIN_DESC, UIIcon::Wifi};
constexpr ModeInfo kCalibre{NetworkMode::CONNECT_CALIBRE, StrId::STR_CALIBRE_WIRELESS, StrId::STR_CALIBRE_DESC,
                            UIIcon::Library};
constexpr ModeInfo kHotspot{NetworkMode::CREATE_HOTSPOT, StrId::STR_CREATE_HOTSPOT, StrId::STR_HOTSPOT_DESC,
                            UIIcon::Hotspot};

// The active menu, in display order. Calibre is hidden in the web-management variant.
std::vector<ModeInfo> activeModes(bool showCalibre) {
  if (showCalibre) return {kJoin, kCalibre, kHotspot};
  return {kJoin, kHotspot};
}
}  // namespace

void NetworkModeSelectionActivity::onEnter() {
  Activity::onEnter();

  // Reset selection
  selectedIndex = 0;

  // Trigger first update
  requestUpdate();
}

void NetworkModeSelectionActivity::onExit() { Activity::onExit(); }

void NetworkModeSelectionActivity::loop() {
  // Handle back button - cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }

  // Handle confirm button - select current option
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const auto modes = activeModes(showCalibre);
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(modes.size())) {
      onModeSelected(modes[static_cast<size_t>(selectedIndex)].mode);
    }
    return;
  }

  // Handle navigation
  const int count = static_cast<int>(activeModes(showCalibre).size());
  buttonNavigator.onNext([this, count] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, count);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, count] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, count);
    requestUpdate();
  });
}

void NetworkModeSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, I18N.get(titleStr));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const auto modes = activeModes(showCalibre);

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(modes.size()), selectedIndex,
      [&modes](int index) { return std::string(I18N.get(modes[static_cast<size_t>(index)].label)); },
      [&modes](int index) { return std::string(I18N.get(modes[static_cast<size_t>(index)].desc)); },
      [&modes](int index) { return modes[static_cast<size_t>(index)].icon; });

  // Draw help text at bottom
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void NetworkModeSelectionActivity::onModeSelected(NetworkMode mode) {
  setResult(NetworkModeResult{mode});
  finish();
}

void NetworkModeSelectionActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
