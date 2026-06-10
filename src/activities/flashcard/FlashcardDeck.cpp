// FlashcardDeck.cpp — CSV-backed deck model for inkpoint flashcards.
//
// SCHEDULING FORMAT NOTE:
//   sr_due, sr_interval, sr_ease are stored as plain unsigned integers.
//   This deliberately diverges from crosspet's date-string format ("DD/MM/YYYY")
//   because inkpoint has no RTC on the X4; "today" is a monotonic session counter
//   (see FlashcardSession). Intervals mean "sessions until next review," not days.
//
// CSV CONTRACT:
//   Header:  card_id,front_content,back_content,sr_due,sr_interval,sr_ease
//   Fields:  RFC-4180 double-quote escaping ("" for embedded quote)
//   Newlines inside fields stored as literal token "/n" (not backslash-n)
//   Front hint encoded as:  main text \ hint text \
//   Blank sr_* columns => new card (SrsState{}, dueDate=0)

#include "FlashcardDeck.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FlashcardCsv.h"

// ---------------------------------------------------------------------------
// FlashcardCard helpers
// ---------------------------------------------------------------------------

std::string FlashcardCard::frontMain() const {
  auto pos = frontContent.find('\\');
  if (pos == std::string::npos) return frontContent;
  // trim trailing space before backslash
  size_t end = pos;
  while (end > 0 && frontContent[end - 1] == ' ') --end;
  return frontContent.substr(0, end);
}

std::string FlashcardCard::frontHint() const {
  auto pos1 = frontContent.find('\\');
  if (pos1 == std::string::npos) return {};
  auto pos2 = frontContent.find('\\', pos1 + 1);
  if (pos2 == std::string::npos) return {};
  // trim surrounding spaces
  size_t start = pos1 + 1;
  while (start < pos2 && frontContent[start] == ' ') ++start;
  size_t end = pos2;
  while (end > start && frontContent[end - 1] == ' ') --end;
  return frontContent.substr(start, end - start);
}

// ---------------------------------------------------------------------------
// FlashcardDeck implementation
// ---------------------------------------------------------------------------

bool FlashcardDeck::loadFromCsv(const std::string& path) {
  cards.clear();
  loadedPath = path;

  HalFile file;
  if (!Storage.openFileForRead("FCDECK", path, file)) {
    LOG_ERR("FCDECK", "open failed: %s", path.c_str());
    return false;
  }

  const size_t fsize = file.fileSize();
  if (fsize == 0) {
    // Empty file is valid (no cards).
    return true;
  }
  if (fsize > FLASHCARD_MAX_FILE_BYTES) {
    LOG_ERR("FCDECK", "file too large: %zu bytes (max %zu)", fsize, FLASHCARD_MAX_FILE_BYTES);
    return false;
  }

  auto buf = makeUniqueNoThrow<char[]>(fsize + 1);
  if (!buf) {
    LOG_ERR("FCDECK", "OOM: %zu bytes", fsize + 1);
    return false;
  }

  // Fix 3: read directly from the already-open HalFile (single open).
  file.seek(0);
  int bytesRead = file.read(buf.get(), fsize);
  if (bytesRead <= 0) {
    LOG_ERR("FCDECK", "read failed: %s", path.c_str());
    return false;
  }
  buf[(size_t)bytesRead] = '\0';
  // file auto-closes at scope exit (DESTRUCTOR_CLOSES_FILE=1)

  // Reserve before parse loop.
  cards.reserve(std::min((size_t)MAX_CARDS_PER_DECK, (size_t)(fsize / 64) + 1));

  const char* p = buf.get();
  const char* end = p + (size_t)bytesRead;
  bool firstLine = true;

  // Fix (cheap): declare fields outside the loop to avoid ~3000 heap events per 500-card deck.
  std::string fields[6];

  while (p < end) {
    // Find end of this line
    const char* lineStart = p;
    while (p < end && *p != '\n') ++p;
    size_t lineLen = (size_t)(p - lineStart);
    // Skip \r at end of Windows line endings
    if (lineLen > 0 && lineStart[lineLen - 1] == '\r') --lineLen;
    if (p < end) ++p;  // consume '\n'

    if (lineLen == 0) continue;  // skip blank lines

    // Skip the header row
    if (firstLine) {
      firstLine = false;
      continue;
    }

    if (cards.size() >= MAX_CARDS_PER_DECK) break;

    // Clear fields from previous iteration before reuse
    for (auto& f : fields) f.clear();

    // Parse the CSV row
    size_t nFields = tokenizeCsvLine(lineStart, lineLen, fields, 6);
    if (nFields < 3) continue;  // need at least id, front, back

    FlashcardCard card;
    card.id = (uint16_t)strtoul(fields[0].c_str(), nullptr, 10);

    // Convert /n tokens back to newlines
    card.frontContent = fields[1];
    strReplaceAll(card.frontContent, "/n", "\n");

    card.backContent = fields[2];
    strReplaceAll(card.backContent, "/n", "\n");

    // sr_due, sr_interval, sr_ease — blank means new card
    if (nFields >= 4 && !fields[3].empty()) {
      card.srs.dueDate = (uint32_t)strtoul(fields[3].c_str(), nullptr, 10);
    }
    if (nFields >= 5 && !fields[4].empty()) {
      card.srs.interval = (uint16_t)strtoul(fields[4].c_str(), nullptr, 10);
    }
    if (nFields >= 6 && !fields[5].empty()) {
      card.srs.ease = (uint16_t)strtoul(fields[5].c_str(), nullptr, 10);
    } else if (nFields < 6 || fields[5].empty()) {
      card.srs.ease = 250;  // default ease for new/migrated cards
    }

    cards.push_back(std::move(card));
  }

  return true;
}

