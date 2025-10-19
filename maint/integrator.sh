#!/usr/bin/env bash
# ============================================================
#  Quark Integrator: StarForth → StarshipOS (Captain’s Orders)
#  Rules: Cap't Bob Sez:
#    1. Never mkdir into target — both repos must already exist.
#    2. Blacklist = Law. Anything matching it is ignored.
#    3. If StarForth file exists AND same relative path exists in StarshipOS → overwrite.
#    4. Otherwise → quarantine it in StarForth/maint/quarantine.
# ============================================================

set -euo pipefail
shopt -s nullglob

ROOT="$(cd "$(dirname "$0")/.." && pwd)"                         # StarForth root
STARSHIPOS_ROOT="${STARSHIPOS_ROOT:-$HOME/CLionProjects/StarshipOS}"
DEST_BASE="$STARSHIPOS_ROOT/l4/pkg/starforth/server"
MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

BLACKLIST="$MAINT/blacklist.txt"
MERGEFILES="$MAINT/mergefiles.txt"

# ANSI
RESET="\033[0m"
RED="\033[1;31m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

echo -e "\n🛰️  Quark webhook engaged — StarForth → StarshipOS (Final Cut)\n"
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

  # Compute StarshipOS path — straight 1:1 mapping under server/
  RELATIVE="${FILE#server/}"
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
if [[ -d "$STARSHIPOS_ROOT/.git" ]]; then
  (cd "$STARSHIPOS_ROOT" && git update-index --really-refresh >/dev/null 2>&1 || true)
fi

echo -e "\n🧾 First Officer’s Report:"
echo "  • Destination base: $DEST_BASE"
echo "  • Quarantine:       $QUAR"
echo "  • Merge list:       $(wc -l < "$MERGEFILES" 2>/dev/null || echo 0) files processed"
echo
echo "Awaiting Cappy’s signature: Captain Bob ✍️"
echo
