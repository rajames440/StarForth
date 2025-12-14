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

# 2^7 Full Factorial DoE Experiment Runner
# Usage: ./run_doe.sh [REPS]
#   REPS = number of replicates per condition (default: 30)
#   Total runs = 128 conditions × REPS
#
# Examples:
#   ./run_doe.sh 4      # Quick test: 512 runs
#   ./run_doe.sh 30     # Standard: 3,840 runs
#   ./run_doe.sh 300    # Full: 38,400 runs
#   ./run_doe.sh 3000   # Extended: 384,000 runs

set -e

REPS=${1:-30}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
MATRIX_FILE="$OUTPUT_DIR/run_matrix_${TIMESTAMP}.csv"
RESULTS_FILE="$OUTPUT_DIR/doe_results_${TIMESTAMP}.csv"

TOTAL_CONDITIONS=128
TOTAL_RUNS=$((TOTAL_CONDITIONS * REPS))

mkdir -p "$OUTPUT_DIR"

echo "======================================================"
echo "2^7 Full Factorial DoE Experiment"
echo "======================================================"
echo "Replicates:    $REPS"
echo "Conditions:    $TOTAL_CONDITIONS"
echo "Total runs:    $TOTAL_RUNS"
echo "======================================================"

# --- Generate shuffled run matrix ---
echo "Generating shuffled run matrix..."

ORDERED=$(
RUN_ID=0
for L1 in 0 1; do
for L2 in 0 1; do
for L3 in 0 1; do
for L4 in 0 1; do
for L5 in 0 1; do
for L6 in 0 1; do
for L7 in 0 1; do
    for rep in $(seq 1 $REPS); do
        RUN_ID=$((RUN_ID + 1))
        echo "$RUN_ID,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep"
    done
done; done; done; done; done; done; done
)

echo "run_id,L1_heat_tracking,L2_rolling_window,L3_linear_decay,L4_pipelining_metrics,L5_window_inference,L6_decay_inference,L7_adaptive_heartrate,replicate" > "$MATRIX_FILE"
echo "$ORDERED" | shuf >> "$MATRIX_FILE"

echo "Matrix:        $MATRIX_FILE"
echo "Results:       $RESULTS_FILE"
echo "======================================================"

cd "$PROJECT_DIR"

# Write CSV header
echo "timestamp,run_id,L1_heat_tracking,L2_rolling_window,L3_linear_decay,L4_pipelining_metrics,L5_window_inference,L6_decay_inference,L7_adaptive_heartrate,replicate,L1_heat,L2_window,L3_decay,L4_pipeline,L5_win_inf,L6_decay_inf,L7_heartrate,total_lookups,cache_hits,cache_hit_pct,bucket_hits,bucket_hit_pct,cache_lat_ns,cache_lat_std,bucket_lat_ns,bucket_lat_std,ctx_pred_total,ctx_correct,ctx_acc_pct,cache_promos,cache_demos,win_diversity_pct,win_final_bytes,win_width,win_total_exec,win_var_q48,decay_slope,total_heat,hot_words,stale_words,stale_ratio,avg_heat,tick_count,tick_target_ns,infer_runs,early_exits,prefetch_acc_pct,prefetch_attempts,prefetch_hits,win_tune_checks,final_win_size,workload_ns_q48,runtime_ms,words_exec,dict_lookups,mem_bytes,speedup,ci_lower,ci_upper,cpu_temp_delta,cpu_freq_delta,decay_rate_q16,decay_min_ns,rolling_win_size,shrink_rate,demotion_thresh,hotwords_enabled,pipelining_enabled" > "$RESULTS_FILE"

# NaN row template (57 NaN values for the VM output columns)
NAN_DATA="NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN"

RUN_NUM=0
FAILED=0
SUCCESS=0
START_TIME=$(date +%s)

tail -n +2 "$MATRIX_FILE" | while IFS=',' read -r run_id L1 L2 L3 L4 L5 L6 L7 rep; do
    RUN_NUM=$((RUN_NUM + 1))
    RUN_TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

    # Progress estimate
    if [ $RUN_NUM -gt 1 ]; then
        ELAPSED=$(($(date +%s) - START_TIME))
        AVG_TIME=$((ELAPSED / (RUN_NUM - 1)))
        REMAINING=$(( (TOTAL_RUNS - RUN_NUM + 1) * AVG_TIME ))
        ETA_MIN=$((REMAINING / 60))
        ETA_SEC=$((REMAINING % 60))
        ETA_STR="ETA: ${ETA_MIN}m${ETA_SEC}s"
    else
        ETA_STR=""
    fi

    echo ""
    echo "[$RUN_NUM/$TOTAL_RUNS] Run $run_id | L1=$L1 L2=$L2 L3=$L3 L4=$L4 L5=$L5 L6=$L6 L7=$L7 | Rep $rep  $ETA_STR"

    # Clean build with this configuration
    make clean >/dev/null 2>&1

    if ! make ARCH=amd64 TARGET=fastest \
        ENABLE_LOOP_1_HEAT_TRACKING=$L1 \
        ENABLE_LOOP_2_ROLLING_WINDOW=$L2 \
        ENABLE_LOOP_3_LINEAR_DECAY=$L3 \
        ENABLE_LOOP_4_PIPELINING_METRICS=$L4 \
        ENABLE_LOOP_5_WINDOW_INFERENCE=$L5 \
        ENABLE_LOOP_6_DECAY_INFERENCE=$L6 \
        ENABLE_LOOP_7_ADAPTIVE_HEARTRATE=$L7 >/dev/null 2>&1; then
        echo "  BUILD FAILED"
        echo "$RUN_TIMESTAMP,$run_id,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$NAN_DATA" >> "$RESULTS_FILE"
        FAILED=$((FAILED + 1))
        continue
    fi

    # Run DoE and capture output
    if OUTPUT=$(timeout 60 ./build/amd64/fastest/starforth --doe 2>&1); then
        CSV_DATA=$(echo "$OUTPUT" | tail -1)
        echo "$RUN_TIMESTAMP,$run_id,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$CSV_DATA" >> "$RESULTS_FILE"
        echo "  OK"
        SUCCESS=$((SUCCESS + 1))
    else
        EXIT_CODE=$?
        echo "  FAILED (exit $EXIT_CODE)"
        echo "$RUN_TIMESTAMP,$run_id,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$NAN_DATA" >> "$RESULTS_FILE"
        FAILED=$((FAILED + 1))
    fi
done

TOTAL_TIME=$(($(date +%s) - START_TIME))
echo ""
echo "======================================================"
echo "Experiment complete!"
echo "Total time:    $((TOTAL_TIME / 60))m $((TOTAL_TIME % 60))s"
echo "Success:       $SUCCESS"
echo "Failed:        $FAILED"
echo "Results:       $RESULTS_FILE"
echo "======================================================"