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

# ============================================================================
# Window Scaling Experiment: Execute Using Pre-Built Binaries (REDESIGNED)
# ============================================================================
# Purpose: Run experiment using pre-built VM configurations from experiments/bin/
#          Much faster than rebuilding for each run!
#
# NEW: Three-mode heartbeat logging (off/summary/full)
# NEW: DoF determined at runtime by L8 Jacquard (not compile-time)
#
# Prerequisites: Run prebuild_all_configs.sh first
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXPERIMENT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$(cd "$EXPERIMENT_DIR/../.." && pwd)"
EXPERIMENTS_DIR="$PROJECT_DIR/experiments"
PREBUILT_DIR="$EXPERIMENTS_DIR/bin"
CONF_DIR="$PROJECT_DIR/conf"

RUN_MATRIX="$EXPERIMENT_DIR/run_matrix_shuffled.csv"
OUTPUT_DIR="$EXPERIMENT_DIR/results/raw"
HEARTBEAT_DIR="$OUTPUT_DIR/hb"  # NEW: Per-tick CSV files for FULL mode
RESULTS_FILE="$OUTPUT_DIR/window_sweep_results.csv"
TMP_VM_OUT="$OUTPUT_DIR/vm_output.tmp"
LOG_FILE="$OUTPUT_DIR/experiment.log"

WORKLOAD_FILE="init-l8-omni.4th"
WORKLOAD_PATH="$CONF_DIR/$WORKLOAD_FILE"

TIMEOUT_SECS=120

echo "============================================================================"
echo "Window Scaling Experiment: Pre-Built Execution (REDESIGNED)"
echo "============================================================================"
echo "Pre-built binaries: $PREBUILT_DIR"
echo "Run matrix:         $RUN_MATRIX"
echo "Output:             $RESULTS_FILE"
echo "Heartbeat logs:     $HEARTBEAT_DIR  (for FULL mode runs)"
echo "DoF:                Runtime (L8 Jacquard)"
echo "Window sizes:       12 (compile-time)"
echo "============================================================================"
echo ""

# ============================================================================
# Validation
# ============================================================================

if [ ! -d "$PREBUILT_DIR" ]; then
  echo "ERROR: Pre-built binary directory not found: $PREBUILT_DIR"
  echo "       Please run prebuild_all_configs.sh first"
  exit 1
fi

if [ ! -f "$RUN_MATRIX" ]; then
  echo "ERROR: Run matrix not found: $RUN_MATRIX"
  echo "       Please run generate_run_matrix.R first"
  exit 1
fi

if [ ! -f "$WORKLOAD_PATH" ]; then
  echo "ERROR: Workload file not found: $WORKLOAD_PATH"
  exit 1
fi

# Count available configs
AVAILABLE_CONFIGS=$(find "$PREBUILT_DIR" -name "starforth" -type f | wc -l)
echo "Available pre-built configs: $AVAILABLE_CONFIGS"

if [ "$AVAILABLE_CONFIGS" -ne 16 ]; then
  echo "⚠ WARNING: Expected 12 window builds, found $AVAILABLE_CONFIGS"
  echo ""
  echo "Missing window builds may cause experiment failures."
  read -p "Continue anyway? (y/N): " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
  fi
fi

TOTAL_RUNS=$(tail -n +2 "$RUN_MATRIX" | wc -l)

echo "Total runs to execute:       $TOTAL_RUNS"
echo "Heartbeat mode:              FULL (all runs)"
echo "Estimated time (no builds):  ~4-5 hours"
echo ""

mkdir -p "$OUTPUT_DIR"
mkdir -p "$HEARTBEAT_DIR"  # NEW: Create directory for full-mode heartbeat logs

# Create symlink to conf directory so VM can find init.4th when running from OUTPUT_DIR
if [ ! -e "$OUTPUT_DIR/conf" ]; then
  ln -sf "$CONF_DIR" "$OUTPUT_DIR/conf"
  echo "Created symlink: $OUTPUT_DIR/conf -> $CONF_DIR"
fi

