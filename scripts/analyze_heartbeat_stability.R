#!/usr/bin/env Rscript

################################################################################
#
#  analyze_heartbeat_stability.R
#  StarForth Heartbeat DoE Analysis Template
#
#  Purpose: Analyze heartbeat stability metrics from Phase 2 DoE experiment
#  Inputs: experiment_results_heartbeat.csv from run_factorial_doe_with_heartbeat.sh
#  Outputs: Stability rankings, visualizations, golden configuration recommendation
#
#  Usage:
#    Rscript analyze_heartbeat_stability.R /path/to/experiment_results_heartbeat.csv
#    Rscript analyze_heartbeat_stability.R  # Uses ./experiment_results_heartbeat.csv
#
#  Dependencies:
#    library(tidyverse)  # Data manipulation
#    library(ggplot2)    # Visualization
#    library(gridExtra)  # Multi-panel plots
#
################################################################################

library(tidyverse)
library(ggplot2)
library(gridExtra)

# ============================================================================
# Configuration
# ============================================================================

# Get input file from command line or use default
args <- commandArgs(trailingOnly = TRUE)
if (length(args) > 0) {
    csv_file <- args[1]
} else {
    csv_file <- "experiment_results_heartbeat.csv"
}

if (!file.exists(csv_file)) {
    stop(sprintf("Error: File not found: %s", csv_file))
}

cat("Loading data from:", csv_file, "\n")

# ============================================================================
# Load and Prepare Data
# ============================================================================

doe_data <- read.csv(csv_file, check.names = FALSE) %>%
  mutate(
    configuration = as.factor(configuration),
    timestamp = as.POSIXct(timestamp),
    # Ensure all numeric columns are numeric
    across(starts_with("tick_"), as.numeric),
    across(starts_with("cache_hit_"), as.numeric),
    across(starts_with("window_"), as.numeric),
    across(starts_with("decay_slope_"), as.numeric),
    across(starts_with("load_"), as.numeric),
    across(starts_with("settling_"), as.numeric),
    overall_stability_score = as.numeric(overall_stability_score)
  )

cat("\n=== DATA SUMMARY ===\n")
cat(sprintf("Total runs: %d\n", nrow(doe_data)))
cat(sprintf("Configurations: %d\n", n_distinct(doe_data$configuration)))
cat(sprintf("Runs per config: %s\n",
            paste(unique(table(doe_data$configuration)), collapse = ", ")))
cat(sprintf("Columns: %d\n", ncol(doe_data)))

# ============================================================================
# MAIN ANALYSIS: Stability Rankings
# ============================================================================

cat("\n=== STABILITY RANKINGS ===\n")

stability_rank <- doe_data %>%
  group_by(configuration) %>%
  summarize(
    n_runs = n(),

    # Jitter control (lower CV = better)
    mean_jitter_cv = mean(tick_interval_cv, na.rm = TRUE),
    sd_jitter_cv = sd(tick_interval_cv, na.rm = TRUE),

    # Outlier ratio (lower = better)
    mean_outlier_ratio = mean(tick_outlier_ratio, na.rm = TRUE),

    # Convergence speed (higher = faster)
    mean_convergence_rate = mean(decay_slope_convergence_rate, na.rm = TRUE),
    sd_convergence_rate = sd(decay_slope_convergence_rate, na.rm = TRUE),

    # Load response coupling (higher = better)
    mean_load_correlation = mean(load_interval_correlation, na.rm = TRUE),

    # Overall stability (higher = better)
    mean_stability_score = mean(overall_stability_score, na.rm = TRUE),
    sd_stability_score = sd(overall_stability_score, na.rm = TRUE),

    # Settling time (lower = faster)
    mean_settling_time = mean(settling_time_ticks, na.rm = TRUE),

    # Performance (duration, lower = faster)
    mean_workload_ms = mean(vm_workload_duration_ns_q48 / 65536 / 1e6, na.rm = TRUE)
  ) %>%
  mutate(
    rank_stability = rank(-mean_stability_score, ties.method = "min"),
    rank_jitter = rank(mean_jitter_cv, ties.method = "min"),
    rank_convergence = rank(-mean_convergence_rate, ties.method = "min"),
    rank_coupling = rank(-mean_load_correlation, ties.method = "min"),
    overall_rank = (rank_stability + rank_jitter + rank_convergence + rank_coupling) / 4
  ) %>%
  arrange(rank_stability)

