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
#  StarForth Full 2^8 Factorial Design of Experiments
#
#  This script runs the complete 2^8 = 256 configuration factorial design to
#  measure all interaction effects between all optimization factors. Every possible
#  combination of the 8 factors is tested.
#
#  Factors (binary: 0 or 1):
#    Factor 0: ENABLE_HOTWORDS_CACHE               (physics-driven hot-words cache)
#    Factor 1: ENABLE_PIPELINING                   (word transition prediction)
#    Factor 2: ENABLE_LOOP_1_HEAT_TRACKING         (execution heat counting)
#    Factor 3: ENABLE_LOOP_2_ROLLING_WINDOW        (rolling window history)
#    Factor 4: ENABLE_LOOP_3_LINEAR_DECAY          (exponential heat decay)
#    Factor 5: ENABLE_LOOP_4_PIPELINING_METRICS    (word transition tracking)
#    Factor 6: ENABLE_LOOP_5_WINDOW_INFERENCE      (window width inference)
#    Factor 7: ENABLE_LOOP_6_DECAY_INFERENCE       (decay slope inference)
#
#  Configuration format: HW_PL_L1_L2_L3_L4_L5_L6 (8 binary digits, 256 total)
#  Examples:
#    00000000 = baseline (all off)
#    11111111 = all on (current optimal)
#    10101010 = alternating pattern
#
#  Each configuration:
#    - Runs with specified factor/loop settings
#    - Collects N samples for statistical validity
#    - Outputs complete metrics
#    - Results enable interaction analysis + main effects
#
#  Usage:
#    ./scripts/run_ablation_study.sh                           # Run all 256 configs × default samples
#    ./scripts/run_ablation_study.sh --iterations 30           # Run all with 30 samples per config (7680 total)
#    ./scripts/run_ablation_study.sh --iterations 30 11111111  # Run specific config only
#
################################################################################

set -e

################################################################################
# Configuration and defaults
################################################################################

EXPERIMENTS_BASE="/home/rajames/CLionProjects/StarForth-DoE/experiments"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
BUILD_PROFILE="fastest"
SAMPLES_PER_CONFIG=30  # Statistical sample size

# Default: run all experiments
EXPERIMENT_FILTER=""
SKIP_BUILD=0

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
        --iterations)
            SAMPLES_PER_CONFIG="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        [01][01][01][01][01][01][01][01])
            # Accept 8-bit configuration (e.g., 00000000, 11111111)
            EXPERIMENT_FILTER="$1"
            shift
            ;;
        *)
            log_error "Unknown option: $1. Use format: [01][01][01][01][01][01][01][01]"
            ;;
    esac
done

################################################################################
# Generate full 2^8 factorial (256 configurations)
################################################################################

# Factors mapping (8 bits):
#  Bit 0: ENABLE_HOTWORDS_CACHE
#  Bit 1: ENABLE_PIPELINING
#  Bit 2: ENABLE_LOOP_1_HEAT_TRACKING
#  Bit 3: ENABLE_LOOP_2_ROLLING_WINDOW
#  Bit 4: ENABLE_LOOP_3_LINEAR_DECAY
#  Bit 5: ENABLE_LOOP_4_PIPELINING_METRICS
#  Bit 6: ENABLE_LOOP_5_WINDOW_INFERENCE
#  Bit 7: ENABLE_LOOP_6_DECAY_INFERENCE

declare -A EXPERIMENTS
declare -a EXP_ORDER

# Generate all 256 configurations (00000000 to 11111111)
for config_num in {0..255}; do
    bit0=$(($config_num & 1))
    bit1=$((($config_num >> 1) & 1))
    bit2=$((($config_num >> 2) & 1))
    bit3=$((($config_num >> 3) & 1))
    bit4=$((($config_num >> 4) & 1))
    bit5=$((($config_num >> 5) & 1))
    bit6=$((($config_num >> 6) & 1))
    bit7=$((($config_num >> 7) & 1))

    config_str="${bit0}${bit1}${bit2}${bit3}${bit4}${bit5}${bit6}${bit7}"

    EXPERIMENTS["$config_str"]="HW=$bit0 PL=$bit1 L1=$bit2 L2=$bit3 L3=$bit4 L4=$bit5 L5=$bit6 L6=$bit7|ENABLE_HOTWORDS_CACHE=$bit0 ENABLE_PIPELINING=$bit1 ENABLE_LOOP_1_HEAT_TRACKING=$bit2 ENABLE_LOOP_2_ROLLING_WINDOW=$bit3 ENABLE_LOOP_3_LINEAR_DECAY=$bit4 ENABLE_LOOP_4_PIPELINING_METRICS=$bit5 ENABLE_LOOP_5_WINDOW_INFERENCE=$bit6 ENABLE_LOOP_6_DECAY_INFERENCE=$bit7"

    EXP_ORDER+=("$config_str")
