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
#  StarForth Factorial DoE with Heartbeat Observability
#
#  FOCUSED EXPERIMENT: Top 5 Configurations (Stage 1 Winners)
#
#  Design:
#  ───────────────────────────────────────────────────────────────────────────
#
#  This script executes a focused factorial design of experiments:
#  - 5 elite configurations (identified from Stage 1 baseline DoE)
#  - Each tested N times (default: 150 runs per config)
#  - Total runs: 5 × 150 = 750 runs (statistically significant for correlation analysis)
#  - All heartbeat dynamics captured: tick interval, jitter, convergence
#
#  Elite Configurations (from 3200-run Stage 1 analysis):
#  ───────────────────────────────────────────────────────────────────────────
#  1. 1_0_1_1_1_0 - Strong performer, minimal inference overhead
#  2. 1_0_1_1_1_1 - Similar to #1, with decay inference
#  3. 1_1_0_1_1_1 - Alternative: heat + window + pipelining + all inference
#  4. 1_0_1_0_1_0 - Lean config: heat + decay only
#  5. 0_1_1_0_1_1 - Contrast: window + decay + inference without heat tracking
#
#  Factors (binary: 0 or 1):
#  ───────────────────────────────────────────────────────────────────────────
#  L1 = ENABLE_LOOP_1_HEAT_TRACKING       (execution heat counting)
#  L2 = ENABLE_LOOP_2_ROLLING_WINDOW      (rolling window history)
#  L3 = ENABLE_LOOP_3_LINEAR_DECAY        (exponential heat decay)
#  L4 = ENABLE_LOOP_4_PIPELINING_METRICS  (word transition tracking)
#  L5 = ENABLE_LOOP_5_WINDOW_INFERENCE    (window width inference)
#  L6 = ENABLE_LOOP_6_DECAY_INFERENCE     (decay slope inference)
#
#  Heartbeat Metrics Collected:
#  ───────────────────────────────────────────────────────────────────────────
#  - Per-tick interval (ns)
#  - Tick jitter (CV, outliers)
#  - Cache hit % stability (rolling window)
#  - Window convergence trajectory
#  - Decay slope fitting progress
#  - Load→heartbeat response coupling
#  - Overall stability score
#
#  Output Structure:
#  ───────────────────────────────────────────────────────────────────────────
#  <base>/<EXPERIMENT_LABEL>/
#    ├── experiment_results_heartbeat.csv  (250 rows, 60+ columns)
#    ├── experiment_summary.txt            (metadata, timing, notes)
#    ├── test_matrix.txt                   (randomized run order)
#    ├── configuration_manifest.txt        (top 5 configs, rationale)
#    ├── run_logs/                         (per-run execution logs)
#    │   ├── 1_0_1_1_1_0_run_1.log
#    │   ├── 1_0_1_1_1_1_run_1.log
#    │   └── ... (250 total logs)
#    └── stability_analysis.txt            (preliminary analysis results_run_01_2025_12_08)
#
#  Expected Runtime:
#  ───────────────────────────────────────────────────────────────────────────
#  - 50 runs/config (250 total): ~45 min - 1.5 hours
#  - 100 runs/config (500 total): ~1.5 - 3 hours
#
#  Usage:
#    ./scripts/run_factorial_doe_with_heartbeat.sh EXPERIMENT_LABEL
#    ./scripts/run_factorial_doe_with_heartbeat.sh --runs-per-config 100 HEARTBEAT_HW
#
#  Example:
#    ./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
#      → stores in /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5
#
################################################################################

set -e

################################################################################
# Parse Arguments
################################################################################

RUNS_PER_CONFIG=150
OUTPUT_DIR=""
EXPERIMENTS_BASE="/home/rajames/CLionProjects/StarForth-DoE/experiments"

while [[ $# -gt 0 ]]; do
    case $1 in
        --runs-per-config)
            RUNS_PER_CONFIG="$2"
            shift 2
            ;;
        -*)
            echo "Unknown option: $1"
            echo "Usage: $0 [--runs-per-config N] EXPERIMENT_LABEL"
            exit 1
            ;;
        *)
            OUTPUT_DIR="$1"
            shift
            ;;
    esac
