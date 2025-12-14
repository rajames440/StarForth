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

# Top 5% Optimal Configurations Long-Term Runoff
# Head-to-head comparison of the 7 best configs from DOE analysis
#
# Usage: ./run_top5_runoff.sh [REPS]
#   REPS = number of replicates per config (default: 100)
#
# Top 5% configs (from DOE optimality analysis - Speed + Stability):
#   0010111 - Optimal #1 (best combined score)
#   0100100 - Optimal #2
#   0010010 - Optimal #3
#   0110111 - Optimal #4
#   0100101 - Optimal #5 (fastest raw speed)
#   1000101 - Optimal #6
#   0000011 - Optimal #7
#   0000000 - Baseline (for comparison)

set -e

REPS=${1:-100}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$SCRIPT_DIR/runoff_${TIMESTAMP}"
RESULTS_FILE="$OUTPUT_DIR/runoff_results.csv"
RUN_MATRIX_FILE="$OUTPUT_DIR/run_matrix_${TIMESTAMP}.csv"
RUN_ORDER_FILE="$OUTPUT_DIR/run_order_${TIMESTAMP}.csv"

mkdir -p "$OUTPUT_DIR"

# Top 5% configs + baseline (L1 L2 L3 L4 L5 L6 L7)
declare -a CONFIGS=(
    "0 0 1 0 1 1 1"  # 0010111 - Optimal #1
    "0 1 0 0 1 0 0"  # 0100100 - Optimal #2
    "0 0 1 0 0 1 0"  # 0010010 - Optimal #3
    "0 1 1 0 1 1 1"  # 0110111 - Optimal #4
    "0 1 0 0 1 0 1"  # 0100101 - Optimal #5 (fastest)
    "1 0 0 0 1 0 1"  # 1000101 - Optimal #6
    "0 0 0 0 0 1 1"  # 0000011 - Optimal #7
    "0 0 0 0 0 0 0"  # 0000000 - Baseline
)

declare -a CONFIG_NAMES=(
    "0010111"
    "0100100"
    "0010010"
    "0110111"
    "0100101"
    "1000101"
    "0000011"
    "0000000"
)

