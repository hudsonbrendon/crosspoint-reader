# File Browser Sorting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a global, persisted file-browser sort mode (Name / Date modified / File size) controlled from Settings → Display, working identically on X3 and X4.

**Architecture:** A new `fileSortMode` enum setting drives sorting inside `FileBrowserActivity::loadFiles()` only. Entries are collected into a temporary struct carrying name + size + FAT modify-time, sorted by the active mode (directories always first and name-sorted), then the ordered names are written back into the existing `std::vector<std::string> files` — so the ~24 downstream consumers are untouched. A small `HalFile::modifiedKey()` accessor exposes the FAT modify timestamp, and the natural-order comparator is factored out of `FsHelpers::sortFileList` into a reusable `FsHelpers::naturalLess`.

**Tech Stack:** C++20, PlatformIO/ESP-IDF, SdFat (`FsFile::getModifyDateTime`), existing `SettingInfo::Enum` settings system, i18n YAML + `scripts/gen_i18n.py`.

**Spec:** `docs/superpowers/specs/2026-06-08-file-browser-sorting-design.md`

---

## ⚠️ Build / test environment note (read once)

This is firmware with **no host unit-test harness** — the verification gate for compilation is `pio run`. The **local PlatformIO build is currently broken by a pre-existing environment issue** unrelated to this feature: `ModuleNotFoundError: No module named 'littlefs'` (Homebrew Python 3.14 / pyexpat ABI on macOS 26; reproduces on a clean checkout). The authoritative compile gate is **CI** (`.github/workflows/ci.yml`, which installs PlatformIO via `uv pip install --system` on Linux).

Therefore per-task verification below uses **targeted inspection/grep** plus a single best-effort `pio run` at the end (Task 6). If `pio run` still fails only with the `littlefs`/`expat` error, that is the known environment problem — record it and rely on CI. If it fails with a compiler error mentioning the files you changed, that is a real failure to fix. Do NOT fake a successful build.

Per-task: each change is shown in full, verified by inspection, and committed. Commit YAML-only for i18n (generated tables are gitignored).

---

## File Structure

| File | Responsibility | Change |
|---|---|---|
| `lib/I18n/translations/english.yaml` | i18n source strings | +4 keys (regenerate; commit YAML only) |
| `src/InkPointSettings.h` | settings schema | `FILE_SORT_MODE` enum + `fileSortMode` field |
| `src/SettingsList.h` | settings registry (device + web) | `SettingInfo::Enum` registration |
| `lib/FsHelpers/FsHelpers.h` / `.cpp` | filename helpers | extract reusable `naturalLess`; `sortFileList` reuses it |
| `lib/hal/HalStorage.h` / `.cpp` | HAL file handle | add `HalFile::modifiedKey()` |
| `src/activities/home/FileBrowserActivity.cpp` | browser data load | collect keys + mode-aware sort in `loadFiles()` |

---

## Task 1: i18n strings

**Files:**
- Modify: `lib/I18n/translations/english.yaml` (line 63 is `STR_SLEEP_SCREEN: "Sleep Screen"`)
- Generated (DO NOT commit — gitignored): `lib/I18n/I18nKeys.h`, `lib/I18n/I18nStrings.h`, `lib/I18n/I18nStrings.cpp`

- [ ] **Step 1: Add four keys to english.yaml**

In `lib/I18n/translations/english.yaml`, immediately after the line `STR_SLEEP_SCREEN: "Sleep Screen"`, insert:
```yaml
STR_FILE_SORT: "File Sorting"
STR_SORT_NAME: "Name"
STR_SORT_DATE: "Date Modified"
STR_SORT_SIZE: "File Size"
```
Resulting context:
```yaml
STR_SLEEP_SCREEN: "Sleep Screen"
STR_FILE_SORT: "File Sorting"
STR_SORT_NAME: "Name"
STR_SORT_DATE: "Date Modified"
STR_SORT_SIZE: "File Size"
STR_QUICK_RESUME_TIMEOUT: "Quick Resume on Timeout"
```

