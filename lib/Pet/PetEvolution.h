#pragma once
#include <cstdint>

#include "PetState.h"

namespace pet {

// Returns the highest stage whose (minDays, minPages) gates are both satisfied.
// EGG is returned when no gate is met. Never returns a stage lower than s.stage
// (evolution is one-way).
PetStage computeStage(const PetState& s, uint16_t ageDays, uint32_t pagesRead);

// Updates s.stage in place using computeStage(). Returns true if it advanced.
bool maybeEvolve(PetState& s, uint16_t ageDays, uint32_t pagesRead);

// Coarse mood from the three needs (for sprite/variant selection).
PetMood computeMood(const PetState& s);

}  // namespace pet
