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
# STARFORTH — L8 Jacquard Mode Selector Validation Experiment
# =====================================================================
# Compares L8 adaptive mode switching vs static optimal configurations
# across diverse workload types.
#
# Usage: ./run_l8_validation.sh <REPS>
#
# Examples:
#   ./run_l8_validation.sh 10   # quick test
#   ./run_l8_validation.sh 50   # standard validation
#   ./run_l8_validation.sh 100  # high precision
# =====================================================================

set -e

# =====================================================================
# CONFIGURATION
# =====================================================================

# Control Strategies (8 levels):
# - L8_ADAPTIVE: New dynamic mode switching (L8 active, L1=0, L4=0, L7=1)
# - Static Top 5% configs from 300-rep DoE

declare -A STRATEGIES
STRATEGIES=(
    ["L8_ADAPTIVE"]="L8_ADAPTIVE,0,1,1,0,1,1,1"   # L1=0, L2/L3/L5/L6 runtime, L4=0, L7=1
    ["C0_BASELINE"]="C0_BASELINE,0,0,0,0,0,0,1"   # All off except L7
    ["C4_TEMPORAL"]="C4_TEMPORAL,0,0,1,0,0,0,1"   # L3=1 only (top 5% rank #6: config 0010001)
    ["C7_FULL_INF"]="C7_FULL_INF,0,0,1,0,1,1,1"   # L3+L5+L6=1 (top 5% ranks #2,#3: 0010111)
    ["C9_DIVERSE_DECAY"]="C9_DIVERSE_DECAY,0,1,0,0,0,1,1"  # L2+L6=1 (top 5% rank #5: 0100011)
    ["C11_DIVERSE_INF"]="C11_DIVERSE_INF,0,1,0,0,1,1,1"    # L2+L5+L6=1 (top 5% rank #4: 0100110)
    ["C12_DIVERSE_TEMPORAL"]="C12_DIVERSE_TEMPORAL,0,1,1,0,0,0,1"  # L2+L3=1 (top 5% rank #1: 0110001)
    ["ALL_ON_EXCEPT_L1L4"]="ALL_ON,0,1,1,0,1,1,1"  # Everything except L1/L4
)

STRATEGY_ORDER=(
    "L8_ADAPTIVE"
    "C0_BASELINE"
    "C4_TEMPORAL"
    "C7_FULL_INF"
    "C9_DIVERSE_DECAY"
    "C11_DIVERSE_INF"
    "C12_DIVERSE_TEMPORAL"
    "ALL_ON_EXCEPT_L1L4"
)

# Workload Types (5 levels):
WORKLOADS=(
    "init-l8-stable.4th"
    "init-l8-diverse.4th"
    "init-l8-volatile.4th"
    "init-l8-temporal.4th"
    "init-l8-transition.4th"
)

WORKLOAD_LABELS=(
    "STABLE"
    "DIVERSE"
    "VOLATILE"
    "TEMPORAL"
    "TRANSITION"
)

# =====================================================================
# SETUP
# =====================================================================
REPS=${1:-50}

if [ "$REPS" -lt 1 ]; then
    echo "ERROR: REPS must be >= 1"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONF_DIR="$PROJECT_DIR/conf"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$SCRIPT_DIR/l8_validation_${TIMESTAMP}"

mkdir -p "$OUTPUT_DIR"

CONDITIONS_RAW="$OUTPUT_DIR/conditions_raw.txt"
CONDITIONS_RAND="$OUTPUT_DIR/conditions_randomized.txt"
RESULTS_FILE="$OUTPUT_DIR/l8_validation_results.csv"
TMP_VM_OUT="$OUTPUT_DIR/vm_output.tmp"

