#pragma once
#include <cstdint>

namespace pet {

// Lifecycle stages, ordered by age.
enum class PetStage : uint8_t { EGG = 0, HATCHLING = 1, YOUNGSTER = 2, COMPANION = 3, ELDER = 4 };

// Coarse mood derived from the three needs.
enum class PetMood : uint8_t { HAPPY = 0, NEUTRAL = 1, SAD = 2, SICK = 3 };

// The need the pet is currently signalling for (drives the attention call).
enum class PetNeed : uint8_t { NONE = 0, HUNGER = 1, HAPPINESS = 2, HEALTH = 3, CLEAN = 4 };

// One evolution gate: minimum age (whole days) AND minimum pages read.
struct EvolutionReq {
  PetStage stage;       // the stage you REACH when both gates are met
  uint16_t minDays;     // days since birth
  uint32_t minPages;    // READING_STATS.totalPagesRead()
};

// Balance constants. constexpr -> compile-time, flash-resident.
struct PetConfig {
  // Persisted file location (HalStorage paths are absolute, SD root = "/").
  static constexpr char STATE_DIR[] = "/.inkpoint/pet";
  static constexpr char STATE_PATH[] = "/.inkpoint/pet/state.json";

  // Decay: points lost per elapsed hour, clamped to [0,100].
  static constexpr uint8_t HUNGER_DECAY_PER_HOUR = 4;
  static constexpr uint8_t HAPPINESS_DECAY_PER_HOUR = 3;
  static constexpr uint8_t HEALTH_DECAY_PER_HOUR = 1;
  // Never apply more than this many hours in a single tick (garbage-epoch guard).
  static constexpr uint32_t MAX_DECAY_HOURS = 720;  // 30 days

  // Action effects (added, clamped to [0,100]).
  static constexpr uint8_t FEED_HUNGER = 30;
  static constexpr uint8_t SNACK_HUNGER = 10;
  static constexpr uint8_t SNACK_HAPPINESS = 5;
  static constexpr uint8_t MEDICINE_HEALTH = 40;
  static constexpr uint8_t EXERCISE_HAPPINESS = 15;
  static constexpr uint8_t CLEAN_HAPPINESS = 10;
  static constexpr uint8_t PET_HAPPINESS = 8;

  // "Meal" derivation: every this many ms of NEW reading time grants one auto-feed.
  static constexpr uint32_t MS_PER_AUTO_MEAL = 1800000;  // 30 min reading == 1 meal
  static constexpr uint8_t AUTO_MEAL_HUNGER = 12;

  // Sickness: when health hits 0, the pet becomes sick.
  static constexpr uint8_t SICK_HEALTH_THRESHOLD = 0;
};

// Evolution gates, evaluated in order; the highest satisfied stage wins.
// EGG->HATCHLING 1d/20p, ->YOUNGSTER 3d/100p, ->COMPANION 7d/500p, ->ELDER 14d/1500p.
inline constexpr EvolutionReq kEvolutionTable[] = {
    {PetStage::HATCHLING, 1, 20},
    {PetStage::YOUNGSTER, 3, 100},
    {PetStage::COMPANION, 7, 500},
    {PetStage::ELDER, 14, 1500},
};
inline constexpr int kEvolutionTableSize = static_cast<int>(sizeof(kEvolutionTable) / sizeof(kEvolutionTable[0]));

// ~70-byte persisted snapshot. Plain-old-data; serialized field-by-field.
struct PetState {
  bool initialized = false;
  PetStage stage = PetStage::EGG;
  char petName[20] = {0};
  uint8_t petType = 0;  // 0..4 (chicken/cat/dog/dragon/bunny)

  uint8_t hunger = 80;     // 0..100 (100 = full)
  uint8_t happiness = 80;  // 0..100
  uint8_t health = 100;    // 0..100

  uint32_t birthTime = 0;            // Unix epoch seconds at hatch
  uint32_t lastTickTime = 0;         // Unix epoch seconds of last decay tick
  uint32_t lastUpdateTimestamp = 0;  // saved time() at last save (clock-restore source)

  uint32_t totalPagesRead = 0;     // mirror of READING_STATS at last sync
  uint32_t lastKnownReadMs = 0;    // READING_STATS.totalReadingMs() at last sync (meal delta source)

  bool isSick = false;
  uint8_t wasteCount = 0;  // increments over time; cleaning resets to 0
  PetNeed currentNeed = PetNeed::NONE;
  bool attentionCall = false;
  bool isSleeping = false;  // persisted but behaviour-dormant (no RTC; see caveat)

  uint8_t evolutionVariant = 0;  // cosmetic variant index (0..2)
};

}  // namespace pet
