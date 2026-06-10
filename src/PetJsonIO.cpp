#include "PetJsonIO.h"

#include <ArduinoJson.h>

#include <cstring>

#include "PetState.h"

namespace PetJsonIO {

using pet::PetNeed;
using pet::PetStage;
using pet::PetState;

std::string serializePetState(const PetState& s) {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  o["initialized"] = s.initialized;
  o["stage"] = static_cast<uint8_t>(s.stage);
  o["petName"] = s.petName;  // null-terminated char[20]
  o["petType"] = s.petType;
  o["hunger"] = s.hunger;
  o["happiness"] = s.happiness;
  o["health"] = s.health;
  o["birthTime"] = s.birthTime;
  o["lastTickTime"] = s.lastTickTime;
  o["lastUpdateTimestamp"] = s.lastUpdateTimestamp;
  o["totalPagesRead"] = s.totalPagesRead;
  o["lastKnownReadMs"] = s.lastKnownReadMs;
  o["isSick"] = s.isSick;
  o["wasteCount"] = s.wasteCount;
  o["currentNeed"] = static_cast<uint8_t>(s.currentNeed);
  o["attentionCall"] = s.attentionCall;
  o["isSleeping"] = s.isSleeping;
  o["evolutionVariant"] = s.evolutionVariant;
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool deserializePetState(const char* json, PetState& out) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) return false;
  if (!doc.is<JsonObject>()) return false;
  JsonObject o = doc.as<JsonObject>();

  out.initialized = o["initialized"] | false;
  out.stage = static_cast<PetStage>(o["stage"] | static_cast<uint8_t>(0));
  {
    std::string name = o["petName"] | std::string("");
    std::strncpy(out.petName, name.c_str(), sizeof(out.petName) - 1);
    out.petName[sizeof(out.petName) - 1] = '\0';
  }
  out.petType = o["petType"] | static_cast<uint8_t>(0);
  out.hunger = o["hunger"] | static_cast<uint8_t>(80);
  out.happiness = o["happiness"] | static_cast<uint8_t>(80);
  out.health = o["health"] | static_cast<uint8_t>(100);
  out.birthTime = o["birthTime"] | static_cast<uint32_t>(0);
  out.lastTickTime = o["lastTickTime"] | static_cast<uint32_t>(0);
  out.lastUpdateTimestamp = o["lastUpdateTimestamp"] | static_cast<uint32_t>(0);
  out.totalPagesRead = o["totalPagesRead"] | static_cast<uint32_t>(0);
  out.lastKnownReadMs = o["lastKnownReadMs"] | static_cast<uint32_t>(0);
  out.isSick = o["isSick"] | false;
  out.wasteCount = o["wasteCount"] | static_cast<uint8_t>(0);
  out.currentNeed = static_cast<PetNeed>(o["currentNeed"] | static_cast<uint8_t>(0));
  out.attentionCall = o["attentionCall"] | false;
  out.isSleeping = o["isSleeping"] | false;
  out.evolutionVariant = o["evolutionVariant"] | static_cast<uint8_t>(0);
  return true;
}

}  // namespace PetJsonIO
