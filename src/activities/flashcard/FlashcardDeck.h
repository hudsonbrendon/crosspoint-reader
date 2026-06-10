#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "FlashcardSrs.h"

inline constexpr const char* FLASHCARD_DIR = "/flashcards";
// Pre-1.7.1 decks lived here (singular). migrateLegacyDir() renames it on boot.
inline constexpr const char* FLASHCARD_DIR_LEGACY = "/flashcard";
inline constexpr size_t MAX_CARDS_PER_DECK = 500;
inline constexpr size_t FLASHCARD_MAX_FILE_BYTES = 256 * 1024;  // local cap; bypasses 50KB readFile cap

struct FlashcardCard {
  uint16_t id = 0;
  std::string frontContent;  // may contain ` \ hint \` markers and "/n" newline tokens
  std::string backContent;
  SrsState srs;

  // Split frontContent on '\' markers: text before first '\' is the main face.
  std::string frontMain() const;
  // Text between the two '\' markers (empty if no hint).
  std::string frontHint() const;
  bool isNew() const { return srs.dueDate == 0; }
  bool isDue(uint32_t today) const { return !isNew() && srs.dueDate <= today; }
};

struct DeckStats {
  size_t newCount = 0;
  size_t dueCount = 0;
  size_t totalCount = 0;
};

struct DeckListEntry {
  std::string name;  // file stem, e.g. "spanish" for /flashcard/spanish.csv
  std::string path;  // full path
};

class FlashcardDeck {
 public:
  // Load all cards from a CSV at `path`. Returns false on I/O/parse error.
  bool loadFromCsv(const std::string& path);
  // Atomically save (stream to .tmp then rename). Returns false on error.
  bool saveToCsv() const;
  // Import a CSV from `srcPath` into FLASHCARD_DIR (copies, assigns fresh blank schedule). Returns false on error.
  static bool importCsv(const std::string& srcPath);
  // Enumerate decks in FLASHCARD_DIR (*.csv).
  static std::vector<DeckListEntry> listDecks();
  // One-time boot migration: rename the legacy "/flashcard" dir to "/flashcards"
  // if the old one exists and the new one does not. Safe to call every boot.
  static void migrateLegacyDir();

  DeckStats getStats(uint32_t today) const;
  // Build the review queue for this session: due cards first, then up to newPerDay
  // new cards, total capped at maxReview. Returns indices into `cards`.
  std::vector<size_t> buildReviewQueue(uint32_t today, size_t newPerDay, size_t maxReview) const;
  // Apply a reviewed state back to card[index].
  void updateCard(size_t index, const SrsState& next) {
    if (index >= cards.size()) return;
    cards[index].srs = next;
  }

  const std::string& path() const { return loadedPath; }
  std::vector<FlashcardCard>& mutableCards() { return cards; }
  const std::vector<FlashcardCard>& getCards() const { return cards; }

 private:
  std::string loadedPath;
  std::vector<FlashcardCard> cards;
};
