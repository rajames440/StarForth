#!/bin/bash
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

# make-disk-image.sh - Create a GPT disk image with FAT32 ESP for UEFI boot
#
# This script creates a disk image WITHOUT using:
#   - loop devices (losetup)
#   - mount/umount
#   - sudo
#
# Instead it uses:
#   - dd (to create the blank image)
#   - sgdisk (to write GPT partition table)
#   - mtools (mformat/mmd/mcopy to populate FAT32 partition)
#
# SAFETY: Dry-run by default. Use --execute to actually create the image.

set -euo pipefail

# Defaults
IMG_PATH="out/starkernel.img"
BOOTLOADER=""
KERNEL=""
IMG_SIZE_MB=64
ESP_SIZE_MB=32
VERBOSE=0
DRY_RUN=1  # Safe by default

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

error() {
    echo -e "${RED}ERROR:${NC} $1" >&2
    exit 1
}

warn() {
    echo -e "${YELLOW}WARNING:${NC} $1" >&2
}

info() {
    echo -e "${GREEN}==>${NC} $1"
}

verbose() {
    if [[ $VERBOSE -eq 1 ]]; then
        echo "    $1"
    fi
}

# Run a command, or print it if dry-run
run() {
    if [[ $DRY_RUN -eq 1 ]]; then
        echo -e "${CYAN}[DRY-RUN]${NC} $*"
    else
        "$@"
    fi
}

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Create a GPT disk image with FAT32 EFI System Partition.

SAFETY: Dry-run by default. Use --execute to actually create the image.

Options:
  -o, --output FILE     Output image path (default: out/starkernel.img)
  -b, --bootloader FILE Path to BOOTX64.EFI (required)
  -k, --kernel FILE     Path to kernel.elf (required)
  -s, --size MB         Total image size in MB (default: 64)
  -e, --esp-size MB     ESP partition size in MB (default: 32)
  -x, --execute         Actually create the disk image (default: dry-run)
  -d, --dry-run         Show what would be done without doing it (default)
  -v, --verbose         Verbose output
  -h, --help            Show this help

Example (dry-run):
  $(basename "$0") -b build/amd64/kernel/starkernel_loader.efi \\
                   -k build/amd64/kernel/starkernel_kernel.elf

Example (execute):
  $(basename "$0") --execute -b build/amd64/kernel/starkernel_loader.efi \\
                             -k build/amd64/kernel/starkernel_kernel.elf

The resulting image can be written to USB with:
  sudo dd if=out/starkernel.img of=/dev/sdX bs=4M conv=fsync status=progress

WARNING: Replace /dev/sdX with your actual USB device. This will destroy all data!
EOF
}

check_dependencies() {
    local missing=()

    if ! command -v sgdisk &>/dev/null; then
        missing+=("sgdisk (package: gdisk)")
    fi

    if ! command -v mformat &>/dev/null; then
        missing+=("mformat (package: mtools)")
    fi

    if ! command -v mcopy &>/dev/null; then
        missing+=("mcopy (package: mtools)")
    fi

    if ! command -v mmd &>/dev/null; then
        missing+=("mmd (package: mtools)")
    fi

    if ! command -v dd &>/dev/null; then
        missing+=("dd (package: coreutils)")
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        error "Missing required tools:\n  ${missing[*]}\n\nInstall with:\n  sudo apt install gdisk mtools"
    fi
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -o|--output)
                IMG_PATH="$2"
                shift 2
                ;;
            -b|--bootloader)
                BOOTLOADER="$2"
                shift 2
                ;;
            -k|--kernel)
                KERNEL="$2"
                shift 2
                ;;
            -s|--size)
                IMG_SIZE_MB="$2"
                shift 2
                ;;
            -e|--esp-size)
                ESP_SIZE_MB="$2"
                shift 2
                ;;
            -x|--execute)
                DRY_RUN=0
                shift
                ;;
            -d|--dry-run)
                DRY_RUN=1
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                error "Unknown option: $1\nUse --help for usage."
                ;;
        esac
    done
}