done

if [ -z "${OUTPUT_DIR}" ]; then
    echo "Usage: $0 [--runs-per-config N] EXPERIMENT_LABEL"
    echo ""
    echo "Examples:"
    echo "  $0 2025_11_20_HEARTBEAT_TOP5           # 150 runs per config (750 total)"
    echo "  $0 --runs-per-config 200 HEARTBEAT_FULL  # 200 runs per config (1000 total)"
    exit 1
fi

if ! [[ "${RUNS_PER_CONFIG}" =~ ^[0-9]+$ ]] || [ "${RUNS_PER_CONFIG}" -lt 1 ]; then
    echo "Error: --runs-per-config must be a positive integer"
    exit 1
fi

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
RESULTS_CSV="${OUTPUT_DIR}/experiment_results_heartbeat.csv"
SUMMARY_LOG="${OUTPUT_DIR}/experiment_summary.txt"
TEST_MATRIX="${OUTPUT_DIR}/test_matrix.txt"
CONFIG_MANIFEST="${OUTPUT_DIR}/configuration_manifest.txt"
ANALYSIS_LOG="${OUTPUT_DIR}/stability_analysis.txt"

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

extract_heartbeat_timeseries() {
    local log_file=$1
    local out_file=$2
    if [ ! -f "${log_file}" ]; then
        return 1
    fi
    grep '^HB,' "${log_file}" > "${out_file}" || true
}

extract_heartbeat_detail_csv() {
    local log_file=$1
    local out_file=$2
    if [ ! -f "${log_file}" ]; then
        return 1
    fi
    awk 'BEGIN{emit=0} /^run_id,config/ {emit=1} emit && NF' "${log_file}" > "${out_file}" || true
}

################################################################################
# Top 5 Configurations (Elite Subset from Stage 1)
################################################################################

declare -a TOP5_CONFIGS=(
    "1_0_1_1_1_0"   # Config 1: F1 F3 F4 F5 (strong, minimal inference)
    "1_0_1_1_1_1"   # Config 2: F1 F3 F4 F5 F6 (add decay inference)
    "1_1_0_1_1_1"   # Config 3: F1 F2 F4 F5 F6 (alternative path)
    "1_0_1_0_1_0"   # Config 4: F1 F3 F5 (lean)
    "0_1_1_0_1_1"   # Config 5: F2 F3 F5 F6 (contrast)
)

declare -A CONFIG_DESCRIPTION=(
    ["1_0_1_1_1_0"]="Heat tracking + Decay + Pipelining metrics + Window inference (minimal overhead)"
    ["1_0_1_1_1_1"]="Above + Decay slope inference"
    ["1_1_0_1_1_1"]="Heat + Window history + Pipelining + Both inferences (skip decay loop)"
    ["1_0_1_0_1_0"]="Heat + Decay + Window inference only (lean config)"
    ["0_1_1_0_1_1"]="Window + Decay + Both inferences (no heat tracking)"
)

################################################################################
# Configuration Mapping
################################################################################

config_to_loop_flags() {
    # Parse config name "L1_L2_L3_L4_L5_L6" and output 6 comma-separated values
    local config=$1

    # config format: "L1_L2_L3_L4_L5_L6"
    local l1=$(echo "$config" | cut -d'_' -f1)
    local l2=$(echo "$config" | cut -d'_' -f2)
    local l3=$(echo "$config" | cut -d'_' -f3)
    local l4=$(echo "$config" | cut -d'_' -f4)
    local l5=$(echo "$config" | cut -d'_' -f5)
    local l6=$(echo "$config" | cut -d'_' -f6)

    echo "${l1},${l2},${l3},${l4},${l5},${l6}"
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
        printf 'total_heartbeat_ticks,tick_interval_mean_ns,tick_interval_stddev_ns,tick_interval_cv,'
        printf 'tick_outlier_count_3sigma,tick_outlier_ratio,'
        printf 'cache_hit_percent_rolling_mean,cache_hit_percent_rolling_stddev,cache_stability_index,'
        printf 'window_final_effective_size,pattern_diversity_final,diversity_growth_final,window_warmth_achieved,'
        printf 'decay_slope_fitted_final,decay_slope_convergence_rate,decay_slope_stable,'
        printf 'load_interval_correlation,response_latency_ticks,overshoot_ratio,settling_time_ticks,'
        printf 'overall_stability_score\n'
    } > "${RESULTS_CSV}"
    log_success "CSV header created (with heartbeat metrics)"
}

