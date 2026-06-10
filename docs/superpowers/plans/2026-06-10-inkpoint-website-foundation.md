# InkPoint Website — Foundation (Pages + Web Flasher) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a static InkPoint website on GitHub Pages with a browser-based USB firmware flasher (esp-web-tools, WebSerial) plus install documentation (USB + SD card), serving both Xteink X3 and X4 from a single auto-detecting firmware image.

**Architecture:** A plain static site (HTML/CSS/vanilla JS, no framework) lives in `site/`. A GitHub Actions workflow builds the production firmware (`pio run -e gh_release`), merges the flash images into one factory `merged-factory.bin` with `esptool merge_bin`, generates an esp-web-tools `manifest.json` stamped with the version from `platformio.ini`, copies them into the published site (so binaries are same-origin — no CORS), and deploys to GitHub Pages via `actions/deploy-pages`. The flasher is the `<esp-web-install-button>` web component pointed at that manifest.

**Tech Stack:** GitHub Pages, GitHub Actions (`actions/deploy-pages`), esp-web-tools (web component, WebSerial), esptool.py (`merge_bin`), PlatformIO (`gh_release` env), plain HTML/CSS/JS, Python 3 (manifest generation).

**Scope boundary:** This is sub-project 1 of the larger site. It deliberately excludes the font builder, unlocker, and debug-controls pages (separate specs). It includes: landing page shell, web flasher page, and install docs (USB + SD). X3/X4 are served by the SAME binary (runtime panel auto-detection — see `BaseTheme.cpp:145`, `OtaBootSwitch.h`); device choice on the site is informational only.

**Verification note:** A static site has no unit-test harness. "Tests" here = JSON validity checks, local HTTP serving, presence/size of built artifacts, and CI dry-runs. Actual on-device USB flashing is human-tester scope (needs a physical X3/X4 + Chrome/Edge). Use the local pio: `~/.platformio/penv/bin/pio`.

---

## File Structure

- **Create** `site/index.html` — landing page (hero + nav: Install, Docs; links to flasher).
- **Create** `site/install.html` — flasher page (esp-web-tools button) + USB + SD instructions.
- **Create** `site/assets/style.css` — shared minimal styling.
- **Create** `site/assets/app.js` — small UI glue (device picker is informational; nav highlight).
- **Create** `site/firmware/.gitkeep` — placeholder; `merged-factory.bin` + `manifest.json` are injected by CI, NOT committed.
- **Create** `scripts/make_web_flasher_assets.py` — builds `merged-factory.bin` + `manifest.json` from a `gh_release` build into a target dir.
- **Create** `.github/workflows/pages.yml` — build firmware, run the script, deploy `site/` to Pages.
- **Modify** `.gitignore` — ignore generated `site/firmware/merged-factory.bin` and `site/firmware/manifest.json`.

---

## Task 1: Static site skeleton (landing + styling)

**Files:**
- Create: `site/index.html`
- Create: `site/assets/style.css`

- [ ] **Step 1: Create the landing page**

`site/index.html`:

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>InkPoint — Open e-reader firmware for Xteink X3/X4</title>
    <meta name="description" content="InkPoint: lightweight open-source EPUB reader firmware for the Xteink X3 and X4. Flash it from your browser." />
    <link rel="stylesheet" href="assets/style.css" />
  </head>
  <body>
    <header class="topbar">
      <a class="brand" href="index.html">InkPoint</a>
      <nav>
        <a href="install.html">Install</a>
        <a href="https://github.com/hudsonbrendon/inkpoint" rel="noopener">GitHub</a>
      </nav>
    </header>

    <main class="hero">
      <h1>InkPoint</h1>
      <p class="tagline">Lightweight open-source EPUB reader firmware for the Xteink X3 &amp; X4.</p>
      <a class="cta" href="install.html">Install in your browser →</a>
      <p class="subtle">Flashes over USB with Chrome or Edge. No tools to download.</p>
    </main>

    <footer class="foot">
      <p>InkPoint is community firmware. Not affiliated with Xteink.</p>
    </footer>
  </body>
