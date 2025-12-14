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

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="/tmp/starforth_doe_test"
LOG_DIR="${OUTPUT_DIR}/run_logs"
RESULTS_CSV="${OUTPUT_DIR}/experiment_results.csv"

mkdir -p "${LOG_DIR}" "${OUTPUT_DIR}"

echo "StarForth Quick DoE Test – CSV Structure Check"
echo "Running: 1 config × 3 runs = 3 tests"
echo ""

cd "${REPO_ROOT}"

# Build once
echo "Building..."
make clean > /dev/null 2>&1
make fastest > /dev/null 2>&1
BINARY="${REPO_ROOT}/build/amd64/fastest/starforth"
echo "✓ Binary ready"

# CSV Header
{
    printf 'timestamp,configuration,run_number,'
    printf 'total_lookups,cache_hits,cache_hit_percent,bucket_hits,bucket_hit_percent,'
    printf 'cache_hit_latency_ns,cache_hit_stddev_ns,bucket_search_latency_ns,bucket_search_stddev_ns,'
    printf 'context_predictions_total,context_correct,context_accuracy_percent,'
    printf 'rolling_window_width,decay_slope,'
    printf 'hot_word_count,stale_word_ratio,avg_word_heat,'
    printf 'prefetch_accuracy_percent,prefetch_attempts,prefetch_hits,window_tuning_checks,final_effective_window_size,'
    printf 'vm_workload_duration_ns_q48,cpu_temp_delta_c_q48,cpu_freq_delta_mhz_q48,'
    printf 'decay_rate_q16,decay_min_interval_ns,rolling_window_size,adaptive_shrink_rate,heat_cache_demotion_threshold,'
    printf 'enable_loop_1_heat_tracking,enable_loop_2_rolling_window,enable_loop_3_linear_decay,'
    printf 'enable_loop_4_pipelining_metrics,enable_loop_5_window_inference,enable_loop_6_decay_inference\n'
} > "${RESULTS_CSV}"

echo "Running tests..."
echo ""

# Run tests
for RUN in 1 2 3; do
    echo "Test $RUN/3..."
    LOG_FILE="${LOG_DIR}/test_run_${RUN}.log"
    
    # Run with --break-me (comprehensive test)
    timeout 60 "${BINARY}" --break-me > "${LOG_FILE}" 2>&1 || true
    
    # Extract metrics (last line that looks like CSV)
    CSV_LINE=$(tail -10 "${LOG_FILE}" | grep -E '^[0-9]+,' | tail -1 || echo "")
    
    if [ -n "${CSV_LINE}" ]; then
        TS=$(date +"%Y-%m-%dT%H:%M:%S")
        CONFIG="1_0_1_1_1_0"
        FLAGS="1,0,1,1,1,0"
        printf '%s,%s,%d,%s,%s\n' "${TS}" "${CONFIG}" "${RUN}" "${CSV_LINE}" "${FLAGS}" >> "${RESULTS_CSV}"
        echo "  ✓ Metrics captured"
    else
        echo "  ✗ No metrics found"
    fi
done

echo ""
echo "═════════════════════════════════════════════════════"
echo "CSV STRUCTURE VERIFICATION"
echo "═════════════════════════════════════════════════════"
echo ""

echo "File: ${RESULTS_CSV}"
echo "Rows: $(wc -l < ${RESULTS_CSV})"
echo "Size: $(wc -c < ${RESULTS_CSV}) bytes"
echo ""

echo "HEADER (first 50 chars per column):"
head -1 "${RESULTS_CSV}" | tr ',' '\n' | head -20 | nl
echo ""

echo "FIRST DATA ROW STRUCTURE:"
LINE=$(tail -1 "${RESULTS_CSV}")
echo "$LINE" | tr ',' '\n' | head -20 | nl
echo ""

echo "COLUMN COUNT:"
HEADER=$(head -1 "${RESULTS_CSV}" | tr ',' '\n' | wc -l)
DATA=$(tail -1 "${RESULTS_CSV}" | tr ',' '\n' | wc -l)
echo "  Header: $HEADER columns"
echo "  Data:   $DATA columns"

if [ "$HEADER" -eq "$DATA" ]; then
    echo "  ✓ MATCH!"
else
    echo "  ✗ MISMATCH"
fi

echo ""
echo "FULL CSV:"
cat "${RESULTS_CSV}"

echo ""
echo "✓ Test complete"
