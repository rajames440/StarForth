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

# Generate 2^7 Full Factorial Run Matrix
# 128 conditions × 300 replicates = 38,400 runs
# Output: Shuffled CSV matrix ready for execution

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_FILE="$OUTPUT_DIR/run_matrix_${TIMESTAMP}.csv"

REPS=${1:-300}
TOTAL_CONDITIONS=128
TOTAL_RUNS=$((TOTAL_CONDITIONS * REPS))

mkdir -p "$OUTPUT_DIR"

echo "======================================================"
echo "2^7 Full Factorial Run Matrix Generator"
echo "======================================================"
echo "Conditions:    $TOTAL_CONDITIONS"
echo "Replicates:    $REPS"
echo "Total runs:    $TOTAL_RUNS"
echo "Output:        $OUTPUT_FILE"
echo "======================================================"

# Generate all 38,400 runs in order with sequential run_id
echo "Generating ordered matrix..."
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

# Write header
echo "run_id,L1_heat_tracking,L2_rolling_window,L3_linear_decay,L4_pipelining_metrics,L5_window_inference,L6_decay_inference,L7_adaptive_heartrate,replicate" > "$OUTPUT_FILE"

# Shuffle the rows (run_id stays with its row)
echo "Shuffling $TOTAL_RUNS runs..."
echo "$ORDERED" | shuf >> "$OUTPUT_FILE"

echo "======================================================"
echo "Done. Matrix written to: $OUTPUT_FILE"
echo "======================================================"