NUM_CONFIGS=${#CONFIGS[@]}
TOTAL_RUNS=$((NUM_CONFIGS * REPS))

echo "======================================================"
echo "Top 5% Optimal Configurations - Long-Term Runoff"
echo "======================================================"
echo "Configurations: $NUM_CONFIGS (7 optimal + baseline)"
echo "Replicates:     $REPS per config"
echo "Total runs:     $TOTAL_RUNS"
echo "Output:         $OUTPUT_DIR"
echo "Run matrix:     $RUN_MATRIX_FILE"
echo "Run order:      $RUN_ORDER_FILE"
echo "======================================================"
echo ""
echo "Configs being tested:"
for i in "${!CONFIG_NAMES[@]}"; do
    echo "  ${CONFIG_NAMES[$i]}"
done
echo "======================================================"

cd "$PROJECT_DIR"

# Write CSV header
echo "timestamp,run_id,config_bits,L1_heat_tracking,L2_rolling_window,L3_linear_decay,L4_pipelining_metrics,L5_window_inference,L6_decay_inference,L7_adaptive_heartrate,replicate,L1_heat,L2_window,L3_decay,L4_pipeline,L5_win_inf,L6_decay_inf,L7_heartrate,total_lookups,cache_hits,cache_hit_pct,bucket_hits,bucket_hit_pct,cache_lat_ns,cache_lat_std,bucket_lat_ns,bucket_lat_std,ctx_pred_total,ctx_correct,ctx_acc_pct,cache_promos,cache_demos,win_diversity_pct,win_final_bytes,win_width,win_total_exec,win_var_q48,decay_slope,total_heat,hot_words,stale_words,stale_ratio,avg_heat,tick_count,tick_target_ns,infer_runs,early_exits,prefetch_acc_pct,prefetch_attempts,prefetch_hits,win_tune_checks,final_win_size,workload_ns_q48,runtime_ms,words_exec,dict_lookups,mem_bytes,speedup,ci_lower,ci_upper,cpu_temp_delta,cpu_freq_delta,decay_rate_q16,decay_min_ns,rolling_win_size,shrink_rate,demotion_thresh,hotwords_enabled,pipelining_enabled" > "$RESULTS_FILE"

# NaN row template
NAN_DATA="NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN"

# Generate run matrix and randomized order (mirrors run_doe.sh behavior)
echo "Generating run matrix..."
echo "run_id,config_idx,config_bits,replicate" > "$RUN_MATRIX_FILE"
RUN_ID=0
for cfg_idx in "${!CONFIGS[@]}"; do
    CONFIG_NAME="${CONFIG_NAMES[$cfg_idx]}"
    for rep in $(seq 1 $REPS); do
        RUN_ID=$((RUN_ID + 1))
        echo "$RUN_ID,$cfg_idx,$CONFIG_NAME,$rep" >> "$RUN_MATRIX_FILE"
    done
done

echo "Shuffling run order..."
tail -n +2 "$RUN_MATRIX_FILE" | shuf > "$RUN_ORDER_FILE"

PROGRESS_NUM=0
FAILED=0
SUCCESS=0
START_TIME=$(date +%s)

# Track per-config stats
declare -A CONFIG_SUCCESS
declare -A CONFIG_FAILED
for name in "${CONFIG_NAMES[@]}"; do
    CONFIG_SUCCESS[$name]=0
    CONFIG_FAILED[$name]=0
done

while IFS=',' read -r run_id cfg_idx config_bits rep; do
    PROGRESS_NUM=$((PROGRESS_NUM + 1))
    RUN_TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

    CONFIG="${CONFIGS[$cfg_idx]}"
    CONFIG_NAME="${CONFIG_NAMES[$cfg_idx]}"
    read -r L1 L2 L3 L4 L5 L6 L7 <<< "$CONFIG"

    # Progress estimate
    if [ $PROGRESS_NUM -gt 1 ]; then
        ELAPSED=$(($(date +%s) - START_TIME))
        AVG_TIME=$((ELAPSED / (PROGRESS_NUM - 1)))
        REMAINING=$(( (TOTAL_RUNS - PROGRESS_NUM + 1) * AVG_TIME ))
        ETA_MIN=$((REMAINING / 60))
        ETA_SEC=$((REMAINING % 60))
        ETA_STR="ETA: ${ETA_MIN}m${ETA_SEC}s"
    else
        ETA_STR=""
    fi

    echo ""
    echo "[$PROGRESS_NUM/$TOTAL_RUNS] Run $run_id | Config $CONFIG_NAME | Rep $rep  $ETA_STR"

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
        echo "$RUN_TIMESTAMP,$run_id,$CONFIG_NAME,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$NAN_DATA" >> "$RESULTS_FILE"
        FAILED=$((FAILED + 1))
        CONFIG_FAILED[$CONFIG_NAME]=$((${CONFIG_FAILED[$CONFIG_NAME]} + 1))
        continue
    fi

    # Run DoE benchmark and capture output
    if OUTPUT=$(timeout 120 ./build/amd64/fastest/starforth --doe 2>&1); then
        CSV_DATA=$(echo "$OUTPUT" | tail -1)
        echo "$RUN_TIMESTAMP,$run_id,$CONFIG_NAME,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$CSV_DATA" >> "$RESULTS_FILE"
        echo "  OK"
        SUCCESS=$((SUCCESS + 1))
        CONFIG_SUCCESS[$CONFIG_NAME]=$((${CONFIG_SUCCESS[$CONFIG_NAME]} + 1))
    else
        EXIT_CODE=$?
        echo "  FAILED (exit $EXIT_CODE)"
        echo "$RUN_TIMESTAMP,$run_id,$CONFIG_NAME,$L1,$L2,$L3,$L4,$L5,$L6,$L7,$rep,$NAN_DATA" >> "$RESULTS_FILE"
        FAILED=$((FAILED + 1))
        CONFIG_FAILED[$CONFIG_NAME]=$((${CONFIG_FAILED[$CONFIG_NAME]} + 1))
    fi
done < "$RUN_ORDER_FILE"

TOTAL_TIME=$(($(date +%s) - START_TIME))

echo ""
echo "======================================================"
echo "Runoff Complete!"
echo "======================================================"
echo "Total time:    $((TOTAL_TIME / 60))m $((TOTAL_TIME % 60))s"
echo "Success:       $SUCCESS"
echo "Failed:        $FAILED"
echo ""
echo "Per-config results:"
for name in "${CONFIG_NAMES[@]}"; do
    echo "  $name: ${CONFIG_SUCCESS[$name]} OK, ${CONFIG_FAILED[$name]} FAILED"
done
echo ""
echo "Results:       $RESULTS_FILE"
echo "======================================================"

# Generate quick summary
echo ""
echo "Generating summary statistics..."

cat > "$OUTPUT_DIR/runoff_analysis.R" << 'RSCRIPT'
#!/usr/bin/env Rscript
suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(htmltools)
})

