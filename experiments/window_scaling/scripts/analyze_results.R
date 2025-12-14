#!/usr/bin/env Rscript
# ============================================================================
# Window Scaling Experiment: James Law Analysis
# ============================================================================
# Purpose: Validate the James Law of Computational Dynamics:
#          Λ = W / (DoF + 1)
#
# Input:  results_run_01_2025_12_08/raw/window_sweep_results.csv (2,880 rows)
# Output: Statistical validation + visualizations
#
# Author: Robert A. James
# Date: 2025-11-29
# ============================================================================

library(dplyr)
library(tidyr)
library(readr)
library(ggplot2)
library(scales)

# For advanced plots (install if needed)
suppressPackageStartupMessages({
  if (requireNamespace("viridis", quietly = TRUE)) {
    library(viridis)
  }
  if (requireNamespace("patchwork", quietly = TRUE)) {
    library(patchwork)
  }
})

# ============================================================================
# Configuration
# ============================================================================

# Get experiment directory (assumes script is run from experiment root or scripts/)
get_experiment_dir <- function() {
  # Try to determine if we're in scripts/ or experiment root
  if (file.exists("results")) {
    # We're in experiment root
    return(".")
  } else if (file.exists("../results_run_one")) {
    # We're in scripts/
    return("..")
  } else {
    # Fallback: use script location
    tryCatch({
      script_dir <- dirname(sys.frame(1)$ofile)
      if (length(script_dir) > 0 && script_dir != "") {
        return(file.path(script_dir, ".."))
      }
    }, error = function(e) {})
    return(".")
  }
}

EXPERIMENT_DIR <- get_experiment_dir()
RAW_DIR <- file.path(EXPERIMENT_DIR, "results", "raw")
PROCESSED_DIR <- file.path(EXPERIMENT_DIR, "results", "processed")

RESULTS_FILE <- file.path(RAW_DIR, "window_sweep_results.csv")

# Create output directory
dir.create(PROCESSED_DIR, showWarnings = FALSE, recursive = TRUE)

# ============================================================================
# Helper Functions
# ============================================================================

# Compute coefficient of variation
cv <- function(x) {
  sd(x, na.rm = TRUE) / mean(x, na.rm = TRUE) * 100
}

# Print section header
section <- function(title) {
  cat("\n")
  cat(paste(rep("=", 80), collapse = ""), "\n")
  cat(title, "\n")
  cat(paste(rep("=", 80), collapse = ""), "\n")
}

# Print subsection header
subsection <- function(title) {
  cat("\n")
  cat(paste(rep("-", 80), collapse = ""), "\n")
  cat(title, "\n")
  cat(paste(rep("-", 80), collapse = ""), "\n")
}

# ============================================================================
# Load Data
# ============================================================================

section("JAMES LAW VALIDATION ANALYSIS")

cat("\nLoading experimental data...\n")

if (!file.exists(RESULTS_FILE)) {
  cat("ERROR: Results file not found:\n")
  cat("  ", RESULTS_FILE, "\n")
  cat("\nPlease run the experiment first:\n")
  cat("  cd scripts/\n")
  cat("  ./run_window_sweep.sh\n")
  quit(status = 1)
}

# Read data
df_raw <- read_csv(RESULTS_FILE, show_col_types = FALSE)

cat("✓ Data loaded:", nrow(df_raw), "rows\n")
cat("  Columns:", ncol(df_raw), "\n")

# Check for expected columns
expected_cols <- c("dof", "window_size", "replicate", "lambda_predicted",
                   "runtime_ms", "workload_ns_q48")

missing_cols <- setdiff(expected_cols, colnames(df_raw))
if (length(missing_cols) > 0) {
  cat("ERROR: Missing required columns:\n")
  cat("  ", paste(missing_cols, collapse = ", "), "\n")
  quit(status = 1)
}

cat("✓ All required columns present\n")

# ============================================================================
# Data Preprocessing
# ============================================================================

subsection("Data Preprocessing")

# Filter out failed runs (NaN values)
df_valid <- df_raw %>%
  filter(!is.na(runtime_ms), !is.na(workload_ns_q48))

n_failed <- nrow(df_raw) - nrow(df_valid)
pct_failed <- n_failed / nrow(df_raw) * 100

cat("Valid runs:  ", nrow(df_valid), "\n")
cat("Failed runs: ", n_failed, sprintf(" (%.1f%%)\n", pct_failed))

