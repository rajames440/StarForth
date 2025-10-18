#!/usr/bin/env bash
# =====================================================================
#  Quark Integrator: StarForth → StarshipOS
#  Runs automatically pre-push to copy changed files atomically.
#  Handles whitelist, blacklist, and quarantine.
# =====================================================================

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${STARSHIP_ROOT:-$HOME/CLionProjects/StarshipOS}"
MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

WHITELIST="$MAINT/whitelist.txt"
BLACKLIST="$MAINT/blacklist.txt"
MERGEFILES="$MAINT/mergefiles.txt"

# ---------------------------------------------------------------------
#  Colors
# ---------------------------------------------------------------------
RESET="\033[0m"
BLACK_ON_WHITE="\033[30;47m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

# ---------------------------------------------------------------------
#  Generate file list automatically if missing
# ---------------------------------------------------------------------
if [[ ! -f "$MERGEFILES" || ! -s "$MERGEFILES" ]]; then
  echo "[INFO] Generating merge list..."
  git diff --name-only HEAD~1 > "$MERGEFILES"
fi

echo -e "\n🛸 Quark sez: commencing atomic file handoff (StarForth → StarshipOS)...\n"
mkdir -p "$QUAR"

# ---------------------------------------------------------------------
#  Main file loop
# ---------------------------------------------------------------------
while IFS= read -r FILE; do
  [[ -z "$FILE" ]] && continue
  SRC="$ROOT/$FILE"
  DEST="$TARGET/$FILE"
  QDST="$QUAR/$FILE"

  # Skip non-existent source files (e.g. deletions)
  [[ ! -f "$SRC" ]] && continue

  # Check blacklist first
  if grep -qE "$FILE" "$BLACKLIST"; then
    echo -e "${BLACK_ON_WHITE}[skipping]${RESET} $FILE"
    continue
  fi

  # Whitelist
  if grep -qxF "$FILE" "$WHITELIST" || grep -qE "^${FILE}$" "$WHITELIST"; then
    echo -e "${GREEN}[Copying...]${RESET} $FILE"
    mkdir -p "$(dirname "$DEST")"
    cp -f "$SRC" "$DEST"
    continue
  fi

  # Everything else → quarantine
  echo -e "${ORANGE}[Quarantining...]${RESET} $FILE"
  mkdir -p "$(dirname "$QDST")"
  cp -f "$SRC" "$QDST"
done < "$MERGEFILES"

# ---------------------------------------------------------------------
#  Summary report
# ---------------------------------------------------------------------
echo -e "\n🧾 First Officer’s Report:"
echo "  • Target:     $TARGET"
echo "  • Quarantine: $QUAR"
echo "  • Merge list: $(wc -l < "$MERGEFILES") files processed"
echo
echo "Awaiting Cappy’s signature: Captain Bob ✍️"
echo "------------------------------------------------------------------"
