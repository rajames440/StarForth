#!/usr/bin/env bash
# Quark Integrator: StarForth → StarshipOS
# Runs automatically pre-push to copy changed files atomically, one file at a time.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${STARSHIP_ROOT:-$HOME/StarshipOS/starforth/server}"
MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

WHITELIST="$MAINT/whitelist.txt"
BLACKLIST="$MAINT/blacklist.txt"
MERGEFILES="$MAINT/mergefiles.txt"   # List of files staged for commit/push

# ANSI colors
RESET="\033[0m"
BLACK_ON_WHITE="\033[30;47m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

echo -e "\n🛸 Quark sez: commencing atomic file handoff...\n"
mkdir -p "$QUAR"

while IFS= read -r FILE; do
  [[ -z "$FILE" ]] && continue

  if grep -qxF "$FILE" "$BLACKLIST"; then
    echo -e "${BLACK_ON_WHITE}[skipping]${RESET} $FILE"
    continue

  elif grep -qxF "$FILE" "$WHITELIST"; then
    echo -e "${GREEN}[Copying...]${RESET} $FILE"
    mkdir -p "$TARGET/$(dirname "$FILE")"
    cp -f "$ROOT/$FILE" "$TARGET/$FILE"

  else
    echo -e "${ORANGE}[Quarantining...]${RESET} $FILE"
    mkdir -p "$QUAR/$(dirname "$FILE")"
    cp -f "$ROOT/$FILE" "$QUAR/$FILE"
  fi
done < "$MERGEFILES"

echo -e "\n🧾 First Officer’s Report:"
echo "  • Target:     $TARGET"
echo "  • Quarantine: $QUAR"
echo "  • Merge list: $(wc -l < "$MERGEFILES") files processed"
echo
echo "Awaiting Cappy’s signature: Captain Bob ✍️"
