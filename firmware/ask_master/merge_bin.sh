#!/usr/bin/env bash
# Merge bootloader + partitions + firmware into a single .bin for M5Burner v3.
# Requires: esptool.py (pip install esptool), PlatformIO build artifacts.

set -euo pipefail

cd "$(dirname "$0")"

ENV=m5stack-cardputer
BUILD_DIR=".pio/build/${ENV}"
OUT="ask-master-cardputer.bin"

if [ ! -d "${BUILD_DIR}" ]; then
  echo "Build artifacts missing. Run: pio run -e ${ENV}" >&2
  exit 1
fi

if ! command -v esptool.py >/dev/null 2>&1; then
  echo "esptool.py not found. Install with: pip install esptool" >&2
  exit 1
fi

esptool.py --chip esp32s3 merge_bin \
  -o "${OUT}" \
  --flash_mode dio \
  --flash_size 8MB \
  0x0000  "${BUILD_DIR}/bootloader.bin" \
  0x8000  "${BUILD_DIR}/partitions.bin" \
  0x10000 "${BUILD_DIR}/firmware.bin"

echo "Wrote ${OUT}"
ls -lh "${OUT}"
