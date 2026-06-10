#pragma once
// FlashcardCsv.h — Pure CSV helpers for the inkpoint flashcard deck format.
//
// No Arduino/HalStorage/Logging dependency — safe for host-side unit tests.
//
// CSV CONTRACT:
//   RFC-4180 double-quote escaping ("" for embedded quote).
//   Newlines inside fields stored as literal token "/n" (not backslash-n).
//   Front hint encoded as:  main text [backslash] hint text [backslash]

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Replace all occurrences of `from` with `to` in `s` (in-place).
void strReplaceAll(std::string& s, const std::string& from, const std::string& to);

// Unescape a RFC-4180 quoted field (outer quotes already stripped).
// `data` points to the raw content between the outer quotes; `len` is its byte length.
std::string csvUnquoteField(const char* data, size_t len);

// Escape a field value for RFC-4180 CSV output.
// Quotes the field if it contains a comma, double-quote, newline, or the "/n" token.
std::string csvEscapeField(const std::string& value);

// Tokenize one CSV row from `line` (length `lineLen`) into `fields[0..maxFields-1]`.
// Handles RFC-4180 double-quote escaping. Stops at '\n' or end of data.
// Returns the number of fields found.
size_t tokenizeCsvLine(const char* line, size_t lineLen, std::string fields[], size_t maxFields);