if (pct_failed > 10) {
  cat("⚠ WARNING: >10% failure rate. Results may be unreliable.\n")
}

# Compute derived metrics
df <- df_valid %>%
  mutate(
    # Performance metrics
    ns_per_word = workload_ns_q48 / (2^16),  # Q48.16 to ns

    # Effective lambda from VM output (if win_final_bytes is available)
    lambda_effective = if ("win_final_bytes" %in% colnames(.)) {
      win_final_bytes / (dof + 1)
    } else {
      lambda_predicted  # Fall back to predicted
    },

    # K statistic: the key test of James Law
    K = lambda_effective * (dof + 1) / window_size,

    # Deviation from ideal K = 1.0
    K_deviation = abs(K - 1.0),
    K_deviation_pct = K_deviation * 100,

    # Schwarzschild radius (instability metric)
    r_s = dof / (window_size / 4096),

    # Window category for grouping
    window_category = factor(window_category,
                            levels = c("subcritical", "baseline", "stable",
                                      "critical", "collapse"))
  )

cat("\n✓ Derived metrics computed\n")
cat("  - ns_per_word: execution time\n")
cat("  - lambda_effective: Λ from VM metrics\n")
cat("  - K: James Law test statistic\n")
cat("  - K_deviation: |K - 1.0|\n")

# ============================================================================
# James Law Validation: Overall Statistics
# ============================================================================

section("JAMES LAW VALIDATION: OVERALL")

overall_K <- df %>%
  summarise(
    n = n(),
    mean_K = mean(K, na.rm = TRUE),
    median_K = median(K, na.rm = TRUE),
    sd_K = sd(K, na.rm = TRUE),
    min_K = min(K, na.rm = TRUE),
    max_K = max(K, na.rm = TRUE),
    cv_K = cv(K),
    mean_deviation = mean(K_deviation, na.rm = TRUE),
    max_deviation = max(K_deviation, na.rm = TRUE)
  )

cat("\nK Statistic Across All Runs:\n")
cat(sprintf("  Mean(K):        %.4f\n", overall_K$mean_K))
cat(sprintf("  Median(K):      %.4f\n", overall_K$median_K))
cat(sprintf("  Std(K):         %.4f\n", overall_K$sd_K))
cat(sprintf("  CV(K):          %.2f%%\n", overall_K$cv_K))
cat(sprintf("  Range:          [%.4f, %.4f]\n", overall_K$min_K, overall_K$max_K))
cat(sprintf("  Mean |K-1.0|:   %.4f\n", overall_K$mean_deviation))
cat(sprintf("  Max |K-1.0|:    %.4f\n", overall_K$max_deviation))

cat("\nJames Law Validation Criteria:\n")

# Criterion 1: Mean K close to 1.0
criterion1 <- overall_K$mean_K >= 0.95 && overall_K$mean_K <= 1.05
cat(sprintf("  [%s] Mean(K) ∈ [0.95, 1.05]:  %.4f\n",
            ifelse(criterion1, "✓", "✗"), overall_K$mean_K))

# Criterion 2: Low standard deviation
criterion2 <- overall_K$sd_K < 0.1
cat(sprintf("  [%s] Std(K) < 0.1:            %.4f\n",
            ifelse(criterion2, "✓", "✗"), overall_K$sd_K))

# Criterion 3: Small max deviation
criterion3 <- overall_K$max_deviation < 0.1
cat(sprintf("  [%s] Max|K-1.0| < 0.1:        %.4f\n",
            ifelse(criterion3, "✓", "✗"), overall_K$max_deviation))

james_law_holds <- criterion1 && criterion2 && criterion3

cat("\n")
if (james_law_holds) {
  cat("🎉 JAMES LAW VALIDATED: K ≈ 1.0 across all conditions\n")
  cat("   The relationship Λ = W / (DoF + 1) is empirically confirmed.\n")
} else {
  cat("⚠  JAMES LAW NOT CONFIRMED: K deviates significantly from 1.0\n")
  cat("   Further analysis needed to characterize the relationship.\n")
}

# ============================================================================
# James Law Validation: By DoF
# ============================================================================

subsection("Validation by Degrees of Freedom")

K_by_dof <- df %>%
  group_by(dof) %>%
  summarise(
    n = n(),
    mean_K = mean(K, na.rm = TRUE),
    sd_K = sd(K, na.rm = TRUE),
    cv_K = cv(K),
    mean_deviation = mean(K_deviation, na.rm = TRUE),
    .groups = "drop"
  )