bool FlashcardDeck::saveToCsv() const {
  std::string tmpPath = loadedPath + ".tmp";

  {
    // Nested block: write HalFile goes out of scope here, closing it
    // BEFORE Storage.rename() below (DESTRUCTOR_CLOSES_FILE=1).
    HalFile file;
    if (!Storage.openFileForWrite("FCDECK", tmpPath, file)) {
      LOG_ERR("FCDECK", "open tmp failed: %s", tmpPath.c_str());
      return false;
    }

    // Write header
    file.print("card_id,front_content,back_content,sr_due,sr_interval,sr_ease\n");

    for (const auto& card : cards) {
      // Convert newlines to /n tokens for storage
      std::string front = card.frontContent;
      strReplaceAll(front, "\n", "/n");
      std::string back = card.backContent;
      strReplaceAll(back, "\n", "/n");

      char idBuf[8];
      snprintf(idBuf, sizeof(idBuf), "%u", (unsigned)card.id);
      file.print(idBuf);
      file.print(",");
      file.print(csvEscapeField(front).c_str());
      file.print(",");
      file.print(csvEscapeField(back).c_str());
      file.print(",");

      if (card.srs.dueDate != 0 || card.srs.interval != 0) {
        // Reviewed card: write scheduling integers
        char srBuf[32];
        snprintf(srBuf, sizeof(srBuf), "%u,%u,%u", (unsigned)card.srs.dueDate, (unsigned)card.srs.interval,
                 (unsigned)card.srs.ease);
        file.print(srBuf);
      }
      // else: new card — leave sr_* blank

      file.print("\n");
    }
    // HalFile destructor closes the file here (DESTRUCTOR_CLOSES_FILE=1)
  }

  if (!Storage.rename(tmpPath.c_str(), loadedPath.c_str())) {
    LOG_ERR("FCDECK", "rename failed: %s -> %s", tmpPath.c_str(), loadedPath.c_str());
    return false;
  }
  return true;
}

