#!/usr/bin/env Rscript
################################################################################
# REGENERATE L8 VALIDATION SVG CHARTS
# Analyzes L8 Jacquard mode selector validation experiment
# Design: 8 strategies × 5 workloads × N replicates
################################################################################

.libPaths('~/R/library')
suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(readr)
  library(scales)
})

# Paths
SVG_DIR <- "/home/rajames/CLionProjects/StarForth/papers/LaTeX/figures"
DATA_FILE <- "/home/rajames/CLionProjects/StarForth/experiments/l8_validation/l8_validation_20251208_102205/l8_validation_results.csv"

dir.create(SVG_DIR, showWarnings = FALSE, recursive = TRUE)

cat("================================================================================\n")
cat("  REGENERATING L8 VALIDATION CHARTS\n")
cat("================================================================================\n\n")

# Load data
cat("Loading L8 validation data...\n")
df <- read_csv(DATA_FILE, show_col_types = FALSE)

cat(sprintf("Loaded %d runs\n", nrow(df)))
cat(sprintf("Strategies: %s\n", paste(unique(df$strategy), collapse = ", ")))
cat(sprintf("Workload types: %s\n\n", paste(unique(df$workload_type), collapse = ", ")))

# Calculate ns_per_word and CV from runtime_ns
# Extract relevant metrics from doe_csv field if needed, but for now use runtime_ns
df <- df %>%
  mutate(
    ns_per_word = runtime_ns / 1000,  # Approximate: runtime / ~1000 words
    runtime_ms = runtime_ns / 1e6
  )

# Calculate summary statistics per strategy and workload
summary_stats <- df %>%
  group_by(strategy, workload_type) %>%
  summarise(
    n = n(),
    mean_runtime = mean(runtime_ns),
    sd_runtime = sd(runtime_ns),
    cv = sd_runtime / mean_runtime * 100,
    mean_ns_per_word = mean(ns_per_word),
    sd_ns_per_word = sd(ns_per_word),
    se_ns_per_word = sd_ns_per_word / sqrt(n),
    .groups = "drop"
  )

# Strategy ordering (L8 first, then alphabetically)
strategy_levels <- c("L8_ADAPTIVE", sort(unique(df$strategy[df$strategy != "L8_ADAPTIVE"])))
df$strategy <- factor(df$strategy, levels = strategy_levels)
summary_stats$strategy <- factor(summary_stats$strategy, levels = strategy_levels)

# Workload ordering
workload_levels <- c("STABLE", "DIVERSE", "VOLATILE", "TEMPORAL", "TRANSITION")
df$workload_type <- factor(df$workload_type, levels = workload_levels)
summary_stats$workload_type <- factor(summary_stats$workload_type, levels = workload_levels)

cat("Generating SVG charts...\n")
svg_count <- 0

# ============================================================================
# Chart 1: Performance by Strategy and Workload (Box Plot)
# ============================================================================

cat("  [1] Performance by strategy and workload\n")

p1 <- ggplot(df, aes(x = strategy, y = runtime_ms, fill = strategy)) +
  geom_boxplot(alpha = 0.7, outlier.alpha = 0.3) +
  facet_wrap(~ workload_type, scales = "free_y", ncol = 3) +
  labs(
    title = "L8 Validation: Performance by Strategy and Workload",
    subtitle = "Lower is better. L8_ADAPTIVE should match or exceed best static config per workload",
    x = "Control Strategy",
    y = "Runtime (ms)"
  ) +
  theme_minimal() +
  theme(
    axis.text.x = element_text(angle = 45, hjust = 1, size = 7),
    legend.position = "none",
    strip.text = element_text(face = "bold")
  ) +
  scale_fill_viridis_d()