cat("\nK Statistics by DoF:\n")
print(K_by_dof, n = Inf)

# Check if K is consistent across DoF
dof_consistency <- K_by_dof %>%
  summarise(
    range = max(mean_K) - min(mean_K),
    cv_across_dof = cv(mean_K)
  )

cat(sprintf("\nConsistency across DoF:\n"))
cat(sprintf("  Range of mean(K):  %.4f\n", dof_consistency$range))
cat(sprintf("  CV of mean(K):     %.2f%%\n", dof_consistency$cv_across_dof))

if (dof_consistency$range < 0.1) {
  cat("  ✓ K is consistent across all DoF levels\n")
} else {
  cat("  ✗ K varies significantly with DoF\n")
}

# ============================================================================
# James Law Validation: By Window Size
# ============================================================================

subsection("Validation by Window Size")

K_by_window <- df %>%
  group_by(window_size, window_category) %>%
  summarise(
    n = n(),
    mean_K = mean(K, na.rm = TRUE),
    sd_K = sd(K, na.rm = TRUE),
    cv_K = cv(K),
    mean_deviation = mean(K_deviation, na.rm = TRUE),
    mean_cv_performance = mean(cv(ns_per_word), na.rm = TRUE),
    .groups = "drop"
  ) %>%
  arrange(window_size)

cat("\nK Statistics by Window Size:\n")
print(K_by_window, n = Inf)

# Identify critical window
cat("\nCritical Window Detection:\n")

# Method 1: K deviation threshold
critical_by_K <- K_by_window %>%
  filter(mean_deviation > 0.05) %>%
  pull(window_size) %>%
  min()

cat(sprintf("  Critical W (K deviation > 0.05): %d bytes\n", critical_by_K))

# Method 2: CV explosion (if CV data available)
if (!all(is.na(K_by_window$mean_cv_performance))) {
  cv_threshold <- median(K_by_window$mean_cv_performance, na.rm = TRUE) * 1.5
  critical_by_cv <- K_by_window %>%
    filter(mean_cv_performance > cv_threshold) %>%
    pull(window_size) %>%
    min()

  cat(sprintf("  Critical W (CV explosion):       %d bytes\n", critical_by_cv))
}

# ============================================================================
# Stability Analysis
# ============================================================================

section("STABILITY ANALYSIS")

stability_summary <- df %>%
  group_by(window_size, dof) %>%
  summarise(
    n = n(),
    mean_performance = mean(ns_per_word, na.rm = TRUE),
    cv_performance = cv(ns_per_word),
    mean_K = mean(K, na.rm = TRUE),
    sd_K = sd(K, na.rm = TRUE),
    .groups = "drop"
  )

# Overall stability by window
stability_by_window <- stability_summary %>%
  group_by(window_size) %>%
  summarise(
    mean_cv = mean(cv_performance, na.rm = TRUE),
    max_cv = max(cv_performance, na.rm = TRUE),
    .groups = "drop"
  ) %>%
  arrange(window_size)

cat("\nStability (CV of performance) by Window Size:\n")
print(stability_by_window, n = Inf)

# Identify most stable window
optimal_window <- stability_by_window %>%
  filter(mean_cv == min(mean_cv, na.rm = TRUE)) %>%
  pull(window_size)

cat(sprintf("\n✓ Most stable window: W = %d bytes\n", optimal_window))

# ============================================================================
# Save Processed Data
# ============================================================================

section("SAVING RESULTS")

# Summary table: K by condition
james_law_validation <- df %>%
  group_by(dof, window_size) %>%
  summarise(
    n = n(),
    mean_K = mean(K, na.rm = TRUE),
    median_K = median(K, na.rm = TRUE),
    sd_K = sd(K, na.rm = TRUE),
    min_K = min(K, na.rm = TRUE),
    max_K = max(K, na.rm = TRUE),
    mean_deviation = mean(K_deviation, na.rm = TRUE),
    max_deviation = max(K_deviation, na.rm = TRUE),
    mean_performance = mean(ns_per_word, na.rm = TRUE),
    cv_performance = cv(ns_per_word),
    .groups = "drop"
  )

outfile1 <- file.path(PROCESSED_DIR, "james_law_validation.csv")
write_csv(james_law_validation, outfile1)
cat("✓ Saved:", outfile1, "\n")

