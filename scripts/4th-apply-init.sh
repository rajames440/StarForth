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

# scripts/apply-init.sh — interactive wrapper for tools/apply-init
# Reads ./conf/init-4.4th and writes its Block N sections into a selected disk image.

set -euo pipefail

# --- Layout ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TOOLS_DIR="${REPO_ROOT}/tools"
DISKS_DIR="${REPO_ROOT}/disks"
CONF_DIR="${REPO_ROOT}/conf"

SRC_C="${TOOLS_DIR}/apply_init.c"
BIN="${TOOLS_DIR}/apply-init"

# Defaults
IN_DEFAULT="${CONF_DIR}/init.4th"
FBS_DEFAULT="1024"
START_GUARD_DEFAULT="0"      # bump to 1024 if you want to default-guard boot area
END_GUARD_DEFAULT="-1"       # -1 = no upper guard

BOLD="$(printf '\033[1m')"
DIM="$(printf '\033[2m')"
RESET="$(printf '\033[0m')"

die(){ echo "ERROR: $*" >&2; exit 1; }
have_cmd(){ command -v "$1" >/dev/null 2>&1; }

pick_cc(){
  if have_cmd gcc; then echo gcc
  elif have_cmd cc; then echo cc
  else die "No C compiler found (need gcc or cc)."
  fi
}

stat_mtime(){
  stat -c '%Y' "$1" 2>/dev/null || stat -f '%m' "$1"
}

need_rebuild(){
  local src="$1" bin="$2"
  [[ ! -f "$bin" ]] && return 0
  [[ ! -f "$src" ]] && return 1
  local s b; s="$(stat_mtime "$src")"; b="$(stat_mtime "$bin")"
  [[ "$s" -gt "$b" ]] && return 0 || return 1
}

build_apply(){
  [[ -f "$SRC_C" ]] || die "Missing source: $SRC_C"
  mkdir -p "$TOOLS_DIR"
  local CC FLAGS
  CC="$(pick_cc)"
  FLAGS="-std=gnu99 -D_FILE_OFFSET_BITS=64 -O2 -Wall -Wextra -Werror"
  echo "${DIM}Building: $CC $FLAGS -o $BIN $SRC_C${RESET}"
  "$CC" $FLAGS -o "$BIN" "$SRC_C"
  echo "${DIM}Built $BIN${RESET}"
}

ensure_layout(){
  [[ -d "$DISKS_DIR" ]] || die "Missing disks directory: $DISKS_DIR"
  [[ -d "$CONF_DIR"  ]] || mkdir -p "$CONF_DIR"
  if need_rebuild "$SRC_C" "$BIN"; then build_apply; fi
  [[ -x "$BIN" ]] || die "apply-init not executable: $BIN"
}

ask_yes_no(){
  local prompt="$1" default="${2:-y}" ans def_disp="Y/n"
  [[ "$default" == "n" ]] && def_disp="y/N"
  read -r -p "$prompt [$def_disp] " ans || true
  ans="${ans:-$default}"
  case "$ans" in y|Y|yes|YES) return 0 ;; n|N|no|NO) return 1 ;; *) echo "y or n, please." >&2; ask_yes_no "$prompt" "$default" ;; esac
}

list_disks(){
  find "$DISKS_DIR" -maxdepth 1 -type f -printf "%f\n" | sort
}

pick_disk(){
  echo "${BOLD}Select a disk image from ${DISKS_DIR}:${RESET}"
  mapfile -t DISKS < <(list_disks)
  [[ "${#DISKS[@]}" -gt 0 ]] || die "No disk images found in $DISKS_DIR"
  local i=1
  for d in "${DISKS[@]}"; do printf "  %2d) %s\n" "$i" "$d"; i=$((i+1)); done
  local choice
  while :; do
    read -r -p "Enter choice [1-${#DISKS[@]}]: " choice || true
    [[ "$choice" =~ ^[0-9]+$ ]] || { echo "Not a number."; continue; }
    (( choice>=1 && choice<=${#DISKS[@]} )) || { echo "Out of range."; continue; }
    DISK_CHOSEN="${DISKS_DIR}/${DISKS[$((choice-1))]}"
    break
  done
  echo "Selected: ${BOLD}${DISK_CHOSEN}${RESET}"
}

ask_int(){
  local prompt="$1" default="$2" __out="$3" val
  read -r -p "$prompt (default ${default}): " val || true
  val="${val:-$default}"
  [[ "$val" =~ ^-?[0-9]+$ ]] || { echo "Not a valid integer: $val"; ask_int "$prompt" "$default" "$__out"; return; }
  printf -v "$__out" "%s" "$val"
}

collect_params(){
  echo
  read -r -p "Path to init.4th [default ${IN_DEFAULT}]: " IN_PATH || true
  IN_PATH="${IN_PATH:-$IN_DEFAULT}"
  [[ -f "$IN_PATH" ]] || die "Input file not found: $IN_PATH"

  echo
  ask_int "Forth block size (FBS)" "$FBS_DEFAULT" FBS
  FBS_FLAG="--fbs=${FBS}"

  echo
  ask_int "Guard: minimum block to allow (start)" "$START_GUARD_DEFAULT" START_GUARD
  ask_int "Guard: maximum block to allow (end, -1 for none)" "$END_GUARD_DEFAULT" END_GUARD

  echo
  if ask_yes_no "Truncate block text if longer than FBS? (--clip)" "n"; then
    CLIP_FLAG="--clip"
  else
    CLIP_FLAG=""
  fi

  if ask_yes_no "Dry run (no writes)?" "n"; then
    DRY_FLAG="--dry-run"
  else
    DRY_FLAG=""
  fi

  if ask_yes_no "Verify after write (read-back & compare)?" "y"; then
    VERIFY_FLAG="--verify"
  else
    VERIFY_FLAG=""
  fi

  if ask_yes_no "Verbose logging?" "n"; then
    VERBOSE_FLAG="--verbose"
  else
    VERBOSE_FLAG=""
  fi
}

build_command(){
  CMD=( "$BIN"
        "--img=${DISK_CHOSEN}"
        "--in=${IN_PATH}"
        "$FBS_FLAG"
  )
  # Guards
  [[ "$START_GUARD" != "0" ]] && CMD+=( "--start=${START_GUARD}" ) || CMD+=( "--start=0" )
  [[ "$END_GUARD" != "-1" ]] && CMD+=( "--end=${END_GUARD}" )
  # Toggles
  [[ -n "$CLIP_FLAG" ]]    && CMD+=( "$CLIP_FLAG" )
  [[ -n "$DRY_FLAG"  ]]    && CMD+=( "$DRY_FLAG" )
  [[ -n "$VERIFY_FLAG" ]]  && CMD+=( "$VERIFY_FLAG" )
  [[ -n "$VERBOSE_FLAG" ]] && CMD+=( "$VERBOSE_FLAG" )
}

main(){
  ensure_layout
  pick_disk
  collect_params
  build_command

  echo
  echo "${BOLD}About to run:${RESET}"
  printf "  %q " "${CMD[@]}"; echo
  echo
  if ask_yes_no "Proceed?" "y"; then
    "${CMD[@]}"
    echo
    echo "${BOLD}Done.${RESET} Applied init.4th → ${DISK_CHOSEN}"
  else
    echo "Aborted."
    exit 1
  fi
}

main "$@"
