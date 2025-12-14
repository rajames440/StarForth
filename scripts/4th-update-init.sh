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

# scripts/update-init.sh — interactive wrapper for tools/extract-init
# Scans a raw image in ./disks and writes a canonical ./conf/init-4.4th

set -euo pipefail

# --- Layout ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TOOLS_DIR="${REPO_ROOT}/tools"
DISKS_DIR="${REPO_ROOT}/disks"
CONF_DIR="${REPO_ROOT}/conf"

SRC_C_PRIMARY="${TOOLS_DIR}/extract_init.c"
BIN_PRIMARY="${TOOLS_DIR}/extract-init"

OUT_DEFAULT="${CONF_DIR}/init.4th"
FBS_DEFAULT="1024"
START_DEFAULT="0"
END_DEFAULT="-1"

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

need_rebuild(){
  # return 0 if rebuild needed
  local src="$1" bin="$2"
  [[ ! -f "$bin" ]] && return 0
  [[ ! -f "$src" ]] && return 1
  local src_mtime bin_mtime
  src_mtime=$(stat -c '%Y' "$src" 2>/dev/null || stat -f '%m' "$src")
  bin_mtime=$(stat -c '%Y' "$bin" 2>/dev/null || stat -f '%m' "$bin")
  [[ "$src_mtime" -gt "$bin_mtime" ]] && return 0 || return 1
}

build_extractor(){
  [[ -f "$SRC_C_PRIMARY" ]] || die "Missing source: $SRC_C_PRIMARY"
  mkdir -p "$TOOLS_DIR"
  local CC FLAGS OUT
  CC=$(pick_cc)
  FLAGS="-std=c99 -O2 -Wall -Wextra -Werror"
  OUT="$BIN_PRIMARY"
  echo "${DIM}Building extractor: $CC $FLAGS -o $OUT $SRC_C_PRIMARY${RESET}"
  "$CC" $FLAGS -o "$OUT" "$SRC_C_PRIMARY"
  echo "${DIM}Built $OUT${RESET}"
}

ensure_layout(){
  [[ -d "$DISKS_DIR" ]] || die "Missing disks directory: $DISKS_DIR"
  [[ -d "$CONF_DIR" ]]  || mkdir -p "$CONF_DIR"
  if need_rebuild "$SRC_C_PRIMARY" "$BIN_PRIMARY"; then
    build_extractor
  fi
  [[ -x "$BIN_PRIMARY" ]] || die "Extractor not executable: $BIN_PRIMARY"
}

ask_yes_no(){
  local prompt="$1" default="${2:-y}" ans def_disp="Y/n"
  [[ "$default" == "n" ]] && def_disp="y/N"
  read -r -p "$prompt [$def_disp] " ans || true
  ans="${ans:-$default}"
  case "$ans" in
    y|Y|yes|YES) return 0 ;;
    n|N|no|NO)   return 1 ;;
    *) echo "Please answer y or n." >&2; ask_yes_no "$prompt" "$default" ;;
  esac
}

list_disks(){
  # List regular files; prefer common extensions but include all
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

pick_mode(){
  echo
  echo "${BOLD}Header mode:${RESET}"
  echo "  1) strict  — require \"(-\" at byte 0"
  echo "  2) loose   — allow BOM/whitespace before \"(-\""
  local choice
  while :; do
    read -r -p "Choose mode [1-2, default 1]: " choice || true
    choice="${choice:-1}"
    case "$choice" in
      1) MODE_FLAG="";    MODE="strict"; break ;;
      2) MODE_FLAG="--loose"; MODE="loose";  break ;;
      *) echo "Pick 1 or 2."; continue ;;
    esac
  done
  echo "Mode: ${BOLD}${MODE}${RESET}"
}

ask_int(){
  local prompt="$1" default="$2" __outvar="$3" val
  read -r -p "$prompt (default ${default}): " val || true
  val="${val:-$default}"
  [[ "$val" =~ ^-?[0-9]+$ ]] || { echo "Not a valid integer: $val"; ask_int "$prompt" "$default" "$__outvar"; return; }
  printf -v "$__outvar" "%s" "$val"
}

collect_params(){
  echo
  ask_int "Start block" "$START_DEFAULT" START_BLOCK
  ask_int "End block (-1 for EOF)" "$END_DEFAULT" END_BLOCK

  echo
  if ask_yes_no "Require closing ')' for the opening '(-' in the same block?" "y"; then
    REQUIRE_CLOSE="--require-close"
  else
    REQUIRE_CLOSE=""
  fi

  echo
  if ask_yes_no "Set a max number of picked blocks?" "n"; then
    ask_int "Max picks" "1" MAX_PICKS_VAL
    MAX_PICKS="--max=${MAX_PICKS_VAL}"
  else
    MAX_PICKS=""
  fi

  echo
  ask_int "Forth block size (FBS)" "$FBS_DEFAULT" FBS
  FBS_FLAG="--fbs=${FBS}"

  echo
  read -r -p "Output path for init.4th [default ${OUT_DEFAULT}]: " OUT_PATH || true
  OUT_PATH="${OUT_PATH:-$OUT_DEFAULT}"
  mkdir -p "$(dirname "$OUT_PATH")"
}

build_command(){
  CMD=( "$BIN_PRIMARY"
        "--img=${DISK_CHOSEN}"
        "--out=${OUT_PATH}"
        "$FBS_FLAG"
        "--start=${START_BLOCK}"
        "--end=${END_BLOCK}"
  )
  [[ -n "$MODE_FLAG" ]]     && CMD+=( "$MODE_FLAG" )
  [[ -n "$REQUIRE_CLOSE" ]] && CMD+=( "$REQUIRE_CLOSE" )
  [[ -n "${MAX_PICKS:-}" ]] && CMD+=( "$MAX_PICKS" )
}

main(){
  ensure_layout
  pick_disk
  pick_mode
  collect_params
  build_command

  echo
  echo "${BOLD}About to run:${RESET}"
  printf "  %q " "${CMD[@]}"; echo
  echo
  if ask_yes_no "Proceed?" "y"; then
    "${CMD[@]}"
    echo
    echo "${BOLD}Done.${RESET} Wrote: ${OUT_PATH}"
  else
    echo "Aborted."
    exit 1
  fi
}

main "$@"
