#pragma once
#include <cstdint>

#include "PetState.h"

namespace pet {

// Apply `elapsedHours` of decay to `s`. `startHour` (0..23) is accepted for
// future sleep-aware decay but is currently unused (no RTC; see plan caveat).
// elapsedHours is internally capped at PetConfig::MAX_DECAY_HOURS.
void applyDecay(PetState& s, uint32_t elapsedHours, uint8_t startHour);

}  // namespace pet
