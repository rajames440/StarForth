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

# flash-rpi4.sh - Write Raspberry Pi 4B image to SD card
# Usage: flash-rpi4.sh <image.img> [device]

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <image.img> [device]"
    exit 1
fi

IMAGE="$1"
DEVICE="${2:-}"

if [ ! -f "$IMAGE" ]; then
    echo "Error: Image not found: $IMAGE"
    exit 1
fi

# Auto-detect SD card if not specified
if [ -z "$DEVICE" ]; then
    echo "Detecting SD card devices..."
    lsblk -o NAME,SIZE,TYPE,MOUNTPOINT | grep -E "^sd[a-z]|^mmcblk[0-9]"
    echo ""
    read -p "Enter device (e.g., /dev/sdb or /dev/mmcblk0): " DEVICE
fi

if [ ! -b "$DEVICE" ]; then
    echo "Error: Device not found: $DEVICE"
    exit 1
fi

# Safety check
echo "WARNING: This will DESTROY all data on $DEVICE"
lsblk "$DEVICE"
echo ""
read -p "Are you sure? Type 'yes' to continue: " CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# Unmount any mounted partitions
echo "Unmounting partitions..."
sudo umount "${DEVICE}"* 2>/dev/null || true

# Write image
echo "Writing image to $DEVICE..."
sudo dd if="$IMAGE" of="$DEVICE" bs=4M status=progress conv=fsync

# Sync and eject
echo "Syncing..."
sync

echo "Done! SD card ready for Raspberry Pi 4B"
echo "Connect serial console: screen /dev/ttyUSB0 115200"
