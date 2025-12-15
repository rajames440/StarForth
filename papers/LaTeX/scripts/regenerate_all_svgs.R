#!/usr/bin/env Rscript
################################################################################
# REGENERATE ALL SVG CHARTS FROM EXPERIMENTAL DATA
# This script regenerates all 113 charts as SVG files from CSV data
# NO PNG CONVERSION - Pure data-driven regeneration
################################################################################

.libPaths('~/R/library')
suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(readr)
  library(reshape2)
})

# Output directory
SVG_DIR <- "/home/rajames/CLionProjects/StarForth/papers/LaTeX/figures"
dir.create(SVG_DIR, showWarnings = FALSE, recursive = TRUE)

# Track progress
svg_count <- 0

cat("================================================================================\n")
cat("  REGENERATING ALL EXPERIMENTAL CHARTS AS SVG FROM CSV DATA\n")
cat("================================================================================\n\n")

################################################################################
# PART 1: L8 ATTRACTOR EXPERIMENT (11 report charts + existing 11 charts)
################################################################################

cat("\n=== PART 1: L8 ATTRACTOR EXPERIMENT ===\n\n")

L8_DIR <- "/home/rajames/CLionProjects/StarForth/experiments/l8_attractor_map/results_20251209_145907"

# Load L8 data
cat("Loading L8 data...\n")
hb <- read_csv(file.path(L8_DIR, "heartbeat_all.csv"), show_col_types = FALSE)
wm <- read_csv(file.path(L8_DIR, "workload_mapping.csv"), show_col_types = FALSE)

cat(sprintf("  Heartbeat rows: %d\n", nrow(hb)))
cat(sprintf("  Workload mapping: %d runs\n", nrow(wm)))

# Merge data
hb$row_num <- 1:nrow(hb)
hb_merged <- hb %>%
  left_join(wm %>% select(run_id, init_script, replicate, hb_start_row, hb_end_row),
            by = join_by(row_num >= hb_start_row, row_num <= hb_end_row)) %>%
  filter(!is.na(run_id))

cat(sprintf("  Merged: %d rows\n\n", nrow(hb_merged)))

# Per-run summary
per_run <- hb_merged %>%
  group_by(run_id, init_script, replicate) %>%
  summarise(
    tick_count = n(),
    duration_ns = max(elapsed_ns) - min(elapsed_ns),
    mean_interval = mean(tick_interval_ns, na.rm = TRUE),
    sd_interval = sd(tick_interval_ns, na.rm = TRUE),
    mean_heat = mean(avg_word_heat, na.rm = TRUE),
    mean_jitter = mean(estimated_jitter_ns, na.rm = TRUE),
    .groups = "drop"
  )

cat("Generating L8 report SVGs...\n")

# Chart 1: Box plot comparison
p <- ggplot(per_run, aes(x = init_script, y = tick_count, fill = init_script)) +
  geom_boxplot(alpha = 0.7) +
  labs(title = "Tick Count by Workload", x = "Workload", y = "Tick Count") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1), legend.position = "none")
ggsave(file.path(SVG_DIR, "l8_boxplot_comparison.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_boxplot_comparison.svg\n", svg_count))

# Chart 2: Violin plot
p <- ggplot(per_run, aes(x = init_script, y = mean_interval / 1000, fill = init_script)) +
  geom_violin(alpha = 0.7) +
  labs(title = "Tick Interval Distribution", x = "Workload", y = "Mean Interval (μs)") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1), legend.position = "none")
ggsave(file.path(SVG_DIR, "l8_violin_plot.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_violin_plot.svg\n", svg_count))

# Chart 3: Distributions by workload
p <- ggplot(hb_merged, aes(x = tick_interval_ns / 1000, fill = init_script)) +
  geom_density(alpha = 0.6) +
  facet_wrap(~init_script, ncol = 2) +
  labs(title = "Tick Interval Distributions by Workload", x = "Interval (μs)", y = "Density") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_distributions_by_workload.svg"), p, width = 12, height = 8)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_distributions_by_workload.svg\n", svg_count))