- [ ] **Step 2: Regenerate the string tables**

Run (`python` may not exist on this machine; use `python3`):
```bash
python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/
```
Expected: completes without error.

- [ ] **Step 3: Verify the keys were generated**

Run:
```bash
grep -nE "STR_FILE_SORT|STR_SORT_NAME|STR_SORT_DATE|STR_SORT_SIZE" lib/I18n/I18nKeys.h
```
Expected: four matching enum lines (PASS). If empty, re-run Step 2.

- [ ] **Step 4: Confirm no generated files are staged, then commit YAML only**

```bash
git status --short
```
Expected: only `lib/I18n/translations/english.yaml` modified (the three generated files must NOT appear as tracked/staged). Then:
```bash
git add lib/I18n/translations/english.yaml
git commit -m "feat: add file-sorting i18n keys"
```

---

## Task 2: FILE_SORT_MODE enum and setting field

**Files:**
- Modify: `src/InkPointSettings.h` (enums at lines 20-36; sleep-screen fields around line 165-169)

- [ ] **Step 1: Add the FILE_SORT_MODE enum**

In `src/InkPointSettings.h`, immediately after the `SLEEP_SCREEN_COVER_FILTER` enum (which ends at line 36 with `};`), insert:
```cpp
  enum FILE_SORT_MODE {
    SORT_NAME = 0,           // natural alphabetical (default, current behaviour)
    SORT_DATE_MODIFIED = 1,  // newest first
    SORT_SIZE = 2,           // largest first
    FILE_SORT_MODE_COUNT
  };
```
Resulting context:
```cpp
  enum SLEEP_SCREEN_COVER_FILTER {
    NO_FILTER = 0,
    BLACK_AND_WHITE = 1,
    INVERTED_BLACK_AND_WHITE = 2,
    SLEEP_SCREEN_COVER_FILTER_COUNT
  };
  enum FILE_SORT_MODE {
    SORT_NAME = 0,           // natural alphabetical (default, current behaviour)
    SORT_DATE_MODIFIED = 1,  // newest first
    SORT_SIZE = 2,           // largest first
    FILE_SORT_MODE_COUNT
  };
```

- [ ] **Step 2: Add the setting field**

Find the sleep-screen fields (around line 165-169):
```cpp
  // Sleep screen cover filter
  uint8_t sleepScreenCoverFilter = NO_FILTER;
```
Immediately after `sleepScreenCoverFilter`, insert:
```cpp
  // File browser sort mode (filesystem-only; works on X3 and X4)
  uint8_t fileSortMode = SORT_NAME;
```
Resulting context:
```cpp
  // Sleep screen cover filter
  uint8_t sleepScreenCoverFilter = NO_FILTER;
  // File browser sort mode (filesystem-only; works on X3 and X4)
  uint8_t fileSortMode = SORT_NAME;
  // Status bar settings (statusBar retained for migration only)
  uint8_t statusBar = FULL;
```

- [ ] **Step 3: Verify**

```bash
grep -nE "FILE_SORT_MODE|fileSortMode = SORT_NAME" src/InkPointSettings.h
```
Expected: enum line + field line present. Persistence is automatic once Task 3 registers the field; no `.cpp` change needed.

- [ ] **Step 4: Commit**

```bash
git add src/InkPointSettings.h
git commit -m "feat: add fileSortMode setting and FILE_SORT_MODE enum"
```

---

## Task 3: Register the Settings → Display enum

**Files:**
- Modify: `src/SettingsList.h` (Display category; `sleepScreenCoverFilter` Enum ends at line 114)

- [ ] **Step 1: Add the SettingInfo::Enum entry**

