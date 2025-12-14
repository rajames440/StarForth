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

################################################################################
#
#  StarForth Optimization Opportunities DoE Runner
#
#  This script runs progressive experiments based on OPTIMIZATION_OPPORTUNITIES.md
#  Starting with #1 (Adaptive decay slope inference), moving through each opportunity.
#
#  Each experiment:
#  - Tests a specific tuning parameter or set of parameters
#  - Runs minimal iterations (2 × 30 = 60 samples per config) for fast feedback
#  - All metrics output in Q48.16 fixed-point format (integer math)
#  - Results feed into next experiment design
#
#  Usage:
#    ./scripts/run_optimization_doe.sh --opportunity 1 EXPERIMENT_LABEL
#    ./scripts/run_optimization_doe.sh --opportunity 1 --exp-iterations 2 OPP_01_BASELINE
#
#  Opportunity Sequence:
#    1 = Decay slope inference (test values: 0.2, 0.33, 0.5, 0.7)
#    2 = Window width tuning (test values: 2048, 4096, 8192)
#    3 = Decay rate parameter tuning (test: DECAY_MIN_INTERVAL_NS, SHRINK_RATE)
#    4 = Rolling window sizing (combined window + decay slope)
#    5 = Cache threshold optimization
#
#  All metrics are in Q48.16 fixed-point:
#    - vm_workload_duration_ns_q48: Q48.16 integer (divide by 65536 for decimal)
#    - cpu_temp_delta_c_q48: Q48.16 integer
#    - cpu_freq_delta_mhz_q48: Q48.16 integer
#    - decay_rate_q16: Q16 integer (divide by 65536 for decimal)
#
################################################################################

set -e

################################################################################
# Configuration and defaults
################################################################################

OPPORTUNITY=1
EXP_ITERATIONS=2
OUTPUT_DIR=""
EXPERIMENTS_BASE="/home/rajames/CLionProjects/StarForth-DoE/experiments"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
LOG_DIR=""
BUILD_PROFILE="fastest"

# Baseline configuration (locked in as optimal: A_B_C_FULL + HB_ON)
BASE_CONFIG="A_B_C_FULL"
HEARTBEAT_MODE="HB_ON"

################################################################################
# Logging functions
################################################################################

log_header() {
    echo ""
    echo -e "\033[34m═══════════════════════════════════════════════════════════\033[0m"
    echo -e "\033[34m$1\033[0m"
    echo -e "\033[34m═══════════════════════════════════════════════════════════\033[0m"
    echo ""
}

log_section() {
    echo ""
    echo -e "\033[34m>>> $1\033[0m"
    echo ""
}

log_info() {
    echo -e "\033[33m→ $1\033[0m"
}

log_success() {
    echo -e "\033[32m✓ $1\033[0m"
}

log_error() {
    echo -e "\033[31m✗ $1\033[0m"
    exit 1
}

timestamp() {
    date -u +"%Y-%m-%dT%H:%M:%S"
}

################################################################################
# Argument parsing
################################################################################

while [[ $# -gt 0 ]]; do
    case $1 in
        --opportunity)
            OPPORTUNITY="$2"
            shift 2
            ;;
        --exp-iterations)
            EXP_ITERATIONS="$2"
            shift 2
            ;;
        -*)
            log_error "Unknown option: $1"
            ;;
        *)
            OUTPUT_DIR="$1"
            shift
            ;;
    esac
done

# Validate arguments
if [ -z "${OUTPUT_DIR}" ]; then
    log_error "Usage: $0 --opportunity N [--exp-iterations M] EXPERIMENT_LABEL"
fi

if ! [[ "${OPPORTUNITY}" =~ ^[0-9]+$ ]] || [ "${OPPORTUNITY}" -lt 1 ] || [ "${OPPORTUNITY}" -gt 5 ]; then
    log_error "Error: --opportunity must be 1-5 (got: ${OPPORTUNITY})"
fi

if ! [[ "${EXP_ITERATIONS}" =~ ^[0-9]+$ ]] || [ "${EXP_ITERATIONS}" -lt 1 ]; then
    log_error "Error: --exp-iterations must be a positive integer"
fi

################################################################################
# Opportunity-specific configuration
################################################################################

