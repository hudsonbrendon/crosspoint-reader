#pragma once
#include <string>

namespace pet {
struct PetState;
}

namespace PetJsonIO {
// Serialize a PetState to a single JSON object string.
std::string serializePetState(const pet::PetState& s);
// Parse a JSON object string into `out`. Returns false on parse error.
bool deserializePetState(const char* json, pet::PetState& out);
}  // namespace PetJsonIO