In `src/SettingsList.h`, find the `sleepScreenCoverFilter` Enum which ends (line 114) with `"sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY),`. Immediately after it, insert:
```cpp
        SettingInfo::Enum(StrId::STR_FILE_SORT, &InkPointSettings::fileSortMode,
                          {StrId::STR_SORT_NAME, StrId::STR_SORT_DATE, StrId::STR_SORT_SIZE},
                          "fileSortMode", StrId::STR_CAT_DISPLAY),
```
Resulting context:
```cpp
        SettingInfo::Enum(StrId::STR_SLEEP_COVER_FILTER, &InkPointSettings::sleepScreenCoverFilter,
                          {StrId::STR_NONE_OPT, StrId::STR_FILTER_CONTRAST, StrId::STR_INVERTED},
                          "sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_FILE_SORT, &InkPointSettings::fileSortMode,
                          {StrId::STR_SORT_NAME, StrId::STR_SORT_DATE, StrId::STR_SORT_SIZE},
                          "fileSortMode", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_QUICK_RESUME_TIMEOUT, &InkPointSettings::quickResumeSleepScreen,
```

> Signature reference (`src/activities/settings/SettingsActivity.h`): `SettingInfo::Enum(StrId nameId, uint8_t InkPointSettings::* ptr, std::vector<StrId> values, const char* key, StrId category)`. The enum option order `{NAME, DATE, SIZE}` maps to `fileSortMode` values `0,1,2` (SORT_NAME, SORT_DATE_MODIFIED, SORT_SIZE). JSON key `"fileSortMode"` must match the field name for persistence.

- [ ] **Step 2: Verify**

```bash
grep -n "STR_FILE_SORT\|fileSortMode" src/SettingsList.h
```
Expected: the new Enum line referencing `&InkPointSettings::fileSortMode`.

- [ ] **Step 3: Commit**

```bash
git add src/SettingsList.h
git commit -m "feat: add File Sorting enum to Display settings"
```

---

## Task 4: Extract reusable `naturalLess` comparator

**Files:**
- Modify: `lib/FsHelpers/FsHelpers.h` (declarations; `sortFileList` declared line 14)
- Modify: `lib/FsHelpers/FsHelpers.cpp` (`sortFileList` at lines 74-122)

- [ ] **Step 1: Declare `naturalLess` in the header**

In `lib/FsHelpers/FsHelpers.h`, find:
```cpp
void sortFileList(std::vector<std::string>& strs);
```
Immediately before it, add:
```cpp
// Case-insensitive natural-order compare of two filenames (numbers compared by value).
// Pure name comparison only — does NOT apply any directory-first rule.
bool naturalLess(const std::string& a, const std::string& b);
```
Both declarations live inside the existing `FsHelpers` namespace (confirm `sortFileList` is already inside it; place `naturalLess` adjacent).

- [ ] **Step 2: Implement `naturalLess` and make `sortFileList` reuse it**