args <- commandArgs(trailingOnly = TRUE)
results_file <- if (length(args) > 0) args[1] else "runoff_results.csv"

results <- read.csv(results_file, stringsAsFactors = FALSE)

# Derive ns_per_word if needed
if (!"ns_per_word" %in% names(results)) {
  results <- results %>% mutate(ns_per_word = workload_ns_q48 / words_exec)
}

# Summary by config
summary_stats <- results %>%
  group_by(config_bits) %>%
  summarise(
    n = n(),
    mean_ns = mean(ns_per_word, na.rm = TRUE),
    sd_ns = sd(ns_per_word, na.rm = TRUE),
    se_ns = sd_ns / sqrt(n),
    cv_pct = 100 * sd_ns / mean_ns,
    min_ns = min(ns_per_word, na.rm = TRUE),
    max_ns = max(ns_per_word, na.rm = TRUE),
    .groups = "drop"
  ) %>%
  arrange(mean_ns)

# Normalize and compute optimality
summary_stats <- summary_stats %>%
  mutate(
    speed_norm = (mean_ns - min(mean_ns)) / (max(mean_ns) - min(mean_ns)),
    stability_norm = (cv_pct - min(cv_pct)) / (max(cv_pct) - min(cv_pct)),
    optimality_score = sqrt(speed_norm^2 + stability_norm^2)
  ) %>%
  arrange(optimality_score) %>%
  mutate(rank = row_number())

cat("\n=== RUNOFF RESULTS ===\n")
print(summary_stats %>% select(rank, config_bits, n, mean_ns, cv_pct, optimality_score))

write.csv(summary_stats, "runoff_summary.csv", row.names = FALSE)

# Pairwise t-tests against winner
winner <- summary_stats$config_bits[1]
winner_data <- results %>% filter(config_bits == winner)

cat("\n=== PAIRWISE T-TESTS vs WINNER (", winner, ") ===\n")
pairwise_results <- data.frame()
for (cfg in summary_stats$config_bits[-1]) {
  cfg_data <- results %>% filter(config_bits == cfg)
  t_result <- t.test(winner_data$ns_per_word, cfg_data$ns_per_word)
  pairwise_results <- bind_rows(pairwise_results, data.frame(
    config = cfg,
    winner_mean = mean(winner_data$ns_per_word, na.rm = TRUE),
    challenger_mean = mean(cfg_data$ns_per_word, na.rm = TRUE),
    diff_pct = (mean(cfg_data$ns_per_word, na.rm = TRUE) - mean(winner_data$ns_per_word, na.rm = TRUE)) /
               mean(winner_data$ns_per_word, na.rm = TRUE) * 100,
    t_stat = t_result$statistic,
    p_value = t_result$p.value,
    significant = t_result$p.value < 0.05
  ))
}
print(pairwise_results)
write.csv(pairwise_results, "runoff_pairwise_tests.csv", row.names = FALSE)

