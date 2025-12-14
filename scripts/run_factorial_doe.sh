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
#  StarForth Complete 2^7 Factorial DoE
#
#  COMPREHENSIVE ONE-AND-DONE EXPERIMENT
#
#  Design:
#  ───────────────────────────────────────────────────────────────────────────
#
#  This script executes a complete 2^7 factorial design of experiments:
#  - 7 binary feedback loop factors (ENABLE_LOOP_1 through ENABLE_LOOP_7)
#  - 2^7 = 128 unique configurations
#  - Each configuration tested N times (default: 20 runs per config)
#  - Total runs: 128 × 20 = 2,560 runs
#  - All results_run_01_2025_12_08 aggregated into ONE dataset for downstream analysis
#
#  Factors (each binary: 0 or 1):
#  ───────────────────────────────────────────────────────────────────────────
#  Loop #1: ENABLE_LOOP_1_HEAT_TRACKING       Execution heat counting
#  Loop #2: ENABLE_LOOP_2_ROLLING_WINDOW      Rolling window history capture
#  Loop #3: ENABLE_LOOP_3_LINEAR_DECAY        Exponential heat decay
#  Loop #4: ENABLE_LOOP_4_PIPELINING_METRICS  Word-to-word transition tracking
#  Loop #5: ENABLE_LOOP_5_WINDOW_INFERENCE    Window width inference (Levene's test)
#  Loop #6: ENABLE_LOOP_6_DECAY_INFERENCE     Decay slope inference (regression)
#  Loop #7: ENABLE_LOOP_7_ADAPTIVE_HEARTRATE  Dynamic tick frequency adjustment
#
#  Design Points:
#  ───────────────────────────────────────────────────────────────────────────
#  Configuration format: L1_L2_L3_L4_L5_L6_L7 (each digit 0 or 1)
#  Examples:
#    0000000 = baseline (all loops OFF - plain FORTH-79)
#    0000001 = only heat tracking
#    1111111 = all loops ON (current optimal)
#    1010101 = alternating pattern
#
#  Randomization:
#  ───────────────────────────────────────────────────────────────────────────
#  - All 64 × N runs randomized into single test matrix
#  - No systematic ordering bias
#  - Prevents thermal/temporal confounds
#
#  Output Structure:
#  ───────────────────────────────────────────────────────────────────────────
#  <base>/<EXPERIMENT_LABEL>/
#    ├── experiment_results.csv        (1920 rows, 38+ columns)
#    ├── experiment_summary.txt        (metadata, timing, notes)
#    ├── test_matrix.txt               (randomized run order)
#    ├── run_logs/                     (per-run execution logs)
#    │   ├── 000000_run_1.log
#    │   ├── 111111_run_1.log
#    │   └── ... (1920 total logs)
#    └── configuration_manifest.txt    (mapping: config → loop flags)
#
#  Expected Runtime:
#  ───────────────────────────────────────────────────────────────────────────
#  - exp-iterations 1 (30 runs/config):  ~2-4 hours   (128 × 30 = 3,840 total runs)
#  - exp-iterations 2 (60 runs/config):  ~4-8 hours   (128 × 60 = 7,680 total runs)
#  - exp-iterations 3 (90 runs/config):  ~6-12 hours  (128 × 90 = 11,520 total runs)
#
#  Usage:
#    ./scripts/run_factorial_doe.sh EXPERIMENT_LABEL  (defaults to exp-iterations 1)
#    ./scripts/run_factorial_doe.sh --exp-iterations 1 BASELINE_DOE
#    ./scripts/run_factorial_doe.sh --exp-iterations 2 COMPREHENSIVE_DOE
#    ./scripts/run_factorial_doe.sh --exp-iterations 3 FULL_COVERAGE
#
#  Example:
#    ./scripts/run_factorial_doe.sh 2025_11_20_BASELINE
#      → stores in /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_BASELINE
#      → runs 128 configs × 30 runs = 3,840 total runs
#
################################################################################

set -e

################################################################################
# Parse Arguments
################################################################################

EXP_ITERATIONS=1
OUTPUT_DIR=""
EXPERIMENTS_BASE="/home/rajames/CLionProjects/StarForth-DoE/experiments"
BASE_RUNS_PER_CONFIG=30

while [[ $# -gt 0 ]]; do
    case $1 in
        --exp-iterations)
            EXP_ITERATIONS="$2"
            shift 2
            ;;
        -*)
            echo "Unknown option: $1"
            echo "Usage: $0 [--exp-iterations N] EXPERIMENT_LABEL"
            exit 1
            ;;
        *)
            OUTPUT_DIR="$1"
            shift
            ;;
    esac