In `lib/FsHelpers/FsHelpers.cpp`, REPLACE the entire current `sortFileList` function (lines 74-122):
```cpp
void sortFileList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    // Directories first
    bool isDir1 = str1.back() == '/';
    bool isDir2 = str2.back() == '/';
    if (isDir1 != isDir2) return isDir1;

    // Start naive natural sort
    const char* s1 = str1.c_str();
    const char* s2 = str2.c_str();

    // Iterate while both strings have characters
    while (*s1 && *s2) {
      // Check if both are at the start of a number
      if (isdigit(*s1) && isdigit(*s2)) {
        // Skip leading zeros and track them
        while (*s1 == '0') s1++;
        while (*s2 == '0') s2++;

        // Count digits to compare lengths first
        int len1 = 0, len2 = 0;
        while (isdigit(s1[len1])) len1++;
        while (isdigit(s2[len2])) len2++;

        // Different length so return smaller integer value
        if (len1 != len2) return len1 < len2;

        // Same length so compare digit by digit
        for (int i = 0; i < len1; i++) {
          if (s1[i] != s2[i]) return s1[i] < s2[i];
        }

        // Numbers equal so advance pointers
        s1 += len1;
        s2 += len2;
      } else {
        // Regular case-insensitive character comparison
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);
        if (c1 != c2) return c1 < c2;
        s1++;
        s2++;
      }
    }

    // One string is prefix of other
    return *s1 == '\0' && *s2 != '\0';
  });
}
```
with this new code (the natural-compare body becomes the standalone `naturalLess`; `sortFileList` keeps the directory-first rule and delegates):
```cpp
bool naturalLess(const std::string& a, const std::string& b) {
  const char* s1 = a.c_str();
  const char* s2 = b.c_str();

  // Iterate while both strings have characters
  while (*s1 && *s2) {
    // Check if both are at the start of a number
    if (isdigit(*s1) && isdigit(*s2)) {
      // Skip leading zeros
      while (*s1 == '0') s1++;
      while (*s2 == '0') s2++;

      // Count digits to compare lengths first
      int len1 = 0, len2 = 0;
      while (isdigit(s1[len1])) len1++;
      while (isdigit(s2[len2])) len2++;

      // Different length so return smaller integer value
      if (len1 != len2) return len1 < len2;

      // Same length so compare digit by digit
      for (int i = 0; i < len1; i++) {
        if (s1[i] != s2[i]) return s1[i] < s2[i];
      }

      // Numbers equal so advance pointers
      s1 += len1;
      s2 += len2;
    } else {
      // Regular case-insensitive character comparison
      char c1 = tolower(*s1);
      char c2 = tolower(*s2);
      if (c1 != c2) return c1 < c2;
      s1++;
      s2++;
    }
  }

  // One string is prefix of other
  return *s1 == '\0' && *s2 != '\0';
}

void sortFileList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    // Directories first
    bool isDir1 = str1.back() == '/';
    bool isDir2 = str2.back() == '/';
    if (isDir1 != isDir2) return isDir1;
    return naturalLess(str1, str2);
  });
}
```

> Behaviour is identical to before for `sortFileList` (used by `BmpViewerActivity` and the name path). `naturalLess` is now independently reusable. Both are inside the `FsHelpers` namespace already open in this file.

- [ ] **Step 3: Verify**

```bash
grep -n "bool naturalLess" lib/FsHelpers/FsHelpers.h lib/FsHelpers/FsHelpers.cpp
grep -n "return naturalLess(str1, str2);" lib/FsHelpers/FsHelpers.cpp
```
Expected: declaration in `.h`, definition in `.cpp`, and `sortFileList` delegating to it.

- [ ] **Step 4: Commit**

```bash
git add lib/FsHelpers/FsHelpers.h lib/FsHelpers/FsHelpers.cpp
git commit -m "refactor: extract reusable FsHelpers::naturalLess"
```

---

## Task 5: Add `HalFile::modifiedKey()`

**Files:**
- Modify: `lib/hal/HalStorage.h` (HalFile public methods, lines 75-96)
- Modify: `lib/hal/HalStorage.cpp` (HalFile method implementations, lines 147-172)

- [ ] **Step 1: Declare the accessor in the header**

In `lib/hal/HalStorage.h`, find (line 78):
```cpp
  size_t fileSize();
```
Immediately after it, add:
```cpp
  // FAT modify timestamp packed as (date << 16) | time, monotonically orderable
  // (newest == largest). Returns 0 if unavailable. Takes the storage mutex.
  uint32_t modifiedKey();
```

- [ ] **Step 2: Implement it in the .cpp**

In `lib/hal/HalStorage.cpp`, find (line 150):
```cpp
size_t HalFile::fileSize() { HAL_FILE_FORWARD_CALL(fileSize, ); }      // already thread-safe, no need to wrap
```
Immediately after that line, add:
```cpp
uint32_t HalFile::modifiedKey() {
  HalStorage::StorageLock lock;  // getModifyDateTime touches SD/SPI; serialize like other wrapped calls
  assert(impl != nullptr);
  uint16_t date = 0, time = 0;
  if (!impl->file.getModifyDateTime(&date, &time)) return 0;
  return (static_cast<uint32_t>(date) << 16) | time;
}
```

