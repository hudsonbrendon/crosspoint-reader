#include "ReadingStats.h"

#include <algorithm>

namespace reading_stats {

namespace {
uint32_t saturatingAddU32(uint32_t a, uint32_t b) {
  const uint64_t sum = static_cast<uint64_t>(a) + b;
  return sum > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(sum);
}

// A strictly increasing ordinal across years for window comparisons.
int64_t dayOrdinal(int16_t year, uint16_t dayOfYear) {
  return static_cast<int64_t>(year) * 366 + dayOfYear;
}
}  // namespace

void ReadingStatsAggregator::load(std::vector<BookStats> books) {
  books_ = std::move(books);
  sessionActive_ = false;
  activeIndex_.reset();
  lastEventMs_ = 0;
}

uint32_t ReadingStatsAggregator::cappedDelta(uint32_t nowMs) const {
  // millis() wraps roughly every 49.7 days; treat a backwards jump as no time.
  if (nowMs < lastEventMs_) return 0;
  const uint32_t delta = nowMs - lastEventMs_;
  return std::min(delta, kMaxPageMs);
}

void ReadingStatsAggregator::beginSession(const std::string& bookPath, uint32_t nowMs) {
  if (sessionActive_) endSession(nowMs);

  auto it = std::find_if(books_.begin(), books_.end(), [&](const BookStats& b) { return b.bookPath == bookPath; });
  if (it == books_.end()) {
    BookStats fresh;
    fresh.bookPath = bookPath;
    books_.push_back(std::move(fresh));
    activeIndex_ = books_.size() - 1;
  } else {
    activeIndex_ = static_cast<std::size_t>(std::distance(books_.begin(), it));
  }
  sessionActive_ = true;
  lastEventMs_ = nowMs;
}

void ReadingStatsAggregator::recordPageTurn(uint32_t nowMs, bool forward) {
  if (!sessionActive_) return;
  BookStats& book = books_[*activeIndex_];
  book.totalReadingMs = saturatingAddU32(book.totalReadingMs, cappedDelta(nowMs));
  if (forward) book.pagesRead++;
  lastEventMs_ = nowMs;
}

void ReadingStatsAggregator::endSession(uint32_t nowMs) {
  if (!sessionActive_) return;
  BookStats& book = books_[*activeIndex_];
  book.totalReadingMs = saturatingAddU32(book.totalReadingMs, cappedDelta(nowMs));
  book.sessionCount++;
  sessionActive_ = false;
  activeIndex_.reset();
}

const BookStats* ReadingStatsAggregator::statsFor(const std::string& bookPath) const {
  auto it = std::find_if(books_.begin(), books_.end(), [&](const BookStats& b) { return b.bookPath == bookPath; });
  return it == books_.end() ? nullptr : &*it;
}

uint32_t ReadingStatsAggregator::totalPagesRead() const {
  uint64_t total = 0;
  for (const auto& b : books_) total += b.pagesRead;
  return total > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(total);
}

uint32_t ReadingStatsAggregator::totalReadingMs() const {
  uint64_t total = 0;
  for (const auto& b : books_) total += b.totalReadingMs;
  return total > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(total);
}

uint32_t ReadingStatsAggregator::pagesPerHour(const std::string& bookPath) const {
  const BookStats* s = statsFor(bookPath);
  if (!s || s->totalReadingMs == 0) return 0;
  return static_cast<uint32_t>(static_cast<uint64_t>(s->pagesRead) * 3600000ULL / s->totalReadingMs);
}

void updateStreak(int16_t curYear, int16_t curDay, int16_t& lastYear, int16_t& lastDay, uint16_t& current,
                  uint16_t& longest) {
  // First ever valid day, or no prior date recorded.
  if (lastYear < 0 || lastDay < 0) {
    current = 1;
  } else if (curYear == lastYear && curDay == lastDay) {
    // Same calendar day: already counted; nothing to do but keep lastYear/lastDay.
    return;
  } else {
    const bool sameYearNext = (curYear == lastYear) && (curDay == lastDay + 1);
    const bool yearRollover = (curYear == lastYear + 1) && (curDay == 0) && (lastDay == 364 || lastDay == 365);
    if (sameYearNext || yearRollover) {
      if (current < UINT16_MAX) current++;
    } else {
      current = 1;
    }
  }
  if (current > longest) longest = current;
  lastYear = curYear;
  lastDay = curDay;
}

void ReadingStatsAggregator::recordReadingMs(int16_t year, uint16_t dayOfYear, uint32_t ms) {
  if (ms == 0) return;
  for (auto& d : days_) {
    if (d.year == year && d.dayOfYear == dayOfYear) { d.ms += ms; return; }
  }
  days_.push_back(DayBucket{year, dayOfYear, ms});
}

uint32_t ReadingStatsAggregator::msForDay(int16_t year, uint16_t dayOfYear) const {
  for (const auto& d : days_) if (d.year == year && d.dayOfYear == dayOfYear) return d.ms;
  return 0;
}

uint32_t ReadingStatsAggregator::bestDayMs() const {
  uint32_t best = 0;
  for (const auto& d : days_) best = (d.ms > best) ? d.ms : best;
  return best;
}

uint32_t ReadingStatsAggregator::annualMs(int16_t year) const {
  uint32_t sum = 0;
  for (const auto& d : days_) if (d.year == year) sum += d.ms;
  return sum;
}

uint32_t ReadingStatsAggregator::windowMs(int16_t year, uint16_t dayOfYear, uint16_t count) const {
  const int64_t end = dayOrdinal(year, dayOfYear);
  const int64_t start = end - (static_cast<int64_t>(count) - 1);
  uint32_t sum = 0;
  for (const auto& d : days_) {
    const int64_t o = dayOrdinal(d.year, d.dayOfYear);
    if (o >= start && o <= end) sum += d.ms;
  }
  return sum;
}

GoalStreak ReadingStatsAggregator::computeGoalStreak(int16_t todayYear, uint16_t todayDayOfYear,
                                                     uint32_t goalMs) const {
  if (goalMs == 0) return {};
  std::vector<int64_t> met;
  met.reserve(days_.size());
  for (const auto& d : days_)
    if (d.ms >= goalMs) met.push_back(dayOrdinal(d.year, d.dayOfYear));
  std::sort(met.begin(), met.end());

  GoalStreak gs;
  uint16_t run = 0;
  int64_t prev = INT64_MIN;
  for (int64_t o : met) {
    run = (o == prev + 1) ? static_cast<uint16_t>(run + 1) : 1;
    if (run > gs.max) gs.max = run;
    prev = o;
  }
  const int64_t today = dayOrdinal(todayYear, todayDayOfYear);
  int64_t cur = today;
  while (std::binary_search(met.begin(), met.end(), cur)) {
    gs.current++;
    cur -= 1;
  }
  return gs;
}

}  // namespace reading_stats
