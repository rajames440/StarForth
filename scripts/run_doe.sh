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
#  StarForth Physics Engine Experimental Iteration Runner
#
#  ONE EXPERIMENT = (30 × iterations) × 8 builds
#
#  This script conducts a single empirical experimental iteration across all
#  four base build configurations (A_BASELINE, A+B, A+C, A+B+C) crossed with
#  two heartbeat modes (threaded ON/OFF). Sample size per combination scales
#  with the --exp-iterations parameter.
#
#  The iterations parameter controls statistical power:
#  - iterations=1 (120 runs):   Quick check ("Is this tuning worth pursuing?")
#  - iterations=2 (240 runs):   Baseline comprehensive run
#  - iterations=3 (360 runs):   Push it harder for clearer picture
#  - iterations=4 (480 runs):   Even more data for optimal stability
#
#  Design:
#  ─────────────────────────────────────────────────────────────────────────
#  Sample Size (per base config × heartbeat mode): 30 × iterations
#  Build Variants: 8 (4 base configs × 2 heartbeat modes)
#  Total Runs: 30 × iterations × 8
#
#  All iterations are aggregated into ONE dataset for unified analysis.
#  Randomized execution order (per DoE principles).
#  Pre-generated test matrix shown to user before execution begins.
#
#  Workload: Complete Test Harness
#  ─────────────────────────────────────────────────────────────────────────
#  The --doe-experiment flag triggers:
#    1. Physics metrics reset (PHYSICS-RESET-STATS)
#    2. Comprehensive test harness execution (936+ FORTH tests)
#    3. Metrics collection from hotwords cache and runtime state
#    4. CSV row output to stdout
#
#  The test harness IS the workload - comprehensive, deterministic, CPU-bound,
#  realistic representation of StarForth VM capabilities.
#
#  Usage:
#    ./scripts/run_doe.sh [--exp-iterations N] EXPERIMENT_LABEL
#
#  Examples:
#    ./scripts/run_doe.sh DOE_01
#              → stores data in /home/rajames/CLionProjects/StarForth-DoE/experiments/DOE_01
#
#    ./scripts/run_doe.sh --exp-iterations 2 TST_02
#              → same base path, subdirectory TST_02
#
#  Output (always rooted at /home/rajames/CLionProjects/StarForth-DoE/experiments):
#    <base>/<EXPERIMENT_LABEL>/
#      ├── experiment_results.csv    (N rows of metrics, 35 columns per run)
#      ├── experiment_summary.txt    (metadata, runtime, analysis notes)
#      ├── test_matrix.txt           (complete randomized run order)
#      ├── run_logs/                 (individual per-run logs)
#      │   ├── A_BASELINE_run_1.log
#      │   ├── A_B_CACHE_run_1.log
#      │   ├── A_C_FULL_run_1.log
#      │   ├── A_B_C_FULL_run_1.log
#      │   └── ... (N total logs)
#      └── experiment_notes.txt      (observations for tuning next iteration)
#
#  Expected Runtime:
#    - 120 runs (iter=1):  2-4 minutes (via C-level hook, shared VM state)
#    - 240 runs (iter=2):  4-8 minutes
#    - 360 runs (iter=3):  6-12 minutes
#    - 480 runs (iter=4):  8-16 minutes
#
################################################################################

set -e

# Parse command-line arguments
EXP_ITERATIONS=1
OUTPUT_DIR=""
EXPERIMENTS_BASE="/home/rajames/CLionProjects/StarForth-DoE/experiments"

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

# Validate arguments
if [ -z "${OUTPUT_DIR}" ]; then
    echo "Usage: $0 [--exp-iterations N] EXPERIMENT_LABEL"
    echo ""
    echo "Examples:"
    echo "  $0 DOE_01"
    echo "  $0 --exp-iterations 2 TST_02"
    echo ""
    exit 1
fi

# Validate iterations parameter
if ! [[ "${EXP_ITERATIONS}" =~ ^[0-9]+$ ]] || [ "${EXP_ITERATIONS}" -lt 1 ]; then
    echo "Error: --exp-iterations must be a positive integer (got: ${EXP_ITERATIONS})"
    exit 1
fi

# Calculate total runs
RUNS_PER_CONFIG=$((30 * EXP_ITERATIONS))
BUILD_PROFILE="fastest"