write_configuration_manifest() {
    # Document the 5 elite configurations
    {
        echo "# StarForth Heartbeat DoE - Top 5 Elite Configurations"
        echo "# Generated: $(timestamp)"
        echo "# Derived from: Stage 1 baseline (3200 runs, 2^6 factorial)"
        echo ""
        echo "## Rationale"
        echo "These 5 configurations were selected from Stage 1 analysis as exhibiting"
        echo "the best trade-offs between performance and stability. This Phase 2 experiment"
        echo "adds heartbeat observability to measure temporal stability characteristics:"
        echo "- Heartbeat jitter control (CV of tick interval)"
        echo "- Convergence speed (decay slope fitting)"
        echo "- Load-response coupling (correlation of workload to interval)"
        echo "- Overall stability score (composite metric)"
        echo ""
        echo "## Configuration Format: L1_L2_L3_L4_L5_L6"
        echo ""
        echo "Factors:"
        echo "  L1 = ENABLE_LOOP_1_HEAT_TRACKING (execution heat counting)"
        echo "  L2 = ENABLE_LOOP_2_ROLLING_WINDOW (rolling window history)"
        echo "  L3 = ENABLE_LOOP_3_LINEAR_DECAY (exponential heat decay)"
        echo "  L4 = ENABLE_LOOP_4_PIPELINING_METRICS (word transition tracking)"
        echo "  L5 = ENABLE_LOOP_5_WINDOW_INFERENCE (window width inference)"
        echo "  L6 = ENABLE_LOOP_6_DECAY_INFERENCE (decay slope inference)"
        echo ""
        echo "## Elite Configurations (Top 5)"
        echo ""

        local idx=1
        for config in "${TOP5_CONFIGS[@]}"; do
            printf "%d. %s\n" $idx "$config"
            printf "   Description: %s\n" "${CONFIG_DESCRIPTION[$config]}"
            echo ""
            idx=$((idx + 1))
        done

        echo "## Analysis Dimensions"
        echo ""
        echo "### Jitter Control"
        echo "  - Metric: tick_interval_cv (coefficient of variation)"
        echo "  - Interpretation: Lower CV = steadier heartbeat = better temporal stability"
        echo "  - Target: < 0.15 (15% variation is acceptable)"
        echo ""
        echo "### Convergence Speed"
        echo "  - Metric: decay_slope_convergence_rate"
        echo "  - Interpretation: Faster convergence = inference engine tunes quicker"
        echo "  - Target: Converge within 5000 ticks (5% of typical run)"
        echo ""
        echo "### Load Response"
        echo "  - Metric: load_interval_correlation"
        echo "  - Interpretation: Higher correlation = heartbeat responds smoothly to load"
        echo "  - Target: > 0.7 (strong coupling, no lag)"
        echo ""
        echo "### Overall Stability"
        echo "  - Metric: overall_stability_score (0-100, composite)"
        echo "  - Components: jitter (weight 1/3) + convergence (1/3) + coupling (1/3)"
        echo "  - Target: > 75 (excellent stability)"
        echo ""
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

    cd "${REPO_ROOT}"

    # Clean previous build
    make clean > /dev/null 2>&1 || true

    # Build with specific loop configuration
    log_info "Building: make TARGET=${BUILD_PROFILE} with loops: L1=${l1} L2=${l2} L3=${l3} L4=${l4} L5=${l5} L6=${l6}"

    if make TARGET="${BUILD_PROFILE}" \
            ENABLE_LOOP_1_HEAT_TRACKING="${l1}" \
            ENABLE_LOOP_2_ROLLING_WINDOW="${l2}" \
            ENABLE_LOOP_3_LINEAR_DECAY="${l3}" \
            ENABLE_LOOP_4_PIPELINING_METRICS="${l4}" \
            ENABLE_LOOP_5_WINDOW_INFERENCE="${l5}" \
            ENABLE_LOOP_6_DECAY_INFERENCE="${l6}" \
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
# Test Matrix Generation (Randomized)
################################################################################

generate_test_matrix() {
    local matrix_file="${TEST_MATRIX}"
    > "${matrix_file}"

    # Generate all configs × runs
    for config in "${TOP5_CONFIGS[@]}"; do
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

    log_header "RANDOMIZED HEARTBEAT EXPERIMENT (${TOTAL_RUNS} runs)"

    local current_config=""
    local current_binary=""
    local current_build_status="OK"
    local run_index=0
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
            local l6="${flags#*,}"

            # Attempt to build configuration
            if current_binary=$(build_configuration "${config_name}" "${l1}" "${l2}" "${l3}" "${l4}" "${l5}" "${l6}" 2>/dev/null); then
                current_build_status="OK"
                log_success "Build successful for ${config_name}"
            else
                # Build failed
                current_build_status="BUILD_FAILED"
                log_error "Configuration ${config_name} failed to build - skipping runs"
                current_config="${config_name}"
                current_binary=""
                continue
            fi
            current_config="${config_name}"
        fi

        # Skip execution if build failed
        if [ "${current_build_status}" = "BUILD_FAILED" ]; then
            log_info "Run ${run_index}/${TOTAL_RUNS} - config ${config_name} #${run_number} skipped (build failed)"
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
                log_error "Run ${run_index}/${TOTAL_RUNS} missing metrics"
                continue
            fi

            if [ -z "${csv_row}" ]; then
                log_error "Run ${run_index}/${TOTAL_RUNS} empty metrics"
                continue
            fi

            # Extract heartbeat streams for analysis
            local hb_ts="${LOG_DIR}/${config_name}_run_${run_number}_hb_timeseries.csv"
            local hb_detail="${LOG_DIR}/${config_name}_run_${run_number}_hb_detail.csv"
            extract_heartbeat_timeseries "${run_log}" "${hb_ts}"
            extract_heartbeat_detail_csv "${run_log}" "${hb_detail}"

            # Append config flags and heartbeat data to CSV row
            local flags=$(config_to_loop_flags "${config_name}")
            local ts_now=$(timestamp)
            printf '%s,%s,%s,%s,%s,%s,%s\n' "${ts_now}" "${config_name}" "${run_number}" "OK" "OK" "${csv_row}" "${flags}" >> "${RESULTS_CSV}"

            successful_runs=$((successful_runs + 1))
            local elapsed=$(runtime_seconds ${start_time} $(date +%s))
            log_success "Run ${run_index}/${TOTAL_RUNS} completed (${elapsed}s)"
        else
            log_error "Configuration ${config_name} crashed during execution"
        fi

    done < "${matrix_file}"

    log_header "EXPERIMENT COMPLETE"
    log_success "Successful runs: ${successful_runs}/${TOTAL_RUNS}"
}

################################################################################
# Main
################################################################################

main() {
    local experiment_start=$(date +%s)
    local start_time=$(timestamp)

    # Calculate totals
    TOTAL_CONFIGS=${#TOP5_CONFIGS[@]}
    TOTAL_RUNS=$((TOTAL_CONFIGS * RUNS_PER_CONFIG))

    log_header "STARFORTH HEARTBEAT DoE - TOP 5 ELITE CONFIGURATIONS"
    log_info "Factors: 6 feedback loops (LOOP_1 through LOOP_6)"
    log_info "Configurations: 5 elite (selected from Stage 1 baseline)"
    log_info "Runs per config: ${RUNS_PER_CONFIG}"
    log_info "TOTAL RUNS: ${TOTAL_RUNS}"
    log_info "Heartbeat metrics: YES (per-tick jitter, convergence, coupling)"
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
    log_success "All runs completed!"
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
    log_info "  2. Run R analysis: Rscript analyze_heartbeat_stability.R"
    log_info "  3. Identify golden configuration by stability score"
    log_info "  4. Use golden config as baseline for MamaForth/PapaForth"

    return 0
}

main "$@"
exit $?