done

if [ -z "${OUTPUT_DIR}" ]; then
    echo "Usage: $0 [--exp-iterations N] EXPERIMENT_LABEL"
    echo ""
    echo "Iteration Levels (runs-per-config = iteration × 30):"
    echo "  --exp-iterations 1  → 30 runs per config"
    echo "  --exp-iterations 2  → 60 runs per config"
    echo "  --exp-iterations 3  → 90 runs per config"
    echo ""
    echo "Examples:"
    echo "  $0 2025_11_19_FULL_FACTORIAL"
    echo "  $0 --exp-iterations 1 BASELINE_DOE"
    echo "  $0 --exp-iterations 2 COMPREHENSIVE_DOE"
    exit 1
fi

if ! [[ "${EXP_ITERATIONS}" =~ ^[0-9]+$ ]] || [ "${EXP_ITERATIONS}" -lt 1 ]; then
    echo "Error: --exp-iterations must be a positive integer"
    exit 1
fi

# Calculate runs per config based on iteration level
RUNS_PER_CONFIG=$((EXP_ITERATIONS * BASE_RUNS_PER_CONFIG))

# Paths
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
BUILD_PROFILE="fastest"

# Ensure base directory exists
mkdir -p "${EXPERIMENTS_BASE}" || {
    echo "Error: unable to create experiments base at ${EXPERIMENTS_BASE}"
    exit 1
}

OUTPUT_DIR="${EXPERIMENTS_BASE%/}/${OUTPUT_DIR}"
LOG_DIR="${OUTPUT_DIR}/run_logs"
RESULTS_CSV="${OUTPUT_DIR}/experiment_results.csv"
SUMMARY_LOG="${OUTPUT_DIR}/experiment_summary.txt"
TEST_MATRIX="${OUTPUT_DIR}/test_matrix.txt"
CONFIG_MANIFEST="${OUTPUT_DIR}/configuration_manifest.txt"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p "${LOG_DIR}" "${OUTPUT_DIR}"

################################################################################
# Logging Functions
################################################################################

log_header() {
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}" >&2
    echo -e "${BLUE}$1${NC}" >&2
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}" >&2
}

log_success() {
    echo -e "${GREEN}✓ $1${NC}" >&2
}

log_error() {
    echo -e "${RED}✗ $1${NC}" >&2
}

log_info() {
    echo -e "${YELLOW}→ $1${NC}" >&2
}

log_section() {
    echo -e "\n${BLUE}>>> $1${NC}" >&2
}

timestamp() {
    date +"%Y-%m-%dT%H:%M:%S"
}

runtime_seconds() {
    local start=$1
    local end=$2
    echo $((end - start))
}

format_duration() {
    local seconds=$1
    local hours=$((seconds / 3600))
    local minutes=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))
    printf "%dh %dm %ds" $hours $minutes $secs
}

extract_final_csv_row() {
    local log_file=$1
    if [ ! -f "${log_file}" ]; then
        return 1
    fi
    awk '!/^HB,/ && NF {line=$0} END {if (length(line)) print line}' "${log_file}"
}

################################################################################
# Generate All 128 Configurations (2^7)
################################################################################

generate_all_configurations() {
    # Generate all 128 binary combinations of 7 loops
    # Format: L1_L2_L3_L4_L5_L6_L7 (each digit 0 or 1)
    local configs=()

    for i in $(seq 0 127); do
        local l1=$(( (i >> 0) & 1 ))
        local l2=$(( (i >> 1) & 1 ))
        local l3=$(( (i >> 2) & 1 ))
        local l4=$(( (i >> 3) & 1 ))
        local l5=$(( (i >> 4) & 1 ))
        local l6=$(( (i >> 5) & 1 ))
        local l7=$(( (i >> 6) & 1 ))

        local config_name="${l1}_${l2}_${l3}_${l4}_${l5}_${l6}_${l7}"
        configs+=("$config_name")
    done

    echo "${configs[@]}"
}

################################################################################
# Configuration Mapping
################################################################################

config_to_loop_flags() {
    # Parse config name "L1_L2_L3_L4_L5_L6_L7" and output 7 comma-separated values
    local config=$1

    # config format: "L1_L2_L3_L4_L5_L6_L7"
    local l1=$(echo "$config" | cut -d'_' -f1)
    local l2=$(echo "$config" | cut -d'_' -f2)
    local l3=$(echo "$config" | cut -d'_' -f3)
    local l4=$(echo "$config" | cut -d'_' -f4)
    local l5=$(echo "$config" | cut -d'_' -f5)
    local l6=$(echo "$config" | cut -d'_' -f6)
    local l7=$(echo "$config" | cut -d'_' -f7)

    echo "${l1},${l2},${l3},${l4},${l5},${l6},${l7}"
}

