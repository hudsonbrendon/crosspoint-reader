#!/usr/bin/env python3
"""Build esp-web-tools assets (merged-factory.bin + manifest.json) from a
gh_release PlatformIO build. Run after `pio run -e gh_release`.

Usage:
  python3 scripts/make_web_flasher_assets.py \
      --build-dir .pio/build/gh_release \
      --boot-app0 <path/to/boot_app0.bin> \
      --version 1.7.1 \
      --out-dir site/firmware

  # Multi-token esptool (project python + esptool script):
  python3 scripts/make_web_flasher_assets.py \
      --build-dir .pio/build/gh_release \
      --boot-app0 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
      --version 1.7.1 \
      --out-dir site/firmware \
      --esptool "~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py"
"""
import argparse
import json
import os
import shlex
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

    # Split --esptool on whitespace so both single-token ("esptool.py") and
    # multi-token ("python /path/to/esptool.py") invocations work correctly.
    # subprocess treats a list's first element as the executable name, so a
    # two-word string passed as one element would fail with FileNotFoundError.
    esptool_tokens = shlex.split(args.esptool)

    # ESP32-C3 factory layout. Offsets are safety-critical — do not change.
    #   0x00000  bootloader
    #   0x08000  partition table
    #   0x0e000  boot_app0 (OTA selection)
    #   0x10000  app0 firmware  ← matches partitions.csv app0 offset
    cmd = esptool_tokens + [
        "--chip", "esp32c3", "merge_bin",
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
