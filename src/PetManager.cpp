#include "PetManager.h"

#include <Logging.h>

#include <algorithm>
#include <ctime>

#include "PetDecayEngine.h"
#include "PetEvolution.h"
#include "PetStore.h"
#include "ReadingStatsStore.h"

namespace {
constexpr uint32_t SECONDS_PER_HOUR = 3600;
constexpr uint32_t SECONDS_PER_DAY = 86400;
}  // namespace

void PetManager::addClamped(uint8_t& field, uint8_t amount) {
  field = static_cast<uint8_t>(std::min<int>(100, field + amount));
}

void PetManager::begin() {
  if (!PetStore::load(state_)) {
    // Fresh pet: an unhatched egg born "now".
    state_ = pet::PetState{};
    state_.initialized = true;
    state_.stage = pet::PetStage::EGG;
    state_.birthTime = static_cast<uint32_t>(time(nullptr));
    state_.lastTickTime = state_.birthTime;
    LOG_INF("PET", "Created new egg");
  }
  loaded_ = true;
  tick();
  syncFromReadingStats();
  save();
}

void PetManager::tick() {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (state_.lastTickTime == 0 || now <= state_.lastTickTime) {
    state_.lastTickTime = now;  // no forward progress (clock reset or first run)
    return;
  }
  const uint32_t elapsedSec = now - state_.lastTickTime;
  const uint32_t elapsedHours = elapsedSec / SECONDS_PER_HOUR;
  if (elapsedHours == 0) return;  // keep lastTickTime; sub-hour deltas accumulate

  // startHour is unused (no RTC); pass 0.
  pet::applyDecay(state_, elapsedHours, 0);
  state_.lastTickTime = state_.lastTickTime + elapsedHours * SECONDS_PER_HOUR;
}

void PetManager::syncFromReadingStats() {
  const uint32_t pages = READING_STATS.totalPagesRead();
  const uint32_t readMs = READING_STATS.totalReadingMs();

  // 1) Reading-time delta -> auto meals (hunger). Each MS_PER_AUTO_MEAL of new
  //    reading time grants AUTO_MEAL_HUNGER, capped per sync to avoid a huge
  //    one-shot jump after a long offline period.
  if (readMs > state_.lastKnownReadMs) {
    const uint32_t deltaMs = readMs - state_.lastKnownReadMs;
    uint32_t meals = deltaMs / pet::PetConfig::MS_PER_AUTO_MEAL;
    meals = std::min<uint32_t>(meals, 8);  // cap
    if (meals > 0) {
      addClamped(state_.hunger, static_cast<uint8_t>(
                                   std::min<uint32_t>(255, meals * pet::PetConfig::AUTO_MEAL_HUNGER)));
    }
  }
  state_.lastKnownReadMs = readMs;

  // 2) Pages drive the evolution gates directly (no seconds/90 estimate).
  state_.totalPagesRead = pages;
  hatchIfEgg();
  const uint16_t days = ageDays();
  if (pet::maybeEvolve(state_, days, pages)) {
    LOG_INF("PET", "Evolved to stage %d", static_cast<int>(state_.stage));
  }
}

void PetManager::hatchIfEgg() {
  // Leaving EGG happens through the evolution table (HATCHLING gate). Nothing
  // extra to do here yet, but kept as the single place to add hatch side
  // effects (e.g. naming) later.
}

bool PetManager::save() { return PetStore::save(state_); }

void PetManager::feed() {
  addClamped(state_.hunger, pet::PetConfig::FEED_HUNGER);
  save();
}

void PetManager::snack() {
  addClamped(state_.hunger, pet::PetConfig::SNACK_HUNGER);
  addClamped(state_.happiness, pet::PetConfig::SNACK_HAPPINESS);
  save();
}

void PetManager::giveMedicine() {
  addClamped(state_.health, pet::PetConfig::MEDICINE_HEALTH);
  if (state_.health > pet::PetConfig::SICK_HEALTH_THRESHOLD) state_.isSick = false;
  save();
}

void PetManager::exercise() {
  addClamped(state_.happiness, pet::PetConfig::EXERCISE_HAPPINESS);
  save();
}

void PetManager::clean() {
  state_.wasteCount = 0;
  addClamped(state_.happiness, pet::PetConfig::CLEAN_HAPPINESS);
  save();
}

void PetManager::petThePet() {
  addClamped(state_.happiness, pet::PetConfig::PET_HAPPINESS);
  save();
}

pet::PetMood PetManager::mood() const { return pet::computeMood(state_); }

uint16_t PetManager::ageDays() const {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (now <= state_.birthTime || state_.birthTime == 0) return 0;
  return static_cast<uint16_t>(std::min<uint32_t>(65535, (now - state_.birthTime) / SECONDS_PER_DAY));
}
