#include "RssJsonIO.h"

#include <ArduinoJson.h>

#include "RssFeedStore.h"
#include "RssParser.h"

namespace RssJsonIO {

std::string serializeFeeds(const std::vector<RssFeed>& feeds) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& f : feeds) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = f.name;
    o["url"] = f.url;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool deserializeFeeds(const char* json, std::vector<RssFeed>& out) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) return false;
  if (!doc.is<JsonArray>()) return false;
  for (JsonObject o : doc.as<JsonArray>()) {
    RssFeed f;
    f.name = o["name"] | std::string("");
    f.url = o["url"] | std::string("");
    if (!f.url.empty()) out.push_back(std::move(f));
  }
  return true;
}

std::string serializeIndex(const std::vector<RssEntry>& items) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& it : items) {
    JsonObject o = arr.add<JsonObject>();
    o["title"] = it.title;
    o["date"] = it.date;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool deserializeIndex(const char* json, std::vector<RssEntry>& out) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) return false;
  if (!doc.is<JsonArray>()) return false;
  for (JsonObject o : doc.as<JsonArray>()) {
    RssEntry it;
    it.title = o["title"] | std::string("");
    it.date = o["date"] | std::string("");
    out.push_back(std::move(it));
  }
  return true;
}

}  // namespace RssJsonIO
