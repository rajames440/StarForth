#!/usr/bin/env bash
# ============================================================
#  Quark Integrator Reverse: StarshipOS → StarForth
#  Rules:
#    1. Never mkdir into source — StarForth must already exist.
#    2. Blacklist = Law. Anything matching it is ignored.
#    3. If StarshipOS file exists AND same relative path exists in StarForth → overwrite.
#    4. Otherwise → quarantine it in StarshipOS/maint/quarantine for Captain’s review.
# ============================================================

set -euo pipefail
shopt -s nullglob

ROOT="$(cd "$(dirname "$0")/.." && pwd)"                                           # StarshipOS root
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

  # Rule 2: blacklist enforcement
  if grep -Fqx -f "$BLACKLIST" <<< "$FILE"; then
    echo -e "${RED}[Blocked]${RESET} $FILE"
    continue
  fi

  # Compute StarForth destination path
  RELATIVE="${FILE#server/}"
  DEST="$DEST_BASE/$RELATIVE"

  # Rule 3 & 4
  if [[ -f "$DEST" ]]; then
    echo -e "${GREEN}[Overwriting]${RESET} $RELATIVE"
    cp -f "$SRC" "$DEST"
  else
    echo -e "${ORANGE}[Quarantining]${RESET} $FILE"
    mkdir -p "$QUAR/$(dirname "$FILE")"
    cp -f "$SRC" "$QUAR/$FILE"
  fi

done < "$MERGEFILES"

# Refresh IDE/Git index
if [[ -d "$STARFORTH_ROOT/.git" ]]; then
  (cd "$STARFORTH_ROOT" && git update-index --really-refresh >/dev/null 2>&1 || true)
fi

echo -e "\n🧾 First Officer’s Reverse Report:"
echo "  • Destination base: $DEST_BASE"
echo "  • Quarantine:       $QUAR"
echo "  • Merge list:       $(wc -l < "$MERGEFILES" 2>/dev/null || echo 0) files processed"
echo
echo "Awaiting Cappy’s signature: Captain Bob ✍️"
echo
