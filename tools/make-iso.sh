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

set -euo pipefail

if [ $# -ne 3 ]; then
    echo "Usage: $0 <loader.efi> <kernel.elf> <output.img>"
    exit 1
fi

LOADER_EFI="$1"
KERNEL_ELF="$2"
OUTPUT_IMG="$3"

for f in "$LOADER_EFI" "$KERNEL_ELF"; do
    [ -f "$f" ] || { echo "Missing file: $f"; exit 1; }
done

# Check for required tools
for cmd in mformat mcopy; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: $cmd not found. Install with: sudo apt install mtools"
        exit 1
    fi
done

# Image size: 34MB (enough for FAT32 with proper cluster count)
# FAT32 requires at least 65525 clusters, so we need enough space
IMG_SIZE_MB=34

echo "Creating ${IMG_SIZE_MB}MB UEFI boot image..."
echo "  Loader EFI: $LOADER_EFI"
echo "  Kernel ELF: $KERNEL_ELF"
echo "  Output IMG: $OUTPUT_IMG"

# Create empty image
dd if=/dev/zero of="$OUTPUT_IMG" bs=1M count=$IMG_SIZE_MB status=none

# Format as FAT32 (superfloppy - no partition table)
# -F forces FAT, -T 65536 forces enough sectors for FAT32
echo "Formatting as FAT32..."
mformat -i "$OUTPUT_IMG" -v STARKERNEL -F -T 65536 ::

# Install files to standard UEFI removable media path
echo "Installing boot files..."
mmd -i "$OUTPUT_IMG" ::/EFI
mmd -i "$OUTPUT_IMG" ::/EFI/BOOT
mcopy -i "$OUTPUT_IMG" "$LOADER_EFI" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$OUTPUT_IMG" "$KERNEL_ELF" ::/kernel.elf

# Verify contents
echo ""
echo "Image contents:"
mdir -i "$OUTPUT_IMG" ::/
mdir -i "$OUTPUT_IMG" ::/EFI/BOOT/

echo ""
echo "UEFI boot image created: $OUTPUT_IMG"
echo "  Size: $(du -h "$OUTPUT_IMG" | cut -f1)"
echo ""
echo "To test in QEMU:"
echo "  qemu-system-x86_64 \\"
echo "    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \\"
echo "    -drive if=pflash,format=raw,file=build/amd64/kernel/OVMF_VARS.fd \\"
echo "    -drive format=raw,file=$OUTPUT_IMG \\"
echo "    -serial stdio -m 1024"
echo ""
echo "To write to USB for hardware testing:"
echo "  sudo dd if=$OUTPUT_IMG of=/dev/sdX bs=4M conv=fsync status=progress"
echo ""
echo "NOTE: This is a dev image. For firmware requiring GPT+ESP, use installer workflow."
