#pragma once

#include <cstdint>

#include "activities/Activity.h"

class ReadingHeatmapActivity final : public Activity {
 public:
  ReadingHeatmapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int16_t viewYear = 0;   // displayed year
  uint8_t viewMonth = 1;  // 1..12
  bool haveDate = false;
  int16_t today_y = 0;
  uint16_t today_doy = 0;
  uint32_t goalMs = 0;

  void prevMonth();
  void nextMonth();
};