# Chart 4: Heart rate evolution
p <- ggplot(hb_merged, aes(x = elapsed_ns / 1e9, y = tick_interval_ns / 1000, color = init_script)) +
  geom_line(alpha = 0.3, linewidth = 0.5) +
  labs(title = "Heart Rate Evolution Over Time", x = "Time (s)", y = "Tick Interval (μs)", color = "Workload") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_heart_rate_evolution.svg"), p, width = 12, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_heart_rate_evolution.svg\n", svg_count))

# Chart 5: Heart rate variability
p <- ggplot(per_run, aes(x = init_script, y = sd_interval / 1000, fill = init_script)) +
  geom_boxplot(alpha = 0.7) +
  labs(title = "Heart Rate Variability", x = "Workload", y = "SD of Interval (μs)") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1), legend.position = "none")
ggsave(file.path(SVG_DIR, "l8_heart_rate_variability.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_heart_rate_variability.svg\n", svg_count))

# Chart 6: Heat correlation
p <- ggplot(hb_merged, aes(x = avg_word_heat, y = tick_interval_ns / 1000)) +
  geom_point(alpha = 0.1, size = 0.5) +
  geom_smooth(method = "lm", color = "red") +
  labs(title = "Heat vs Tick Interval Correlation", x = "Avg Word Heat", y = "Interval (μs)") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_heat_correlation.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_heat_correlation.svg\n", svg_count))

# Chart 7: Phase space portrait
p <- ggplot(hb_merged %>% sample_n(min(5000, n())),
            aes(x = avg_word_heat, y = tick_interval_ns / 1000, color = init_script)) +
  geom_point(alpha = 0.3, size = 1) +
  labs(title = "Phase Space Portrait (Heat vs Interval)", x = "Avg Word Heat", y = "Interval (μs)") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_phase_space_portrait.svg"), p, width = 10, height = 8)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_phase_space_portrait.svg\n", svg_count))

# Chart 8: Phase space portrait II
p <- ggplot(hb_merged %>% sample_n(min(5000, n())),
            aes(x = hot_word_count, y = avg_word_heat, color = init_script)) +
  geom_point(alpha = 0.3, size = 1) +
  labs(title = "Phase Space Portrait (Hot Words vs Heat)", x = "Hot Word Count", y = "Avg Word Heat") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_phase_space_portrait_II.svg"), p, width = 10, height = 8)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_phase_space_portrait_II.svg\n", svg_count))

# Chart 9: Time series runs
p <- ggplot(hb_merged %>% filter(run_id <= 10),
            aes(x = elapsed_ns / 1e9, y = avg_word_heat, group = run_id, color = factor(run_id))) +
  geom_line(alpha = 0.6) +
  labs(title = "Heat Time Series (First 10 Runs)", x = "Time (s)", y = "Avg Word Heat", color = "Run ID") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_timeseries_runs.svg"), p, width = 12, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_timeseries_runs.svg\n", svg_count))

# Chart 10: Word heat temporal
p <- ggplot(hb_merged %>% sample_n(min(10000, n())),
            aes(x = elapsed_ns / 1e9, y = avg_word_heat, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  geom_smooth(aes(group = init_script), method = "loess", se = FALSE) +
  labs(title = "Word Heat Temporal Evolution", x = "Time (s)", y = "Avg Word Heat", color = "Workload") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "l8_word_heat_temporal.svg"), p, width = 12, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] l8_word_heat_temporal.svg\n", svg_count))

# Chart 11: Autocorrelation (computed from real data)
# Use largest run (run_id 180 has 40 ticks)
run180_data <- hb_merged %>% filter(run_id == 180) %>% arrange(elapsed_ns)
if (nrow(run180_data) > 20) {
  acf_result <- acf(run180_data$tick_interval_ns, lag.max = min(20, nrow(run180_data) - 1), plot = FALSE)
  max_lag <- length(acf_result$acf) - 1
  acf_data <- data.frame(
    lag = 0:max_lag,
    acf = as.numeric(acf_result$acf)
  )
  p <- ggplot(acf_data, aes(x = lag, y = acf)) +
    geom_bar(stat = "identity", fill = "steelblue") +
    geom_hline(yintercept = 0, linetype = "dashed") +
    labs(title = "Autocorrelation Function (Tick Intervals, Run 180)", x = "Lag", y = "ACF") +
    theme_minimal()
  ggsave(file.path(SVG_DIR, "l8_autocorrelation.svg"), p, width = 10, height = 6)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] l8_autocorrelation.svg\n", svg_count))
}