validate_inputs() {
    if [[ -z "$BOOTLOADER" ]]; then
        error "Bootloader path required. Use -b or --bootloader."
    fi

    if [[ -z "$KERNEL" ]]; then
        error "Kernel path required. Use -k or --kernel."
    fi

    if [[ ! -f "$BOOTLOADER" ]]; then
        error "Bootloader not found: $BOOTLOADER"
    fi

    if [[ ! -f "$KERNEL" ]]; then
        error "Kernel not found: $KERNEL"
    fi

    if [[ $ESP_SIZE_MB -ge $IMG_SIZE_MB ]]; then
        error "ESP size ($ESP_SIZE_MB MB) must be smaller than image size ($IMG_SIZE_MB MB)"
    fi

    # Check that files will fit (rough estimate: need space for both + overhead)
    local bootloader_size
    local kernel_size
    local total_needed
    bootloader_size=$(stat -c%s "$BOOTLOADER")
    kernel_size=$(stat -c%s "$KERNEL")
    total_needed=$(( (bootloader_size + kernel_size + 1048576) / 1048576 )) # +1MB overhead, round up

    if [[ $total_needed -gt $ESP_SIZE_MB ]]; then
        error "ESP size ($ESP_SIZE_MB MB) too small for files (need at least $total_needed MB)"
    fi
}

create_disk_image() {
    info "Creating disk image: $IMG_PATH ($IMG_SIZE_MB MB)"

    # Create output directory if needed
    local out_dir
    out_dir=$(dirname "$IMG_PATH")
    run mkdir -p "$out_dir"

    # Remove existing image to avoid stale partition data
    run rm -f "$IMG_PATH"

    # Create blank disk image
    verbose "Creating ${IMG_SIZE_MB}MB blank image..."
    run dd if=/dev/zero of="$IMG_PATH" bs=1M count="$IMG_SIZE_MB" status=none

    if [[ $DRY_RUN -eq 0 ]]; then
        verbose "Image created: $(stat -c%s "$IMG_PATH") bytes"
    fi
}

create_gpt() {
    info "Creating GPT partition table"

    # Clear any existing partition table and create new GPT
    # Partition 1: EFI System Partition starting at sector 2048
    # Type EF00 = EFI System Partition
    verbose "Writing GPT with ESP partition (type EF00)..."

    run sgdisk --clear \
           --new=1:2048:+${ESP_SIZE_MB}M \
           --typecode=1:EF00 \
           --change-name=1:ESP \
           "$IMG_PATH"

    # Verify partition was created
    if [[ $DRY_RUN -eq 0 && $VERBOSE -eq 1 ]]; then
        verbose "Partition table:"
        sgdisk --print "$IMG_PATH"
    fi
}

format_esp() {
    info "Formatting ESP as FAT32"

    # Calculate partition offset
    # GPT partition 1 starts at LBA 2048
    # With 512 bytes/sector: 2048 * 512 = 1048576 bytes = 1MB
    local part_offset=1048576

    # mtools requires MTOOLS_SKIP_CHECK to work with image files
    # The @@offset syntax tells mtools to access partition at that byte offset
    verbose "Formatting partition at offset $part_offset..."

    # -F = FAT32
    # -v = volume label
    # -h = hidden sectors (heads) - set to match partition start
    # -s = sectors per track
    # Calculate partition size in sectors for mformat
    local esp_sectors=$(( ESP_SIZE_MB * 1024 * 1024 / 512 ))

    run env MTOOLS_SKIP_CHECK=1 mformat -i "${IMG_PATH}@@${part_offset}" \
        -F \
        -v "STARKERNEL" \
        -T "$esp_sectors" \
        ::

    verbose "FAT32 filesystem created"
}