> `impl->file` is a SdFat `FsFile`; `bool FsFile::getModifyDateTime(uint16_t* pdate, uint16_t* ptime)` is confirmed at `.pio/libdeps/default/SdFat/src/FsLib/FsFile.h:305`. FAT date/time fields are bit-packed in descending significance (year→month→day, hour→minute→second), so the combined `(date<<16)|time` sorts chronologically. `StorageLock` is the same recursive-mutex guard used by every other wrapped `HalFile` method (defined at `HalStorage.cpp:32`).

- [ ] **Step 3: Verify**

```bash
grep -n "modifiedKey" lib/hal/HalStorage.h lib/hal/HalStorage.cpp
```
Expected: declaration in `.h` and definition in `.cpp`.

- [ ] **Step 4: Commit**

```bash
git add lib/hal/HalStorage.h lib/hal/HalStorage.cpp
git commit -m "feat: add HalFile::modifiedKey for FAT modify timestamp"
```

---

## Task 6: Mode-aware sorting in `FileBrowserActivity::loadFiles()`

**Files:**
- Modify: `src/activities/home/FileBrowserActivity.cpp` (anon namespace lines 18-21; `loadFiles` lines 23-64)

This task depends on Tasks 2 (`fileSortMode`/enum), 4 (`naturalLess`), and 5 (`modifiedKey`).

- [ ] **Step 1: Add the BrowserEntry struct to the anonymous namespace**

In `src/activities/home/FileBrowserActivity.cpp`, find the existing anonymous namespace (lines 18-21):
```cpp
namespace {
constexpr unsigned long GO_HOME_MS = 1000;
constexpr size_t NAME_BUFFER_SIZE = 500;
}  // namespace
```
Replace it with:
```cpp
namespace {
constexpr unsigned long GO_HOME_MS = 1000;
constexpr size_t NAME_BUFFER_SIZE = 500;

// One directory entry plus the keys needed to sort it. `name` keeps the trailing
// '/' for directories (matching how the browser stores/displays entries).
struct BrowserEntry {
  std::string name;
  uint32_t date;  // FAT modify key; 0 for directories or if unavailable
  uint32_t size;  // bytes; 0 for directories
};
}  // namespace
```

- [ ] **Step 2: Replace the body of `loadFiles()`**

