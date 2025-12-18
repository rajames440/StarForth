#!/usr/bin/env bash
# make-iso.sh - Create bootable ISO image for amd64
# Usage: make-iso.sh <starkernel.efi> <output.iso>

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 <starkernel.efi> <output.iso>"
    exit 1
fi

EFI_BINARY="$1"
OUTPUT_ISO="$2"

if [ ! -f "$EFI_BINARY" ]; then
    echo "Error: EFI binary not found: $EFI_BINARY"
    exit 1
fi

echo "Creating ISO image for amd64..."
echo "  EFI binary: $EFI_BINARY"
echo "  Output ISO: $OUTPUT_ISO"

# Create ESP directory structure
ESP_DIR=$(mktemp -d)
trap "rm -rf $ESP_DIR" EXIT

mkdir -p "$ESP_DIR/EFI/BOOT"
cp "$EFI_BINARY" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"

# Generate ISO with xorriso
if command -v xorriso >/dev/null 2>&1; then
    xorriso -as mkisofs \
        -R -f \
        -e EFI/BOOT/BOOTX64.EFI \
        -no-emul-boot \
        -o "$OUTPUT_ISO" \
        "$ESP_DIR"
    echo "ISO created successfully: $OUTPUT_ISO"
else
    echo "Error: xorriso not found. Install with: sudo apt install xorriso"
    exit 1
fi
