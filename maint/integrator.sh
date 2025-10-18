#!/usr/bin/env bash
# Quark Integrator: StarForth → StarshipOS
# Runs automatically pre-push to copy changed files atomically, one file at a time.

set -euo pipefail

# === PATH CONFIG ==============================================================
STARSHIP_ROOT="${STARSHIP_ROOT:-$HOME/CLionProjects/StarshipOS}"
SRC_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_ROOT="$STARSHIP_ROOT/l4/pkg/starforth"
MAINT_DIR="$SRC_ROOT/maint"
LOG="$MAINT_DIR/integrator.log"
STAMP=$(date '+%F %T')

# === COLORS ===================================================================
RED='\033[1;31m'; GREEN='\033[1;32m'; BLUE='\033[1;34m'
CYAN='\033[1;36m'; PURPLE='\033[1;35m'; RESET='\033[0m'

echo -e "${CYAN}[$STAMP] Starting StarForth → StarshipOS sync${RESET}" | tee -a "$LOG"
[ -d "$DEST_ROOT" ] || { echo -e "${RED}ERROR:${RESET} Destination not found: $DEST_ROOT"; exit 1; }

# === FILE LISTS ===============================================================
CHANGED="$MAINT_DIR/files.changed"
WHITE="$MAINT_DIR/bridge.whitelist"
BLACK="$MAINT_DIR/bridge.blacklist"

[ -f "$CHANGED" ] || { echo "No $CHANGED file. Nothing to sync."; exit 0; }

mapfile -t files < <(grep -v '^#' "$CHANGED" | grep -Ff "$WHITE" | grep -vFf "$BLACK" || true)
[ ${#files[@]} -eq 0 ] && { echo "No files eligible for transfer." | tee -a "$LOG"; exit 0; }

# === COPY PHASE ===============================================================
for f in "${files[@]}"; do
  src="$SRC_ROOT/$f"
  dest="$DEST_ROOT/$f"
  mkdir -p "$(dirname "$dest")"
  cp -u "$src" "$dest"
  echo -e "${GREEN}  → copied:${RESET} $f" | tee -a "$LOG"
done

# === FUN QUARK QUOTE ==========================================================
quotes=(
  "Warp core integrity holding steady!"
  "Engage the entropy drives!"
  "Spacetime’s lookin’ sexy today, Captain."
  "Reality distortion field: synchronized."
  "Entropic vectors realigned for maximum chaos!"
)
quote="${quotes[$((RANDOM % ${#quotes[@]}))]}"

# === QUARK REPORT GENERATOR ===================================================
generate_quark_report() {
  local direction="$1"
  local commit_msg="$2"
  local officer_report="$3"
  local outfile="$4"
  local originator accent emblem reset cappy
  originator="🟣 **StarForth Command**"
  accent="\033[1;35m"; reset="\033[0m"
  emblem="🌀"; cappy="🧭 Captain Bob"
  local stamp; stamp=$(date '+%F %T')

  {
    echo -e "# ${emblem} Quark Report — $stamp"
    echo
    echo -e "╔════════════════════════════════════════════════════════════════╗"
    echo -e "║ $(echo -e \"${accent}QUARK SEZ:${reset}\")"
    echo -e "╠════════════════════════════════════════════════════════════════╣"
    echo -e "║ ${commit_msg}"
    echo -e "╠════════════════════════════════════════════════════════════════╣"
    echo -e "║ **FIRST OFFICER'S REPORT:**"
    echo -e "║ ${officer_report}"
    echo -e "╠════════════════════════════════════════════════════════════════╣"
    echo -e "║ **ACTIONS:** Awaiting ${cappy}'s signature."
    echo -e "╚════════════════════════════════════════════════════════════════╝"
    echo
    echo -e "> Originator: ${originator}"
    echo -e "> Transmission Path: **${direction}**"
    echo
    echo '---'
    echo -e "_Quark’s Log_: “Reality distortion field nominal. Entropy vectors in alignment.”"
  } > "$outfile"

  echo "Generated markdown Quark Report → $outfile"
}

# === GIT COMMIT ===============================================================
last_msg=$(git -C "$SRC_ROOT" log -1 --pretty=%B | head -n 1)
msg_prefix="Quark sez: [StarForth→StarshipOS] ⚙️💫"

cd "$DEST_ROOT"
git add "${files[@]}"
git commit -m "$(echo -e "$msg_prefix\n\n$quote\n\n$last_msg")" || true
echo -e "${PURPLE}Commit complete:${RESET} $msg_prefix" | tee -a "$LOG"

# === REPORT PHASE =============================================================
REPORT_PATH="$DEST_ROOT/.git/QUARK_REPORT.md"
OFFICER_REPORT="Subsystem sync successful. All thrusters green. Ready for warp."

generate_quark_report \
  "StarForth→StarshipOS" \
  "$last_msg" \
  "$OFFICER_REPORT" \
  "$REPORT_PATH"

echo -e "${CYAN}[$STAMP] Transmission logged:${RESET} $REPORT_PATH" | tee -a "$LOG"
echo -e "${GREEN}Mission accomplished.${RESET}"