REPLACE the entire current `loadFiles()` function (lines 23-64):
```cpp
void FileBrowserActivity::loadFiles() {
  files.clear();

  auto root = Storage.open(basepath.c_str());
  if (!root || !root.isDirectory()) {
    return;
  }

  root.rewindDirectory();

  if (!fileNameBuffer) {
    LOG_ERR("FileBrowser", "fileNameBuffer not allocated");
    root.close();
    return;
  }

  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(fileNameBuffer.get(), NAME_BUFFER_SIZE);
    if ((!SETTINGS.showHiddenFiles && fileNameBuffer[0] == '.') ||
        strcmp(fileNameBuffer.get(), "System Volume Information") == 0) {
      continue;
    }

    if (file.isDirectory()) {
      files.emplace_back(std::string(fileNameBuffer.get()) + "/");
    } else {
      std::string_view filename{fileNameBuffer.get()};
      if (mode == Mode::PickFirmware) {
        // Firmware picker: only show .bin files.
        if (FsHelpers::checkFileExtension(filename, ".bin")) {
          files.emplace_back(filename);
        }
      } else if (FsHelpers::hasEpubExtension(filename) || FsHelpers::hasXtcExtension(filename) ||
                 FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename) ||
                 FsHelpers::hasBmpExtension(filename)) {
        files.emplace_back(filename);
      }
    }
  }
  root.close();
  FsHelpers::sortFileList(files);
}
```
with this new version:
```cpp
void FileBrowserActivity::loadFiles() {
  files.clear();

  auto root = Storage.open(basepath.c_str());
  if (!root || !root.isDirectory()) {
    return;
  }

  root.rewindDirectory();

  if (!fileNameBuffer) {
    LOG_ERR("FileBrowser", "fileNameBuffer not allocated");
    root.close();
    return;
  }

  std::vector<BrowserEntry> entries;
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(fileNameBuffer.get(), NAME_BUFFER_SIZE);
    if ((!SETTINGS.showHiddenFiles && fileNameBuffer[0] == '.') ||
        strcmp(fileNameBuffer.get(), "System Volume Information") == 0) {
      continue;
    }

    if (file.isDirectory()) {
      // Directories are always name-sorted and listed first; sort keys unused.
      entries.push_back({std::string(fileNameBuffer.get()) + "/", 0, 0});
    } else {
      std::string_view filename{fileNameBuffer.get()};
      const bool keep = (mode == Mode::PickFirmware)
                            ? FsHelpers::checkFileExtension(filename, ".bin")
                            : (FsHelpers::hasEpubExtension(filename) || FsHelpers::hasXtcExtension(filename) ||
                               FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename) ||
                               FsHelpers::hasBmpExtension(filename));
      if (keep) {
        entries.push_back({std::string(filename), file.modifiedKey(), static_cast<uint32_t>(file.fileSize())});
      }
    }
  }
  root.close();

  const uint8_t sortMode = SETTINGS.fileSortMode;
  std::sort(entries.begin(), entries.end(), [sortMode](const BrowserEntry& a, const BrowserEntry& b) {
    const bool aDir = a.name.back() == '/';
    const bool bDir = b.name.back() == '/';
    if (aDir != bDir) return aDir;                              // directories first
    if (aDir) return FsHelpers::naturalLess(a.name, b.name);    // dirs always by name
    switch (sortMode) {
      case InkPointSettings::SORT_DATE_MODIFIED:
        if (a.date != b.date) return a.date > b.date;           // newest first
        return FsHelpers::naturalLess(a.name, b.name);
      case InkPointSettings::SORT_SIZE:
        if (a.size != b.size) return a.size > b.size;           // largest first
        return FsHelpers::naturalLess(a.name, b.name);
      case InkPointSettings::SORT_NAME:
      default:
        return FsHelpers::naturalLess(a.name, b.name);
    }
  });

  files.reserve(entries.size());
  for (auto& e : entries) files.push_back(std::move(e.name));
}
```

> Notes:
> - `files` stays `std::vector<std::string>`; only the load path changed, so all downstream consumers (`files[selectorIndex]`, `getFileName`, `findEntry`, render callbacks) are untouched.
> - `<algorithm>` (for `std::sort`) is already included at `FileBrowserActivity.cpp:9`; `InkPointSettings.h` at line 11; `FsHelpers.h` at line 3. No new includes needed.
> - `file.modifiedKey()` and `file.fileSize()` are read while the `HalFile` is still open in the loop.
> - Directories get `{name, 0, 0}`; their keys are never consulted because the comparator routes dirs to `naturalLess`.

- [ ] **Step 3: Verify the change inspect-only**

```bash
grep -n "BrowserEntry\|modifiedKey\|fileSortMode\|naturalLess" src/activities/home/FileBrowserActivity.cpp
```
Expected: struct usage, `file.modifiedKey()`, `SETTINGS.fileSortMode`, and `FsHelpers::naturalLess` all present. Confirm `FsHelpers::sortFileList(files);` is no longer called in `loadFiles` (replaced by the local sort).

- [ ] **Step 4: Format**

```bash
find src lib -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```
Re-stage only files this feature touched if clang-format reformatted them; if it touched unrelated files, revert those with `git checkout -- <file>`.

- [ ] **Step 5: Best-effort compile gate (CI is authoritative)**