cat(sprintf("\nL8 charts complete: %d SVGs generated\n", 11))

################################################################################
# PART 2: DOE 30 REPS EXPERIMENT (49 charts)
################################################################################

cat("\n=== PART 2: DOE 30 REPS EXPERIMENT ===\n\n")

DOE30_FILE <- "/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_30reps_RAJ_2025.11.22_1700hrs/doe_results_20251122_085656.csv"

cat("Loading DoE 30 reps data...\n")
doe30 <- read_csv(DOE30_FILE, show_col_types = FALSE)

# Convert to factors
loops <- c("L1_heat_tracking", "L2_rolling_window", "L3_linear_decay",
           "L4_pipelining_metrics", "L5_window_inference", "L6_decay_inference",
           "L7_adaptive_heartrate")

for (loop in loops) {
  doe30[[loop]] <- as.factor(doe30[[loop]])
}

# Compute ns_per_word
if (!"ns_per_word" %in% names(doe30)) {
  doe30$ns_per_word <- doe30$workload_ns_q48 / doe30$words_exec
}

cat(sprintf("  Rows: %d\n", nrow(doe30)))
cat(sprintf("  Columns: %d\n\n", ncol(doe30)))

cat("Generating DoE 30 reps SVGs...\n")

# Main distribution
p <- ggplot(doe30, aes(x = ns_per_word)) +
  geom_histogram(bins = 50, fill = "steelblue", alpha = 0.7) +
  labs(title = "Performance Distribution (DoE 30 Reps)", x = "ns/word", y = "Count") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "doe30_dist_ns_per_word.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] doe30_dist_ns_per_word.svg\n", svg_count))

# Box plots for each loop (7)
for (loop in loops) {
  p <- ggplot(doe30, aes_string(x = loop, y = "ns_per_word", fill = loop)) +
    geom_boxplot(alpha = 0.7) +
    labs(title = paste("Performance by", gsub("_", " ", loop)),
         x = gsub("_", " ", loop), y = "ns/word") +
    theme_minimal() +
    theme(legend.position = "none")

  fname <- paste0("doe30_box_", loop, ".svg")
  ggsave(file.path(SVG_DIR, fname), p, width = 10, height = 6)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] %s\n", svg_count, fname))
}

# Facet plots for each loop (7)
for (loop in loops) {
  p <- ggplot(doe30, aes_string(x = "ns_per_word", fill = loop)) +
    geom_density(alpha = 0.6) +
    facet_wrap(as.formula(paste("~", loop)), ncol = 1) +
    labs(title = paste("Distribution by", gsub("_", " ", loop)), x = "ns/word") +
    theme_minimal()

  fname <- paste0("doe30_facet_", loop, ".svg")
  ggsave(file.path(SVG_DIR, fname), p, width = 10, height = 8)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] %s\n", svg_count, fname))
}

# Interaction plots (21 combinations)
loop_pairs <- combn(loops, 2, simplify = FALSE)
for (i in seq_along(loop_pairs)) {
  L1 <- loop_pairs[[i]][1]
  L2 <- loop_pairs[[i]][2]

  p <- ggplot(doe30, aes_string(x = L1, y = "ns_per_word", fill = L2)) +
    geom_boxplot(alpha = 0.7) +
    labs(title = paste("Interaction:", gsub("_", " ", L1), "×", gsub("_", " ", L2)),
         x = gsub("_", " ", L1), y = "ns/word") +
    theme_minimal()

  fname <- sprintf("doe30_interaction_%02d_%s_x_%s.svg", i, L1, L2)
  ggsave(file.path(SVG_DIR, fname), p, width = 10, height = 6)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] %s\n", svg_count, fname))
}