################################################################################
# CSV Functions
################################################################################

init_csv_header() {
    {
        printf 'timestamp,configuration,run_number,build_status,run_status,'
        printf 'total_lookups,cache_hits,cache_hit_percent,bucket_hits,bucket_hit_percent,'
        printf 'cache_hit_latency_ns,cache_hit_stddev_ns,bucket_search_latency_ns,bucket_search_stddev_ns,'
        printf 'context_predictions_total,context_correct,context_accuracy_percent,'
        printf 'rolling_window_width,decay_slope,'
        printf 'hot_word_count,stale_word_ratio,avg_word_heat,'
        printf 'prefetch_accuracy_percent,prefetch_attempts,prefetch_hits,window_tuning_checks,final_effective_window_size,'
        printf 'vm_workload_duration_ns_q48,cpu_temp_delta_c_q48,cpu_freq_delta_mhz_q48,'
        printf 'decay_rate_q16,decay_min_interval_ns,rolling_window_size,adaptive_shrink_rate,heat_cache_demotion_threshold,'
        printf 'enable_loop_1_heat_tracking,enable_loop_2_rolling_window,enable_loop_3_linear_decay,'
        printf 'enable_loop_4_pipelining_metrics,enable_loop_5_window_inference,enable_loop_6_decay_inference,'
        printf 'enable_loop_7_adaptive_heartrate\n'
    } > "${RESULTS_CSV}"
    log_success "CSV header created (with build_status and run_status columns for tracking incompatible configs)"
}

write_configuration_manifest() {
    # Document all 128 configurations for reference
    {
        echo "# StarForth 2^7 Factorial DoE - Configuration Manifest"
        echo "# Generated: $(timestamp)"
        echo ""
        echo "Configuration format: LOOP1_LOOP2_LOOP3_LOOP4_LOOP5_LOOP6_LOOP7"
        echo ""
        echo "Factors:"
        echo "  LOOP1 = ENABLE_LOOP_1_HEAT_TRACKING (execution heat counting)"
        echo "  LOOP2 = ENABLE_LOOP_2_ROLLING_WINDOW (rolling window history)"
        echo "  LOOP3 = ENABLE_LOOP_3_LINEAR_DECAY (exponential heat decay)"
        echo "  LOOP4 = ENABLE_LOOP_4_PIPELINING_METRICS (word transition tracking)"
        echo "  LOOP5 = ENABLE_LOOP_5_WINDOW_INFERENCE (window width inference)"
        echo "  LOOP6 = ENABLE_LOOP_6_DECAY_INFERENCE (decay slope inference)"
        echo "  LOOP7 = ENABLE_LOOP_7_ADAPTIVE_HEARTRATE (dynamic tick frequency adjustment)"
        echo ""
        echo "All 128 configurations:"
        echo ""

        local idx=0
        for config in $(generate_all_configurations); do
            printf "%3d. %s\n" $((idx + 1)) "$config"
            idx=$((idx + 1))
        done
    } > "${CONFIG_MANIFEST}"
    log_success "Configuration manifest written: ${CONFIG_MANIFEST}"
}

################################################################################
# Build Functions
################################################################################

build_configuration() {
    local config_name=$1
    local l1=$2
    local l2=$3
    local l3=$4
    local l4=$5
    local l5=$6
    local l6=$7
    local l7=$8

    cd "${REPO_ROOT}"

    # Clean previous build
    make clean > /dev/null 2>&1 || true

    # Build with specific loop configuration
    log_info "Building: make TARGET=${BUILD_PROFILE} with loops: L1=${l1} L2=${l2} L3=${l3} L4=${l4} L5=${l5} L6=${l6} L7=${l7}"

    if make TARGET="${BUILD_PROFILE}" \
            ENABLE_LOOP_1_HEAT_TRACKING="${l1}" \
            ENABLE_LOOP_2_ROLLING_WINDOW="${l2}" \
            ENABLE_LOOP_3_LINEAR_DECAY="${l3}" \
            ENABLE_LOOP_4_PIPELINING_METRICS="${l4}" \
            ENABLE_LOOP_5_WINDOW_INFERENCE="${l5}" \
            ENABLE_LOOP_6_DECAY_INFERENCE="${l6}" \
            ENABLE_LOOP_7_ADAPTIVE_HEARTRATE="${l7}" \
            > "${LOG_DIR}/build_${config_name}.log" 2>&1; then
        log_success "Build completed for ${config_name}"
        echo "${BUILD_DIR}/amd64/${BUILD_PROFILE}/starforth"
    else
        log_error "Build failed for ${config_name}"
        cat "${LOG_DIR}/build_${config_name}.log" >&2
        return 1
    fi
}

