#!/usr/bin/env bash
# =====================================================================
#  Quark Integrator: StarForth → StarshipOS
#  One-file-at-a-time copy with blacklist/whitelist/quarantine.
#  FIXES:
#   - Proper blacklist regex application
#   - Whitelist glob patterns (include/**, src/**, …)
#   - Correct DEST root: StarshipOS/starforth/server/
#   - Auto-generate merge list and pre-filter maint/ + Makefile*
# =====================================================================

set -euo pipefail

# Enable useful pattern matching for whitelist
shopt -s globstar nullglob extglob

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# DEST root is the StarshipOS *server* subtree
STARSHIP_ROOT="${STARSHIP_ROOT:-$HOME/CLionProjects/StarshipOS}"
TARGET="$STARSHIP_ROOT/starforth/server"

MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

WHITELIST="$MAINT/whitelist.txt"     # glob patterns (e.g., include/**)
BLACKLIST="$MAINT/blacklist.txt"     # regex patterns
MERGEFILES="$MAINT/mergefiles.txt"   # relative file paths per commit/push

# Colors
RESET="\033[0m"
BLACK_ON_WHITE="\033[30;47m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

echo -e "\n🛸 Quark sez: commencing atomic file handoff (StarForth → StarshipOS)...\n"
mkdir -p "$QUAR"

# ---------------------------------------------------------------------
# Generate merge list if missing or empty
# ---------------------------------------------------------------------
if [[ ! -s "$MERGEFILES" ]]; then
  echo "[INFO] Generating merge list from last commit..."
  (cd "$ROOT" && git diff --name-only HEAD~1) > "$MERGEFILES" || true
fi

# Normalize and pre-filter: strip leading "./", drop maint/ and Makefile* up front
# (so maint changes like maint/integrator.sh never enter the loop)
TMP_LIST="$(mktemp)"
sed -E 's|^\./||' "$MERGEFILES" \
  | grep -Ev '^(maint/|Makefile($|[.].*))' \
  | sed '/^\s*$/d' > "$TMP_LIST"
mv "$TMP_LIST" "$MERGEFILES"

# Helpers --------------------------------------------------------------
is_blacklisted() {
  local f="$1"
  [[ -f "$BLACKLIST" ]] && grep -Eq -f "$BLACKLIST" <<< "$f"
}

is_whitelisted() {
  local f="$1"
  [[ -f "$WHITELIST" ]] || return 1
  # Iterate whitelist patterns (globs). Empty lines / comments ignored.
  while IFS= read -r pat; do
    [[ -z "$pat" || "$pat" =~ ^\s*# ]] && continue
    # Match using shell glob semantics
    if [[ "$f" == $pat ]]; then
      return 0
    fi
  done < "$WHITELIST"
  return 1
}

# Main loop ------------------------------------------------------------
while IFS= read -r FILE; do
  [[ -z "$FILE" ]] && continue

  SRC="$ROOT/$FILE"
  DEST="$TARGET/$FILE"
  QDST="$QUAR/$FILE"

  # Skip if source file no longer exists (e.g., deletions)
  [[ -f "$SRC" ]] || continue

  if is_blacklisted "$FILE"; then
    echo -e "${BLACK_ON_WHITE}[skipping]${RESET} $FILE"
    continue

  elif is_whitelisted "$FILE"; then
    echo -e "${GREEN}[Copying...]${RESET} $FILE"
    mkdir -p "$(dirname "$DEST")"
    cp -f "$SRC" "$DEST"
    continue

  else
    echo -e "${ORANGE}[Quarantining...]${RESET} $FILE"
    mkdir -p "$(dirname "$QDST")"
    cp -f "$SRC" "$QDST"
  fi
done < "$MERGEFILES"

# Summary --------------------------------------------------------------
echo -e "\n🧾 First Officer’s Report:"
echo "  • Target:     $TARGET"
echo "  • Quarantine: $QUAR"
echo "  • Merge list: $(wc -l < "$MERGEFILES" 2>/dev/null || echo 0) files processed"
echo
echo "Awaiting Cappy’s signature: Captain Bob ✍️"
echo "------------------------------------------------------------------"
