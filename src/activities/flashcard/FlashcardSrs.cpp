#include "FlashcardSrs.h"

#include <algorithm>

namespace FlashcardSrs {

uint16_t clampEase(int ease) {
  if (ease < (int)EASE_MIN) return EASE_MIN;
  if (ease > (int)EASE_MAX) return EASE_MAX;
  return (uint16_t)ease;
}

SrsState review(const SrsState& current, SrsRating rating, uint32_t today) {
  SrsState next = current;
  const bool isNew = (current.interval == 0);
  switch (rating) {
    case SrsRating::Again:
      next.interval = 0;
      next.ease = clampEase((int)current.ease - 20);
      next.dueDate = today;
      break;
    case SrsRating::Hard:
      next.interval = isNew ? 1 : std::max((uint32_t)1, (uint32_t)(current.interval * 1.2f));
      next.ease = clampEase((int)current.ease - 15);
      next.dueDate = today + next.interval;
      break;
    case SrsRating::Good:
      next.interval = isNew ? 1 : std::max((uint32_t)1, (uint32_t)(current.interval * current.ease / 100.0f));
      next.dueDate = today + next.interval;
      // ease unchanged
      break;
    case SrsRating::Easy:
      next.interval = isNew ? 4 : std::max((uint32_t)1, (uint32_t)(current.interval * current.ease / 100.0f * 1.3f));
      next.ease = clampEase((int)current.ease + 15);
      next.dueDate = today + next.interval;
      break;
  }
  return next;
}

uint16_t previewInterval(const SrsState& current, SrsRating rating) { return review(current, rating, 0).interval; }

}  // namespace FlashcardSrs