</html>
```

- [ ] **Step 2: Create the stylesheet**

`site/assets/style.css`:

```css
:root {
  --ink: #1a1a1a;
  --paper: #f7f7f4;
  --accent: #2b6cb0;
  --muted: #666;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  color: var(--ink);
  background: var(--paper);
  line-height: 1.5;
}
.topbar {
  display: flex; align-items: center; justify-content: space-between;
  padding: 1rem 1.5rem; border-bottom: 1px solid #ddd;
}
.brand { font-weight: 700; font-size: 1.25rem; text-decoration: none; color: var(--ink); }
.topbar nav a { margin-left: 1.25rem; text-decoration: none; color: var(--accent); }
.hero { max-width: 720px; margin: 0 auto; padding: 4rem 1.5rem; text-align: center; }
.hero h1 { font-size: 3rem; margin: 0 0 0.5rem; }
.tagline { font-size: 1.25rem; color: var(--muted); }
.cta {
  display: inline-block; margin-top: 1.5rem; padding: 0.75rem 1.5rem;
  background: var(--accent); color: #fff; border-radius: 8px; text-decoration: none; font-weight: 600;
}
.subtle { color: var(--muted); font-size: 0.9rem; margin-top: 0.75rem; }
main.doc { max-width: 760px; margin: 0 auto; padding: 2rem 1.5rem; }
main.doc h2 { margin-top: 2rem; }
.method { border: 1px solid #ddd; border-radius: 10px; padding: 1.25rem 1.5rem; margin: 1rem 0; background: #fff; }
.method h3 { margin-top: 0; }
.callout { border-left: 4px solid var(--accent); background: #eef4fb; padding: 0.75rem 1rem; border-radius: 4px; }
.callout.warn { border-left-color: #c05621; background: #fdf1e9; }
.device-pick button {
  margin-right: 0.5rem; padding: 0.4rem 0.9rem; border: 1px solid var(--accent);
  background: #fff; color: var(--accent); border-radius: 6px; cursor: pointer;
}
.device-pick button.active { background: var(--accent); color: #fff; }
code, pre { background: #eee; border-radius: 4px; }
code { padding: 0.1rem 0.3rem; }
pre { padding: 0.75rem 1rem; overflow-x: auto; }
.foot { text-align: center; color: var(--muted); padding: 2rem 1.5rem; font-size: 0.85rem; }
```

- [ ] **Step 3: Verify it serves locally**

Run: `cd site && python3 -m http.server 8099 >/tmp/inkpoint-site.log 2>&1 & sleep 1 && curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8099/index.html && kill %1`
Expected: `200`

- [ ] **Step 4: Commit**

```bash
git add site/index.html site/assets/style.css
git commit -m "feat(site): landing page skeleton + base styles"
```

---

## Task 2: Install page with web flasher + instructions

**Files:**
- Create: `site/install.html`
- Create: `site/assets/app.js`

- [ ] **Step 1: Create the install page**

`site/install.html` — the `<esp-web-install-button>` points at `firmware/manifest.json` (injected by CI in Task 4):

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Install InkPoint</title>
    <link rel="stylesheet" href="assets/style.css" />
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js"></script>
  </head>
  <body>
    <header class="topbar">
      <a class="brand" href="index.html">InkPoint</a>
      <nav>
        <a href="install.html">Install</a>
        <a href="https://github.com/hudsonbrendon/inkpoint" rel="noopener">GitHub</a>
      </nav>
    </header>

    <main class="doc">
      <h1>Install InkPoint</h1>
      <p>InkPoint runs on both the Xteink <strong>X3</strong> and <strong>X4</strong> from a single
         firmware image — the device is auto-detected at boot.</p>

      <div class="device-pick" aria-label="Your device (informational)">
        <span>My device:</span>
        <button data-device="X4" class="active">X4 (800×480)</button>
        <button data-device="X3">X3 (528×792)</button>
      </div>
      <p class="subtle" id="device-note">The same firmware is flashed for both; this only tailors the notes below.</p>

      <div class="method">
        <h3>Method 1 — USB web flasher (recommended)</h3>
        <p>Connect the reader with a USB-C cable, make sure it is awake on the home screen, then:</p>
        <p>
          <esp-web-install-button manifest="firmware/manifest.json">
            <button slot="activate" class="cta">Flash InkPoint via USB</button>
            <span slot="unsupported">Your browser does not support WebSerial. Use Chrome or Edge on desktop.</span>
            <span slot="not-allowed">USB access was blocked. Reload and allow the serial port.</span>
          </esp-web-install-button>
        </p>
        <p class="subtle">Requires Chrome or Edge on desktop. If the serial picker does not show your
           device, try another USB port or cable before assuming the device is locked.</p>
      </div>

      <div class="method">
        <h3>Method 2 — SD card</h3>
        <p>Works even if USB flashing is unavailable:</p>
        <ol>
          <li>Download the latest <code>firmware.bin</code> from the
            <a href="https://github.com/hudsonbrendon/inkpoint/releases/latest" rel="noopener">latest release</a>.</li>
          <li>Copy <code>firmware.bin</code> to the root of the SD card (rename it as you like, e.g. <code>inkpoint.bin</code>).</li>
          <li>Insert the card, then on the device open <strong>Settings → Firmware update</strong> and pick the file.</li>
        </ol>
        <div class="callout warn">
          Only flash firmware that includes SD-flashing or OTA update support, or you may lose the ability to update.
        </div>
      </div>

      <div class="method">
        <h3>Method 3 — Over-the-air (already on InkPoint)</h3>
        <p>If you already run InkPoint with Wi-Fi, open <strong>Settings → Firmware update</strong> and the
           device pulls the latest release automatically.</p>
      </div>
    </main>

    <footer class="foot"><p>InkPoint is community firmware. Not affiliated with Xteink.</p></footer>
    <script src="assets/app.js"></script>
  </body>
</html>
```

- [ ] **Step 2: Create the device-picker glue**

`site/assets/app.js`:

```js
// Informational device picker: the same firmware flashes to both X3 and X4
// (runtime auto-detection), so this only updates the helper note.
document.querySelectorAll('.device-pick button').forEach((btn) => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.device-pick button').forEach((b) => b.classList.remove('active'));
    btn.classList.add('active');
    const note = document.getElementById('device-note');
    if (note) {
      const dev = btn.getAttribute('data-device');
      note.textContent =
        dev === 'X3'
          ? 'X3 detected automatically at boot (528×792 panel). The same firmware is flashed.'
          : 'X4 detected automatically at boot (800×480 panel). The same firmware is flashed.';
    }
  });
});
```

- [ ] **Step 3: Verify the page serves and references the manifest**

Run: `cd site && python3 -m http.server 8099 >/tmp/inkpoint-site.log 2>&1 & sleep 1 && curl -s http://localhost:8099/install.html | grep -c 'firmware/manifest.json' && kill %1`
Expected: `1` (the manifest reference is present)

- [ ] **Step 4: Commit**

```bash
git add site/install.html site/assets/app.js
git commit -m "feat(site): install page with esp-web-tools flasher + USB/SD/OTA docs"
```

---

## Task 3: Flasher asset generator (merged bin + manifest)

**Files:**
- Create: `scripts/make_web_flasher_assets.py`
- Create: `site/firmware/.gitkeep`
- Modify: `.gitignore`

- [ ] **Step 1: Create the placeholder so the dir exists in git**

```bash
mkdir -p site/firmware
touch site/firmware/.gitkeep
```

- [ ] **Step 2: Write the generator script**

`scripts/make_web_flasher_assets.py` — merges the four images into one factory blob at offset 0 and writes an esp-web-tools manifest. Offsets come from `partitions.csv` (app0 @ 0x10000) and the ESP32-C3 standard layout (bootloader 0x0, partitions 0x8000, otadata's boot_app0 0xe000):

```python
#!/usr/bin/env python3
"""Build esp-web-tools assets (merged-factory.bin + manifest.json) from a
gh_release PlatformIO build. Run after `pio run -e gh_release`.

Usage:
  python3 scripts/make_web_flasher_assets.py \
      --build-dir .pio/build/gh_release \
      --boot-app0 <path/to/boot_app0.bin> \
      --version 1.7.1 \
      --out-dir site/firmware
"""
import argparse
import json
import os
import subprocess
import sys


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True)
    ap.add_argument("--boot-app0", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--esptool", default="esptool.py")
    args = ap.parse_args()

    bootloader = os.path.join(args.build_dir, "bootloader.bin")
    partitions = os.path.join(args.build_dir, "partitions.bin")
    firmware = os.path.join(args.build_dir, "firmware.bin")
    for p in (bootloader, partitions, firmware, args.boot_app0):
        if not os.path.isfile(p):
            print(f"ERROR: missing input: {p}", file=sys.stderr)
            return 1

    os.makedirs(args.out_dir, exist_ok=True)
    merged = os.path.join(args.out_dir, "merged-factory.bin")

    # ESP32-C3 factory layout. app0 offset 0x10000 matches partitions.csv.
    cmd = [
        args.esptool, "--chip", "esp32c3", "merge_bin",
        "-o", merged,
        "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB",
        "0x0", bootloader,
        "0x8000", partitions,
        "0xe000", args.boot_app0,
        "0x10000", firmware,
    ]
    print("RUN:", " ".join(cmd))
    subprocess.run(cmd, check=True)

    manifest = {
        "name": "InkPoint",
        "version": args.version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-C3",
                "parts": [{"path": "merged-factory.bin", "offset": 0}],
            }
        ],
    }
    manifest_path = os.path.join(args.out_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    size = os.path.getsize(merged)
    print(f"OK: {merged} ({size} bytes), {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 3: Make it executable**

Run: `chmod +x scripts/make_web_flasher_assets.py`

- [ ] **Step 4: Ignore the generated binaries (keep the dir, not the artifacts)**

Append to `.gitignore`:

```gitignore
# Web flasher assets (generated by CI from the gh_release build)
site/firmware/merged-factory.bin
site/firmware/manifest.json
```

- [ ] **Step 5: Generate assets locally against the existing build and validate**

(The repo already has a `gh_release` build from the 1.7.1 release; reuse it.)

Run:
```bash
~/.platformio/penv/bin/pio run -e gh_release >/dev/null 2>&1
python3 scripts/make_web_flasher_assets.py \
  --build-dir .pio/build/gh_release \
  --boot-app0 "$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" \
  --version "$(grep -E '^version =' platformio.ini | head -1 | awk '{print $3}')" \
  --out-dir site/firmware \
  --esptool "$HOME/.platformio/penv/bin/python $HOME/.platformio/packages/tool-esptoolpy/esptool.py"
```
Expected: prints `OK: site/firmware/merged-factory.bin (<N> bytes)`. (Note: if `--esptool` with a space arg fails to invoke, see Step 6 fallback.)

- [ ] **Step 6: If the esptool invocation fails, use the module form**

The `--esptool` default is `esptool.py`. In environments where only the package script exists, invoke via python. Verify the merged file and manifest exist and the manifest is valid JSON:

Run:
```bash
ls -la site/firmware/merged-factory.bin && python3 -c "import json,sys; json.load(open('site/firmware/manifest.json')); print('manifest OK')"
```
Expected: the `.bin` exists (a few MB) and `manifest OK` prints.

- [ ] **Step 7: Commit (script + placeholder + gitignore only)**

```bash
git add scripts/make_web_flasher_assets.py site/firmware/.gitkeep .gitignore
git commit -m "feat(site): generator for merged factory image + esp-web-tools manifest"
```

Confirm the generated `merged-factory.bin` / `manifest.json` are NOT staged (gitignored).

---

## Task 4: GitHub Pages deploy workflow

**Files:**
- Create: `.github/workflows/pages.yml`

- [ ] **Step 1: Write the Pages workflow**

`.github/workflows/pages.yml` — builds firmware, generates flasher assets into `site/firmware/`, uploads `site/` as the Pages artifact, deploys. Mirrors the toolchain pin in `release.yml`:

```yaml
name: Deploy Website

on:
  push:
    branches: [inkpoint]
    paths:
      - 'site/**'
      - 'scripts/make_web_flasher_assets.py'
      - '.github/workflows/pages.yml'
      - 'platformio.ini'
  release:
    types: [published]
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: true

jobs:
  build-deploy:
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deploy.outputs.page_url }}
    steps:
      - uses: actions/checkout@v6
        with:
          submodules: recursive

      - uses: actions/setup-python@v6
        with:
          python-version: '3.14'

      - name: Install uv
        uses: astral-sh/setup-uv@v7
        with:
          version: "latest"
          enable-cache: false

      - name: Install PlatformIO Core
        run: uv pip install --system -U https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip

      - name: Cache PlatformIO packages
        uses: actions/cache@v4
        with:
          path: ~/.platformio
          key: pio-${{ runner.os }}-${{ hashFiles('platformio.ini') }}
          restore-keys: pio-${{ runner.os }}-

      - name: Build firmware (gh_release)
        run: pio run -e gh_release

      - name: Locate boot_app0.bin
        id: bootapp0
        run: echo "path=$(find ~/.platformio/packages -name boot_app0.bin | head -1)" >> "$GITHUB_OUTPUT"

      - name: Read version
        id: ver
        run: echo "version=$(grep -E '^version =' platformio.ini | head -1 | awk '{print $3}')" >> "$GITHUB_OUTPUT"

      - name: Generate web flasher assets
        run: |
          python3 scripts/make_web_flasher_assets.py \
            --build-dir .pio/build/gh_release \
            --boot-app0 "${{ steps.bootapp0.outputs.path }}" \
            --version "${{ steps.ver.outputs.version }}" \
            --out-dir site/firmware \
            --esptool "$(python3 -c 'import shutil,glob,os;print(shutil.which("esptool.py") or glob.glob(os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py"))[0])')"

      - name: Validate manifest
        run: python3 -c "import json; json.load(open('site/firmware/manifest.json')); print('manifest OK')"

      - uses: actions/configure-pages@v5

      - uses: actions/upload-pages-artifact@v3
        with:
          path: site

      - id: deploy
        uses: actions/deploy-pages@v4
```

- [ ] **Step 2: Lint the workflow YAML locally**

Run: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/pages.yml')); print('yaml OK')"`
Expected: `yaml OK` (install pyyaml first if missing: `pip install pyyaml`)

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/pages.yml
git commit -m "ci(site): build firmware + deploy website to GitHub Pages"
```

- [ ] **Step 4: Enable Pages (one-time, manual — flag for the user)**

This cannot be done from code. The repo owner must, in **GitHub → Settings → Pages**, set **Source = GitHub Actions**. Document this as a required manual step; the workflow's `deploy-pages` will fail until it is set. Verify after enabling:

Run: `gh api repos/hudsonbrendon/inkpoint/pages 2>&1 | python3 -c "import sys,json; d=json.load(sys.stdin); print('build_type:', d.get('build_type'))"`
Expected: `build_type: workflow` once enabled. (Before enabling, the API returns 404 — that is the signal to toggle the setting.)

---

## Task 5: Push, deploy, and verify the live flasher

**Files:** none (operational).

- [ ] **Step 1: Push the branch**

Run: `git push origin inkpoint`
Expected: push succeeds; the `Deploy Website` workflow starts (it triggers on `site/**` changes).

- [ ] **Step 2: Watch the workflow to completion**

Run: `gh run list --workflow "Deploy Website" --repo hudsonbrendon/inkpoint --limit 1` then `gh run watch <run-id> --repo hudsonbrendon/inkpoint`
Expected: all steps green; `deploy` step outputs a `page_url`.

- [ ] **Step 3: Verify the site is live and the manifest is served**

Run:
```bash
URL=$(gh api repos/hudsonbrendon/inkpoint/pages --jq .html_url)
curl -s -o /dev/null -w "index:%{http_code}\n" "$URL"
curl -s -o /dev/null -w "install:%{http_code}\n" "${URL}install.html"
curl -s -o /dev/null -w "manifest:%{http_code}\n" "${URL}firmware/manifest.json"
curl -s "${URL}firmware/manifest.json" | python3 -c "import sys,json; d=json.load(sys.stdin); print('version:', d['version'], 'chip:', d['builds'][0]['chipFamily'])"
```
Expected: `index:200`, `install:200`, `manifest:200`, and the version/chip line printing the current version and `ESP32-C3`.

- [ ] **Step 4: Human-tester verification (flag for the user — needs hardware)**

Document for the user (cannot be automated here):
1. Open the live `install.html` in **Chrome or Edge on desktop**.
2. Connect an X4 (and separately an X3) via USB-C, awake on the home screen.
3. Click **Flash InkPoint via USB**, pick the serial port, confirm the flash completes.
4. Device reboots into InkPoint; confirm correct rendering on each panel (X4 800×480, X3 528×792).
5. Confirm the SD-card method link points at the latest release `firmware.bin`.

---

## Self-Review Notes (already reconciled)

- **Spec coverage:** static site on Pages (Tasks 1–2, 4) ✓; web flasher esp-web-tools (Task 2) ✓; merged flashable image + manifest, since releases only ship `firmware.bin` (Task 3) ✓; X3+X4 from one auto-detecting binary, device picker informational (Task 2 copy + `app.js`) ✓; install docs USB + SD + OTA (Task 2) ✓; GitHub Pages hosting via Actions, same-origin binaries to avoid CORS (Task 4) ✓.
- **Out of scope (separate sub-plans):** font builder, unlocker, debug controls, roadmap/extra content pages. Stated in the scope boundary.
- **Type/path consistency:** the install button references `firmware/manifest.json`; the generator writes `--out-dir site/firmware` → `site/firmware/manifest.json` and `merged-factory.bin`; the manifest's `parts[0].path` is `merged-factory.bin` (relative to the manifest, same dir) ✓. The workflow uploads `path: site`, so the deployed paths are `/install.html` and `/firmware/manifest.json` ✓.
- **Offsets:** bootloader `0x0`, partitions `0x8000`, boot_app0 `0xe000`, firmware `0x10000` — matches `partitions.csv` (app0 @ 0x10000) and ESP32-C3 layout ✓.
- **Known manual step:** Pages Source = GitHub Actions must be toggled once by the owner (Task 4 Step 4) — flagged, not assumed.
- **Known external dependency:** esp-web-tools is loaded from unpkg CDN. If offline-resilience is later desired, vendor the script into `site/assets/` — noted, not required for v1.
```
