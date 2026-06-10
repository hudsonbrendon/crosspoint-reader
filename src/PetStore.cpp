#include "PetStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <ctime>

#include "PetJsonIO.h"

namespace PetStore {

bool save(const pet::PetState& s) {
  Storage.ensureDirectoryExists(pet::PetConfig::STATE_DIR);
  pet::PetState copy = s;
  copy.lastUpdateTimestamp = static_cast<uint32_t>(time(nullptr));
  const std::string json = PetJsonIO::serializePetState(copy);
  const bool ok = Storage.writeFile(pet::PetConfig::STATE_PATH, String(json.c_str()));
  if (!ok) {
    LOG_ERR("PET", "Failed to write %s", pet::PetConfig::STATE_PATH);
  }
  return ok;
}

bool load(pet::PetState& out) {
  if (!Storage.exists(pet::PetConfig::STATE_PATH)) return false;
  String json = Storage.readFile(pet::PetConfig::STATE_PATH);
  if (json.isEmpty()) return false;
  if (!PetJsonIO::deserializePetState(json.c_str(), out)) {
    LOG_ERR("PET", "Corrupt state at %s", pet::PetConfig::STATE_PATH);
    return false;
  }
  // No-RTC mitigation: the X4 clock resets each boot. If the saved wall time is
  // ahead of the current clock, push the system clock forward so subsequent
  // time() deltas approximate real elapsed time. WiFi NTP, if it ran, would
  // already be ahead and is left untouched.
  const uint32_t nowSec = static_cast<uint32_t>(time(nullptr));
  if (out.lastUpdateTimestamp > nowSec) {
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(out.lastUpdateTimestamp);
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    LOG_INF("PET", "Restored clock to saved time %lu", (unsigned long)out.lastUpdateTimestamp);
  }
  return true;
}

bool exists() { return Storage.exists(pet::PetConfig::STATE_PATH); }

}  // namespace PetStore