# Box plot
png("runoff_boxplot.png", width = 1200, height = 800)
ggplot(results, aes(x = reorder(config_bits, ns_per_word), y = ns_per_word, fill = config_bits)) +
  geom_boxplot(alpha = 0.7, outlier.alpha = 0.3) +
  geom_jitter(width = 0.2, alpha = 0.1, size = 0.5) +
  theme_bw(base_size = 14) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "Top 5% Runoff: ns_per_word Distribution",
       subtitle = paste("N =", nrow(results), "| Configs:", length(unique(results$config_bits))),
       x = "Configuration", y = "ns_per_word") +
  guides(fill = "none")
dev.off()
cat("  Saved: runoff_boxplot.png\n")

# Violin plot with means
png("runoff_violin.png", width = 1200, height = 800)
ggplot(results, aes(x = reorder(config_bits, ns_per_word), y = ns_per_word, fill = config_bits)) +
  geom_violin(alpha = 0.6, trim = FALSE) +
  geom_boxplot(width = 0.15, alpha = 0.8, outlier.shape = NA) +
  stat_summary(fun = mean, geom = "point", shape = 23, size = 3, fill = "white") +
  theme_bw(base_size = 14) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "Top 5% Runoff: Distribution Shape",
       subtitle = "Diamond = mean | Box = IQR",
       x = "Configuration", y = "ns_per_word") +
  guides(fill = "none")
dev.off()
cat("  Saved: runoff_violin.png\n")

# Time series stability plot
png("runoff_timeseries.png", width = 1400, height = 900)
ggplot(results, aes(x = run_id, y = ns_per_word, color = config_bits)) +
  geom_point(alpha = 0.5, size = 1) +
  geom_smooth(se = FALSE, method = "loess", span = 0.3) +
  theme_bw(base_size = 12) +
  labs(title = "Runoff Time Series: Stability Over Time",
       subtitle = "Each point = one run | Lines = LOESS smoothed trend",
       x = "Run ID (randomized order)", y = "ns_per_word",
       color = "Config")
dev.off()
cat("  Saved: runoff_timeseries.png\n")

# Optimality comparison
png("runoff_pareto.png", width = 1000, height = 800)
ggplot(summary_stats, aes(x = mean_ns, y = cv_pct, label = config_bits)) +
  geom_point(aes(color = optimality_score, size = 4)) +
  geom_text(hjust = -0.1, vjust = 0, size = 4) +
  scale_color_gradient(low = "darkgreen", high = "red", name = "Optimality\n(lower=better)") +
  theme_bw(base_size = 14) +
  labs(title = "Runoff Pareto: Speed vs Stability",
       subtitle = "Lower-left = optimal",
       x = "Mean ns_per_word", y = "CV%") +
  guides(size = "none")
dev.off()
cat("  Saved: runoff_pareto.png\n")

cat("\n=== WINNER ===\n")
cat("Configuration:", winner, "\n")
cat("Mean ns_per_word:", format(summary_stats$mean_ns[1], scientific = TRUE), "\n")
cat("CV%:", round(summary_stats$cv_pct[1], 2), "%\n")
cat("Optimality score:", round(summary_stats$optimality_score[1], 4), "\n")

cat("\nDONE.\n")
RSCRIPT

chmod +x "$OUTPUT_DIR/runoff_analysis.R"

echo ""
echo "To analyze results, run:"
echo "  cd $OUTPUT_DIR && Rscript runoff_analysis.R runoff_results.csv"
echo ""