# Base configurations and heartbeat modes
# NOTE (2025-11-19): Experiments show A_B_C_FULL (cache + pipelining) with HB_ON is optimal
# All future experiments use this single configuration for measuring variance, tuning knobs, etc.
BASE_CONFIGS=(
    "A_B_C_FULL"
)
HEARTBEAT_MODES=("HB_ON")

NUM_BUILDS=$(( ${#BASE_CONFIGS[@]} * ${#HEARTBEAT_MODES[@]} ))
TOTAL_RUNS=$((RUNS_PER_CONFIG * NUM_BUILDS))

# Paths - must be run from repo root
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"

# Resolve experiment label to absolute destination under experiments base
if [[ "${OUTPUT_DIR}" == *"/"* ]]; then
    echo "Error: experiment label must not contain '/' characters (got: ${OUTPUT_DIR})"
    exit 1
fi

if [[ "${OUTPUT_DIR}" == *".."* ]]; then
    echo "Error: experiment label must not contain '..' sequences"
    exit 1
fi

# Ensure base directory exists and then expand label into full path
mkdir -p "${EXPERIMENTS_BASE}" || {
    echo "Error: unable to create experiments base at ${EXPERIMENTS_BASE}"
    exit 1
}

OUTPUT_DIR="${EXPERIMENTS_BASE%/}/${OUTPUT_DIR}"

LOG_DIR="${OUTPUT_DIR}/run_logs"
RESULTS_CSV="${OUTPUT_DIR}/experiment_results.csv"
SUMMARY_LOG="${OUTPUT_DIR}/experiment_summary.txt"
TEST_MATRIX="${LOG_DIR}/test_matrix.txt"
EXPERIMENT_NOTES="${OUTPUT_DIR}/experiment_notes.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Ensure output directories exist
mkdir -p "${LOG_DIR}"
mkdir -p "${OUTPUT_DIR}"

################################################################################
# Helper Functions
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

extract_final_csv_row() {
    local log_file=$1
    if [ ! -f "${log_file}" ]; then
        return 1
    fi

    awk '!/^HB,/ && NF {line=$0} END {if (length(line)) print line}' "${log_file}"
}

################################################################################
# CSV and Metrics Functions
################################################################################

init_csv_header() {
    # Create CSV header with metadata columns + metrics from C API
    # Format: timestamp,configuration,run_number,<35 C API metrics>
    {
        printf 'timestamp,configuration,run_number,'
        # The C function outputs the metrics header (without trailing newline for this format)
        # We replicate it here to ensure consistency with what metrics_write_csv_row() outputs
        printf 'total_lookups,cache_hits,cache_hit_percent,bucket_hits,bucket_hit_percent,'
        printf 'cache_hit_latency_ns,cache_hit_stddev_ns,bucket_search_latency_ns,bucket_search_stddev_ns,'
        printf 'context_predictions_total,context_correct,context_accuracy_percent,'
        printf 'rolling_window_width,decay_slope,'
        printf 'hot_word_count,stale_word_ratio,avg_word_heat,'
        printf 'prefetch_accuracy_percent,prefetch_attempts,prefetch_hits,window_tuning_checks,final_effective_window_size,'
        printf 'vm_workload_duration_ns_q48,cpu_temp_delta_c_q48,cpu_freq_delta_mhz_q48,'
        printf 'decay_rate_q16,decay_min_interval_ns,rolling_window_size,adaptive_shrink_rate,heat_cache_demotion_threshold,'
        printf 'enable_hotwords_cache,enable_pipelining\n'
    } > "${RESULTS_CSV}"
    log_success "CSV header created: ${RESULTS_CSV}"
    log_info "CSV columns: timestamp (metadata) + 35 metrics from C API + configuration metadata"
}

################################################################################
# Build Functions
################################################################################

build_configuration() {
    local config_name=$1
    local cache_flag=$2
    local pipeline_flag=$3
    local heartbeat_flag=$4

    log_section "Building Configuration: ${config_name}"

    cd "${REPO_ROOT}"

    # Clean previous build
    make clean > /dev/null 2>&1 || true

    # Build with specific configuration
    log_info "Building: make TARGET=${BUILD_PROFILE} ENABLE_HOTWORDS_CACHE=${cache_flag} ENABLE_PIPELINING=${pipeline_flag} HEARTBEAT_THREAD_ENABLED=${heartbeat_flag}"

    if make TARGET="${BUILD_PROFILE}" \
            ENABLE_HOTWORDS_CACHE="${cache_flag}" \
            ENABLE_PIPELINING="${pipeline_flag}" \
            HEARTBEAT_THREAD_ENABLED="${heartbeat_flag}" \
            > "${LOG_DIR}/build_${config_name}.log" 2>&1; then
        log_success "Build completed for ${config_name}"
        echo "${BUILD_DIR}/amd64/${BUILD_PROFILE}/starforth"
    else
        log_error "Build failed for ${config_name}"
        cat "${LOG_DIR}/build_${config_name}.log"
        return 1
    fi
}

################################################################################
# Configuration Mapping
################################################################################

config_to_build_flags() {
    local config=$1
    case "${config}" in
        A_B_C_FULL)
            # Optimal configuration: cache enabled (1), pipelining enabled (1)
            echo "1,1"
            ;;
        *)
            # Default to optimal config if unknown
            echo "1,1"
            ;;
    esac
}

heartbeat_flag_from_label() {
    local label=$1
    case "${label}" in
        HB_OFF)
            echo 0
            ;;
        *)
            echo 1
            ;;
    esac
}