bool FlashcardDeck::importCsv(const std::string& srcPath) {
  if (!Storage.ensureDirectoryExists(FLASHCARD_DIR)) {
    LOG_ERR("FCDECK", "ensureDir failed: %s", FLASHCARD_DIR);
    return false;
  }

  // Derive destination filename from the source stem
  std::string stem = srcPath;
  {
    auto slash = stem.rfind('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    auto dot = stem.rfind('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
  }
  std::string destPath = std::string(FLASHCARD_DIR) + "/" + stem + ".csv";

  // Fix 2: check destPath existence (not tmpPath) — guard unchanged.
  if (Storage.exists(destPath.c_str())) {
    LOG_ERR("FCDECK", "import: dest already exists: %s", destPath.c_str());
    return false;
  }

  // Open source for reading
  HalFile srcFile;
  if (!Storage.openFileForRead("FCDECK", srcPath, srcFile)) {
    LOG_ERR("FCDECK", "import: open src failed: %s", srcPath.c_str());
    return false;
  }
  const size_t fsize = srcFile.fileSize();
  if (fsize > FLASHCARD_MAX_FILE_BYTES) {
    LOG_ERR("FCDECK", "import: file too large: %zu", fsize);
    return false;
  }

  // Read the source CSV into buffer
  auto buf = makeUniqueNoThrow<char[]>(fsize + 1);
  if (!buf) {
    LOG_ERR("FCDECK", "OOM: %zu bytes", fsize + 1);
    return false;
  }

  // Fix 3 (importCsv): read directly from the already-open srcFile (single open).
  srcFile.seek(0);
  int bytesRead = srcFile.read(buf.get(), fsize);
  if (bytesRead <= 0) {
    LOG_ERR("FCDECK", "import: read failed");
    return false;
  }
  buf[(size_t)bytesRead] = '\0';
  // srcFile auto-closes at scope exit (DESTRUCTOR_CLOSES_FILE=1)

  // Fix 2: write to a .tmp path, then rename atomically after block close.
  std::string tmpPath = destPath + ".tmp";

  {
    // Nested block so destFile (HalFile) closes before Storage.rename().
    HalFile destFile;
    if (!Storage.openFileForWrite("FCDECK", tmpPath, destFile)) {
      LOG_ERR("FCDECK", "import: open tmp failed: %s", tmpPath.c_str());
      return false;
    }

    destFile.print("card_id,front_content,back_content,sr_due,sr_interval,sr_ease\n");

    const char* p = buf.get();
    const char* end = p + (size_t)bytesRead;
    bool firstLine = true;

    // Fix 1: cap at MAX_CARDS_PER_DECK during import write.
    size_t cardCount = 0;

    // Fix (cheap): declare fields outside loop to avoid repeated heap allocation.
    std::string fields[6];

    while (p < end) {
      const char* lineStart = p;
      while (p < end && *p != '\n') ++p;
      size_t lineLen = (size_t)(p - lineStart);
      if (lineLen > 0 && lineStart[lineLen - 1] == '\r') --lineLen;
      if (p < end) ++p;

      if (lineLen == 0) continue;
      if (firstLine) {
        firstLine = false;
        continue;
      }

      // Fix 1: enforce cap — stop writing rows once MAX_CARDS_PER_DECK is reached.
      if (cardCount >= MAX_CARDS_PER_DECK) {
        LOG_ERR("FCDECK", "import: capped at %zu cards, remaining rows discarded", MAX_CARDS_PER_DECK);
        break;
      }

      // Clear fields from previous iteration
      for (auto& f : fields) f.clear();

      size_t nFields = tokenizeCsvLine(lineStart, lineLen, fields, 6);
      if (nFields < 3) continue;

      // Write id, front, back with blank sr_*
      // Preserve /n tokens as-is (already stored in /n form in the source)
      char idBuf[8];
      snprintf(idBuf, sizeof(idBuf), "%s", fields[0].c_str());
      destFile.print(idBuf);
      destFile.print(",");
      destFile.print(csvEscapeField(fields[1]).c_str());
      destFile.print(",");
      destFile.print(csvEscapeField(fields[2]).c_str());
      destFile.print(",,,\n");  // blank sr_due, sr_interval, sr_ease

      ++cardCount;
    }
    // destFile destructor closes here (DESTRUCTOR_CLOSES_FILE=1)
  }

  // Fix 2: atomic rename of tmp -> dest after the write block has closed the file.
  if (!Storage.rename(tmpPath.c_str(), destPath.c_str())) {
    LOG_ERR("FCDECK", "import: rename failed: %s -> %s", tmpPath.c_str(), destPath.c_str());
    return false;
  }

  return true;
}

std::vector<DeckListEntry> FlashcardDeck::listDecks() {
  std::vector<DeckListEntry> result;
  result.reserve(8);

  HalFile dir = Storage.open(FLASHCARD_DIR);
  if (!dir || !dir.isDirectory()) {
    return result;
  }

  char nameBuf[128];
  for (HalFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    entry.getName(nameBuf, sizeof(nameBuf));
    std::string name(nameBuf);

    // Skip hidden files and non-.csv files
    if (name.empty() || name[0] == '.') continue;
    if (name.size() < 4) continue;
    // Case-insensitive .csv check
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    if (ext != ".csv") continue;

    DeckListEntry e;
    e.name = name.substr(0, name.size() - 4);  // file stem
    e.path = std::string(FLASHCARD_DIR) + "/" + name;
    result.push_back(std::move(e));
    // entry auto-closes on next iteration or at loop exit (DESTRUCTOR_CLOSES_FILE=1)
  }
  // dir auto-closes here (DESTRUCTOR_CLOSES_FILE=1)

  return result;
}

void FlashcardDeck::migrateLegacyDir() {
  // Rename "/flashcard" -> "/flashcards" once. Skip if the legacy dir is absent
  // or the new dir already exists (avoid clobbering). The whole directory moves,
  // so decks and the .session counter travel with it.
  if (!Storage.exists(FLASHCARD_DIR_LEGACY)) return;
  if (Storage.exists(FLASHCARD_DIR)) {
    LOG_DBG("FCDECK", "both %s and %s exist; leaving legacy dir untouched", FLASHCARD_DIR_LEGACY, FLASHCARD_DIR);
    return;
  }
  if (Storage.rename(FLASHCARD_DIR_LEGACY, FLASHCARD_DIR)) {
    LOG_INF("FCDECK", "migrated %s -> %s", FLASHCARD_DIR_LEGACY, FLASHCARD_DIR);
  } else {
    LOG_ERR("FCDECK", "failed to migrate %s -> %s", FLASHCARD_DIR_LEGACY, FLASHCARD_DIR);
  }
}

DeckStats FlashcardDeck::getStats(uint32_t today) const {
  DeckStats stats;
  stats.totalCount = cards.size();
  for (const auto& card : cards) {
    if (card.isNew()) {
      ++stats.newCount;
    } else if (card.isDue(today)) {
      ++stats.dueCount;
    }
  }
  return stats;
}

std::vector<size_t> FlashcardDeck::buildReviewQueue(uint32_t today, size_t newPerDay, size_t maxReview) const {
  std::vector<size_t> dueIdx;
  std::vector<size_t> newIdx;
  dueIdx.reserve(cards.size());
  newIdx.reserve(cards.size());

  for (size_t i = 0; i < cards.size(); ++i) {
    if (cards[i].isNew()) {
      newIdx.push_back(i);
    } else if (cards[i].isDue(today)) {
      dueIdx.push_back(i);
    }
  }

  // Sort due cards by dueDate ascending (earliest due first)
  std::sort(dueIdx.begin(), dueIdx.end(),
            [this](size_t a, size_t b) { return cards[a].srs.dueDate < cards[b].srs.dueDate; });

  std::vector<size_t> queue;
  queue.reserve(std::min(maxReview, cards.size()));

  for (size_t idx : dueIdx) {
    if (queue.size() >= maxReview) break;
    queue.push_back(idx);
  }

  size_t newAdded = 0;
  for (size_t idx : newIdx) {
    if (queue.size() >= maxReview) break;
    if (newAdded >= newPerDay) break;
    queue.push_back(idx);
    ++newAdded;
  }

  return queue;
}