################################################################################
# Test Matrix Generation
################################################################################

generate_test_matrix() {
    local matrix_file="${TEST_MATRIX}"
    > "${matrix_file}"

    # Generate all configs × runs
    for config in $(generate_all_configurations); do
        for run in $(seq 1 ${RUNS_PER_CONFIG}); do
            echo "${config},${run}" >> "${matrix_file}"
        done
    done

    # Randomize entire matrix
    sort -R "${matrix_file}" > "${matrix_file}.shuffled"
    mv "${matrix_file}.shuffled" "${matrix_file}"

    echo "${matrix_file}"
}

################################################################################
# Execution Functions
################################################################################

run_single_iteration() {
    local binary=$1
    local config=$2
    local run_num=$3
    local output_log=$4

    if "${binary}" --doe-experiment > "${output_log}" 2>&1; then
        return 0
    else
        return 1
    fi
}

run_experiment() {
    local matrix_file=$1

    log_header "RANDOMIZED FACTORIAL EXPERIMENT (${TOTAL_RUNS} runs)"

    local current_config=""
    local current_binary=""
    local current_build_status="OK"
    local run_index=0
    local incompatible_configs=()
    local successful_runs=0
    local failed_configs=0

    # Read shuffled test matrix
    while IFS=',' read -r config_name run_number; do
        run_index=$((run_index + 1))

        # Build if configuration changed
        if [ "${current_config}" != "${config_name}" ]; then
            local flags=$(config_to_loop_flags "${config_name}")
            local l1="${flags%,*}"
            flags="${flags#*,}"
            local l2="${flags%,*}"
            flags="${flags#*,}"
            local l3="${flags%,*}"
            flags="${flags#*,}"
            local l4="${flags%,*}"
            flags="${flags#*,}"
            local l5="${flags%,*}"
            flags="${flags#*,}"
            local l6="${flags%,*}"
            flags="${flags#*,}"
            local l7="${flags#*,}"

            # Attempt to build configuration
            if current_binary=$(build_configuration "${config_name}" "${l1}" "${l2}" "${l3}" "${l4}" "${l5}" "${l6}" "${l7}" 2>/dev/null); then
                current_build_status="OK"
                log_success "Build successful for ${config_name}"
            else
                # Build failed - mark as incompatible and continue
                current_build_status="BUILD_FAILED"
                incompatible_configs+=("${config_name}")
                failed_configs=$((failed_configs + 1))
                log_error "Configuration ${config_name} is INCOMPATIBLE (build failure) - recording as data point"
                current_config="${config_name}"
                current_binary=""
                continue
            fi
            current_config="${config_name}"
        fi

        # Skip execution if build failed for this config
        if [ "${current_build_status}" = "BUILD_FAILED" ]; then
            log_info "Run ${run_index}/${TOTAL_RUNS} - config ${config_name} #${run_number} skipped (incompatible)"
            continue
        fi

        # Execute run
        local run_log="${LOG_DIR}/${config_name}_run_${run_number}.log"
        local start_time=$(date +%s)

        log_info "Run ${run_index}/${TOTAL_RUNS} - config ${config_name} #${run_number}..."

        if run_single_iteration "${current_binary}" "${config_name}" "${run_number}" "${run_log}"; then
            # Extract CSV row
            local csv_row
            if ! csv_row=$(extract_final_csv_row "${run_log}"); then
                log_error "Run ${run_index}/${TOTAL_RUNS} missing metrics - may indicate crash"
                # Record as crash (incompatible)
                current_build_status="CRASH"
                if [[ ! " ${incompatible_configs[@]} " =~ " ${config_name} " ]]; then
                    incompatible_configs+=("${config_name}")
                    failed_configs=$((failed_configs + 1))
                fi
                log_error "Configuration ${config_name} is INCOMPATIBLE (StarForth crash) - recording as data point"
                continue
            fi

            if [ -z "${csv_row}" ]; then
                log_error "Run ${run_index}/${TOTAL_RUNS} empty metrics - configuration may be unstable"
                current_build_status="CRASH"
                if [[ ! " ${incompatible_configs[@]} " =~ " ${config_name} " ]]; then
                    incompatible_configs+=("${config_name}")
                    failed_configs=$((failed_configs + 1))
                fi
                log_error "Configuration ${config_name} is INCOMPATIBLE (no metrics) - recording as data point"
                continue
            fi

            # Append config flags to CSV row with build status
            local flags=$(config_to_loop_flags "${config_name}")
            local ts_now=$(timestamp)
            printf '%s,%s,%s,%s,%s,%s,%s\n' "${ts_now}" "${config_name}" "${run_number}" "OK" "OK" "${csv_row}" "${flags}" >> "${RESULTS_CSV}"

            successful_runs=$((successful_runs + 1))
            local elapsed=$(runtime_seconds ${start_time} $(date +%s))
            log_success "Run ${run_index}/${TOTAL_RUNS} completed (${elapsed}s)"
        else
            # Binary execution failed (crash/signal)
            current_build_status="CRASH"
            if [[ ! " ${incompatible_configs[@]} " =~ " ${config_name} " ]]; then
                incompatible_configs+=("${config_name}")
                failed_configs=$((failed_configs + 1))
            fi
            log_error "Configuration ${config_name} is INCOMPATIBLE (StarForth crash/signal exit) - recording as data point"

            # Record incompatibility without metrics
            local flags=$(config_to_loop_flags "${config_name}")
            local ts_now=$(timestamp)
            printf '%s,%s,%s,%s,%s,%s\n' "${ts_now}" "${config_name}" "${run_number}" "OK" "CRASH" "${flags}" >> "${RESULTS_CSV}"
        fi

    done < "${matrix_file}"

    log_header "EXPERIMENT COMPLETE (with incompatibility detection)"
    log_success "Successful runs: ${successful_runs}/${TOTAL_RUNS}"
    log_success "Failed configurations (incompatible): ${failed_configs}"

    if [ ${#incompatible_configs[@]} -gt 0 ]; then
        echo ""
        log_info "Incompatible configurations detected:"
        for config in "${incompatible_configs[@]}"; do
            log_error "  ${config}"
        done
        echo ""
        log_info "These configs are marked in CSV with build_status or run_status = INCOMPATIBLE"
        log_info "They represent configurations where loops cannot coexist without conflict"
    fi
}

################################################################################
# Main
################################################################################

main() {
    local experiment_start=$(date +%s)
    local start_time=$(timestamp)

    # Calculate totals
    TOTAL_CONFIGS=128
    TOTAL_RUNS=$((TOTAL_CONFIGS * RUNS_PER_CONFIG))

    log_header "STARFORTH COMPLETE 2^7 FACTORIAL DoE"
    log_info "Factors: 7 feedback loops (LOOP_1 through LOOP_7)"
    log_info "Configurations: 2^7 = 128"
    log_info "Runs per config: ${RUNS_PER_CONFIG}"
    log_info "TOTAL RUNS: ${TOTAL_RUNS}"
    log_info "Workload: Complete test harness (936+ FORTH tests)"
    log_info "Output: ${OUTPUT_DIR}"

    # Initialize
    init_csv_header
    write_configuration_manifest

    log_section "Generating randomized test matrix..."
    local matrix_file
    if ! matrix_file=$(generate_test_matrix); then
        log_error "Failed to generate test matrix"
        return 1
    fi
    log_success "Test matrix generated: ${matrix_file}"

    # Show preview
    echo ""
    log_info "Test Matrix Preview (first 10 of ${TOTAL_RUNS} runs):"
    head -10 "${matrix_file}" | sed 's/^/  /'
    echo "  ..."
    echo ""

    # Wait for confirmation
    log_info "Complete test matrix saved: ${matrix_file}"
    read -p "Press ENTER to begin execution (Ctrl+C to abort): "

    # Execute
    if ! run_experiment "${matrix_file}"; then
        log_error "Experiment failed"
        return 1
    fi

    # Summary
    local experiment_end=$(date +%s)
    local total_seconds=$(runtime_seconds ${experiment_start} ${experiment_end})
    local end_time=$(timestamp)

    log_header "EXPERIMENT COMPLETE"
    log_success "All ${TOTAL_RUNS} runs completed!"
    log_success "Start time: ${start_time}"
    log_success "End time: ${end_time}"
    log_success "Total runtime: $(format_duration ${total_seconds})"
    log_success "Results: ${RESULTS_CSV}"
    log_success "Configs: ${CONFIG_MANIFEST}"
    log_success "Logs: ${LOG_DIR}/"

    echo ""
    log_info "CSV Results Preview (first 5 data rows):"
    head -6 "${RESULTS_CSV}" | sed 's/^/  /'

    echo ""
    log_info "Next steps:"
    log_info "  1. Download results to analysis repository"
    log_info "  2. Run factorial analysis (main effects, interactions)"
    log_info "  3. Identify optimal configuration subset"

    return 0
}

main "$@"
exit $?
