#!/usr/bin/env bash
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

set -euo pipefail

echo "══════════════════════════════════════════════════════════════"
echo "   StarForth L8 Attractor Experiment — setup.sh"
echo "══════════════════════════════════════════════════════════════"
echo

# -----------------------------------------------------------------------------
# Resolve paths
# -----------------------------------------------------------------------------
EXPERIMENT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPTS_DIR="$EXPERIMENT_ROOT/scripts"
LATEST_FILE="$EXPERIMENT_ROOT/.latest_results_dir"

TS="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="$EXPERIMENT_ROOT/results_${TS}"
HB_DIR="$RESULTS_DIR/l8_attractor_hb"

echo "Creating results directory:"
echo "  $RESULTS_DIR"
mkdir -p "$HB_DIR"

# Save for run.sh and dash.R
echo "$RESULTS_DIR" > "$LATEST_FILE"

# -----------------------------------------------------------------------------
# Generate run matrix via R (NO ARGS)
# -----------------------------------------------------------------------------
echo "Generating run matrix in:"
echo "  $RESULTS_DIR/l8_attractor_run_matrix.csv"

Rscript "$SCRIPTS_DIR/generate_l8_attractor_matrix.R"

# -----------------------------------------------------------------------------
# Create CSV headers for data collection
# -----------------------------------------------------------------------------
echo "Creating CSV headers..."

# Heartbeat CSV header (matches heartbeat_emit_tick_row format from heartbeat_export.c)
HB_CSV="$RESULTS_DIR/heartbeat_all.csv"
echo "tick_number,elapsed_ns,tick_interval_ns,cache_hits_delta,bucket_hits_delta,word_executions_delta,hot_word_count,avg_word_heat,window_width,predicted_label_hits,estimated_jitter_ns" > "$HB_CSV"

echo "  → $HB_CSV"

echo
echo "Setup complete."
echo "Run the experiment with:"
echo "  ./scripts/run.sh"
echo
