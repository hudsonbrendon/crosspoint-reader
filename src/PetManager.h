#pragma once

#include "PetState.h"

// Owns the single PetState and all mutations. Access via PET_MANAGER.
class PetManager {
 public:
  static PetManager& getInstance() {
    static PetManager instance;
    return instance;
  }

  // Loads state from SD (or creates a fresh egg if none/corrupt), then ticks
  // decay forward to "now" and syncs reading-driven growth. Call on screen open.
  void begin();

  // Applies decay for the hours elapsed since lastTickTime (persisted epoch).
  void tick();

  // Drives growth from the reader: pages -> evolution gates; reading-time delta
  // -> auto "meals" (hunger). Pulls from READING_STATS. Safe to call repeatedly.
  void syncFromReadingStats();

  // Persists current state to SD.
  bool save();

  // --- User actions (each clamps to [0,100] and persists) ---
  void feed();
  void snack();
  void giveMedicine();
  void exercise();
  void clean();
  void petThePet();  // "pet" verb (avoid clashing with namespace)

  // --- Read-only access for the UI ---
  const pet::PetState& state() const { return state_; }
  pet::PetMood mood() const;
  uint16_t ageDays() const;

 private:
  PetManager() = default;
  void addClamped(uint8_t& field, uint8_t amount);
  void hatchIfEgg();

  pet::PetState state_{};
  bool loaded_ = false;
};

#define PET_MANAGER PetManager::getInstance()
