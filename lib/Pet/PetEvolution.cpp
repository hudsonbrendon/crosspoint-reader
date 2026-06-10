#include "PetEvolution.h"

namespace pet {

PetStage computeStage(const PetState& s, uint16_t ageDays, uint32_t pagesRead) {
  PetStage reached = PetStage::EGG;
  for (int i = 0; i < kEvolutionTableSize; ++i) {
    const EvolutionReq& req = kEvolutionTable[i];
    if (ageDays >= req.minDays && pagesRead >= req.minPages) {
      reached = req.stage;
    }
  }
  // One-way: never regress below the already-recorded stage.
  if (static_cast<uint8_t>(reached) < static_cast<uint8_t>(s.stage)) {
    return s.stage;
  }
  return reached;
}

bool maybeEvolve(PetState& s, uint16_t ageDays, uint32_t pagesRead) {
  const PetStage next = computeStage(s, ageDays, pagesRead);
  if (static_cast<uint8_t>(next) > static_cast<uint8_t>(s.stage)) {
    s.stage = next;
    return true;
  }
  return false;
}

PetMood computeMood(const PetState& s) {
  if (s.isSick || s.health == 0) return PetMood::SICK;
  const int avg = (static_cast<int>(s.hunger) + s.happiness + s.health) / 3;
  if (avg >= 66) return PetMood::HAPPY;
  if (avg >= 33) return PetMood::NEUTRAL;
  return PetMood::SAD;
}

}  // namespace pet