done

################################################################################
# Main execution
################################################################################

log_header "STARFORTH FULL 2^8 FACTORIAL DESIGN OF EXPERIMENTS"

log_info "Purpose: Measure all interaction effects between 8 optimization factors"
log_info "Configurations: ${#EXP_ORDER[@]} (2^8 = 256 total)"
log_info "Samples per config: ${SAMPLES_PER_CONFIG}"
log_info "Total runs: $((${#EXP_ORDER[@]} * SAMPLES_PER_CONFIG))"
log_info "Output directory: ${EXPERIMENTS_BASE}"
log_info "Factors: HOTWORDS(HW) + PIPELINING(PL) + 6 LOOPS (L1-L6)"
log_info "Run order: RANDOMIZED (to prevent thermal/temporal confounds)"
log_info ""

################################################################################
# Build phase
################################################################################

if [ $SKIP_BUILD -eq 0 ]; then
    log_section "Building StarForth (if needed)"
    if [ ! -f "${BUILD_DIR}/amd64/${BUILD_PROFILE}/starforth" ]; then
        log_info "Binary not found, rebuilding..."
        cd "${REPO_ROOT}"
        make "${BUILD_PROFILE}" > /dev/null 2>&1
        log_success "Build complete"
    else
        log_success "Binary already built and up-to-date"
    fi
else
    log_info "Skipping build (--skip-build specified)"
fi

################################################################################
# Generate randomized test matrix
################################################################################

log_section "Generating randomized test matrix (${#EXP_ORDER[@]} configs × ${SAMPLES_PER_CONFIG} reps)"

declare -a TEST_MATRIX
declare -a CURRENT_SAMPLE_COUNT

# Initialize sample counters for each config
for exp_name in "${EXP_ORDER[@]}"; do
    CURRENT_SAMPLE_COUNT["$exp_name"]=0
done

