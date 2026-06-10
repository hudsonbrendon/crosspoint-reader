#pragma once
#include <cstdint>

// Monotonic session counter persisted at /flashcard/.session on SD. Stands in for
// "today" because the X4 has no RTC. Incremented once per app boot (see main.cpp wiring).
class FlashcardSession {
 public:
  static FlashcardSession& getInstance() { return instance; }
  // Load the counter from SD (0 if absent). Idempotent.
  void load();
  // Increment and persist. Call exactly once per boot, before any flashcard activity.
  void advance();
  // Current session counter ("today").
  uint32_t today() const { return counter; }

 private:
  static FlashcardSession instance;
  uint32_t counter = 0;
  bool loaded = false;
};

#define FLASHCARD_SESSION FlashcardSession::getInstance()