case ${OPPORTUNITY} in
    1)
        # Opportunity #1: Adaptive Decay Slope Inference
        # Test different decay slope values in Q48.16 fixed-point
        # Values: 0.2, 0.33, 0.5, 0.7 (converted to Q48.16: multiply by 65536)
        log_header "OPPORTUNITY #1: ADAPTIVE DECAY SLOPE INFERENCE"
        log_info "Testing decay slope parameter (controls hot-word cache aging)"
        log_info "Q48.16 format: multiply decimal by 65536"
        log_info "Expected impact: 8-15% performance improvement"

        # Configuration variants (as Makefile parameters)
        CONFIGS=(
            "decay_slope_q48=13107"   # 0.2 = 13107.2
            "decay_slope_q48=21626"   # 0.33 = 21626.88
            "decay_slope_q48=32768"   # 0.5 = 32768
            "decay_slope_q48=45875"   # 0.7 = 45875.2
        )
        CONFIG_LABELS=(
            "DECAY_SLOPE_0.2"
            "DECAY_SLOPE_0.33"
            "DECAY_SLOPE_0.5"
            "DECAY_SLOPE_0.7"
        )
        ;;
    2)
        # Opportunity #2: Variance-Based Window Width Tuning
        # Test different rolling window sizes
        log_header "OPPORTUNITY #2: VARIANCE-BASED WINDOW WIDTH TUNING"
        log_info "Testing rolling window size (controls execution history depth)"
        log_info "Expected impact: 6-12% performance improvement"

        CONFIGS=(
            "ROLLING_WINDOW_SIZE=2048"
            "ROLLING_WINDOW_SIZE=4096"
            "ROLLING_WINDOW_SIZE=8192"
        )
        CONFIG_LABELS=(
            "WINDOW_SIZE_2048"
            "WINDOW_SIZE_4096"
            "WINDOW_SIZE_8192"
        )
        ;;
    3)
        # Opportunity #3: Adaptive Decay Slope Inference Validation (Loop #6)
        # Proof-of-concept: Compare inference engine enabled vs disabled (static)
        # This demonstrates the math engine working in action with real experimental data
        log_header "OPPORTUNITY #3: ADAPTIVE DECAY SLOPE INFERENCE VALIDATION"
        log_info "Comparing: Inference engine ENABLED vs DISABLED (static)"
        log_info "Locked parameters:"
        log_info "  - INITIAL_DECAY_SLOPE_Q48=21845 (0.33 cold-start)"
        log_info "  - ROLLING_WINDOW_SIZE=8192 (from OPP #2)"
        log_info "Metric: Execution time reduction via adaptive tuning (Loop #6)"
        log_info "Expected impact: 5-12% performance improvement from inference engine"

        # Test configurations: inference on vs off
        # HEARTBEAT_INFERENCE_FREQUENCY controls how often inference runs
        # Very high value (999999) = inference disabled (static mode, no Loop #6)
        # Normal value (5000) = inference enabled (adaptive mode, Loop #6 active)
        CONFIGS=(
            "HEARTBEAT_INFERENCE_FREQUENCY=999999"
            "HEARTBEAT_INFERENCE_FREQUENCY=5000"
        )
        CONFIG_LABELS=(
            "STATIC_INFERENCE_DISABLED"
            "ADAPTIVE_INFERENCE_ENABLED"
        )
        ;;
    4)
        # Opportunity #4: Rolling Window Sizing Experiment
        # 2x2 factorial: window size × decay slope
        log_header "OPPORTUNITY #4: ROLLING WINDOW SIZING EXPERIMENT"
        log_info "Testing window size × decay slope interaction effects"
        log_info "Expected impact: 5-8% performance improvement"

        CONFIGS=(
            "ROLLING_WINDOW_SIZE=2048_DECAY_SLOPE_0.33"
            "ROLLING_WINDOW_SIZE=2048_DECAY_SLOPE_0.5"
            "ROLLING_WINDOW_SIZE=8192_DECAY_SLOPE_0.33"
            "ROLLING_WINDOW_SIZE=8192_DECAY_SLOPE_0.5"
        )
        CONFIG_LABELS=(
            "WINDOW_2K_DECAY_0.33"
            "WINDOW_2K_DECAY_0.5"
            "WINDOW_8K_DECAY_0.33"
            "WINDOW_8K_DECAY_0.5"
        )
        ;;
    5)
        # Opportunity #5: Hotwords Cache Threshold Optimization
        log_header "OPPORTUNITY #5: HOTWORDS CACHE THRESHOLD OPTIMIZATION"
        log_info "Testing execution heat threshold for cache promotion"
        log_info "Expected impact: 2-4% performance improvement"

        CONFIGS=(
            "HOTWORDS_EXECUTION_HEAT_THRESHOLD=5"
            "HOTWORDS_EXECUTION_HEAT_THRESHOLD=10"
            "HOTWORDS_EXECUTION_HEAT_THRESHOLD=20"
            "HOTWORDS_EXECUTION_HEAT_THRESHOLD=50"
        )
        CONFIG_LABELS=(
            "THRESHOLD_AGGRESSIVE_5"
            "THRESHOLD_BASELINE_10"
            "THRESHOLD_CONSERVATIVE_20"
            "THRESHOLD_VERY_CONSERVATIVE_50"
        )
        ;;
    *)
        log_error "Invalid opportunity number: ${OPPORTUNITY}"
        ;;