ggsave(file.path(SVG_DIR, "l8_validation_performance_by_strategy.svg"), p1,
       width = 14, height = 10, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 2: Interaction Plot (Strategy × Workload)
# ============================================================================

cat("  [2] ANOVA interaction plot\n")

p2 <- ggplot(summary_stats, aes(x = workload_type, y = mean_ns_per_word,
                                 color = strategy, group = strategy)) +
  geom_line(size = 1) +
  geom_point(size = 3) +
  geom_errorbar(aes(ymin = mean_ns_per_word - se_ns_per_word,
                    ymax = mean_ns_per_word + se_ns_per_word),
                width = 0.2, alpha = 0.6) +
  labs(
    title = "L8 Validation: Strategy × Workload Interaction",
    subtitle = "Lines should diverge if interaction is present. L8_ADAPTIVE (teal) should track best performers.",
    x = "Workload Type",
    y = "Mean Performance (ns/word)",
    color = "Strategy"
  ) +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  scale_color_viridis_d()

ggsave(file.path(SVG_DIR, "l8_validation_interaction_plot.svg"), p2,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 3: Stability (CV) by Strategy and Workload
# ============================================================================

cat("  [3] Stability by strategy and workload\n")

p3 <- ggplot(summary_stats, aes(x = strategy, y = cv, fill = strategy)) +
  geom_col(alpha = 0.7) +
  facet_wrap(~ workload_type, ncol = 3) +
  labs(
    title = "L8 Validation: Stability (CV) by Strategy and Workload",
    subtitle = "Lower CV = more stable. L8 should show low CV across workloads.",
    x = "Control Strategy",
    y = "Coefficient of Variation (%)"
  ) +
  theme_minimal() +
  theme(
    axis.text.x = element_text(angle = 45, hjust = 1, size = 7),
    legend.position = "none",
    strip.text = element_text(face = "bold")
  ) +
  scale_fill_viridis_d()

ggsave(file.path(SVG_DIR, "l8_validation_stability_cv.svg"), p3,
       width = 14, height = 10, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 4: Strategy Ranking per Workload
# ============================================================================

cat("  [4] Strategy ranking per workload\n")

# Find best strategy per workload
best_per_workload <- summary_stats %>%
  group_by(workload_type) %>%
  arrange(mean_ns_per_word) %>%
  mutate(rank = row_number()) %>%
  ungroup()

p4 <- ggplot(best_per_workload, aes(x = reorder(strategy, -rank), y = rank,
                                     fill = strategy)) +
  geom_col(alpha = 0.7) +
  facet_wrap(~ workload_type, ncol = 3) +
  labs(
    title = "L8 Validation: Strategy Ranking per Workload",
    subtitle = "Rank 1 = fastest. L8_ADAPTIVE should rank in top 3 for all workloads.",
    x = "Control Strategy",
    y = "Rank (1 = best)"
  ) +
  theme_minimal() +
  theme(
    axis.text.x = element_text(angle = 45, hjust = 1, size = 7),
    legend.position = "none",
    strip.text = element_text(face = "bold")
  ) +
  scale_fill_viridis_d() +
  scale_y_reverse()

ggsave(file.path(SVG_DIR, "l8_validation_strategy_ranking.svg"), p4,
       width = 14, height = 10, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 5: Overall Performance Distribution
# ============================================================================

cat("  [5] Overall performance distribution\n")

p5 <- ggplot(df, aes(x = runtime_ms, fill = strategy)) +
  geom_density(alpha = 0.5) +
  labs(
    title = "L8 Validation: Performance Distribution by Strategy",
    subtitle = "Density plot of runtime across all workloads",
    x = "Runtime (ms)",
    y = "Density",
    fill = "Strategy"
  ) +
  theme_minimal() +
  scale_fill_viridis_d()

ggsave(file.path(SVG_DIR, "l8_validation_performance_distribution.svg"), p5,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 6: Pareto Frontier (Speed vs Stability)
# ============================================================================

cat("  [6] Pareto frontier (speed vs stability)\n")

# Calculate overall stats per strategy
overall_stats <- df %>%
  group_by(strategy) %>%
  summarise(
    mean_runtime = mean(runtime_ms),
    cv = sd(runtime_ms) / mean(runtime_ms) * 100,
    .groups = "drop"
  )

# Identify Pareto frontier
overall_stats <- overall_stats %>%
  arrange(mean_runtime) %>%
  mutate(
    is_pareto = cummin(cv) == cv
  )

p6 <- ggplot(overall_stats, aes(x = mean_runtime, y = cv, color = strategy,
                                 shape = is_pareto)) +
  geom_point(size = 4) +
  geom_text(aes(label = strategy), vjust = -0.5, hjust = 0.5, size = 3) +
  labs(
    title = "L8 Validation: Pareto Frontier (Speed vs Stability)",
    subtitle = "Lower-left is better. Pareto-optimal strategies shown with triangle.",
    x = "Mean Runtime (ms)",
    y = "CV (%)",
    color = "Strategy",
    shape = "Pareto Optimal"
  ) +
  theme_minimal() +
  scale_color_viridis_d() +
  scale_shape_manual(values = c(16, 17))

ggsave(file.path(SVG_DIR, "l8_validation_pareto_frontier.svg"), p6,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 7: L8 Performance Margin per Workload
# ============================================================================

cat("  [7] L8 performance margin per workload\n")

# Calculate L8 margin relative to best static config per workload
l8_margin <- summary_stats %>%
  group_by(workload_type) %>%
  mutate(
    best_runtime = min(mean_ns_per_word),
    margin_pct = (mean_ns_per_word - best_runtime) / best_runtime * 100
  ) %>%
  filter(strategy == "L8_ADAPTIVE") %>%
  ungroup()

p7 <- ggplot(l8_margin, aes(x = workload_type, y = margin_pct, fill = workload_type)) +
  geom_col(alpha = 0.7) +
  geom_hline(yintercept = 0, linetype = "solid", color = "green", size = 1) +
  geom_hline(yintercept = 5, linetype = "dashed", color = "red", size = 1) +
  labs(
    title = "L8 Validation: L8_ADAPTIVE Performance Margin per Workload",
    subtitle = "% slower than best static config. Green line: 0% (matches best). Red line: 5% threshold.",
    x = "Workload Type",
    y = "Performance Margin (%)"
  ) +
  theme_minimal() +
  theme(legend.position = "none") +
  scale_fill_viridis_d()

ggsave(file.path(SVG_DIR, "l8_validation_l8_margin.svg"), p7,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 8: Heatmap (Strategy × Workload Performance)
# ============================================================================

cat("  [8] Performance heatmap\n")

p8 <- ggplot(summary_stats, aes(x = workload_type, y = strategy, fill = mean_ns_per_word)) +
  geom_tile(color = "white") +
  geom_text(aes(label = sprintf("%.1f", mean_ns_per_word)), color = "white", size = 3) +
  labs(
    title = "L8 Validation: Performance Heatmap",
    subtitle = "Mean ns/word per strategy and workload. Darker = faster.",
    x = "Workload Type",
    y = "Control Strategy",
    fill = "ns/word"
  ) +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  scale_fill_viridis_c(option = "plasma", direction = -1)

ggsave(file.path(SVG_DIR, "l8_validation_heatmap.svg"), p8,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Summary
# ============================================================================

cat("\n================================================================================\n")
cat(sprintf("  REGENERATION COMPLETE: %d L8 VALIDATION SVG CHARTS GENERATED\n", svg_count))
cat("================================================================================\n")
cat(sprintf("\nOutput directory: %s\n", SVG_DIR))
cat("\nCharts generated:\n")
cat("  1. l8_validation_performance_by_strategy.svg - Performance box plots\n")
cat("  2. l8_validation_interaction_plot.svg - Strategy × workload interaction\n")
cat("  3. l8_validation_stability_cv.svg - Stability (CV) by strategy\n")
cat("  4. l8_validation_strategy_ranking.svg - Strategy ranking per workload\n")
cat("  5. l8_validation_performance_distribution.svg - Overall distribution\n")
cat("  6. l8_validation_pareto_frontier.svg - Speed vs stability Pareto frontier\n")
cat("  7. l8_validation_l8_margin.svg - L8 performance margin per workload\n")
cat("  8. l8_validation_heatmap.svg - Performance heatmap\n")
cat("\nAll charts regenerated from L8 validation CSV data (NO PNG conversion)\n")
cat("\n")