print(stability_rank)

# Write rankings to file
write.csv(stability_rank, "stability_rankings.csv", row.names = FALSE)
cat("\nRankings saved to: stability_rankings.csv\n")

# ============================================================================
# VISUALIZATION 1: Stability Score Boxplots
# ============================================================================

cat("\n=== VISUALIZATION 1: Stability Scores ===\n")

p1 <- ggplot(doe_data, aes(x = reorder(configuration, overall_stability_score, FUN=median),
                            y = overall_stability_score,
                            fill = configuration)) +
  geom_boxplot(alpha = 0.7, outlier.alpha = 0.3) +
  geom_jitter(width = 0.2, alpha = 0.4, size = 2) +
  coord_flip() +
  theme_minimal() +
  theme(
    plot.title = element_text(size = 14, face = "bold"),
    axis.title = element_text(size = 12),
    legend.position = "none"
  ) +
  labs(title = "Overall Stability Scores by Configuration",
       subtitle = "Higher = more stable (0-100 scale)",
       x = "Configuration",
       y = "Stability Score")

ggsave("01_stability_scores.png", p1, width = 10, height = 6, dpi = 300)
cat("Saved: 01_stability_scores.png\n")

# ============================================================================
# VISUALIZATION 2: Jitter Control (CV)
# ============================================================================

cat("\n=== VISUALIZATION 2: Jitter Control ===\n")

p2 <- ggplot(doe_data, aes(x = reorder(configuration, tick_interval_cv, FUN=median),
                            y = tick_interval_cv,
                            fill = configuration)) +
  geom_boxplot(alpha = 0.7, outlier.alpha = 0.3) +
  coord_flip() +
  theme_minimal() +
  theme(
    plot.title = element_text(size = 14, face = "bold"),
    axis.title = element_text(size = 12),
    legend.position = "none"
  ) +
  labs(title = "Heartbeat Jitter Control (Coefficient of Variation)",
       subtitle = "Lower CV = steadier heartbeat (target < 0.15)",
       x = "Configuration",
       y = "CV (standard deviation / mean)")

ggsave("02_jitter_control.png", p2, width = 10, height = 6, dpi = 300)
cat("Saved: 02_jitter_control.png\n")

# ============================================================================
# VISUALIZATION 3: Convergence Speed
# ============================================================================

cat("\n=== VISUALIZATION 3: Convergence Speed ===\n")

p3 <- ggplot(doe_data, aes(x = reorder(configuration, decay_slope_convergence_rate, FUN=median),
                            y = decay_slope_convergence_rate,
                            fill = configuration)) +
  geom_boxplot(alpha = 0.7, outlier.alpha = 0.3) +
  coord_flip() +
  theme_minimal() +
  theme(
    plot.title = element_text(size = 14, face = "bold"),
    axis.title = element_text(size = 12),
    legend.position = "none"
  ) +
  labs(title = "Decay Slope Convergence Speed",
       subtitle = "Higher = faster convergence to optimal slope",
       x = "Configuration",
       y = "Convergence Rate")

ggsave("03_convergence_speed.png", p3, width = 10, height = 6, dpi = 300)
cat("Saved: 03_convergence_speed.png\n")

# ============================================================================
# VISUALIZATION 4: Load-Response Coupling Strength
# ============================================================================

cat("\n=== VISUALIZATION 4: Load-Response Coupling ===\n")

p4 <- ggplot(doe_data, aes(x = reorder(configuration, load_interval_correlation, FUN=median),
                            y = load_interval_correlation,
                            fill = configuration)) +
  geom_boxplot(alpha = 0.7, outlier.alpha = 0.3) +
  coord_flip() +
  theme_minimal() +
  theme(
    plot.title = element_text(size = 14, face = "bold"),
    axis.title = element_text(size = 12),
    legend.position = "none"
  ) +
  labs(title = "Load-Heartbeat Response Coupling",
       subtitle = "Higher correlation = smoother load response (target > 0.7)",
       x = "Configuration",
       y = "Correlation Coefficient")

ggsave("04_load_coupling.png", p4, width = 10, height = 6, dpi = 300)
cat("Saved: 04_load_coupling.png\n")