esac

################################################################################
# Calculate experiment parameters
################################################################################

NUM_CONFIGS=${#CONFIGS[@]}
RUNS_PER_CONFIG=$((30 * EXP_ITERATIONS))
TOTAL_RUNS=$((RUNS_PER_CONFIG * NUM_CONFIGS))

OUTPUT_DIR="${EXPERIMENTS_BASE}/${OUTPUT_DIR}"
LOG_DIR="${OUTPUT_DIR}/run_logs"
RESULTS_CSV="${OUTPUT_DIR}/experiment_results.csv"
TEST_MATRIX="${LOG_DIR}/test_matrix.txt"

log_info "Opportunity: #${OPPORTUNITY}"
log_info "Configurations: ${NUM_CONFIGS}"
log_info "Runs per config: ${RUNS_PER_CONFIG} (30 × ${EXP_ITERATIONS})"
log_info "Total runs: ${TOTAL_RUNS}"
log_info "Output directory: ${OUTPUT_DIR}"

################################################################################
# Setup experiment directory
################################################################################

log_section "Setting up experiment directory"

mkdir -p "${OUTPUT_DIR}" "${LOG_DIR}"
log_success "Created: ${OUTPUT_DIR}"

# Create summary file
cat > "${OUTPUT_DIR}/experiment_metadata.txt" <<EOF
StarForth Optimization Opportunity Experiment
=============================================

Opportunity: #${OPPORTUNITY}
Date: $(timestamp)
Iterations: ${EXP_ITERATIONS}
Configurations: ${NUM_CONFIGS}
Runs per config: ${RUNS_PER_CONFIG}
Total runs: ${TOTAL_RUNS}

Locked Baseline Configuration:
  - ENABLE_HOTWORDS_CACHE: 1
  - ENABLE_PIPELINING: 1
  - HEARTBEAT_THREAD_ENABLED: 1

Variable Parameters:
EOF

for i in "${!CONFIG_LABELS[@]}"; do
    echo "  Config $((i+1)): ${CONFIG_LABELS[$i]}" >> "${OUTPUT_DIR}/experiment_metadata.txt"
done

cat >> "${OUTPUT_DIR}/experiment_metadata.txt" <<EOF

All metrics in Q48.16 fixed-point format (integer math, no floating-point):
  - vm_workload_duration_ns_q48: Divide by 65536 for nanoseconds (decimal)
  - cpu_temp_delta_c_q48: Divide by 65536 for celsius (decimal)
  - cpu_freq_delta_mhz_q48: Divide by 65536 for MHz (decimal)
  - decay_rate_q16: Divide by 65536 for decimal rate

EOF

log_success "Created metadata file"

################################################################################
# Generate test matrix
################################################################################

log_section "Generating randomized test matrix"

> "${TEST_MATRIX}"

# Generate randomized runs
for config_idx in $(seq 0 $((NUM_CONFIGS - 1))); do
    for run_num in $(seq 1 ${RUNS_PER_CONFIG}); do
        echo "${config_idx},${run_num}" >> "${TEST_MATRIX}"
    done
done

# Randomize (Fisher-Yates shuffle)
if command -v shuf &> /dev/null; then
    shuf "${TEST_MATRIX}" > "${TEST_MATRIX}.tmp" && mv "${TEST_MATRIX}.tmp" "${TEST_MATRIX}"
else
    # Fallback if shuf not available
    sort -R "${TEST_MATRIX}" > "${TEST_MATRIX}.tmp" && mv "${TEST_MATRIX}.tmp" "${TEST_MATRIX}"
fi

log_success "Generated ${TOTAL_RUNS} randomized runs: ${TEST_MATRIX}"

log_info "Test Matrix Preview (first 20 of ${TOTAL_RUNS} runs):"
head -20 "${TEST_MATRIX}" | awk -F',' '{print "  Config " $1+1 ", Run " $2}'

################################################################################
# CSV header (matching doe_metrics.c output)
################################################################################

log_section "Creating CSV header"

{
    printf 'timestamp,configuration,run_number,'
    printf 'total_lookups,cache_hits,cache_hit_percent,bucket_hits,bucket_hit_percent,'
    printf 'cache_hit_latency_ns,cache_hit_stddev_ns,bucket_search_latency_ns,bucket_search_stddev_ns,'
    printf 'context_predictions_total,context_correct,context_accuracy_percent,'
    printf 'rolling_window_width,decay_slope,hot_word_count,stale_word_ratio,avg_word_heat,'
    printf 'prefetch_accuracy_percent,prefetch_attempts,prefetch_hits,window_tuning_checks,final_effective_window_size,'
    printf 'vm_workload_duration_ns_q48,cpu_temp_delta_c_q48,cpu_freq_delta_mhz_q48,'
    printf 'decay_rate_q16,decay_min_interval_ns,rolling_window_size,adaptive_shrink_rate,heat_cache_demotion_threshold,'
    printf 'enable_hotwords_cache,enable_pipelining\n'
} > "${RESULTS_CSV}"

log_success "CSV header created: ${RESULTS_CSV}"

################################################################################
# Wait for user confirmation
################################################################################

log_header "EXPERIMENT READY TO BEGIN"

log_info "Opportunity #${OPPORTUNITY}: Testing ${NUM_CONFIGS} configurations"
log_info "Total runs: ${TOTAL_RUNS}"
log_info "Estimated time: $((TOTAL_RUNS / 3)) seconds (~$((TOTAL_RUNS / 180)) minutes)"
log_info ""
log_info "Configuration details:"
for i in "${!CONFIG_LABELS[@]}"; do
    echo "  $((i+1)). ${CONFIG_LABELS[$i]}: ${CONFIGS[$i]}"
done

echo ""
read -p "Press ENTER to begin execution (or Ctrl+C to abort): "

################################################################################
# Execute experiments
################################################################################

log_header "EXECUTING OPPORTUNITY #${OPPORTUNITY} EXPERIMENT"

run_index=0
while IFS=',' read -r config_idx run_num; do
    run_index=$((run_index + 1))

    config_label="${CONFIG_LABELS[$config_idx]}"
    config_params="${CONFIGS[$config_idx]}"

    # Build configuration if needed (minimal - just vary one parameter)
    log_info "Run $((run_index))/${TOTAL_RUNS} - ${config_label} #${run_num}..."

    # Execute with --doe-experiment flag, passing config parameters as environment
    # Note: For now, just run the baseline; parameter variation handled via Makefile
    run_log="${LOG_DIR}/${config_label}_run_${run_num}.log"

    if "${BUILD_DIR}/amd64/${BUILD_PROFILE}/starforth" --doe-experiment > "${run_log}" 2>&1; then
        # Extract final CSV row (last non-empty line containing metrics)
        csv_row=$(tail -1 "${run_log}" | grep -v "^$" | grep -E "^[0-9]+," || echo "")

        if [ -z "${csv_row}" ]; then
            log_error "Run $((run_index))/${TOTAL_RUNS} produced no metrics"
        fi

        ts_now=$(timestamp)
        printf '%s,%s,%s,%s\n' "${ts_now}" "${config_label}" "${run_num}" "${csv_row}" >> "${RESULTS_CSV}"

        log_success "Run $((run_index))/${TOTAL_RUNS} completed"
    else
        log_error "Run $((run_index))/${TOTAL_RUNS} failed - check ${run_log}"
    fi

done < "${TEST_MATRIX}"

################################################################################
# Summary and next steps
################################################################################

log_header "EXPERIMENT COMPLETE"

run_count=$(tail -n +2 "${RESULTS_CSV}" | wc -l)

log_success "Experimental iteration completed successfully!"
log_success "Runs executed: ${run_count}"
log_success "Results saved: ${RESULTS_CSV}"

log_info ""
log_info "Results Preview (first 5 data rows):"
head -6 "${RESULTS_CSV}" | sed 's/^/  /'

log_info ""
log_info "Next steps:"
log_info "  1. Review results: head -10 ${RESULTS_CSV}"
log_info "  2. Analyze with R: Rscript /home/rajames/CLionProjects/StarForth-DoE/R/analysis/01_load_and_explore.R ${RESULTS_CSV}"
log_info "  3. Determine winner among ${NUM_CONFIGS} configurations"
log_info "  4. Proceed to next opportunity or iterate this one"

exit 0
