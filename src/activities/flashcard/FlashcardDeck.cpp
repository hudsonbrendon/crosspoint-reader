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

// ---------------------------------------------------------------------------
// Pure CSV helpers (no Arduino/HalStorage dependency — safe for host linking
// if extracted, but currently compiled only as part of the firmware unit).
// ---------------------------------------------------------------------------

// Replace all occurrences of `from` with `to` in `s` (in-place).
static void strReplaceAll(std::string& s, const std::string& from, const std::string& to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

// Unescape a RFC-4180 quoted field (with "" for embedded ").
// Input is the raw token between the outer quotes (outer quotes already stripped).
static std::string csvUnquoteField(const char* data, size_t len) {
  std::string out;
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    if (data[i] == '"' && i + 1 < len && data[i + 1] == '"') {
      out += '"';
      ++i;
    } else {
      out += data[i];
    }
  }
  return out;
}

// Escape a field value for RFC-4180 CSV output.
// Quotes the field if it contains a comma, quote, or newline-token.
static std::string csvEscapeField(const std::string& value) {
  bool needsQuote = false;
  for (char c : value) {
    if (c == '"' || c == ',' || c == '\n' || c == '\r') {
      needsQuote = true;
      break;
    }
  }
  // Also quote if field contains the /n newline token
  if (!needsQuote && value.find("/n") != std::string::npos) {
    needsQuote = true;
  }
  if (!needsQuote) return value;

  std::string out;
  out.reserve(value.size() + 4);
  out += '"';
  for (char c : value) {
    if (c == '"') out += '"';  // RFC-4180: double the quote
    out += c;
  }
  out += '"';
  return out;
}

// Tokenize one CSV row from `line` into fields.
// Handles RFC-4180 double-quote escaping. Stops at `\n` or end of string.
// Returns the number of fields found.
static size_t tokenizeCsvLine(const char* line, size_t lineLen, std::string fields[], size_t maxFields) {
  size_t fieldIdx = 0;
  size_t i = 0;
  while (i <= lineLen && fieldIdx < maxFields) {
    if (i == lineLen || line[i] == '\n' || line[i] == '\r') {
      // end of row (empty last field handled by prior loop iteration)
      break;
    }
    if (line[i] == '"') {
      // Quoted field
      ++i;  // skip opening quote
      const char* start = line + i;
      // Find the closing quote (accounting for "" escapes)
      size_t fieldLen = 0;
      size_t j = i;
      while (j < lineLen) {
        if (line[j] == '"') {
          if (j + 1 < lineLen && line[j + 1] == '"') {
            // escaped quote
            fieldLen += 2;
            j += 2;
          } else {
            // closing quote
            ++j;  // skip closing quote
            break;
          }
        } else {
          ++fieldLen;
          ++j;
        }
      }
      fields[fieldIdx++] = csvUnquoteField(start, (size_t)(line + j - 1 - start));
      i = j;
      // skip comma
      if (i < lineLen && line[i] == ',') ++i;
    } else {
      // Unquoted field: ends at comma, \n, or end
      const char* start = line + i;
      size_t len = 0;
      while (i < lineLen && line[i] != ',' && line[i] != '\n' && line[i] != '\r') {
        ++len;
        ++i;
      }
      fields[fieldIdx++] = std::string(start, len);
      if (i < lineLen && line[i] == ',') ++i;
    }
  }
  return fieldIdx;
}

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

  size_t bytesRead =
      Storage.readFileToBuffer(path.c_str(), buf.get(), fsize + 1, FLASHCARD_MAX_FILE_BYTES);
  if (bytesRead == 0) {
    LOG_ERR("FCDECK", "read failed: %s", path.c_str());
    return false;
  }
  buf[bytesRead] = '\0';

  // Reserve before parse loop.
  cards.reserve(std::min((size_t)MAX_CARDS_PER_DECK, (size_t)(fsize / 64) + 1));

  const char* p = buf.get();
  const char* end = p + bytesRead;
  bool firstLine = true;

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

    // Parse the CSV row
    std::string fields[6];
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
        snprintf(srBuf, sizeof(srBuf), "%u,%u,%u", (unsigned)card.srs.dueDate,
                 (unsigned)card.srs.interval, (unsigned)card.srs.ease);
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

  // Read the source CSV
  auto buf = makeUniqueNoThrow<char[]>(fsize + 1);
  if (!buf) {
    LOG_ERR("FCDECK", "OOM: %zu bytes", fsize + 1);
    return false;
  }
  size_t bytesRead =
      Storage.readFileToBuffer(srcPath.c_str(), buf.get(), fsize + 1, FLASHCARD_MAX_FILE_BYTES);
  if (bytesRead == 0) {
    LOG_ERR("FCDECK", "import: read failed");
    return false;
  }
  buf[bytesRead] = '\0';

  // Write to destination with fresh (blank) scheduling state.
  // Parse and rewrite, stripping any existing sr_* values.
  {
    HalFile destFile;
    if (!Storage.openFileForWrite("FCDECK", destPath, destFile)) {
      LOG_ERR("FCDECK", "import: open dest failed: %s", destPath.c_str());
      return false;
    }

    destFile.print("card_id,front_content,back_content,sr_due,sr_interval,sr_ease\n");

    const char* p = buf.get();
    const char* end = p + bytesRead;
    bool firstLine = true;

    while (p < end) {
      const char* lineStart = p;
      while (p < end && *p != '\n') ++p;
      size_t lineLen = (size_t)(p - lineStart);
      if (lineLen > 0 && lineStart[lineLen - 1] == '\r') --lineLen;
      if (p < end) ++p;

      if (lineLen == 0) continue;
      if (firstLine) { firstLine = false; continue; }

      std::string fields[6];
      size_t nFields = tokenizeCsvLine(lineStart, lineLen, fields, 6);
      if (nFields < 3) continue;

      // Write id, front, back with blank sr_*
      std::string front = fields[1];
      std::string back = fields[2];
      // Preserve /n tokens as-is (they were already stored in /n form in the source)

      char idBuf[8];
      snprintf(idBuf, sizeof(idBuf), "%s", fields[0].c_str());
      destFile.print(idBuf);
      destFile.print(",");
      destFile.print(csvEscapeField(front).c_str());
      destFile.print(",");
      destFile.print(csvEscapeField(back).c_str());
      destFile.print(",,,\n");  // blank sr_due, sr_interval, sr_ease
    }
    // destFile destructor closes here
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

std::vector<size_t> FlashcardDeck::buildReviewQueue(uint32_t today, size_t newPerDay,
                                                     size_t maxReview) const {
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