# ============================================================================
# VISUALIZATION 5: Multi-Metric Comparison
# ============================================================================

cat("\n=== VISUALIZATION 5: Multi-Metric Heatmap ===\n")

# Normalize metrics to 0-100 scale for heatmap
metrics_normalized <- stability_rank %>%
  select(configuration, mean_jitter_cv, mean_convergence_rate,
         mean_load_correlation, mean_stability_score) %>%
  mutate(
    # Invert jitter (lower is better)
    jitter_inverted = 100 * (1 - (mean_jitter_cv / max(mean_jitter_cv))),
    convergence_norm = 100 * (mean_convergence_rate / max(mean_convergence_rate)),
    coupling_norm = 100 * (mean_load_correlation / max(mean_load_correlation)),
    stability_norm = mean_stability_score
  ) %>%
  select(configuration, jitter_inverted, convergence_norm, coupling_norm, stability_norm) %>%
  pivot_longer(-configuration, names_to = "metric", values_to = "score") %>%
  mutate(
    metric = factor(metric, levels = c("jitter_inverted", "convergence_norm", "coupling_norm", "stability_norm"),
                   labels = c("Jitter Control", "Convergence", "Load Coupling", "Overall Stability"))
  )

p5 <- ggplot(metrics_normalized, aes(x = configuration, y = metric, fill = score)) +
  geom_tile(color = "white", size = 1) +
  geom_text(aes(label = sprintf("%.0f", score)), color = "black", size = 4) +
  scale_fill_gradient(low = "#d73027", mid = "#fee090", high = "#1a9850",
                      midpoint = 50, limits = c(0, 100),
                      name = "Score") +
  theme_minimal() +
  theme(
    plot.title = element_text(size = 14, face = "bold"),
    axis.title = element_text(size = 12),
    axis.text = element_text(size = 10),
    legend.position = "right"
  ) +
  labs(title = "Heartbeat Stability Metrics Heatmap",
       subtitle = "Green = good, Red = poor (all normalized to 0-100)",
       x = "Configuration",
       y = "Metric")

ggsave("05_metrics_heatmap.png", p5, width = 10, height = 6, dpi = 300)
cat("Saved: 05_metrics_heatmap.png\n")

# ============================================================================
# VISUALIZATION 6: Scatter - Jitter vs Convergence
# ============================================================================

cat("\n=== VISUALIZATION 6: Jitter vs Convergence Trade-off ===\n")

p6 <- ggplot(doe_data, aes(x = tick_interval_cv,
                            y = decay_slope_convergence_rate,
                            color = configuration,
                            shape = configuration)) +
  geom_point(alpha = 0.6, size = 3) +
  geom_smooth(method = "lm", se = FALSE, alpha = 0.2) +
  facet_wrap(~configuration, nrow = 2) +
  theme_minimal() +
  theme(
    plot.title = element_text(size = 14, face = "bold"),
    axis.title = element_text(size = 12),
    legend.position = "bottom"
  ) +
  labs(title = "Jitter vs Convergence Trade-off",
       subtitle = "Upper-left = best (low jitter, fast convergence)",
       x = "Heartbeat Jitter (CV)",
       y = "Convergence Rate")

ggsave("06_tradeoff_jitter_vs_convergence.png", p6, width = 12, height = 8, dpi = 300)
cat("Saved: 06_tradeoff_jitter_vs_convergence.png\n")

# ============================================================================
# STATISTICAL TESTS
# ============================================================================

cat("\n=== STATISTICAL VALIDATION ===\n")

# Test if top configuration is significantly better than others
top_config <- stability_rank$configuration[1]
top_data <- doe_data %>% filter(configuration == top_config) %>% pull(overall_stability_score)
other_data <- doe_data %>% filter(configuration != top_config) %>% pull(overall_stability_score)

# T-test
t_test <- t.test(top_data, other_data, alternative = "greater")
cat(sprintf("\nT-test: %s vs all others\n", top_config))
cat(sprintf("  Mean stability (%s): %.2f ± %.2f\n",
           top_config, mean(top_data, na.rm=TRUE), sd(top_data, na.rm=TRUE)))
cat(sprintf("  Mean stability (others): %.2f ± %.2f\n",
           mean(other_data, na.rm=TRUE), sd(other_data, na.rm=TRUE)))
