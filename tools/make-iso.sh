#!/usr/bin/env bash
#
#  StarForth — Steady-State Virtual Machine Runtime
#
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
# See the License for the specific language governing permissions and
# limitations under the License.
#
# StarForth — Steady-State Virtual Machine Runtime
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#

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