# Build full test matrix: all config-rep pairs
TOTAL_RUNS=$((${#EXP_ORDER[@]} * SAMPLES_PER_CONFIG))
for config_idx in "${!EXP_ORDER[@]}"; do
    exp_name="${EXP_ORDER[$config_idx]}"
    for sample_num in $(seq 1 ${SAMPLES_PER_CONFIG}); do
        TEST_MATRIX+=("${exp_name}:${sample_num}")
    done
done

# Randomize: Fisher-Yates shuffle in bash
for ((i=${#TEST_MATRIX[@]}-1; i>0; i--)); do
    j=$((RANDOM % (i+1)))
    temp="${TEST_MATRIX[$i]}"
    TEST_MATRIX[$i]="${TEST_MATRIX[$j]}"
    TEST_MATRIX[$j]="$temp"
done

log_success "Randomized ${TOTAL_RUNS} total runs"
log_info "Test matrix written to: ${EXPERIMENTS_BASE}/test_matrix.txt"
(IFS=$'\n'; echo "${TEST_MATRIX[*]}") > "${EXPERIMENTS_BASE}/test_matrix.txt"
log_info ""

################################################################################
# Run randomized test matrix
################################################################################

log_section "Executing randomized test matrix"

RUN_NUMBER=0
CURRENT_CONFIG=""
declare -a CONFIG_RESULTS_FILES

for test_case in "${TEST_MATRIX[@]}"; do
    IFS=':' read -r exp_name sample_num <<< "$test_case"
    RUN_NUMBER=$((RUN_NUMBER + 1))

    # Only rebuild when config changes
    if [ "${exp_name}" != "${CURRENT_CONFIG}" ]; then
        CURRENT_CONFIG="${exp_name}"

        IFS='|' read -r exp_desc exp_config <<< "${EXPERIMENTS[$exp_name]}"

        log_header "CONFIG: ${exp_name} | RUN ${RUN_NUMBER}/${TOTAL_RUNS}"
        log_info "Description: ${exp_desc}"
        log_info "Configuration: ${exp_config}"
        log_info ""

        # Create experiment directory if needed
        EXP_DIR="${EXPERIMENTS_BASE}/${exp_name}"
        mkdir -p "${EXP_DIR}"

        # Initialize results_run_01_2025_12_08 files for this config
        RESULTS_FILE="${EXP_DIR}/results.txt"
        TIMING_FILE="${EXP_DIR}/timing.txt"
        > "${RESULTS_FILE}"
        > "${TIMING_FILE}"
        CONFIG_RESULTS_FILES["${exp_name}"]="${RESULTS_FILE}"
        CONFIG_BUILD_STATUS["${exp_name}"]="UNKNOWN"

        # Build for this configuration
        log_section "Building StarForth with loop configuration"
        log_info "Rebuilding with: ${exp_config}"

        cd "${REPO_ROOT}"
        BUILD_OUTPUT=$(mktemp)
        make clean > /dev/null 2>&1
        if make ${BUILD_PROFILE} ${exp_config} > "${BUILD_OUTPUT}" 2>&1; then
            log_success "Build complete for ${exp_name}"
            CONFIG_BUILD_STATUS["${exp_name}"]="SUCCESS"
        else
            BUILD_ERROR=$(tail -20 "${BUILD_OUTPUT}" | tr '\n' ' ')
            log_section "Build FAILED for ${exp_name}"
            log_info "Error: ${BUILD_ERROR}"
            CONFIG_BUILD_STATUS["${exp_name}"]="FAILED"
            # Log build failure to timing file for ALL reps of this config
            for i in $(seq 1 ${SAMPLES_PER_CONFIG}); do
                echo "sample_${i}: BUILD_FAILED" >> "${TIMING_FILE}"
            done
            echo "build_error: ${BUILD_ERROR}" >> "${TIMING_FILE}"
            rm -f "${BUILD_OUTPUT}"
            continue  # Skip execution for this config, move to next test case
        fi
        rm -f "${BUILD_OUTPUT}"
    fi

    # Check if build succeeded before attempting execution
    EXP_DIR="${EXPERIMENTS_BASE}/${exp_name}"
    RESULTS_FILE="${CONFIG_RESULTS_FILES[$exp_name]}"
    TIMING_FILE="${EXP_DIR}/timing.txt"

    if [ "${CONFIG_BUILD_STATUS[$exp_name]}" != "SUCCESS" ]; then
        log_info "Skipping run ${sample_num}/${SAMPLES_PER_CONFIG} for ${exp_name} (overall ${RUN_NUMBER}/${TOTAL_RUNS}) - build failed"
        echo "sample_${sample_num}: BUILD_FAILED" >> "${TIMING_FILE}"
        continue
    fi

    log_info "Run ${sample_num}/${SAMPLES_PER_CONFIG} for ${exp_name} (overall ${RUN_NUMBER}/${TOTAL_RUNS})..."

    # Run the binary and capture execution time + exit status
    START_NS=$(date +%s%N)
    RUN_OUTPUT=$(mktemp)
    if "${BUILD_DIR}/amd64/${BUILD_PROFILE}/starforth" --doe-experiment > "${RUN_OUTPUT}" 2>&1; then
        cat "${RUN_OUTPUT}" >> "${RESULTS_FILE}"
        RUN_STATUS="SUCCESS"
        END_NS=$(date +%s%N)
        DURATION_NS=$((END_NS - START_NS))
    else
        RUN_ERROR=$(tail -20 "${RUN_OUTPUT}" | tr '\n' ' ')
        cat "${RUN_OUTPUT}" >> "${RESULTS_FILE}"
        RUN_STATUS="RUNTIME_ERROR"
        END_NS=$(date +%s%N)
        DURATION_NS=$((END_NS - START_NS))
        log_info "⚠ Runtime error on sample ${sample_num}: ${RUN_ERROR}"
    fi
    rm -f "${RUN_OUTPUT}"

    # Log to summary with status
    echo "sample_${sample_num}: ${DURATION_NS} ns (${RUN_STATUS})" >> "${TIMING_FILE}"
done

log_success "Completed all ${TOTAL_RUNS} randomized runs"
log_info ""

################################################################################
# Summary
################################################################################

log_header "ABLATION STUDY COMPLETE"

log_info "Experiments run:"
for exp_name in "${EXP_ORDER[@]}"; do
    if [ -n "${EXPERIMENT_FILTER}" ] && [ "${exp_name}" != "${EXPERIMENT_FILTER}" ]; then
        continue
    fi
    EXP_DIR="${EXPERIMENTS_BASE}/${exp_name}"
    if [ -d "${EXP_DIR}" ]; then
        SAMPLE_COUNT=$(ls -1 "${EXP_DIR}"/timing.txt 2>/dev/null | wc -l)
        log_success "  ${exp_name}: ${EXP_DIR}"
    fi
done

log_info ""
log_info "Next steps:"
log_info "  1. Review results: ls -la ${EXPERIMENTS_BASE}/"
log_info "  2. Analyze timing data to measure genetic imprint of each loop"
log_info "  3. Compare EXP_00 vs EXP_06 to quantify total improvement"
log_info "  4. Measure incremental gains: EXP_N vs EXP_N-1"
log_info ""
log_info "Key metrics to extract:"
log_info "  - Mean execution time per experiment"
log_info "  - Standard deviation (consistency)"
log_info "  - Additive performance delta per loop"
log_info "  - Confidence intervals for statistical validity"

exit 0