cat(sprintf("  t-statistic: %.3f\n", t_test$statistic))
cat(sprintf("  p-value: %.4f\n", t_test$p.value))

if (t_test$p.value < 0.05) {
    cat(sprintf("  ✓ SIGNIFICANT: %s is statistically better (p < 0.05)\n", top_config))
} else {
    cat(sprintf("  ✗ NOT SIGNIFICANT: Difference may be due to chance\n"))
}

# Effect size (Cohen's d)
cohens_d <- (mean(top_data, na.rm=TRUE) - mean(other_data, na.rm=TRUE)) /
            sqrt((var(top_data, na.rm=TRUE) + var(other_data, na.rm=TRUE)) / 2)
cat(sprintf("  Effect size (Cohen's d): %.3f\n", cohens_d))

# ANOVA: Are all groups different?
anova_result <- aov(overall_stability_score ~ configuration, data = doe_data)
anova_summary <- summary(anova_result)
cat("\nANOVA: Configuration effect on overall stability\n")
print(anova_summary)

# ============================================================================
# GOLDEN CONFIGURATION SELECTION
# ============================================================================

cat("\n\n")
cat("═══════════════════════════════════════════════════════════════════════════════\n")
cat("GOLDEN CONFIGURATION RECOMMENDATION\n")
cat("═══════════════════════════════════════════════════════════════════════════════\n")

golden <- stability_rank %>% slice(1)

cat(sprintf("\n✓ SELECTED: %s\n", as.character(golden$configuration)))
cat(sprintf("\n  Stability Metrics:\n"))
cat(sprintf("    • Overall Stability Score: %.2f / 100\n", golden$mean_stability_score))
cat(sprintf("    • Heartbeat Jitter (CV): %.4f (target < 0.15)\n", golden$mean_jitter_cv))
cat(sprintf("    • Outlier Ratio: %.2f%%\n", golden$mean_outlier_ratio * 100))
cat(sprintf("    • Convergence Rate: %.2f (higher = faster)\n", golden$mean_convergence_rate))
cat(sprintf("    • Load Coupling: %.4f (target > 0.7)\n", golden$mean_load_correlation))
cat(sprintf("    • Settling Time: %.1f ticks\n", golden$mean_settling_time))
cat(sprintf("    • Mean Workload Duration: %.2f ms\n", golden$mean_workload_ms))

cat(sprintf("\n  Performance vs Baseline:\n"))
baseline <- stability_rank %>% arrange(rank_stability) %>% slice(n())
cat(sprintf("    • vs Worst Config (%s):\n", as.character(baseline$configuration)))
cat(sprintf("      - Stability improvement: %.1f%%\n",
           100 * (golden$mean_stability_score - baseline$mean_stability_score) / baseline$mean_stability_score))
cat(sprintf("      - Jitter reduction: %.1f%%\n",
           100 * (baseline$mean_jitter_cv - golden$mean_jitter_cv) / baseline$mean_jitter_cv))

cat(sprintf("\n  Recommendation for Use:\n"))
cat(sprintf("    • Deploy as physics baseline for MamaForth/PapaForth\n"))
cat(sprintf("    • Use as default configuration for production\n"))
cat(sprintf("    • Consider this configuration stable and reliable\n"))

cat("\n═══════════════════════════════════════════════════════════════════════════════\n\n")

# ============================================================================
# Summary Report
# ============================================================================

cat("\n=== ANALYSIS COMPLETE ===\n")
cat(sprintf("\nGenerated files:\n"))
cat("  • stability_rankings.csv          - Detailed rankings table\n")
cat("  • 01_stability_scores.png         - Main stability comparison\n")
cat("  • 02_jitter_control.png           - Heartbeat jitter by config\n")
cat("  • 03_convergence_speed.png        - Convergence speed comparison\n")
cat("  • 04_load_coupling.png            - Load-response coupling strength\n")
cat("  • 05_metrics_heatmap.png          - All metrics normalized heatmap\n")
cat("  • 06_tradeoff_jitter_vs_convergence.png - Trade-off analysis\n")
cat("\nNext steps:\n")
cat("  1. Review stability_rankings.csv for complete rankings\n")
cat("  2. Examine PNG visualizations for patterns\n")
cat("  3. Use golden configuration as production baseline\n")
cat("  4. Archive results for future reference\n")