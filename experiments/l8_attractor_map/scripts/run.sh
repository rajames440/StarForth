#!/usr/bin/env bash
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

set -euo pipefail

echo "══════════════════════════════════════════════════════════════"
echo "   StarForth L8 Attractor Experiment — run.sh"
echo "══════════════════════════════════════════════════════════════"
echo

# -----------------------------------------------------------------------------
# Resolve directories
# -----------------------------------------------------------------------------
EXPERIMENT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LATEST_FILE="$EXPERIMENT_ROOT/.latest_results_dir"

# ABSOLUTE CONF DIRECTORY (no guessing)
INIT_DIR="/home/rajames/CLionProjects/StarForth/conf"

# -----------------------------------------------------------------------------
# Ensure setup.sh has been executed
# -----------------------------------------------------------------------------
if [ ! -f "$LATEST_FILE" ]; then
    echo "ERROR: No results directory found."
    echo "Run ./scripts/setup.sh first."
    exit 1
fi

RESULTS_DIR="$(cat "$LATEST_FILE")"

if [ ! -d "$RESULTS_DIR" ]; then
    echo "ERROR: Stored results directory does not exist:"
    echo "  $RESULTS_DIR"
    exit 1
fi

echo "Using results directory:"
echo "  $RESULTS_DIR"
echo

# -----------------------------------------------------------------------------
# Load matrix
# -----------------------------------------------------------------------------
MATRIX_FILE="$RESULTS_DIR/l8_attractor_run_matrix.csv"

if [ ! -f "$MATRIX_FILE" ]; then
    echo "ERROR: Matrix file missing:"
    echo "  $MATRIX_FILE"
    exit 1
fi

echo "Run matrix:"
echo "  $MATRIX_FILE"
echo

# -----------------------------------------------------------------------------
# Heartbeat output file
# -----------------------------------------------------------------------------
HB_FILE="$RESULTS_DIR/heartbeat_all.csv"
echo "Heartbeat output will go to:"
echo "  $HB_FILE"
echo

echo "Beginning execution…"
echo

# -----------------------------------------------------------------------------
# MAIN LOOP — NO SUBSHELLS
# -----------------------------------------------------------------------------
{
    # Skip header line from CSV
    read

    while IFS=',' read -r run_id init_script replicate
    do
        echo "------------------------------------------------------------"
        echo " Run $run_id: $init_script (replicate $replicate)"
        echo "------------------------------------------------------------"

        # Strip quotes
        clean_init="$(echo "$init_script" | tr -d '"')"

        # Validate path
        if [ ! -f "$INIT_DIR/$clean_init" ]; then
            echo "ERROR: Missing init script: $INIT_DIR/$clean_init"
            exit 1
        fi

        # Overwrite init.4th
        cp "$INIT_DIR/$clean_init" "$INIT_DIR/init.4th"

        # Execute the VM (don't fail if no heartbeat output yet)
        ../../build/amd64/fastest/starforth --doe \
            1>/dev/null \
            2>>"$HB_FILE" || true

        echo

    done
} < "$MATRIX_FILE"

echo
echo "══════════════════════════════════════════════════════════════"
echo "           L8 Attractor Experiment Completed"
echo "══════════════════════════════════════════════════════════════"
echo

# -----------------------------------------------------------------------------
# Generate workload mapping for analysis
# -----------------------------------------------------------------------------
echo "Generating workload mapping..."

WORKLOAD_MAP="$RESULTS_DIR/workload_mapping.csv"

echo "run_id,init_script,replicate,hb_start_row,hb_end_row,hb_row_count" > "$WORKLOAD_MAP"

# Extract restart row numbers (VM startup markers)
mapfile -t RESTART_ROWS < <(awk -F',' '$3 > 1000000000 {print NR}' "$HB_FILE")

# Read matrix and assign heartbeat row ranges
tail -n +2 "$MATRIX_FILE" | while IFS=',' read -r run_id init_script replicate; do
    # Array index (0-based)
    idx=$((run_id - 1))

    start_row=${RESTART_ROWS[$idx]}

    # End row is one before next restart (or EOF)
    if [ $idx -lt 179 ]; then
        end_row=$((${RESTART_ROWS[$((idx + 1))]} - 1))
    else
        # Last run goes to EOF
        end_row=$(wc -l < "$HB_FILE")
    fi

    row_count=$((end_row - start_row + 1))

    echo "$run_id,$init_script,$replicate,$start_row,$end_row,$row_count"
done >> "$WORKLOAD_MAP"

echo "  → $WORKLOAD_MAP"
echo
echo "Workload mapping complete. Analysis ready."
