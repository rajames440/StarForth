#!/usr/bin/env bash
# ============================================================
#  Quark Integrator: StarForth → StarshipOS (precise & safe)
#  Writes ONLY to: $STARSHIPOS_ROOT/l4/pkg/starforth/server/...
#  - No mkdir spray: refuses to create unknown dirs (quarantines)
#  - Normalizes any accidental "server/..." or "l4/pkg/..." inputs
#  - Blacklist = regex; Whitelist = globs; Everything else = quarantine
# ============================================================

set -euo pipefail
shopt -s globstar nullglob extglob

# --- Paths ----------------------------------------------------
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STARSHIPOS_ROOT="${STARSHIPOS_ROOT:-$HOME/CLionProjects/StarshipOS}"
DEST_BASE="$STARSHIPOS_ROOT/l4/pkg/starforth/server"   # canonical destination root

MAINT="$ROOT/maint"
QUAR="$MAINT/quarantine"

WHITELIST="$MAINT/bridge.whitelist"   # globs like: include/**, src/**, conf/init.4th
BLACKLIST="$MAINT/bridge.blacklist"   # regex patterns
MERGEFILES="$MAINT/mergefiles.txt"    # relative paths (from StarForth root)

# --- Colors ---------------------------------------------------
RESET="\033[0m"
BLACK_ON_WHITE="\033[30;47m"
GREEN="\033[1;32m"
ORANGE="\033[38;5;208m"

echo -e "\n🛰️  Quark webhook engaged — StarForth → StarshipOS (surgical mode)\n"
mkdir -p "$QUAR"

# --- Ensure we have a file list --------------------------------
if [[ ! -s "$MERGEFILES" ]]; then
  echo "[INFO] Generating merge list from last commit..."
  (cd "$ROOT" && git diff --name-only HEAD~1) > "$MERGEFILES" || true
fi

# --- Pre-filter: strip ./, drop maint/ and Makefile* upfront ----
TMP_LIST="$(mktemp)"
sed -E 's|^\./||' "$MERGEFILES" \
  | grep -Ev '^(maint/|Makefile($|[.].*))' \
  | sed '/^\s*$/d' > "$TMP_LIST"
mv "$TMP_LIST" "$MERGEFILES"

# --- Helpers ---------------------------------------------------
is_blacklisted() {
  local f="$1"
  [[ -f "$BLACKLIST" ]] && grep -Eq -f "$BLACKLIST" <<< "$f"
}

is_whitelisted() {
  local f="$1"
  [[ -f "$WHITELIST" ]] || return 1
  while IFS= read -r pat; do
    [[ -z "$pat" || "$pat" =~ ^\s*# ]] && continue
    if [[ "$f" == $pat ]]; then
      return 0
    fi
  done < "$WHITELIST"
  return 1
}

# Normalize any incoming path to a RELATIVE path under StarForth root,
# then map it to DEST under .../l4/pkg/starforth/server/REL.
normalize_rel_for_dest() {
  local f="$1"
  case "$f" in
    l4/pkg/starforth/server/*)
      # strip the prefix to get REL under server/
      echo "${f#l4/pkg/starforth/server/}"
      ;;
    starforth/server/*)
      # strip accidental short prefix
      echo "${f#starforth/server/}"
      ;;
    *)
      # expect StarForth-native paths (include/, src/, conf/, etc.)
      echo "$f"
      ;;
  esac
}

safe_copy() {
  local src_rel="$1"                      # e.g., include/vm.h
  local src_abs="$ROOT/$src_rel"
  local rel_for_dest
  rel_for_dest="$(normalize_rel_for_dest "$src_rel")"
  local dest_abs="$DEST_BASE/$rel_for_dest"
  local dest_dir
  dest_dir="$(dirname "$dest_abs")"

  # Only overwrite if:
  #  - destination stays under DEST_BASE
  #  - destination directory ALREADY exists
  #  - source file exists
  case "$dest_abs" in
    "$DEST_BASE"/*)
      if [[ -f "$src_abs" && -d "$dest_dir" ]]; then
        cp -f --remove-destination "$src_abs" "$dest_abs"
        return 0
      fi
      ;;
  esac

  # Otherwise quarantine
  echo -e "${ORANGE}[Quarantining unsafe/missing path]${RESET} $src_rel"
  mkdir -p "$QUAR/$(dirname "$src_rel")"
  cp -f "$src_abs" "$QUAR/$src_rel"
  return 1
}

# --- Main loop -------------------------------------------------
while IFS= read -r FILE; do
  [[ -z "$FILE" ]] && continue
  [[ -f "$ROOT/$FILE" ]] || continue   # skip deletions

  if is_blacklisted "$FILE"; then
    echo -e "${BLACK_ON_WHITE}[skipping]${RESET} $FILE"
    continue

  elif is_whitelisted "$FILE"; then
    echo -e "${GREEN}[Copying...]${RESET} $FILE"
    safe_copy "$FILE" || true

  else
    echo -e "${ORANGE}[Quarantining...]${RESET} $FILE"
    mkdir -p "$QUAR/$(dirname "$FILE")"
    cp -f "$ROOT/$FILE" "$QUAR/$FILE"
  fi
done < "$MERGEFILES"

# --- Make StarshipOS repo notice fresh bytes (optional refresh) ---
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
