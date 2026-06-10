// FlashcardCsv.cpp — Pure CSV helpers for the inkpoint flashcard deck format.
// No Arduino/HalStorage/Logging dependency.

#include "FlashcardCsv.h"

void strReplaceAll(std::string& s, const std::string& from, const std::string& to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::string csvUnquoteField(const char* data, size_t len) {
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

std::string csvEscapeField(const std::string& value) {
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

size_t tokenizeCsvLine(const char* line, size_t lineLen, std::string fields[], size_t maxFields) {
  size_t fieldIdx = 0;
  size_t i = 0;
  while (i <= lineLen && fieldIdx < maxFields) {
    if (i == lineLen || line[i] == '\n' || line[i] == '\r') {
      // end of row
      break;
    }
    if (line[i] == '"') {
      // Quoted field
      ++i;  // skip opening quote
      const char* start = line + i;
      // Find the closing quote (accounting for "" escapes)
      size_t j = i;
      while (j < lineLen) {
        if (line[j] == '"') {
          if (j + 1 < lineLen && line[j + 1] == '"') {
            // escaped quote — skip both characters
            j += 2;
          } else {
            // closing quote
            ++j;  // skip closing quote
            break;
          }
        } else {
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