# ============================================================================
# CSV Header (REDUCED: 24 columns based on 2^7 DoE top predictors)
# ============================================================================

# Core metadata (8): timestamp, run_id, window, replicate, log_mode, workload_duration, L8_mode
# James Law (3): lambda_effective, K_statistic, K_deviation
# Top DoE predictors (8): entropy, cv, temporal_decay, stability_score, ns_per_word, window_final, hot_words, avg_heat
# Summary buckets (5): bucket_mean_K, bucket_cv_K, bucket_window_var, bucket_heat_var, bucket_mode_transitions

echo "timestamp,run_id_shuffled,window_size,replicate,log_mode,workload_duration_ns,l8_mode_final,lambda_effective,K_statistic,K_deviation,entropy,cv,temporal_decay,stability_score,ns_per_word,total_runtime_ms,window_final,hot_word_count,avg_heat,bucket_mean_K,bucket_cv_K,bucket_window_var,bucket_heat_var,bucket_mode_transitions,bucket_collapse_flag" > "$RESULTS_FILE"

# ============================================================================
# Backup and Install Workload
# ============================================================================

ORIGINAL_INIT="$CONF_DIR/init.4th"
OUTPUT_INIT="$OUTPUT_DIR/conf/init.4th"
BACKUP_INIT="$OUTPUT_DIR/init_original.4th.backup"

if [ -f "$ORIGINAL_INIT" ]; then
  cp "$ORIGINAL_INIT" "$BACKUP_INIT"
  echo "Backed up original init.4th"
fi

echo "Installing omni workload as init.4th in experiment directory..."
cp "$WORKLOAD_PATH" "$OUTPUT_INIT"
echo ""

# ============================================================================
# Experiment Loop - Execute Pre-Built Binaries
# ============================================================================

RUN_NUM=0
SUCCESS=0
FAILED=0
MISSING=0
START_TIME=$(date +%s)

echo "============================================================================"
echo "Starting experimental runs (using pre-built binaries)..."
echo "============================================================================"
echo "" | tee -a "$LOG_FILE"