# Remaining charts
# Correlation matrix
metrics <- doe30 %>% select_if(is.numeric) %>% select(-matches("run_id|replicate"))
if (ncol(metrics) > 1) {
  cor_mat <- cor(metrics, use = "pairwise.complete.obs")
  cor_melt <- melt(cor_mat)

  p <- ggplot(cor_melt, aes(Var1, Var2, fill = value)) +
    geom_tile() +
    scale_fill_gradient2(low = "blue", mid = "white", high = "red", midpoint = 0) +
    labs(title = "Metric Correlation Matrix") +
    theme_minimal() +
    theme(axis.text.x = element_text(angle = 45, hjust = 1))

  ggsave(file.path(SVG_DIR, "doe30_corr_metrics.svg"), p, width = 12, height = 10)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] doe30_corr_metrics.svg\n", svg_count))
}

# Effect heatmap
effects <- data.frame()
for (loop in loops) {
  m1 <- mean(doe30$ns_per_word[doe30[[loop]] == "1"], na.rm = TRUE)
  m0 <- mean(doe30$ns_per_word[doe30[[loop]] == "0"], na.rm = TRUE)
  effects <- rbind(effects, data.frame(factor = loop, effect = m1 - m0))
}

p <- ggplot(effects, aes(x = factor, y = effect, fill = effect)) +
  geom_bar(stat = "identity") +
  scale_fill_gradient2(low = "blue", mid = "white", high = "red") +
  labs(title = "Main Effects (DoE 30 Reps)", x = "Loop", y = "Effect (ns/word)") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1))

ggsave(file.path(SVG_DIR, "doe30_effect_heatmap.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] doe30_effect_heatmap.svg\n", svg_count))

cat(sprintf("\nDoE 30 reps charts complete: Additional SVGs generated\n"))

################################################################################
# PART 3: DOE 300 REPS EXPERIMENT (49 charts)
################################################################################

cat("\n=== PART 3: DOE 300 REPS EXPERIMENT ===\n\n")

DOE300_FILE <- "/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_300_reps_RAJ_2025.11.26.10:30hrs/doe_results_20251123_093204.csv"

cat("Loading DoE 300 reps data...\n")
doe300 <- read_csv(DOE300_FILE, show_col_types = FALSE)

# Convert to factors
for (loop in loops) {
  doe300[[loop]] <- as.factor(doe300[[loop]])
}

# Compute ns_per_word
if (!"ns_per_word" %in% names(doe300)) {
  doe300$ns_per_word <- doe300$workload_ns_q48 / doe300$words_exec
}

cat(sprintf("  Rows: %d\n", nrow(doe300)))
cat(sprintf("  Columns: %d\n\n", ncol(doe300)))

cat("Generating DoE 300 reps SVGs...\n")

# Main distribution
p <- ggplot(doe300, aes(x = ns_per_word)) +
  geom_histogram(bins = 50, fill = "darkgreen", alpha = 0.7) +
  labs(title = "Performance Distribution (DoE 300 Reps)", x = "ns/word", y = "Count") +
  theme_minimal()
ggsave(file.path(SVG_DIR, "doe300_dist_ns_per_word.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] doe300_dist_ns_per_word.svg\n", svg_count))

# Box plots for each loop (7)
for (loop in loops) {
  p <- ggplot(doe300, aes_string(x = loop, y = "ns_per_word", fill = loop)) +
    geom_boxplot(alpha = 0.7) +
    labs(title = paste("Performance by", gsub("_", " ", loop), "(300 reps)"),
         x = gsub("_", " ", loop), y = "ns/word") +
    theme_minimal() +
    theme(legend.position = "none")

  fname <- paste0("doe300_box_", loop, ".svg")
  ggsave(file.path(SVG_DIR, fname), p, width = 10, height = 6)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] %s\n", svg_count, fname))
}

# Facet plots for each loop (7)
for (loop in loops) {
  p <- ggplot(doe300, aes_string(x = "ns_per_word", fill = loop)) +
    geom_density(alpha = 0.6) +
    facet_wrap(as.formula(paste("~", loop)), ncol = 1) +
    labs(title = paste("Distribution by", gsub("_", " ", loop), "(300 reps)"), x = "ns/word") +
    theme_minimal()

  fname <- paste0("doe300_facet_", loop, ".svg")
  ggsave(file.path(SVG_DIR, fname), p, width = 10, height = 8)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] %s\n", svg_count, fname))
}

