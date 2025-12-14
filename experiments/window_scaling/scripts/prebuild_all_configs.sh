#!/bin/bash
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

# ============================================================================
# Pre-Build All VM Configurations (Window-Only)
# ============================================================================
# Purpose: Build one binary per window size. Loop activation is chosen at
#          runtime by the Jacquard selector, so no loop-specific builds.
#          Outputs are stored under experiments/bin/ (survives make clean).
#
# Benefit: Avoids redundant rebuilds; reuse binaries across all DoF cases.
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXPERIMENT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$(cd "$EXPERIMENT_DIR/../.." && pwd)"
EXPERIMENTS_DIR="$PROJECT_DIR/experiments"
PREBUILT_DIR="$EXPERIMENTS_DIR/bin"

ARCH="amd64"
TARGET="fastest"

# Window sizes
WINDOWS=(512 768 1024 1536 2048 3072 4096 6144 8192 12288 16384 24576 32768 49152 65536)
TOTAL_CONFIGS=${#WINDOWS[@]}

echo "============================================================================"
echo "Pre-Building VM Binaries (Window-Only)"
echo "============================================================================"
echo "Window sizes:   ${#WINDOWS[@]}"
echo "Total builds:   $TOTAL_CONFIGS"
echo "Output:         $PREBUILT_DIR"
echo "Loops:          Runtime (Jacquard selector)"
echo "============================================================================"
echo ""

# Create prebuilt directory (safe from make clean)
mkdir -p "$PREBUILT_DIR"

# Track progress
BUILT=0
FAILED=0
START_TIME=$(date +%s)

for W in "${WINDOWS[@]}"; do
  BUILT=$((BUILT + 1))

  CONFIG_NAME="w${W}"
  CONFIG_DIR="$PREBUILT_DIR/$CONFIG_NAME"

  echo "[$BUILT/$TOTAL_CONFIGS] Building: W=$W"

  # Clean build directory (safe - doesn't touch experiments/bin/)
  make -C "$PROJECT_DIR" clean >/dev/null 2>&1 || true

  if make -C "$PROJECT_DIR" \
      ARCH="$ARCH" \
      TARGET="$TARGET" \
      ROLLING_WINDOW_SIZE="$W" \
      >/dev/null 2>&1; then

    mkdir -p "$CONFIG_DIR"
    cp "$PROJECT_DIR/build/$ARCH/$TARGET/starforth" "$CONFIG_DIR/starforth"

    cat > "$CONFIG_DIR/config.txt" <<EOF
window_size=$W
loops=jacquard_runtime
built=$(date +"%Y-%m-%d %H:%M:%S")
EOF

    echo "  ✓ Built: $CONFIG_DIR/starforth"
  else
    echo "  ✗ FAILED: W=$W"
    FAILED=$((FAILED + 1))
  fi

  echo ""
done

# Summary
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo "============================================================================"
echo "Pre-Build Complete"
echo "============================================================================"
echo "Successful builds:  $((BUILT - FAILED))"
echo "Failed builds:      $FAILED"
echo "Elapsed time:       ${MINUTES}m${SECONDS}s"
echo "Binary directory:   $PREBUILT_DIR"
echo "Disk usage:         $(du -sh "$PREBUILT_DIR" 2>/dev/null | cut -f1)"
echo "============================================================================"
echo ""

if [ $FAILED -gt 0 ]; then
  echo "⚠ WARNING: Some builds failed. Check output above."
  exit 1
fi

echo "✓ All $((BUILT - FAILED)) window configurations built successfully!"
echo ""
echo "Binaries stored in: $PREBUILT_DIR"
echo "  (Safe from 'make clean')"
echo ""
echo "Next step: Run experiment using pre-built binaries"
echo "  cd $SCRIPT_DIR"
echo "  ./run_window_sweep_prebuilt.sh"
echo ""

exit 0