tail -n +2 "$RUN_MATRIX" | while IFS=',' read -r window_size replicate window_category log_mode run_id_sequential run_id_shuffled; do
  RUN_NUM=$((RUN_NUM + 1))
  RUN_TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

  # Progress tracking
  if [ $RUN_NUM -gt 1 ]; then
    ELAPSED=$(($(date +%s) - START_TIME))
    AVG_TIME=$((ELAPSED / (RUN_NUM - 1)))
    REMAINING=$(( (TOTAL_RUNS - RUN_NUM + 1) * AVG_TIME ))
    ETA_HOURS=$((REMAINING / 3600))
    ETA_MINS=$(( (REMAINING % 3600) / 60))
    ETA_STR="ETA: ${ETA_HOURS}h${ETA_MINS}m"
    RATE=$(awk "BEGIN {printf \"%.2f\", $RUN_NUM / ($ELAPSED / 60)}")
    RATE_STR="(${RATE} runs/min)"
  else
    ETA_STR=""
    RATE_STR=""
  fi

  echo "[$RUN_NUM/$TOTAL_RUNS] W=$window_size Rep=$replicate Mode=$log_mode | $ETA_STR $RATE_STR" | tee -a "$LOG_FILE"

  # Find pre-built binary
  CONFIG_NAME="w${window_size}"
  VM_BINARY="$PREBUILT_DIR/$CONFIG_NAME/starforth"

  if [ ! -f "$VM_BINARY" ]; then
    echo "  ✗ Pre-built binary not found: $VM_BINARY" | tee -a "$LOG_FILE"
    NAN_DATA="NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN"
    echo "$RUN_TIMESTAMP,$run_id_shuffled,$window_size,$replicate,$log_mode,$NAN_DATA" >> "$RESULTS_FILE"
    FAILED=$((FAILED + 1))
    MISSING=$((MISSING + 1))
    continue
  fi

  # NEW: Set environment variable for run ID (used by FULL mode for filename)
  export STARFORTH_RUN_NUMBER="$run_id_shuffled"

  # Execute with heartbeat logging mode from OUTPUT_DIR (VM writes hb files directly here)
  # CSV data goes to stderr (captured), test output goes to stdout (discarded)
  SAVED_PWD=$(pwd)
  cd "$OUTPUT_DIR"
  timeout "$TIMEOUT_SECS" "$VM_BINARY" --doe --heartbeat-log="$log_mode" 2> "$TMP_VM_OUT" 1>/dev/null
  VM_EXIT=$?
  cd "$SAVED_PWD"

  if [ $VM_EXIT -ne 0 ]; then
    echo "  ✗ VM run FAILED or TIMEOUT" | tee -a "$LOG_FILE"
    NAN_DATA="NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN"
    echo "$RUN_TIMESTAMP,$run_id_shuffled,$window_size,$replicate,$log_mode,$NAN_DATA" >> "$RESULTS_FILE"
    FAILED=$((FAILED + 1))
    continue
  fi

  # Parse VM output (reduced to 24 columns)
  VM_DATA=$(cat "$TMP_VM_OUT")

  if [ -z "$VM_DATA" ]; then
    echo "  ✗ VM output EMPTY" | tee -a "$LOG_FILE"
    NAN_DATA="NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN"
    echo "$RUN_TIMESTAMP,$run_id_shuffled,$window_size,$replicate,$log_mode,$NAN_DATA" >> "$RESULTS_FILE"
    FAILED=$((FAILED + 1))
    continue
  fi

  # Append to results_run_01_2025_12_08
  echo "$RUN_TIMESTAMP,$run_id_shuffled,$window_size,$replicate,$log_mode,$VM_DATA" >> "$RESULTS_FILE"

  SUCCESS=$((SUCCESS + 1))

  # Check for heartbeat file creation (file is written directly to HEARTBEAT_DIR now)
  if [ "$log_mode" = "full" ]; then
    HB_FILE="$HEARTBEAT_DIR/run-${run_id_shuffled}.csv"
    if [ -f "$HB_FILE" ]; then
      TICK_COUNT=$(tail -n +2 "$HB_FILE" | wc -l)
      echo "  ✓ Success (FULL: $TICK_COUNT ticks logged)" | tee -a "$LOG_FILE"
    else
      echo "  ⚠ Success but no heartbeat file created" | tee -a "$LOG_FILE"
    fi
  else
    echo "  ✓ Success" | tee -a "$LOG_FILE"
  fi

done

# ============================================================================
# Cleanup
# ============================================================================

if [ -f "$BACKUP_INIT" ]; then
  echo ""
  echo "Restoring original init.4th..."
  cp "$BACKUP_INIT" "$ORIGINAL_INIT"
fi

# ============================================================================
# Summary
# ============================================================================

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
HOURS=$((ELAPSED / 3600))
MINS=$(( (ELAPSED % 3600) / 60 ))
SECS=$((ELAPSED % 60))

# Count heartbeat files created
HB_FILES_CREATED=$(find "$HEARTBEAT_DIR" -name "run-*.csv" -type f 2>/dev/null | wc -l)

echo ""
echo "============================================================================"
echo "Experiment Complete"
echo "============================================================================"
echo "Total runs:            $TOTAL_RUNS"
echo "Successful:            $SUCCESS"
echo "Failed:                $FAILED"
echo "  Missing binary:      $MISSING"
echo "  Runtime error:       $((FAILED - MISSING))"
echo "Elapsed time:          ${HOURS}h${MINS}m${SECS}s"
echo "Results:               $RESULTS_FILE"
echo "Heartbeat files:       $HB_FILES_CREATED (in $HEARTBEAT_DIR)"
echo "============================================================================"
echo ""

if [ $FAILED -gt 0 ]; then
  echo "WARNING: $FAILED runs failed. Check $LOG_FILE for details."
  exit 1
fi

echo "✓ All runs completed successfully!"
echo ""
echo "Next step: Analyze results"
echo "  cd $SCRIPT_DIR"
echo "  ./analyze_results.R"
echo ""

exit 0