TOTAL_STRATEGIES=${#STRATEGY_ORDER[@]}
TOTAL_WORKLOADS=${#WORKLOADS[@]}
TOTAL_RUNS=$((TOTAL_STRATEGIES * TOTAL_WORKLOADS * REPS))

echo "======================================================"
echo "L8 Jacquard Validation Experiment"
echo "======================================================"
echo "Strategies:     ${TOTAL_STRATEGIES}"
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
        echo "       Please create workload files in conf/ directory"
        exit 1
    fi
    echo "  OK: $wfile"
done

# =====================================================================
# GENERATE CONDITIONS
# =====================================================================
echo ""
echo "Generating conditions..."

> "$CONDITIONS_RAW"
for sidx in "${!STRATEGY_ORDER[@]}"; do
    sname="${STRATEGY_ORDER[$sidx]}"
    sdata="${STRATEGIES[$sname]}"
    IFS=',' read -r label L1 L2 L3 L4 L5 L6 L7 <<< "$sdata"

    for widx in "${!WORKLOADS[@]}"; do
        wfile="${WORKLOADS[$widx]}"
        wlabel="${WORKLOAD_LABELS[$widx]}"

        for rep in $(seq 1 $REPS); do
            echo "$sidx,$sname,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$widx,$wfile,$wlabel,$rep"
        done
    done
done >> "$CONDITIONS_RAW"

shuf "$CONDITIONS_RAW" > "$CONDITIONS_RAND"

echo "  conditions_raw.txt:        $(wc -l < "$CONDITIONS_RAW") lines"
echo "  conditions_randomized.txt: $(wc -l < "$CONDITIONS_RAND") lines"

# =====================================================================
# CSV HEADER
# =====================================================================
echo "timestamp,run_id,strategy,workload_type,L1,L2,L3,L4,L5,L6,L7,replicate,runtime_ns,doe_csv" > "$RESULTS_FILE"

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
    rm -f "$TMP_VM_OUT"
}
trap cleanup EXIT

# =====================================================================
# RUN EXPERIMENT
# =====================================================================
echo ""
echo "Starting experiment..."
echo ""

echo "Building StarForth once for all runs..."
if ! make -C "$PROJECT_DIR" ARCH=amd64 TARGET=fastest >/dev/null 2>&1; then
    echo "ERROR: Build failed"
    exit 1
fi
STARFORTH="$PROJECT_DIR/build/amd64/fastest/starforth"

RUN_NUM=0
SUCCESS=0
FAILED=0
START_TIME=$(date +%s)

while IFS=',' read -r sidx sname L1 L2 L3 L4 L5 L6 L7 widx wfile wlabel rep; do
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

    printf "[%d/%d] %-20s | %-10s | rep=%-3d | %s\n" "$RUN_NUM" "$TOTAL_RUNS" "$sname" "$wlabel" "$rep" "$ETA_STR"

    # Swap workload file
    if [ -f "$BACKUP_INIT" ]; then
        cp "$BACKUP_INIT" "$ORIGINAL_INIT"
    fi
    if [ "$wfile" != "init.4th" ]; then
        cp "$CONF_DIR/$wfile" "$ORIGINAL_INIT"
    fi

    # External timing
    T_START_NS=$(date +%s%N)

    # Run --doe
    EXIT_CODE=0
    if ! timeout 120 "$STARFORTH" --doe > "$TMP_VM_OUT" 2>/dev/null; then
        EXIT_CODE=$?
    fi

    T_END_NS=$(date +%s%N)
    RUNTIME_NS=$((T_END_NS - T_START_NS))

    # Read DoE CSV output
    DOE_CSV=""
    if [ -f "$TMP_VM_OUT" ] && [ -s "$TMP_VM_OUT" ]; then
        DOE_CSV=$(tail -1 "$TMP_VM_OUT" | tr -d '\n\r')
    fi

    # Write result
    echo "$RUN_TIMESTAMP,$RUN_NUM,$sname,$wlabel,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$RUNTIME_NS,\"$DOE_CSV\"" >> "$RESULTS_FILE"

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
echo ""
echo "Next steps:"
echo "  1. Run R analysis: Rscript analyze_l8.R $OUTPUT_DIR"
echo "  2. Review plots in: $OUTPUT_DIR"
echo "======================================================"
