#pragma once
#include <cstdint>
#include <string>

// Scheduling state stored per card. Persisted in the deck CSV.
struct SrsState {
  uint16_t interval = 0;   // sessions until next review; 0 == new card
  uint16_t ease = 250;     // ease factor * 100; clamped to [130, 400]
  uint32_t dueDate = 0;    // session counter on which the card is next due; 0 == new
};

enum class SrsRating : uint8_t { Again = 0, Hard = 1, Good = 2, Easy = 3 };

namespace FlashcardSrs {
inline constexpr uint16_t EASE_MIN = 130;
inline constexpr uint16_t EASE_MAX = 400;

// Clamp an ease value into [EASE_MIN, EASE_MAX].
uint16_t clampEase(int ease);

// Pure SM-2 transition. `today` is the current session counter (see FlashcardSession).
// Returns the next scheduling state. Never calls time().
SrsState review(const SrsState& current, SrsRating rating, uint32_t today);

// What interval (in sessions) `rating` would produce, without mutating state.
// Used for the "+N" hints on the review buttons.
uint16_t previewInterval(const SrsState& current, SrsRating rating);
}  // namespace FlashcardSrs
