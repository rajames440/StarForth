#!/usr/bin/env bash
# ============================================================
#  Quark Integrator: StarForth → StarshipOS
#  Atomic, validated, file-by-file synchronization.
#  Captain Bob edition — precision surgery, no bullshit.
# ============================================================

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${STARSHIPOS_ROOT:-$HOME/CLionProjects/StarshipOS}"
MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

WHITELIST="$MAINT/bridge.whitelist"
BLACKLIST="$MAINT/bridge.blacklist"
MERGEFILES="$MAINT/mergefiles.txt"

# ANSI colors
RESET="\033[0m"
BLACK_ON_WHITE="\033[30;47m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

echo -e "\n🛰️  Quark webhook engaged — StarForth → StarshipOS handoff beginning...\n"

mkdir -p "$QUAR"

# --- Mapping function ---
map_path() {
  local f="$1"
  case "$f" in
    include/*) echo "l4/pkg/starforth/server/$f" ;;
    src/*)     echo "l4/pkg/starforth/server/$f" ;;
    conf/*)    echo "l4/pkg/starforth/server/$f" ;;
    *)         echo "l4/pkg/starforth/server/$f" ;;
  esac
}

# --- Safe copy enforcement ---
safe_copy() {
  local src="$1"
  local rel="$2"
  local mapped
  mapped=$(map_path "$rel")
  local dest="$TARGET/$mapped"
  local dest_dir
  dest_dir=$(dirname "$dest")

  # allow only inside legitimate subtree
  if [[ "$dest" == "$TARGET/l4/pkg/starforth/server/"* ]]; then
    if [[ -d "$dest_dir" && -f "$src" ]]; then
      cp -f --remove-destination "$src" "$dest"
    else
      echo -e "${ORANGE}[Quarantining unsafe directory]${RESET} $rel"
      mkdir -p "$QUAR/$(dirname "$rel")"
      cp -f "$src" "$QUAR/$rel"
    fi
  else
    echo -e "${ORANGE}[Quarantining outside target scope]${RESET} $rel"
    mkdir -p "$QUAR/$(dirname "$rel")"
    cp -f "$src" "$QUAR/$rel"
  fi
}

# --- Main loop ---
if [[ ! -s "$MERGEFILES" ]]; then
  echo "[WARN] No $MERGEFILES file. Nothing to sync."
  exit 0
fi

while IFS= read -r FILE; do
  [[ -z "$FILE" ]] && continue

  if grep -qxF "$FILE" "$BLACKLIST"; then
    echo -e "${BLACK_ON_WHITE}[skipping]${RESET} $FILE"
    continue

  elif grep -qxF "$FILE" "$WHITELIST"; then
    echo -e "${GREEN}[Copying...]${RESET} $FILE"
    safe_copy "$ROOT/$FILE" "$FILE"

  else
    echo -e "${ORANGE}[Quarantining...]${RESET} $FILE"
    mkdir -p "$QUAR/$(dirname "$FILE")"
    cp -f "$ROOT/$FILE" "$QUAR/$FILE"
  fi
done < "$MERGEFILES"

# --- Refresh index on the receiving repo ---
if [[ -d "$TARGET/.git" ]]; then
  (cd "$TARGET" && git update-index --really-refresh >/dev/null 2>&1 || true)
fi

echo -e "\n🧾 First Officer’s Report:"
echo "  • Target:     $TARGET"
echo "  • Quarantine: $QUAR"
echo "  • Merge list: $(wc -l < "$MERGEFILES") files processed"
echo
echo "Awaiting Cappy’s signature: Captain Bob ✍️"
echo
