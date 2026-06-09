#include "RssParser.h"

#include <algorithm>
#include <cstring>

RssParser::RssParser() {
  parser = XML_ParserCreate(nullptr);
  if (parser) {
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetCharacterDataHandler(parser, characterData);
  } else {
    errorOccured = true;
  }
}

RssParser::~RssParser() {
  if (parser) XML_ParserFree(parser);
}

void RssParser::clear() {
  entries.clear();
  current = RssEntry{};
  descriptionHtml.clear();
  text.clear();
  inItem = inTitle = inLink = inDate = inDescription = inContentEncoded = false;
}

size_t RssParser::write(uint8_t b) { return write(&b, 1); }

size_t RssParser::write(const uint8_t* data, size_t len) {
  if (errorOccured || !parser) return 0;
  if (XML_Parse(parser, reinterpret_cast<const char*>(data), static_cast<int>(len), XML_FALSE) == XML_STATUS_ERROR) {
    errorOccured = true;
    return 0;
  }
  return len;
}

void RssParser::flush() {
  if (errorOccured || !parser) return;
  if (XML_Parse(parser, nullptr, 0, XML_TRUE) == XML_STATUS_ERROR) errorOccured = true;
}

const char* RssParser::localName(const XML_Char* name) {
  const char* colon = strrchr(name, ':');
  return colon ? colon + 1 : name;
}

const char* RssParser::findAttribute(const XML_Char** atts, const char* name) {
  for (int i = 0; atts && atts[i]; i += 2) {
    if (strcmp(localName(atts[i]), name) == 0) return atts[i + 1];
  }
  return nullptr;
}

void RssParser::startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  auto* self = static_cast<RssParser*>(userData);
  const char* tag = localName(name);

  if (!self->inItem) {
    if (strcmp(tag, "item") == 0 || strcmp(tag, "entry") == 0) {
      self->inItem = true;
      self->current = RssEntry{};
      self->descriptionHtml.clear();
    }
    return;
  }

  // Inside an item/entry:
  if (strcmp(tag, "title") == 0) {
    self->inTitle = true;
    self->text.clear();
  } else if (strcmp(tag, "link") == 0) {
    // Atom: <link rel="alternate" href="..."/> (no text). RSS: <link>text</link>.
    const char* href = findAttribute(atts, "href");
    if (href) {
      const char* rel = findAttribute(atts, "rel");
      if (self->current.link.empty() || (rel && strcmp(rel, "alternate") == 0)) self->current.link = href;
    } else {
      self->inLink = true;
      self->text.clear();
    }
  } else if (strcmp(tag, "pubDate") == 0 || strcmp(tag, "updated") == 0 || strcmp(tag, "published") == 0) {
    if (self->current.date.empty()) {
      self->inDate = true;
      self->text.clear();
    }
  } else if (strcmp(tag, "encoded") == 0 || strcmp(tag, "content") == 0) {
    // content:encoded (RSS) or atom <content>
    self->inContentEncoded = true;
    self->text.clear();
  } else if (strcmp(tag, "description") == 0 || strcmp(tag, "summary") == 0) {
    self->inDescription = true;
    self->text.clear();
  }
}

void RssParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<RssParser*>(userData);
  const char* tag = localName(name);

  if (strcmp(tag, "item") == 0 || strcmp(tag, "entry") == 0) {
    // content:encoded/content wins; else description/summary.
    if (self->current.contentHtml.empty()) self->current.contentHtml = self->descriptionHtml;
    if (self->itemCallback) {
      // Streaming: hand the item off (caller persists it) and free its content
      // immediately, so we never hold more than one item's content in RAM.
      self->itemCallback(self->current);
    } else {
      self->entries.push_back(std::move(self->current));
    }
    self->current = RssEntry{};
    self->descriptionHtml.clear();
    self->text.clear();
    self->text.shrink_to_fit();
    self->inItem = false;
    return;
  }
  if (!self->inItem) return;

  if (self->inTitle && strcmp(tag, "title") == 0) {
    self->current.title = self->text;
    self->inTitle = false;
  } else if (self->inLink && strcmp(tag, "link") == 0) {
    if (self->current.link.empty()) self->current.link = self->text;
    self->inLink = false;
  } else if (self->inDate && (strcmp(tag, "pubDate") == 0 || strcmp(tag, "updated") == 0 || strcmp(tag, "published") == 0)) {
    self->current.date = self->text;
    self->inDate = false;
  } else if (self->inContentEncoded && (strcmp(tag, "encoded") == 0 || strcmp(tag, "content") == 0)) {
    self->current.contentHtml = self->text;  // preferred
    self->inContentEncoded = false;
  } else if (self->inDescription && (strcmp(tag, "description") == 0 || strcmp(tag, "summary") == 0)) {
    self->descriptionHtml = self->text;  // fallback
    self->inDescription = false;
  }
}

void RssParser::characterData(void* userData, const XML_Char* s, int len) {
  auto* self = static_cast<RssParser*>(userData);
  if (self->inTitle || self->inLink || self->inDate || self->inContentEncoded || self->inDescription) {
    // Cap accumulation so a single huge field (full-text article) can't OOM the heap.
    if (self->text.size() >= MAX_FIELD_BYTES) return;
    const size_t room = MAX_FIELD_BYTES - self->text.size();
    self->text.append(s, std::min(static_cast<size_t>(len), room));
  }
}
