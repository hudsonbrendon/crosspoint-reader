#include "VirtualPetActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <PetSpriteData.h>
#include <PetSpriteRenderer.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "PetManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

VirtualPetActivity::VirtualPetActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("VirtualPet", renderer, mappedInput) {}

void VirtualPetActivity::onEnter() {
  Activity::onEnter();
  PET_MANAGER.begin();  // load + tick + sync + save
  selectedIndex = 0;
  // If Confirm is still held from selecting this screen in the Home menu,
  // swallow its release so it doesn't act on the first action.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate();
}

void VirtualPetActivity::onExit() {
  PET_MANAGER.save();
  Activity::onExit();
}

const char* VirtualPetActivity::actionLabel(int index) const {
  switch (static_cast<Action>(index)) {
    case Action::Feed:
      return tr(STR_PET_FEED);
    case Action::Snack:
      return tr(STR_PET_SNACK);
    case Action::Medicine:
      return tr(STR_PET_MEDICINE);
    case Action::Exercise:
      return tr(STR_PET_EXERCISE);
    case Action::Clean:
      return tr(STR_PET_CLEAN);
    case Action::Pet:
      return tr(STR_PET_PET);
    default:
      return "";
  }
}

void VirtualPetActivity::runAction(Action action) {
  switch (action) {
    case Action::Feed:
      PET_MANAGER.feed();
      break;
    case Action::Snack:
      PET_MANAGER.snack();
      break;
    case Action::Medicine:
      PET_MANAGER.giveMedicine();
      break;
    case Action::Exercise:
      PET_MANAGER.exercise();
      break;
    case Action::Clean:
      PET_MANAGER.clean();
      break;
    case Action::Pet:
      PET_MANAGER.petThePet();
      break;
    default:
      break;
  }
  requestUpdate();
}

void VirtualPetActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    runAction(static_cast<Action>(selectedIndex));
    return;
  }

  const int itemCount = static_cast<int>(Action::Count);
  buttonNavigator.onNext([this, itemCount] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this, itemCount] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });
}

void VirtualPetActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_VIRTUAL_PET));

  const pet::PetState& s = PET_MANAGER.state();

  // Sprite: 24x24 source at scale 4 -> 96x96, horizontally centred near top.
  const int scale = 4;
  const int spriteW = pet::kSpriteSize * scale;
  const int spriteX = (pageWidth - spriteW) / 2;
  const int spriteY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  pet::drawSprite(renderer, s.stage, spriteX, spriteY, scale);

  // Stats line under the sprite.
  char stats[96];
  std::snprintf(stats, sizeof(stats), "H:%u  J:%u  L:%u  %u%s", (unsigned)s.hunger, (unsigned)s.happiness,
                (unsigned)s.health, (unsigned)PET_MANAGER.ageDays(), tr(STR_PET_DAYS_SUFFIX));
  const int statsY = spriteY + spriteW + metrics.verticalSpacing;
  renderer.drawCenteredText(UI_10_FONT_ID, statsY, stats);

  if (s.isSick) {
    renderer.drawCenteredText(UI_10_FONT_ID, statsY + renderer.getLineHeight(UI_10_FONT_ID), tr(STR_PET_SICK));
  }

  // Action list.
  const int listTop = statsY + renderer.getLineHeight(UI_10_FONT_ID) * 2 + metrics.verticalSpacing;
  const int rowH = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
    const int rowY = listTop + i * rowH;
    const bool selected = (selectedIndex == i);
    if (selected) {
      renderer.fillRect(metrics.contentSidePadding, rowY, pageWidth - metrics.contentSidePadding * 2, rowH, true);
    } else {
      renderer.drawRect(metrics.contentSidePadding, rowY, pageWidth - metrics.contentSidePadding * 2, rowH);
    }
    const char* label = actionLabel(i);
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
    const int tx = (pageWidth - tw) / 2;
    const int ty = rowY + (rowH - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, tx, ty, label, !selected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  (void)pageHeight;
}