parse_combined_config_name() {
    local combined=$1
    local base="$combined"
    local hb_label="HB_ON"
    if [[ "$combined" == *"__HB_"* ]]; then
        base=${combined%%__HB_*}
        local hb_suffix=${combined##*__HB_}
        hb_label="HB_${hb_suffix}"
    fi
    local hb_flag
    hb_flag=$(heartbeat_flag_from_label "${hb_label}")
    echo "${base} ${hb_flag} ${hb_label}"
}

################################################################################
# Test Matrix Generation
################################################################################

generate_test_matrix() {
    # Generate ONE complete test matrix with all runs
    # Format: config_name,run_number_within_config
    # All iterations aggregated together

    local matrix_file="${TEST_MATRIX}"
    > "${matrix_file}"  # Clear file

    # Generate runs for each base config + heartbeat mode
    for config in "${BASE_CONFIGS[@]}"; do
        for hb in "${HEARTBEAT_MODES[@]}"; do
            local combined="${config}__${hb}"
            for run in $(seq 1 ${RUNS_PER_CONFIG}); do
                echo "${combined},${run}" >> "${matrix_file}"
            done
        done
    done

    # Randomize the entire matrix
    sort -R "${matrix_file}" > "${matrix_file}.shuffled"
    mv "${matrix_file}.shuffled" "${matrix_file}"

    echo "${matrix_file}"
}

################################################################################
# Binary Execution Functions
################################################################################

run_single_doe_iteration() {
    local binary=$1
    local config=$2
    local run_num=$3
    local output_log=$4

    # Execute binary with --doe-experiment flag
    # The binary:
    #   1. Runs the test harness (936+ FORTH tests)
    #   2. Collects metrics during execution
    #   3. Outputs CSV row to stdout
    #   4. Exits
    #
    # The test harness IS the workload - comprehensive, deterministic, CPU-bound
    if "${binary}" --doe-experiment > "${output_log}" 2>&1; then
        return 0
    else
        return 1
    fi
}

################################################################################
# Experiment Execution
################################################################################

run_experiment() {
    local matrix_file=$1

    log_header "RANDOMIZED EXPERIMENTAL ITERATION (${TOTAL_RUNS} runs, ${EXP_ITERATIONS} × (30 × 8))"

    local current_config=""
    local current_binary=""
    local run_index=0

    # Read shuffled test matrix and execute
    while IFS=',' read -r config_name run_number; do
        run_index=$((run_index + 1))

        read -r base_config heartbeat_flag heartbeat_label <<< "$(parse_combined_config_name "${config_name}")"

        # Build new configuration if needed
        if [ "${current_config}" != "${config_name}" ]; then
            local flags=$(config_to_build_flags "${base_config}")
            local cache_flag="${flags%,*}"
            local pipeline_flag="${flags#*,}"

            if ! current_binary=$(build_configuration "${config_name}" "${cache_flag}" "${pipeline_flag}" "${heartbeat_flag}"); then
                log_error "Failed to build ${config_name}"
                return 1
            fi

            log_success "Binary ready for ${config_name}"
            current_config="${config_name}"
        fi

        # Execute run
        local run_log="${LOG_DIR}/${config_name}_run_${run_number}.log"
        local start_time=$(date +%s)

        local hb_display="${heartbeat_label}"
        if [[ "${hb_display}" == HB_* ]]; then
            hb_display=${hb_display#HB_}
        fi
        log_info "Run ${run_index}/${TOTAL_RUNS} - ${config_name} (HB=${hb_display}) #${run_number}..."

        if run_single_doe_iteration "${current_binary}" "${config_name}" "${run_number}" "${run_log}"; then
            # Binary outputs CSV row at the end of the log; extract just that row for the CSV
            local csv_row
            if ! csv_row=$(extract_final_csv_row "${run_log}"); then
                log_error "Run ${run_index}/${TOTAL_RUNS} missing final metrics row - check ${run_log}"
                return 1
            fi

            if [ -z "${csv_row}" ]; then
                log_error "Run ${run_index}/${TOTAL_RUNS} produced empty metrics row - check ${run_log}"
                return 1
            fi

            local ts_now=$(timestamp)
            printf '%s,%s,%s,%s\n' "${ts_now}" "${config_name}" "${run_number}" "${csv_row}" >> "${RESULTS_CSV}"

            local elapsed=$(runtime_seconds ${start_time} $(date +%s))
            log_success "Run ${run_index}/${TOTAL_RUNS} completed (${elapsed}s)"
        else
            log_error "Run ${run_index}/${TOTAL_RUNS} failed - check ${run_log}"
            return 1
        fi

    done < "${matrix_file}"

    log_success "All ${TOTAL_RUNS} runs completed successfully!"
}

################################################################################
# Main Execution
################################################################################

main() {
    local experiment_start=$(date +%s)
    local start_time=$(timestamp)

    log_header "STARFORTH PHYSICS ENGINE EXPERIMENTAL ITERATION"
    log_info "Iterations: ${EXP_ITERATIONS}"
log_info "Runs per base configuration: ${RUNS_PER_CONFIG} (30 × ${EXP_ITERATIONS})"
log_info "Heartbeat modes per base config: ${#HEARTBEAT_MODES[@]}"
log_info "Number of Build Variants: ${NUM_BUILDS}"
log_info "TOTAL RUNS: ${TOTAL_RUNS}"
    log_info "Workload: Complete test harness (936+ FORTH tests)"
    log_info "Experiments base: ${EXPERIMENTS_BASE}"
    log_info "Experiment directory: ${OUTPUT_DIR}"

    # Initialize CSV
    init_csv_header

    # Generate complete test matrix
    log_section "Generating complete randomized test matrix..."
    local matrix_file
    if ! matrix_file=$(generate_test_matrix); then
        log_error "Failed to generate test matrix"
        return 1
    fi
    log_success "Test matrix generated with all ${TOTAL_RUNS} runs: ${matrix_file}"

    # Show test matrix preview (first 20 runs)
    echo ""
    log_info "Test Matrix Preview (first 20 of ${TOTAL_RUNS} randomized runs):"
    head -20 "${matrix_file}" | sed 's/^/  /'
    echo "  ..."
    echo ""

    # Wait for user confirmation before execution
    log_info "Complete test matrix saved to: ${matrix_file}"
    read -p "Press ENTER to begin execution (or Ctrl+C to abort): "

    # Execute experiment
    if ! run_experiment "${matrix_file}"; then
        log_error "Experiment failed"
        return 1
    fi

    # Summary
    local experiment_end=$(date +%s)
    local total_seconds=$(runtime_seconds ${experiment_start} ${experiment_end})
    local total_minutes=$((total_seconds / 60))
    local total_hours=$((total_minutes / 60))
    local end_time=$(timestamp)

    log_header "EXPERIMENTAL ITERATION COMPLETE"

    log_success "Experimental iteration completed successfully!"
    log_success "Start time:    ${start_time}"
    log_success "End time:      ${end_time}"
    log_success "Total runtime: ${total_hours}h ${total_minutes}m ${total_seconds}s"
    log_success "Total runs:    ${TOTAL_RUNS}"
    log_success "Results saved: ${RESULTS_CSV}"

    echo ""
    log_info "CSV Results Preview (first 5 data rows):"
    head -6 "${RESULTS_CSV}" | sed 's/^/  /'

    echo ""
    echo ""
    log_info "Next steps:"
    log_info "  1. Review results: ${RESULTS_CSV}"
    log_info "  2. View test matrix: cat ${TEST_MATRIX}"
    log_info "  3. Analyze data: python3 scripts/analyze_doe_results.py ${RESULTS_CSV}"
    log_info "  4. Decide: Refine tuning and run next iteration, or sufficient data?"

    return 0
}

# Execute
main "$@"
exit $?
