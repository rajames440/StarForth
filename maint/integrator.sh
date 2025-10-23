#!/usr/bin/env bash
#
#                                  ***   StarForth   ***
#
#  integrator.sh- FORTH-79 Standard and ANSI C99 ONLY
#  Modified by - rajames
#  Last modified - 2025-10-23T10:55:24.633-04
#
#  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
#
#  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
#  To the extent possible under law, the author(s) have dedicated all copyright and related
#  and neighboring rights to this software to the public domain worldwide.
#  This software is distributed without any warranty.
#
#  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#  /home/rajames/CLionProjects/StarForth/maint/integrator.sh
#

# ============================================================
#  Quark Integrator Reverse: StarshipOS → StarForth
#  Rules: Cap't Bob Sez:
#    1. Never mkdir into target — both repos must already exist.
#    2. Blacklist = Law. Anything matching it is ignored.
#    3. If StarshipOS file exists AND same relative path exists in StarForth → overwrite.
#    4. Otherwise → quarantine it in StarshipOS/maint/quarantine.
# ============================================================

set -euo pipefail
shopt -s nullglob

ROOT="$(cd "$(dirname "$0")/.." && pwd)"                         # StarshipOS root
STARFORTH_ROOT="${STARFORTH_ROOT:-$HOME/CLionProjects/StarForth}"
DEST_BASE="$STARFORTH_ROOT"
MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

BLACKLIST="$MAINT/blacklist.txt"
MERGEFILES="$MAINT/mergefiles.txt"

# ANSI
RESET="\033[0m"
RED="\033[1;31m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

echo -e "\n🛰️  Quark webhook engaged — StarshipOS → StarForth (Reverse Sync)\n"
mkdir -p "$QUAR"

[[ -s "$MERGEFILES" ]] || {
  echo "[INFO] No mergefiles list found — generating diff from last commit."
  (cd "$ROOT" && git diff --name-only HEAD~1) > "$MERGEFILES" || true
}

while IFS= read -r FILE; do
  [[ -z "$FILE" ]] && continue
  SRC="$ROOT/$FILE"
  [[ -f "$SRC" ]] || continue

  # Rule 2: blacklist enforcement (filter out empty lines and comments)
  if grep -Eq -f <(sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d' "$BLACKLIST") <<< "$FILE"; then
    echo -e "${RED}[Blocked]${RESET} $FILE"
    continue
  fi

  # Compute StarForth path — strip l4/pkg/starforth/server/ prefix
  RELATIVE="${FILE#l4/pkg/starforth/server/}"
  DEST="$DEST_BASE/$RELATIVE"

  # Rule 3 & 4: drop logic
  if [[ -f "$DEST" ]]; then
    echo -e "${GREEN}[Overwriting]${RESET} $RELATIVE"
    cp -f "$SRC" "$DEST"
  else
    echo -e "${ORANGE}[Quarantining]${RESET} $FILE"
    mkdir -p "$QUAR/$(dirname "$FILE")"
    cp -f "$SRC" "$QUAR/$FILE"
  fi

done < "$MERGEFILES"

# Gentle poke to IDEs / Git index
if [[ -d "$STARFORTH_ROOT/.git" ]]; then
  (cd "$STARFORTH_ROOT" && git update-index --really-refresh >/dev/null 2>&1 || true)
fi

echo -e "\n🧾 First Officer's Reverse Report:"
echo "  • Destination base: $DEST_BASE"
echo "  • Quarantine:       $QUAR"
echo "  • Merge list:       $(wc -l < "$MERGEFILES" 2>/dev/null || echo 0) files processed"
echo
echo "Awaiting Cappy's signature: Captain Bob ✍️"
echo