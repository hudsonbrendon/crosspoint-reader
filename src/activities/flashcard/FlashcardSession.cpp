#include "FlashcardSession.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstdlib>

#include "FlashcardDeck.h"  // for FLASHCARD_DIR

FlashcardSession FlashcardSession::instance;
static constexpr const char* SESSION_PATH = "/flashcard/.session";

void FlashcardSession::load() {
  if (loaded) return;
  loaded = true;
  counter = 0;
  if (!Storage.exists(SESSION_PATH)) return;
  char buf[16] = {0};
  size_t n = Storage.readFileToBuffer(SESSION_PATH, buf, sizeof(buf));
  if (n > 0) counter = (uint32_t)strtoul(buf, nullptr, 10);
}

void FlashcardSession::advance() {
  load();
  counter += 1;
  Storage.ensureDirectoryExists(FLASHCARD_DIR);
  char buf[16];
  int len = snprintf(buf, sizeof(buf), "%u", (unsigned)counter);
  if (len < 0) {
    LOG_ERR("FCSESSION", "format failed");
    return;
  }
  if (!Storage.writeFile(SESSION_PATH, String(buf))) {
    LOG_ERR("FCSESSION", "persist failed");
  }
}
