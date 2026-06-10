#pragma once

#include "PetState.h"

// Thin SD-persistence layer for PetState, mirroring RssFeedStore. All SD access
// goes through the Storage (HalStorage) singleton.
namespace PetStore {

// Writes `s` (with lastUpdateTimestamp set to now) to PetConfig::STATE_PATH.
bool save(const pet::PetState& s);

// Loads PetState from disk into `out`. Returns false if the file is missing or
// unparseable. On success, restores the system clock forward to the saved
// timestamp (best-effort, no-RTC mitigation; see plan caveat).
bool load(pet::PetState& out);

// True if a saved state file exists on the SD card.
bool exists();

}  // namespace PetStore