```bash
pio run
```
Expected outcomes:
- **SUCCESS** → great.
- **Fails only with** `ModuleNotFoundError: No module named 'littlefs'` / `pyexpat` / `expat` → the known pre-existing environment breakage (see top note). Record it; the CI build is the real gate.
- **Fails with a compiler error naming `FileBrowserActivity.cpp`, `HalStorage`, `FsHelpers`, `InkPointSettings`, or `SettingsList`** → a real error in this feature; fix it before committing.

- [ ] **Step 6: Commit**

```bash
git add src/activities/home/FileBrowserActivity.cpp
git commit -m "feat: sort file browser by name, date modified, or size"
```

---

## Task 7: Final verification & handoff

- [ ] **Step 1: Confirm no generated/ignored files were committed across the feature**

```bash
git log --oneline -6
git show --stat HEAD~5..HEAD | grep -iE "I18nKeys|I18nStrings|\.generated\.h|\.pio/|compile_commands" && echo "!!! GENERATED FILE COMMITTED" || echo "OK: no generated/ignored files committed"
```
Expected: `OK: no generated/ignored files committed`.

- [ ] **Step 2: Confirm the full feature diff is the six intended files**

```bash
git diff --stat HEAD~6 HEAD
```
Expected only: `english.yaml`, `InkPointSettings.h`, `SettingsList.h`, `FsHelpers.h`, `FsHelpers.cpp`, `HalStorage.h`, `HalStorage.cpp`, `FileBrowserActivity.cpp` (7 source files across 6 logical units).

- [ ] **Step 3: Human-tester checklist (device — verify on X4 and, when available, X3)**

The feature is hardware-agnostic; it must behave identically on both. Flag these for the user:
- 🔲 Settings → Display shows **File Sorting** with options Name / Date Modified / File Size.
- 🔲 **Name**: directories first, natural alphabetical (unchanged from today).
- 🔲 **Date Modified**: copy a new file to a folder; it appears at the top.
- 🔲 **File Size**: largest file first; directories still grouped first by name.
- 🔲 Setting **persists across reboot**.
- 🔲 A folder containing files with missing/zero timestamps does not crash and orders predictably.
- 🔲 Free heap (`ESP.getFreeHeap()`) is stable entering/leaving folders (the temporary `entries` vector is freed at function exit).
- 🔲 CI build (`ci.yml`) is green.

---

## Self-Review

**Spec coverage:**
- Criteria Name/Date/Size → Task 2 enum + Task 6 comparator. ✅
- Fixed direction per criterion → comparator hardcodes `>` for date/size, `naturalLess` for name. ✅
- Global persisted setting → Task 2 field + Task 3 registration (JSON key `fileSortMode`). ✅
- Settings → Display Enum control → Task 3. ✅
- Directories first + always name-sorted → comparator `aDir` branch. ✅
- HAL `modifiedKey` → Task 5. ✅
- Reusable `naturalLess` + `sortFileList`/BmpViewer unchanged → Task 4. ✅
- i18n keys → Task 1. ✅
- Out-of-scope items (metadata sort, per-folder, asc/desc, web listing) → not implemented. ✅

**Placeholder scan:** No TBD/“handle edge cases”/vague steps; every code step shows full code. The only deferred note is the environment build caveat, which is explicit and actionable. ✅

**Type consistency:** `fileSortMode` (uint8_t) used in Tasks 2/3/6; enum members `SORT_NAME/SORT_DATE_MODIFIED/SORT_SIZE` consistent across Tasks 2 and 6; `naturalLess(const std::string&, const std::string&)` signature identical in Task 4 decl/def and Task 6 calls; `modifiedKey()` returns `uint32_t` in Task 5 and is assigned to `BrowserEntry::date` (uint32_t) in Task 6; `fileSize()` returns `size_t`, cast to `uint32_t` for `BrowserEntry::size`. ✅
