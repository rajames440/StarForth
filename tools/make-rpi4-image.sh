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

# make-rpi4-image.sh - Create bootable SD card image for Raspberry Pi 4B
# Usage: make-rpi4-image.sh <starkernel.efi> <output.img>

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 <starkernel.efi> <output.img>"
    exit 1
fi

EFI_BINARY="$1"
OUTPUT_IMG="$2"

if [ ! -f "$EFI_BINARY" ]; then
    echo "Error: EFI binary not found: $EFI_BINARY"
    exit 1
fi

echo "Creating Raspberry Pi 4B SD card image..."
echo "  EFI binary: $EFI_BINARY"
echo "  Output image: $OUTPUT_IMG"

# Image size: 256MB
IMAGE_SIZE=$((256 * 1024 * 1024))

# Create empty image
dd if=/dev/zero of="$OUTPUT_IMG" bs=1M count=256 status=progress

# Create FAT32 partition
echo "Creating partition table..."
parted -s "$OUTPUT_IMG" mklabel msdos
parted -s "$OUTPUT_IMG" mkpart primary fat32 1MiB 100%
parted -s "$OUTPUT_IMG" set 1 boot on

# Set up loop device
LOOP_DEV=$(sudo losetup -f --show "$OUTPUT_IMG")
trap "sudo losetup -d $LOOP_DEV" EXIT

sudo partprobe "$LOOP_DEV"
PART_DEV="${LOOP_DEV}p1"

# Format partition
sudo mkfs.vfat -F 32 -n STARKERNEL "$PART_DEV"

# Mount and copy files
MOUNT_DIR=$(mktemp -d)
trap "sudo umount $MOUNT_DIR 2>/dev/null || true; rm -rf $MOUNT_DIR; sudo losetup -d $LOOP_DEV" EXIT

sudo mount "$PART_DEV" "$MOUNT_DIR"

# Download Raspberry Pi 4B firmware (if not already present)
FIRMWARE_DIR="$HOME/.cache/starforth/rpi4-firmware"
if [ ! -d "$FIRMWARE_DIR" ]; then
    echo "Downloading Raspberry Pi 4B firmware..."
    mkdir -p "$FIRMWARE_DIR"
    cd "$FIRMWARE_DIR"
    wget -q https://github.com/raspberrypi/firmware/raw/master/boot/bootcode.bin
    wget -q https://github.com/raspberrypi/firmware/raw/master/boot/start4.elf
    wget -q https://github.com/raspberrypi/firmware/raw/master/boot/fixup4.dat
fi

# Copy firmware files
sudo cp "$FIRMWARE_DIR"/{bootcode.bin,start4.elf,fixup4.dat} "$MOUNT_DIR/"

# Create config.txt
sudo tee "$MOUNT_DIR/config.txt" >/dev/null <<EOF
# Raspberry Pi 4B configuration for StarKernel
arm_64bit=1
kernel=starkernel.efi
enable_uart=1
uart_2ndstage=1
EOF

# Copy kernel
sudo cp "$EFI_BINARY" "$MOUNT_DIR/starkernel.efi"

# Unmount and cleanup
sudo umount "$MOUNT_DIR"
sudo losetup -d "$LOOP_DEV"
trap - EXIT

echo "Raspberry Pi 4B image created successfully: $OUTPUT_IMG"
echo "Write to SD card with: sudo dd if=$OUTPUT_IMG of=/dev/sdX bs=4M status=progress"
