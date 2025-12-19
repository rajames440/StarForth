#!/usr/bin/env bash
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
