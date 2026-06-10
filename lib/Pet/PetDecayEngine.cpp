#include "PetDecayEngine.h"

#include <algorithm>

namespace pet {

namespace {
uint8_t subClamp(uint8_t value, uint32_t amount) { return static_cast<uint8_t>(amount >= value ? 0 : value - amount); }
}  // namespace

void applyDecay(PetState& s, uint32_t elapsedHours, uint8_t startHour) {
  (void)startHour;  // reserved for sleep-aware decay; dormant without RTC
  if (elapsedHours == 0) return;
  const uint32_t hours = std::min(elapsedHours, PetConfig::MAX_DECAY_HOURS);

  s.hunger = subClamp(s.hunger, hours * PetConfig::HUNGER_DECAY_PER_HOUR);
  s.happiness = subClamp(s.happiness, hours * PetConfig::HAPPINESS_DECAY_PER_HOUR);

  // Health only decays once hunger is fully depleted (starvation), so an
  // attended pet never gets sick from time alone.
  if (s.hunger == 0) {
    s.health = subClamp(s.health, hours * PetConfig::HEALTH_DECAY_PER_HOUR);
  }

  // Waste accumulates over time; rolls into the sickness signal.
  const uint32_t waste = s.wasteCount + hours;
  s.wasteCount = static_cast<uint8_t>(std::min<uint32_t>(waste, 255));

  if (s.health <= PetConfig::SICK_HEALTH_THRESHOLD) {
    s.isSick = true;
  }
}

}  // namespace pet
