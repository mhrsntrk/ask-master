#!/bin/bash
set -e

# Build merged binary for M5Burner v3
# Usage: ./merge_bin.sh [environment]
# Default environment: m5stack-cardputer

ENV="${1:-m5stack-cardputer}"
PIO_BUILD_DIR=".pio/build/$ENV"
OUTPUT="ask-master-cardputer.bin"

echo "Building firmware for $ENV..."

# Build with PlatformIO
pio run -e "$ENV"

echo "Merging binaries for M5Burner..."

# Merge bootloader + partition table + app into single bin
# Addresses for ESP32-S3 with 8MB flash (M5Stack Cardputer ADV)
esptool.py --chip esp32s3 merge_bin \
  -o "$OUTPUT" \
  --flash_mode dio \
  --flash_size 8MB \
  0x0000 "$PIO_BUILD_DIR/bootloader.bin" \
  0x8000 "$PIO_BUILD_DIR/partitions.bin" \
  0x10000 "$PIO_BUILD_DIR/firmware.bin"

echo "Merged binary created: $OUTPUT"
echo "Size: $(ls -lh "$OUTPUT" | awk '{print $5}')"
echo ""
echo "Upload this file to M5Burner:"
echo "  M5Burner → USER CUSTOM → Publish → Select $OUTPUT"