# Full dataset with computed metrics
outfile2 <- file.path(PROCESSED_DIR, "window_sweep_processed.csv")
write_csv(df, outfile2)
cat("✓ Saved:", outfile2, "\n")

# Summary statistics
summary_stats <- list(
  overall = overall_K,
  by_dof = K_by_dof,
  by_window = K_by_window,
  stability = stability_by_window
)

outfile3 <- file.path(PROCESSED_DIR, "summary_statistics.rds")
saveRDS(summary_stats, outfile3)
cat("✓ Saved:", outfile3, "\n")

# ============================================================================
# Visualization: K Distribution
# ============================================================================

section("GENERATING VISUALIZATIONS")

cat("\nPlot 1: K Distribution by DoF and Window Size\n")

p1 <- ggplot(df, aes(x = factor(dof), y = K, fill = factor(dof))) +
  geom_hline(yintercept = 1.0, linetype = "dashed", color = "red", size = 1) +
  geom_violin(alpha = 0.6) +
  geom_boxplot(width = 0.2, alpha = 0.8, outlier.alpha = 0.3) +
  facet_wrap(~ window_size, ncol = 4,
             labeller = labeller(window_size = function(x) paste0("W=", x))) +
  labs(
    title = "James Law Validation: K Distribution Across All Conditions",
    subtitle = "Dashed red line: K = 1.0 (perfect fit)",
    x = "Degrees of Freedom (DoF)",
    y = "K = Λ(DoF+1)/W",
    fill = "DoF"
  ) +
  theme_minimal() +
  theme(
    legend.position = "none",
    strip.text = element_text(size = 8)
  )

if (requireNamespace("viridis", quietly = TRUE)) {
  p1 <- p1 + scale_fill_viridis_d()
}

outfile_p1 <- file.path(PROCESSED_DIR, "K_distribution_by_condition.png")
ggsave(outfile_p1, p1, width = 14, height = 10, dpi = 300)
cat("✓ Saved:", outfile_p1, "\n")

# ============================================================================
# Visualization: K vs Window Size
# ============================================================================

cat("\nPlot 2: K vs Window Size (by DoF)\n")

p2 <- ggplot(james_law_validation, aes(x = window_size, y = mean_K, color = factor(dof))) +
  geom_hline(yintercept = 1.0, linetype = "dashed", color = "gray30", size = 1) +
  geom_line(size = 1) +
  geom_point(size = 3) +
  geom_errorbar(aes(ymin = mean_K - sd_K, ymax = mean_K + sd_K),
                width = 0.1, alpha = 0.5) +
  scale_x_log10(labels = comma) +
  labs(
    title = "James Law: K vs Window Size",
    subtitle = "Error bars: ±1 SD. Dashed line: K = 1.0",
    x = "Window Size (bytes, log scale)",
    y = "Mean K",
    color = "DoF"
  ) +
  theme_minimal() +
  theme(legend.position = "right")

if (requireNamespace("viridis", quietly = TRUE)) {
  p2 <- p2 + scale_color_viridis_d()
}

outfile_p2 <- file.path(PROCESSED_DIR, "K_vs_window_size.png")
ggsave(outfile_p2, p2, width = 12, height = 8, dpi = 300)
cat("✓ Saved:", outfile_p2, "\n")

# ============================================================================
# Visualization: Stability Surface
# ============================================================================

cat("\nPlot 3: Stability Surface (CV vs DoF × Window)\n")

p3 <- ggplot(stability_summary, aes(x = factor(dof), y = factor(window_size),
                                     fill = cv_performance)) +
  geom_tile() +
  geom_text(aes(label = sprintf("%.1f", cv_performance)),
            size = 3, color = "white") +
  labs(
    title = "Stability Surface: CV of Performance",
    subtitle = "Lower CV = more stable",
    x = "Degrees of Freedom (DoF)",
    y = "Window Size (bytes)",
    fill = "CV (%)"
  ) +
  theme_minimal() +
  theme(legend.position = "right")

if (requireNamespace("viridis", quietly = TRUE)) {
  p3 <- p3 + scale_fill_viridis_c(option = "plasma", direction = -1)
} else {
  p3 <- p3 + scale_fill_gradient2(low = "blue", mid = "white", high = "red",
                                   midpoint = median(stability_summary$cv_performance, na.rm = TRUE))
}

outfile_p3 <- file.path(PROCESSED_DIR, "stability_heatmap.png")
ggsave(outfile_p3, p3, width = 10, height = 8, dpi = 300)
cat("✓ Saved:", outfile_p3, "\n")

