<p align="center">
  <img src="site/assets/inkpoint-mark.svg" alt="InkPoint" width="92" height="92" />
</p>

<h1 align="center">InkPoint Reader</h1>

<p align="center">
  Open-source, fully hackable e-reader firmware for the ESP32-C3 Xteink readers.<br/>
  <strong>Flash it from your browser. No account. No telemetry. Yours forever.</strong>
</p>

<p align="center">
  <a href="https://hudsonbrendon.github.io/inkpoint/">🌐 Website &amp; web flasher</a> ·
  <a href="https://hudsonbrendon.github.io/inkpoint/install.html">⚡ Install</a> ·
  <a href="https://github.com/hudsonbrendon/inkpoint/releases/latest">📦 Latest release</a>
</p>

InkPoint is community-built e-reader firmware. It is a fork of [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) that keeps everything that makes CrossPoint great and adds a layer of reading-habit and focus features on top — built with the same hard constraint in mind: the ESP32-C3 has only ~380 KB of RAM, so stability and careful memory use come before everything else.

**Runs on:** ESP32-C3-based Xteink [X4](https://www.xteink.com/products/xteink-x4) (800×480) and [X3](https://www.xteink.com/products/xteink-x3) (528×792). A **single firmware image** boots on both — the panel is detected automatically at startup.

![InkPoint Reader running on an Xteink device](./docs/images/cover.jpg)

---

## ✨ What InkPoint adds over CrossPoint

These are the features InkPoint ships on top of the CrossPoint base. All of them run within the device's memory budget and are localized in every supported language.

### 📊 Reading statistics & habits
A dedicated reading-stats engine and screen. Tracks **books finished**, reading **streaks**, total reading time and pages, and per-book progress. Surfaces your reading habits directly on the device — no app, no cloud.

### 🐣 Virtual Pet
A companion that lives on your reader and **grows as you read**. Feed, snack, give medicine, exercise, clean, and pet it; evolution is driven by your real reading milestones (pages read and care over time). A playful nudge to keep the reading streak alive — strictly opt-in and never intrusive.

### 🗂️ Flashcards (spaced repetition)
Study decks stored as plain **CSV files on the SD card**. Review sessions use a lightweight **SRS (spaced-repetition)** schedule so cards you struggle with come back sooner. Because the X4 has no real-time clock, scheduling is measured in **review sessions** ("sessions until next review"), not wall-clock days, so it works without a clock. Decks also show up in the file browser inside the `flashcards/` folder.

### 🍅 Pomodoro
A focus timer designed **for e-ink**, not bolted onto it. Fixed classic cadence — **25 min focus / 5 min break / 15 min long break** every 4 cycles. The screen refreshes once per minute (no e-ink hammering), a **full-screen flash** marks every phase change (the only attention signal an e-ink panel has), and the device is **kept awake** while a session is running. Confirm = start/pause, Left = reset, Right = skip, Back = exit.

### 📰 RSS / Atom reader + web management
Subscribe to feeds, fetch over Wi-Fi, and **cache articles to the SD card** for offline reading in the text reader. Manage feeds on-device or from the browser via Web Management. Read articles appear in **Recent Books** with the article title and source.

### 🔄 Self-hosted, owner-controlled OTA
Over-the-air updates pull from **this repository's** GitHub Releases (`hudsonbrendon/inkpoint`), so you control the update channel end to end. **Settings → Firmware update** checks for and installs the latest release over Wi-Fi.

### 🌐 Official website + browser web flasher
A first-class [website](https://hudsonbrendon.github.io/inkpoint/) with a **WebSerial-based web flasher** — install or update straight from Chrome or Edge with no tools to download. The site is fully translated into every UI language InkPoint supports (with RTL).

### 🎨 New brand identity
A new InkPoint mark (the "page" tile) used across the boot/sleep splash on the device, the site, and the favicon.

---

## What can InkPoint do? (full feature set)

- **Reader engine**: EPUB 2/3 rendering with embedded-style option, image handling, hyphenation, kerning, chapter navigation, footnotes, bookmarks, go-to-percent, auto page turn, orientation control, focus reading, KOReader progress sync, and more.
- **Formats**: native handling for `.epub`, `.xtc/.xtch`, `.txt`, and `.bmp`.
- **Reading statistics**: books finished, streaks, time and pages read, per-book progress.
- **Virtual Pet**, **Flashcards (SRS)**, and **Pomodoro** focus timer (see above).
- **RSS/Atom reader** with offline article caching and web feed management.
- **Custom fonts**: install your own fonts from the SD card — no reflash needed.
- **Tilt page turn** (X3 only).
- **Library workflow**: folder browser with **sorting**, hidden-file toggle, long-press delete, recent books, SD-cache management.
- **Wireless workflows**:
  - File transfer web UI + EPUB Optimizer
  - Web settings UI/API (edit many device settings from the browser)
  - WebSocket fast uploads and a WebDAV handler
  - AP mode (hotspot) and STA mode (join existing Wi-Fi), both with QR helpers
  - Calibre wireless connect flow
  - OPDS browser with saved servers (up to 8), search, pagination, and direct download
  - RSS/Atom feed management web UI
  - OTA update checks and installs from GitHub Releases
- **Customization**: multiple themes (Classic, Lyra, Lyra Extended, RoundedRaff), sleep-screen modes, front/side button remapping, status-bar controls, power-button behavior, refresh cadence, and more.
- **Localization**: 24 UI languages and counting, including full RTL support (Hebrew). Every new feature above ships translated.
- **Screenshots** straight from the device.

### Coming soon

- **Dictionary lookup** — inline word lookup without leaving the reader (the headline roadmap item).
- **In-book settings menu** — per-book font / margins / spacing.
- **Live reading-session timer** in the reader.
- More themes, and much more — see the [roadmap](./docs/superpowers/plans/2026-06-08-inkpoint-feature-roadmap.md).

---

## USB-locked devices (Xteink Unlocker)

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing **locked** from the factory. If your device is locked, you must run the **Xteink Unlocker** before you can flash InkPoint. InkPoint is an officially supported target of the unlocker.

**You do not need this if you bought directly from xteink.com** — those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try the [web flasher](https://hudsonbrendon.github.io/inkpoint/install.html) first. If the browser's serial device picker doesn't show your device, try a different USB port, cable, or browser before assuming it's locked. Only reach for the unlocker if it still doesn't appear.

> ### ⚠️ READ THIS BEFORE USING ANY UNLOCKER ⚠️
>
> On a USB-locked device, flashing firmware that **does not support SD-card flashing or OTA updates** can leave you **permanently stuck on it with no recovery path** — once USB flashing is re-locked, OTA is your only way back. InkPoint supports both SD and OTA updates. **Only flash firmware that includes a working update path.**

---

## Install firmware

InkPoint can be installed three ways. The web flasher is the easiest; SD-card is the fallback when USB is unavailable; OTA is for devices already running InkPoint.

### 1. Web flasher (recommended)

1. Connect your device via USB-C and wake it to the home screen.
2. Open **<https://hudsonbrendon.github.io/inkpoint/install.html>** in **Chrome or Edge on desktop**.
3. Click **Flash InkPoint via USB**, pick the serial port, and let it complete. The device reboots into InkPoint.

The same image works on both X3 and X4 — there is nothing to choose.

### 2. SD card

Works even when USB flashing isn't available:

1. Download the latest `firmware.bin` from [Releases](https://github.com/hudsonbrendon/inkpoint/releases/latest).
2. Copy it to the **root of the SD card** (rename it however you like, e.g. `inkpoint.bin`).
3. Insert the card, then on the device open **Settings → Firmware update** and pick the file.

### 3. Over-the-air (OTA)

If you already run InkPoint with Wi-Fi configured, open **Settings → Firmware update** and the device pulls the latest release from GitHub on its own.

### 4. Command line (esptool)

1. Install [`esptool`](https://github.com/espressif/esptool): `pip install esptool`
2. Download `firmware.bin` from the [releases page](https://github.com/hudsonbrendon/inkpoint/releases/latest).
3. Connect the device via USB-C and find its port (`dmesg` on Linux after connecting; on macOS `ls /dev/cu.usb*`).
4. Flash (the app image lives at offset `0x10000`):

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Adjust the port to match your system.

### 5. Build it yourself

See [Development quick start](#development-quick-start) below.

---

## Custom SD-card fonts

Convert your own TTF/OTF fonts into the firmware's `.cpfont` format that loads from the SD card — **no reflash needed**.

1. Run the repo's converter (host build):

   ```bash
   python3 lib/EpdFont/scripts/fontconvert_sdcard.py --help
   ```

   Supply up to four styles (regular, bold, italic, bold-italic), the family name, point sizes, and Unicode range.
2. Copy the generated files to your SD card under `/fonts/YourFont/` (or `/.fonts/YourFont/` to hide the folder).
3. Select the font on the device from the font settings.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md) · [Web server endpoints](./docs/webserver-endpoints.md)
- [File formats](./docs/file-formats.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)
- [Feature roadmap](./docs/superpowers/plans/2026-06-08-inkpoint-feature-roadmap.md)

---

## Development quick start

### Prerequisites

- [pioarduino](https://github.com/pioarduino/pioarduino), or VS Code + the pioarduino plugin
- Python 3.8+
- `clang-format` 21
- A USB-C cable that supports data transfer

### Setup

```bash
git clone --recursive https://github.com/hudsonbrendon/inkpoint
cd inkpoint

# if you cloned without --recursive:
git submodule update --init --recursive
```

### Build environments

Configured in `platformio.ini`:

| Environment      | Purpose                                   |
| ---------------- | ----------------------------------------- |
| `default`        | Development (verbose logging, serial)     |
| `gh_release`     | Production release (`LOG_LEVEL=0`)        |
| `gh_release_rc`  | Release candidate (`LOG_LEVEL=1`)         |
| `slim`           | Minimal build (no serial logging)         |

### Build / flash / monitor

```bash
pio run                         # build (default env)
pio run --target upload         # build + flash over USB
pio run -e gh_release           # build the production image
pio device monitor              # serial monitor
```

### Contributor pre-PR checks

```bash
./bin/clang-format-fix          # format C++ to clang-format 21
pio check -e default            # static analysis (cppcheck)
pio run -e default              # build must be clean (0 errors/warnings)
```

CI runs the build and a clang-format check on every PR — fix both locally first.

### Debugging

After flashing, capture detailed serial logs:

```bash
python3 -m pip install pyserial colorama matplotlib

# Linux (tested on Debian; most distros work)
python3 scripts/debugging_monitor.py

# macOS (pass your port)
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be needed on Windows.

---

## Internals

InkPoint is aggressive about caching data to the SD card to minimise RAM use. The ESP32-C3 has only ~380 KB of usable RAM, so most design decisions trace back to that constraint.

### Data caching

The first time a book's chapters are loaded they are cached to the SD card; later loads are served from the cache. The cache lives at `.inkpoint/` on the SD card:

```text
.inkpoint/
├── epub_<hash>/         # one directory per book, named by content hash
│   ├── progress.bin     # reading position (chapter, page, etc.)
│   ├── cover.bmp        # generated cover image
│   ├── book.bin         # metadata: title, author, spine, TOC
│   └── sections/        # per-chapter layout cache
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
```

Deleting `/.inkpoint` clears all cached metadata and forces a full regeneration on next open. The cache is **not** cleared automatically when you delete a book, and moving a file to a new path resets its reading progress. See the [file-formats document](./docs/file-formats.md) for the binary structures.

---

## Contributing

Contributions are welcome. If you're new to the codebase, start with the [contributing docs](./docs/contributing/README.md) and open an [issue](https://github.com/hudsonbrendon/inkpoint/issues) before starting larger work so effort isn't duplicated. Everyone here is a volunteer — please be respectful and patient. See [GOVERNANCE.md](./GOVERNANCE.md) for community expectations.

---

## About

InkPoint Reader is open-source firmware maintained by [Hudson Brendon](https://github.com/hudsonbrendon).

- 🌐 **Website & web flasher:** <https://hudsonbrendon.github.io/inkpoint/>
- 📦 **Releases:** <https://github.com/hudsonbrendon/inkpoint/releases>

InkPoint is **not affiliated with Xteink or any device manufacturer**.

It is a fork of [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) — huge thanks to the CrossPoint community for the foundation. CrossPoint itself was inspired by [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader).
