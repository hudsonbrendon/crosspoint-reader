#include "HtmlToText.h"

#include <cctype>
#include <cstring>

namespace {
// Append a Unicode code point as UTF-8.
void appendUtf8(std::string& out, unsigned cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// Decode one entity starting at html[i] (i points at '&'); advance i past ';'.
// Appends decoded text to out. If not a valid entity, emits '&' literally.
void decodeEntity(const std::string& html, size_t& i, std::string& out) {
  size_t semi = html.find(';', i);
  if (semi == std::string::npos || semi - i > 10) {
    out.push_back('&');
    ++i;
    return;
  }
  std::string name = html.substr(i + 1, semi - i - 1);
  unsigned cp = 0;
  if (!name.empty() && name[0] == '#') {
    cp = (name.size() > 1 && (name[1] == 'x' || name[1] == 'X'))
             ? static_cast<unsigned>(strtoul(name.c_str() + 2, nullptr, 16))
             : static_cast<unsigned>(strtoul(name.c_str() + 1, nullptr, 10));
  } else if (name == "amp") {
    cp = '&';
  } else if (name == "lt") {
    cp = '<';
  } else if (name == "gt") {
    cp = '>';
  } else if (name == "quot") {
    cp = '"';
  } else if (name == "apos") {
    cp = '\'';
  } else if (name == "nbsp") {
    cp = ' ';
  } else if (name == "mdash") {
    cp = 0x2014;
  } else if (name == "ndash") {
    cp = 0x2013;
  } else if (name == "hellip") {
    cp = 0x2026;
  } else if (name == "rsquo" || name == "lsquo") {
    cp = (name[0] == 'r') ? 0x2019 : 0x2018;
  } else if (name == "rdquo" || name == "ldquo") {
    cp = (name[0] == 'r') ? 0x201D : 0x201C;
  } else {
    out.push_back('&');  // unknown entity: keep literal
    ++i;
    return;
  }
  appendUtf8(out, cp);
  i = semi + 1;
}

bool tagIs(const std::string& tag, const char* nameLower) {
  // tag is the inner text of <...>; compare its leading element name (case-insensitive),
  // ignoring a leading '/'.
  size_t s = (!tag.empty() && tag[0] == '/') ? 1 : 0;
  size_t n = strlen(nameLower);
  for (size_t k = 0; k < n; ++k) {
    if (s + k >= tag.size()) return false;
    if (std::tolower(static_cast<unsigned char>(tag[s + k])) != nameLower[k]) return false;
  }
  char after = (s + n < tag.size()) ? tag[s + n] : '\0';
  return after == ' ' || after == '/' || after == '\0' || (s + n == tag.size());
}
}  // namespace

// Internal sentinels for newlines injected by tag processing (not raw text).
// These are not valid UTF-8 continuation bytes and won't appear in the source.
static constexpr char kSingleNewline = '\x01';  // <br>
static constexpr char kDoubleNewline = '\x02';  // </p>, </div>, etc.

std::string htmlToText(const std::string& html) {
  std::string raw;
  raw.reserve(html.size());
  size_t i = 0;
  while (i < html.size()) {
    char c = html[i];
    if (c == '<') {
      size_t close = html.find('>', i);
      if (close == std::string::npos) break;
      std::string tag = html.substr(i + 1, close - i - 1);
      if (tagIs(tag, "script") || tagIs(tag, "style")) {
        // Skip to the matching close tag.
        size_t endPos = std::string::npos;
        size_t searchFrom = close + 1;
        bool isScript = tagIs(tag, "script");
        while (searchFrom < html.size()) {
          size_t candidate = html.find('<', searchFrom);
          if (candidate == std::string::npos) break;
          size_t candidateClose = html.find('>', candidate);
          if (candidateClose == std::string::npos) break;
          std::string candidateTag = html.substr(candidate + 1, candidateClose - candidate - 1);
          if (!candidateTag.empty() && candidateTag[0] == '/') {
            if (isScript && tagIs(candidateTag, "script")) {
              endPos = candidateClose + 1;
              break;
            }
            if (!isScript && tagIs(candidateTag, "style")) {
              endPos = candidateClose + 1;
              break;
            }
          }
          searchFrom = candidateClose + 1;
        }
        i = (endPos == std::string::npos) ? html.size() : endPos;
        continue;
      }
      if (tagIs(tag, "br")) {
        raw.push_back(kSingleNewline);
      } else if (tag[0] == '/' && (tagIs(tag, "p") || tagIs(tag, "div") || tagIs(tag, "li") ||
                                   tagIs(tag, "h1") || tagIs(tag, "h2") || tagIs(tag, "h3") ||
                                   tagIs(tag, "h4") || tagIs(tag, "h5") || tagIs(tag, "h6"))) {
        raw.push_back(kDoubleNewline);
      }
      i = close + 1;
    } else if (c == '&') {
      decodeEntity(html, i, raw);
    } else {
      raw.push_back(c);
      ++i;
    }
  }

  // Collapse whitespace:
  //  - kDoubleNewline sentinels -> "\n\n" (from </p>, etc.)
  //  - kSingleNewline sentinels -> "\n"  (from <br>)
  //  - Raw whitespace (space, tab, \r, \n from original text) -> single space
  // Leading/trailing whitespace is trimmed.
  std::string out;
  out.reserve(raw.size());
  int pendingNewlines = 0;  // 1 = single, 2 = double (from sentinels)
  bool pendingSpace = false;
  bool started = false;
  for (char ch : raw) {
    if (ch == kDoubleNewline) {
      if (pendingNewlines < 2) pendingNewlines = 2;
      pendingSpace = false;
    } else if (ch == kSingleNewline) {
      if (pendingNewlines < 1) pendingNewlines = 1;
      pendingSpace = false;
    } else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
      // Raw whitespace (including literal newlines from source text) -> space
      if (pendingNewlines == 0) pendingSpace = true;
    } else {
      if (started) {
        if (pendingNewlines >= 2)
          out.append("\n\n");
        else if (pendingNewlines == 1)
          out.push_back('\n');
        else if (pendingSpace)
          out.push_back(' ');
      }
      pendingNewlines = 0;
      pendingSpace = false;
      out.push_back(ch);
      started = true;
    }
  }
  return out;
}