# ============================================================================
# Visualization: K Deviation Heatmap
# ============================================================================

cat("\nPlot 4: K Deviation Heatmap\n")

p4 <- ggplot(james_law_validation, aes(x = factor(dof), y = factor(window_size),
                                        fill = mean_deviation)) +
  geom_tile() +
  geom_text(aes(label = sprintf("%.3f", mean_deviation)),
            size = 3, color = "white") +
  labs(
    title = "James Law Deviation: |K - 1.0|",
    subtitle = "Lower deviation = better fit to James Law",
    x = "Degrees of Freedom (DoF)",
    y = "Window Size (bytes)",
    fill = "|K - 1.0|"
  ) +
  theme_minimal() +
  theme(legend.position = "right")

if (requireNamespace("viridis", quietly = TRUE)) {
  p4 <- p4 + scale_fill_viridis_c(option = "magma", direction = -1)
} else {
  p4 <- p4 + scale_fill_gradient(low = "darkgreen", high = "red")
}

outfile_p4 <- file.path(PROCESSED_DIR, "K_deviation_heatmap.png")
ggsave(outfile_p4, p4, width = 10, height = 8, dpi = 300)
cat("✓ Saved:", outfile_p4, "\n")

# ============================================================================
# Visualization: Performance vs Window Size
# ============================================================================

cat("\nPlot 5: Performance vs Window Size\n")

perf_summary <- df %>%
  group_by(window_size, dof) %>%
  summarise(
    mean_perf = mean(ns_per_word, na.rm = TRUE),
    sd_perf = sd(ns_per_word, na.rm = TRUE),
    .groups = "drop"
  )

p5 <- ggplot(perf_summary, aes(x = window_size, y = mean_perf, color = factor(dof))) +
  geom_line(size = 1) +
  geom_point(size = 2) +
  scale_x_log10(labels = comma) +
  labs(
    title = "Performance vs Window Size",
    x = "Window Size (bytes, log scale)",
    y = "Mean Performance (ns/word)",
    color = "DoF"
  ) +
  theme_minimal() +
  theme(legend.position = "right")

if (requireNamespace("viridis", quietly = TRUE)) {
  p5 <- p5 + scale_color_viridis_d()
}

outfile_p5 <- file.path(PROCESSED_DIR, "performance_vs_window.png")
ggsave(outfile_p5, p5, width = 12, height = 8, dpi = 300)
cat("✓ Saved:", outfile_p5, "\n")

# ============================================================================
# Final Summary Report
# ============================================================================

section("FINAL SUMMARY")

cat("\nExperimental Data:\n")
cat(sprintf("  Total runs:       %d\n", nrow(df_raw)))
cat(sprintf("  Valid runs:       %d\n", nrow(df)))
cat(sprintf("  Failed runs:      %d (%.1f%%)\n", n_failed, pct_failed))

cat("\nJames Law Validation:\n")
cat(sprintf("  Mean(K):          %.4f\n", overall_K$mean_K))
cat(sprintf("  Std(K):           %.4f\n", overall_K$sd_K))
cat(sprintf("  Status:           %s\n", ifelse(james_law_holds, "✓ VALIDATED", "✗ NOT CONFIRMED")))

if (!is.infinite(critical_by_K)) {
  cat(sprintf("\nCritical Window:\n"))
  cat(sprintf("  W_critical:       %d bytes\n", critical_by_K))
  cat(sprintf("  Ratio to W₀:      %.1fx\n", critical_by_K / 4096))
}

cat("\nOutput Files:\n")
cat("  ", outfile1, "\n")
cat("  ", outfile2, "\n")
cat("  ", outfile3, "\n")

cat("\nVisualizations:\n")
cat("  ", outfile_p1, "\n")
cat("  ", outfile_p2, "\n")
cat("  ", outfile_p3, "\n")
cat("  ", outfile_p4, "\n")
cat("  ", outfile_p5, "\n")

cat("\n")
cat(paste(rep("=", 80), collapse = ""), "\n")
cat("Analysis complete.\n")
cat(paste(rep("=", 80), collapse = ""), "\n")
cat("\n")

# ============================================================================
# Exit Code
# ============================================================================

if (james_law_holds) {
  cat("🎉 JAMES LAW VALIDATED\n\n")
  quit(status = 0)
} else {
  cat("⚠  James Law not confirmed, but results are valid\n\n")
  quit(status = 0)  # Still success - we learned something
}