populate_esp() {
    info "Populating ESP filesystem"

    local part_offset=1048576
    local img_part="${IMG_PATH}@@${part_offset}"

    # Create directory structure
    verbose "Creating /EFI/BOOT directory..."
    run env MTOOLS_SKIP_CHECK=1 mmd -i "$img_part" ::/EFI
    run env MTOOLS_SKIP_CHECK=1 mmd -i "$img_part" ::/EFI/BOOT

    # Copy bootloader
    verbose "Copying bootloader to /EFI/BOOT/BOOTX64.EFI..."
    run env MTOOLS_SKIP_CHECK=1 mcopy -i "$img_part" "$BOOTLOADER" ::/EFI/BOOT/BOOTX64.EFI

    # Copy kernel
    verbose "Copying kernel to /kernel.elf..."
    run env MTOOLS_SKIP_CHECK=1 mcopy -i "$img_part" "$KERNEL" ::/kernel.elf

    # Create startup.nsh (QEMU/OVMF shim only)
    # Real hardware ignores this and uses UEFI removable-media fallback path.
    # OVMF has a bug where it creates Boot#### entries pointing to raw devices
    # instead of properly loading \EFI\BOOT\BOOTX64.EFI from the ESP.
    verbose "Creating startup.nsh (QEMU/OVMF workaround)..."
    if [[ $DRY_RUN -eq 0 ]]; then
        local tmp_nsh
        tmp_nsh=$(mktemp)
        # Use printf with proper escaping for UEFI paths
        # map -r forces filesystem remapping
        printf 'map -r\r\nFS0:\\EFI\\BOOT\\BOOTX64.EFI\r\n' > "$tmp_nsh"
        MTOOLS_SKIP_CHECK=1 mcopy -i "$img_part" "$tmp_nsh" ::/startup.nsh
        rm -f "$tmp_nsh"
    else
        echo -e "${CYAN}[DRY-RUN]${NC} Create startup.nsh with: map -r && FS0:\\EFI\\BOOT\\BOOTX64.EFI"
    fi

    if [[ $DRY_RUN -eq 0 ]]; then
        info "ESP contents:"
        echo "/"
        MTOOLS_SKIP_CHECK=1 mdir -i "$img_part" ::/ | grep -v "^$" | sed 's/^/  /'
        echo "/EFI/BOOT/"
        MTOOLS_SKIP_CHECK=1 mdir -i "$img_part" ::/EFI/BOOT/ | grep -v "^$" | sed 's/^/  /'
    fi
}

print_summary() {
    if [[ $DRY_RUN -eq 1 ]]; then
        echo ""
        info "DRY-RUN complete. No changes were made."
        echo ""
        echo "To actually create the disk image, run with --execute:"
        echo "  $(basename "$0") --execute -b $BOOTLOADER -k $KERNEL"
        echo ""
        return
    fi

    local img_size
    local bootloader_size
    local kernel_size
    img_size=$(stat -c%s "$IMG_PATH")
    bootloader_size=$(stat -c%s "$BOOTLOADER")
    kernel_size=$(stat -c%s "$KERNEL")

    echo ""
    info "Disk image created successfully!"
    echo ""
    echo "  Image:      $IMG_PATH"
    echo "  Size:       $((img_size / 1048576)) MB"
    echo "  Bootloader: $BOOTLOADER ($bootloader_size bytes)"
    echo "  Kernel:     $KERNEL ($kernel_size bytes)"
    echo ""
    echo "Boot flow:"
    echo "  1. UEFI firmware loads \\EFI\\BOOT\\BOOTX64.EFI"
    echo "  2. BOOTX64.EFI loads \\kernel.elf"
    echo "  3. Control transfers to kernel entry point"
    echo ""
    echo "To boot in QEMU:"
    echo "  make -f Makefile.starkernel qemu"
    echo ""
    echo "To write to USB (DESTRUCTIVE - verify device!):"
    echo "  sudo dd if=$IMG_PATH of=/dev/sdX bs=4M conv=fsync status=progress"
    echo ""
}

main() {
    parse_args "$@"

    # Print mode warning
    if [[ $DRY_RUN -eq 1 ]]; then
        warn "DRY-RUN MODE (default) - no changes will be made"
        warn "Use --execute to actually create the disk image"
        echo ""
    else
        warn "EXECUTE MODE - disk image WILL be created/modified"
        echo ""
    fi

    check_dependencies
    validate_inputs
    create_disk_image
    create_gpt
    format_esp
    populate_esp
    print_summary
}

main "$@"
