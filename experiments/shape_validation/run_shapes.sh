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

# =====================================================================
# STARFORTH — EXPERIMENT C (Workload Shape Validation)
# =====================================================================
# Measures how configuration 0100101 behaves under four INIT workloads.
#
# Usage: ./run_shapes.sh <REPS>
#
# Examples:
#   ./run_shapes.sh 30      # quick exploratory
#   ./run_shapes.sh 300     # medium precision
#   ./run_shapes.sh 900     # paper-grade dataset
# =====================================================================

set -e

# =====================================================================
# CONFIGURATION
# =====================================================================
CONFIG_BITS="0100101"
CONFIG_DECIMAL=37

# Loop flags derived from CONFIG_BITS: 0100101
# Position:  L1 L2 L3 L4 L5 L6 L7
# Value:      0  1  0  0  1  0  1
L1=0
L2=1
L3=0
L4=0
L5=1
L6=0
L7=1

WORKLOADS=(
    "init.4th"
    "init-1.4th"
    "init-2.4th"
    "init-3.4th"
)

WORKLOAD_LABELS=(
    "baseline"
    "square_wave"
    "triangle"
    "damped_sine"
)

# =====================================================================
# SETUP
# =====================================================================
REPS=${1:-30}

if [ "$REPS" -lt 1 ]; then
    echo "ERROR: REPS must be >= 1"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONF_DIR="$PROJECT_DIR/conf"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$SCRIPT_DIR/shape_run_${TIMESTAMP}"

mkdir -p "$OUTPUT_DIR"

CONDITIONS_RAW="$OUTPUT_DIR/conditions_raw.txt"
CONDITIONS_RAND="$OUTPUT_DIR/conditions_randomized.txt"
RESULTS_FILE="$OUTPUT_DIR/shape_results.csv"
TMP_VM_OUT="$OUTPUT_DIR/vm_output.tmp"
TMP_TIME_OUT="$OUTPUT_DIR/time_output.tmp"

