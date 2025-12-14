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

#
#                                  ***   StarForth / StarshipOS   ***
#   rundisk.sh  Path: scripts/rundisk.sh
#   SPDX-License-Identifier: CC0-1.0
#
#   Purpose:
#     Select or create a fixed-size RAW disk image and print its absolute path
#     to stdout (and nothing else), suitable for command substitution:
#       ./build/starforth --disk-img="$(./scripts/rundisk.sh)"
#
#   Behavior:
#     - If --disk is provided and exists, prints it.
#     - Otherwise, lists existing *.img under <repo>/disks and lets you choose
#       or create a new one (interactive unless --yes).
#     - New images are created with qemu-img raw format (fixed-size).
#     - All informational output goes to stderr; stdout is ONLY the path.
#
#   Public Domain (CC0-1.0). No warranty.
#

set -euo pipefail

# --- resolve repo root relative to this script, not $PWD ---
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DISKDIR="$ROOT/disks"

HOSTNAME="$(hostname)"
USERNAME="$(whoami)"

# --- defaults ---
DEFAULT_SIZE_MB=500
DEFAULT_VERSION="1.0"
QEMU_IMG_BIN="${QEMU_IMG_BIN:-qemu-img}"

# --- usage (to stderr) ---
usage() {
  cat >&2 <<'EOF'
Usage: rundisk.sh [options]

Options:
  -s, --size MB        Disk size in megabytes when creating (default: 500)
  -v, --version VER    Artifact version for new disk name (default: 1.0)
  -d, --disk PATH      Use this existing raw disk image (no creation)
  -y, --yes            Non-interactive (accept defaults; pick first existing
                       disk or create new with defaults if none exist)
  -h, --help           Show this help (to stderr)

Notes:
  - Prints ONLY the absolute disk path to stdout on success.
  - All other output (prompts, errors) goes to stderr.
  - RAW image is fixed-size; we never grow/truncate it.
EOF
}

# --- parse args ---
SIZE_MB="$DEFAULT_SIZE_MB"
VERSION="$DEFAULT_VERSION"
DISK_PATH=""
ASSUME_YES=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -s|--size)     SIZE_MB="${2:?}"; shift 2 ;;
    -v|--version)  VERSION="${2:?}"; shift 2 ;;
    -d|--disk)     DISK_PATH="${2:?}"; shift 2 ;;
    -y|--yes)      ASSUME_YES=1; shift ;;
    -h|--help)     usage; exit 0 ;;
    *) echo "rundisk.sh: unknown arg: $1" >&2; usage; exit 1 ;;
  esac
done

# --- sanity checks ---
if ! command -v "$QEMU_IMG_BIN" >/dev/null 2>&1; then
  echo "ERROR: $QEMU_IMG_BIN not found in PATH" >&2
  exit 1
fi

mkdir -p "$DISKDIR"

# normalize to absolute path
abspath() {
  local p="$1"
  if [[ "$p" != /* ]]; then
    p="$(cd -- "$(dirname -- "$p")" && pwd)/$(basename -- "$p")"
  fi
  printf '%s\n' "$p"
}

# --- return a chosen or newly created disk path ---
select_or_create_disk() {
  local disk=""
  if [[ -n "$DISK_PATH" ]]; then
    disk="$(abspath "$DISK_PATH")"
    if [[ ! -f "$disk" ]]; then
      echo "ERROR: specified --disk does not exist: $disk" >&2
      exit 1
    fi
    printf '%s\n' "$disk"
    return 0
  fi

  mapfile -d '' existing < <(find "$DISKDIR" -maxdepth 1 -type f -name "*.img" -print0 2>/dev/null || true)

  if (( ${#existing[@]} == 0 )); then
    # No existing disks -> create new
    if [[ "$ASSUME_YES" -eq 0 ]]; then
      echo "[*] No existing disks found under $DISKDIR" >&2
      read -rp "Enter disk size in MB [${SIZE_MB}]: " size_input >&2 || true
      SIZE_MB="${size_input:-$SIZE_MB}"
      read -rp "Enter version [${VERSION}]: " version_input >&2 || true
      VERSION="${version_input:-$VERSION}"
    fi
    disk="$DISKDIR/$HOSTNAME-$USERNAME-$VERSION.img"
    disk="$(abspath "$disk")"
    if [[ ! -f "$disk" ]]; then
      echo "[*] Creating RAW disk: $disk (${SIZE_MB}M)" >&2
      "$QEMU_IMG_BIN" create -f raw "$disk" "${SIZE_MB}M" >/dev/null
      echo "[+] Created: $disk" >&2
    fi
    printf '%s\n' "$disk"
    return 0
  fi

  # Existing disks present
  if [[ "$ASSUME_YES" -eq 1 ]]; then
    disk="$(abspath "${existing[0]}")"
    printf '%s\n' "$disk"
    return 0
  fi

  echo "[*] Existing disks in $DISKDIR:" >&2
  local i=1
  for f in "${existing[@]}"; do
    printf '  %2d) %s\n' "$i" "$(basename "$f")" >&2
    ((i++))
  done
  printf '  %2d) %s\n' "$i" "Create new disk" >&2

  local choice
  while true; do
    read -rp "Choose [1-$i]: " choice >&2 || true
    if [[ -z "$choice" ]]; then continue; fi
    if (( choice >= 1 && choice < i )); then
      disk="$(abspath "${existing[choice-1]}")"
      printf '%s\n' "$disk"
      return 0
    elif (( choice == i )); then
      read -rp "Enter disk size in MB [${SIZE_MB}]: " size_input >&2 || true
      SIZE_MB="${size_input:-$SIZE_MB}"
      read -rp "Enter version [${VERSION}]: " version_input >&2 || true
      VERSION="${version_input:-$VERSION}"
      disk="$DISKDIR/$HOSTNAME-$USERNAME-$VERSION.img"
      disk="$(abspath "$disk")"
      if [[ ! -f "$disk" ]]; then
        echo "[*] Creating RAW disk: $disk (${SIZE_MB}M)" >&2
        "$QEMU_IMG_BIN" create -f raw "$disk" "${SIZE_MB}M" >/dev/null
        echo "[+] Created: $disk" >&2
      fi
      printf '%s\n' "$disk"
      return 0
    else
      echo "Invalid selection." >&2
    fi
  done
}

# --- emit absolute path ONLY to stdout ---
select_or_create_disk