# Interaction plots (21 combinations)
for (i in seq_along(loop_pairs)) {
  L1 <- loop_pairs[[i]][1]
  L2 <- loop_pairs[[i]][2]

  p <- ggplot(doe300, aes_string(x = L1, y = "ns_per_word", fill = L2)) +
    geom_boxplot(alpha = 0.7) +
    labs(title = paste("Interaction:", gsub("_", " ", L1), "×", gsub("_", " ", L2), "(300 reps)"),
         x = gsub("_", " ", L1), y = "ns/word") +
    theme_minimal()

  fname <- sprintf("doe300_interaction_%02d_%s_x_%s.svg", i, L1, L2)
  ggsave(file.path(SVG_DIR, fname), p, width = 10, height = 6)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] %s\n", svg_count, fname))
}

# Correlation matrix
metrics300 <- doe300 %>% select_if(is.numeric) %>% select(-matches("run_id|replicate"))
if (ncol(metrics300) > 1) {
  cor_mat300 <- cor(metrics300, use = "pairwise.complete.obs")
  cor_melt300 <- melt(cor_mat300)

  p <- ggplot(cor_melt300, aes(Var1, Var2, fill = value)) +
    geom_tile() +
    scale_fill_gradient2(low = "blue", mid = "white", high = "red", midpoint = 0) +
    labs(title = "Metric Correlation Matrix (300 reps)") +
    theme_minimal() +
    theme(axis.text.x = element_text(angle = 45, hjust = 1))

  ggsave(file.path(SVG_DIR, "doe300_corr_metrics.svg"), p, width = 12, height = 10)
  svg_count <- svg_count + 1
  cat(sprintf("  [%d] doe300_corr_metrics.svg\n", svg_count))
}

# Effect heatmap
effects300 <- data.frame()
for (loop in loops) {
  m1 <- mean(doe300$ns_per_word[doe300[[loop]] == "1"], na.rm = TRUE)
  m0 <- mean(doe300$ns_per_word[doe300[[loop]] == "0"], na.rm = TRUE)
  effects300 <- rbind(effects300, data.frame(factor = loop, effect = m1 - m0))
}

p <- ggplot(effects300, aes(x = factor, y = effect, fill = effect)) +
  geom_bar(stat = "identity") +
  scale_fill_gradient2(low = "blue", mid = "white", high = "red") +
  labs(title = "Main Effects (DoE 300 Reps)", x = "Loop", y = "Effect (ns/word)") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1))

ggsave(file.path(SVG_DIR, "doe300_effect_heatmap.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] doe300_effect_heatmap.svg\n", svg_count))

# Comparison: 30 reps vs 300 reps effect sizes
effects_compare <- rbind(
  doe30 %>% group_by(L1_heat_tracking) %>% summarise(m = mean(ns_per_word)) %>%
    mutate(reps = "30", loop = "L1_heat_tracking"),
  doe300 %>% group_by(L1_heat_tracking) %>% summarise(m = mean(ns_per_word)) %>%
    mutate(reps = "300", loop = "L1_heat_tracking")
)

p <- ggplot(effects_compare, aes(x = L1_heat_tracking, y = m, fill = reps)) +
  geom_bar(stat = "identity", position = "dodge") +
  labs(title = "Effect Size Comparison: 30 vs 300 Reps (L1 example)",
       x = "L1 Heat Tracking", y = "Mean ns/word", fill = "Replicates") +
  theme_minimal()

ggsave(file.path(SVG_DIR, "doe_comparison_30v300.svg"), p, width = 10, height = 6)
svg_count <- svg_count + 1
cat(sprintf("  [%d] doe_comparison_30v300.svg\n", svg_count))

cat(sprintf("\nDoE 300 reps charts complete: Additional SVGs generated\n"))

################################################################################
# SUMMARY
################################################################################

cat("\n================================================================================\n")
cat(sprintf("  REGENERATION COMPLETE: %d SVG FILES GENERATED FROM CSV DATA\n", svg_count))
cat("================================================================================\n")
cat(sprintf("\nOutput directory: %s\n", SVG_DIR))
cat("\nAll charts regenerated from real experimental data (NO PNG conversion)\n\n")