TOTAL_WORKLOADS=${#WORKLOADS[@]}
TOTAL_RUNS=$((TOTAL_WORKLOADS * REPS))

echo "======================================================"
echo "EXPERIMENT C: Workload Shape Validation"
echo "======================================================"
echo "Configuration:  $CONFIG_BITS (decimal $CONFIG_DECIMAL)"
echo "Loop flags:     L1=$L1 L2=$L2 L3=$L3 L4=$L4 L5=$L5 L6=$L6 L7=$L7"
echo "Workloads:      ${TOTAL_WORKLOADS}"
echo "Replicates:     $REPS"
echo "Total runs:     $TOTAL_RUNS"
echo "Output:         $OUTPUT_DIR"
echo "======================================================"

# =====================================================================
# VERIFY WORKLOAD FILES EXIST
# =====================================================================
echo ""
echo "Verifying workload files..."
for wfile in "${WORKLOADS[@]}"; do
    if [ ! -f "$CONF_DIR/$wfile" ]; then
        echo "ERROR: Workload file not found: $CONF_DIR/$wfile"
        exit 1
    fi
    echo "  OK: $wfile"
done

# =====================================================================
# BUILD (single build with fixed configuration)
# =====================================================================
echo ""
echo "Building StarForth with configuration $CONFIG_BITS..."
cd "$PROJECT_DIR"

make clean >/dev/null 2>&1 || true

if ! make ARCH=amd64 TARGET=fastest \
    ENABLE_LOOP_1_HEAT_TRACKING=$L1 \
    ENABLE_LOOP_2_ROLLING_WINDOW=$L2 \
    ENABLE_LOOP_3_LINEAR_DECAY=$L3 \
    ENABLE_LOOP_4_PIPELINING_METRICS=$L4 \
    ENABLE_LOOP_5_WINDOW_INFERENCE=$L5 \
    ENABLE_LOOP_6_DECAY_INFERENCE=$L6 \
    ENABLE_LOOP_7_ADAPTIVE_HEARTRATE=$L7 >/dev/null 2>&1; then
    echo "BUILD FAILED"
    exit 1
fi

STARFORTH="$PROJECT_DIR/build/amd64/fastest/starforth"
if [ ! -x "$STARFORTH" ]; then
    echo "ERROR: StarForth binary not found: $STARFORTH"
    exit 1
fi

echo "Build complete."

# =====================================================================
# GENERATE CONDITIONS
# =====================================================================
echo ""
echo "Generating conditions..."

> "$CONDITIONS_RAW"
for widx in "${!WORKLOADS[@]}"; do
    wfile="${WORKLOADS[$widx]}"
    wlabel="${WORKLOAD_LABELS[$widx]}"
    for rep in $(seq 1 $REPS); do
        echo "$widx,$wfile,$wlabel,$rep"
    done
done >> "$CONDITIONS_RAW"

shuf "$CONDITIONS_RAW" > "$CONDITIONS_RAND"

echo "  conditions_raw.txt:        $(wc -l < "$CONDITIONS_RAW") lines"
echo "  conditions_randomized.txt: $(wc -l < "$CONDITIONS_RAND") lines"

# =====================================================================
# CSV HEADER
# =====================================================================
# Experiment metadata + external timing + VM --doe metrics
echo "timestamp,run_id,workload_shape,config_bits,replicate,runtime_ns,doe_csv" > "$RESULTS_FILE"

# =====================================================================
# BACKUP & RESTORE INIT FILE
# =====================================================================
ORIGINAL_INIT="$CONF_DIR/init.4th"
BACKUP_INIT="$OUTPUT_DIR/init_original.4th.backup"

if [ -f "$ORIGINAL_INIT" ]; then
    cp "$ORIGINAL_INIT" "$BACKUP_INIT"
    echo ""
    echo "Backed up original init.4th"
fi

cleanup() {
    if [ -f "$BACKUP_INIT" ]; then
        cp "$BACKUP_INIT" "$ORIGINAL_INIT"
        echo ""
        echo "Restored original init.4th"
    fi
    rm -f "$TMP_VM_OUT" "$TMP_TIME_OUT"
}
trap cleanup EXIT

# =====================================================================
# RUN EXPERIMENT
# =====================================================================
echo ""
echo "Starting experiment..."
echo ""

RUN_NUM=0
SUCCESS=0
FAILED=0
START_TIME=$(date +%s)

while IFS=',' read -r widx wfile wlabel rep; do
    RUN_NUM=$((RUN_NUM + 1))
    RUN_TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

    # ETA calculation
    if [ $RUN_NUM -gt 1 ]; then
        ELAPSED=$(($(date +%s) - START_TIME))
        AVG_TIME=$((ELAPSED / (RUN_NUM - 1)))
        REMAINING=$(( (TOTAL_RUNS - RUN_NUM + 1) * AVG_TIME ))
        ETA_MIN=$((REMAINING / 60))
        ETA_SEC=$((REMAINING % 60))
        ETA_STR="ETA ${ETA_MIN}m${ETA_SEC}s"
    else
        ETA_STR=""
    fi

    printf "[%d/%d] %-12s rep=%-3d %s\n" "$RUN_NUM" "$TOTAL_RUNS" "$wlabel" "$rep" "$ETA_STR"

    # Restore original init-4.4th first, then swap if needed
    if [ -f "$BACKUP_INIT" ]; then
        cp "$BACKUP_INIT" "$ORIGINAL_INIT"
    fi
    if [ "$wfile" != "init.4th" ]; then
        cp "$CONF_DIR/$wfile" "$ORIGINAL_INIT"
    fi

    # External timing: capture nanoseconds before/after
    T_START_NS=$(date +%s%N)

    # Run --doe and capture output
    EXIT_CODE=0
    if ! timeout 120 "$STARFORTH" --doe > "$TMP_VM_OUT" 2>/dev/null; then
        EXIT_CODE=$?
    fi

    T_END_NS=$(date +%s%N)
    RUNTIME_NS=$((T_END_NS - T_START_NS))

    # Read the single CSV line from --doe output
    DOE_CSV=""
    if [ -f "$TMP_VM_OUT" ] && [ -s "$TMP_VM_OUT" ]; then
        DOE_CSV=$(tail -1 "$TMP_VM_OUT" | tr -d '\n\r')
    fi

    # Write result row
    # Quote the DOE_CSV in case it has commas (it will, it's CSV)
    echo "$RUN_TIMESTAMP,$RUN_NUM,$wlabel,$CONFIG_BITS,$rep,$RUNTIME_NS,\"$DOE_CSV\"" >> "$RESULTS_FILE"

    if [ $EXIT_CODE -eq 0 ]; then
        SUCCESS=$((SUCCESS + 1))
        RUNTIME_MS=$((RUNTIME_NS / 1000000))
        echo "    OK  ${RUNTIME_MS}ms"
    else
        FAILED=$((FAILED + 1))
        echo "    FAILED (exit $EXIT_CODE)"
    fi

done < "$CONDITIONS_RAND"

# =====================================================================
# SUMMARY
# =====================================================================
TOTAL_TIME=$(($(date +%s) - START_TIME))

echo ""
echo "======================================================"
echo "Experiment Complete"
echo "======================================================"
echo "Total time:  $((TOTAL_TIME / 60))m $((TOTAL_TIME % 60))s"
echo "Success:     $SUCCESS"
echo "Failed:      $FAILED"
echo "Results:     $RESULTS_FILE"
echo "======================================================"