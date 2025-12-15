#!/usr/bin/env Rscript
################################################################################
# REGENERATE WINDOW SCALING SVG CHARTS FROM HEARTBEAT DATA
# Processes individual heartbeat CSV files and generates 5 SVG charts
################################################################################

.libPaths('~/R/library')
suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(readr)
  library(scales)
})

# Output directory
SVG_DIR <- "/home/rajames/CLionProjects/StarForth/papers/LaTeX/figures"
HB_DIR <- "/home/rajames/CLionProjects/StarForth/experiments/window_scaling/results_run_one/raw/hb"

dir.create(SVG_DIR, showWarnings = FALSE, recursive = TRUE)

cat("================================================================================\n")
cat("  REGENERATING WINDOW SCALING CHARTS FROM HEARTBEAT DATA\n")
cat("================================================================================\n\n")

# Find all heartbeat CSV files
hb_files <- list.files(HB_DIR, pattern = "run-.*\\.csv", full.names = TRUE)

cat(sprintf("Found %d heartbeat files\n", length(hb_files)))

if (length(hb_files) == 0) {
  cat("ERROR: No heartbeat files found in", HB_DIR, "\n")
  quit(status = 1)
}

# Process all heartbeat files
cat("Processing heartbeat files...\n")

all_runs <- list()

for (hb_file in hb_files) {
  run_id <- gsub("run-|\\.csv", "", basename(hb_file))

  tryCatch({
    hb <- read_csv(hb_file, show_col_types = FALSE)

    if (nrow(hb) == 0) next

    # Extract window size from first row
    window_size <- as.numeric(hb$window_width[1])

    # Filter valid tick intervals (after tick 0)
    valid_data <- hb %>%
      filter(tick_number > 0, tick_interval_ns > 0, tick_interval_ns < 1e9)

    if (nrow(valid_data) == 0) next

    intervals <- valid_data$tick_interval_ns

    # Get K values
    k_vals <- valid_data %>%
      filter(K_approx > 0, K_approx < 10) %>%
      pull(K_approx)

    if (length(intervals) > 10 && length(k_vals) > 10) {
      # Calculate summary statistics
      run_summary <- data.frame(
        run_id = as.numeric(run_id),
        window_size = window_size,
        ticks = nrow(valid_data),
        mean_interval = mean(intervals),
        sd_interval = sd(intervals),
        cv_interval = sd(intervals) / mean(intervals) * 100,
        freq_hz = 1e9 / mean(intervals),
        mean_K = mean(k_vals),
        median_K = median(k_vals),
        sd_K = sd(k_vals),
        min_K = min(k_vals),
        max_K = max(k_vals),
        K_deviation = abs(mean(k_vals) - 1.0)
      )

      all_runs[[length(all_runs) + 1]] <- run_summary
    }
  }, error = function(e) {
    cat(sprintf("  Warning: Failed to process %s: %s\n", basename(hb_file), e$message))
  })
}

# Combine all runs
df <- bind_rows(all_runs)

cat(sprintf("Processed %d runs successfully\n\n", nrow(df)))

if (nrow(df) == 0) {
  cat("ERROR: No valid runs processed\n")
  quit(status = 1)
}

# Add performance metric and categories
df <- df %>%
  mutate(
    ns_per_word = mean_interval,  # Approximation
    window_category = case_when(
      window_size < 4096 ~ "subcritical",
      window_size == 4096 ~ "baseline",
      window_size <= 16384 ~ "stable",
      window_size <= 65536 ~ "critical",
      TRUE ~ "collapse"
    ),
    window_category = factor(window_category,
                            levels = c("subcritical", "baseline", "stable", "critical", "collapse")),
    window_size_kb = window_size / 1024
  )

cat("Window sizes tested:", paste(unique(sort(df$window_size)), collapse = ", "), "\n")
cat("Runs per window size:\n")
print(table(df$window_size))

cat("Generating SVG charts...\n")
svg_count <- 0

# ============================================================================
# Chart 1: K Distribution by Window Size
# ============================================================================

cat("  [1] K distribution by window size\n")

p1 <- ggplot(df, aes(x = factor(window_size), y = mean_K, fill = factor(window_size))) +
  geom_hline(yintercept = 1.0, linetype = "dashed", color = "red", size = 1) +
  geom_violin(alpha = 0.6) +
  geom_boxplot(width = 0.4, alpha = 0.8, outlier.alpha = 0.3) +
  labs(
    title = "James Law Validation: K Distribution by Window Size",
    subtitle = "Dashed red line: K = 1.0 (perfect fit)",
    x = "Window Size (bytes)",
    y = "K Statistic",
    fill = "Window"
  ) +
  theme_minimal() +
  theme(
    legend.position = "none",
    axis.text.x = element_text(angle = 45, hjust = 1)
  ) +
  scale_fill_viridis_d()

ggsave(file.path(SVG_DIR, "window_K_distribution_by_size.svg"), p1,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 2: K vs Window Size
# ============================================================================

cat("  [2] K vs window size\n")

james_law_summary <- df %>%
  group_by(window_size) %>%
  summarise(
    mean_K = mean(mean_K, na.rm = TRUE),
    sd_K = sd(mean_K, na.rm = TRUE),
    se_K = sd_K / sqrt(n()),
    n = n(),
    .groups = "drop"
  )

p2 <- ggplot(james_law_summary, aes(x = window_size, y = mean_K)) +
  geom_hline(yintercept = 1.0, linetype = "dashed", color = "red", size = 1) +
  geom_ribbon(aes(ymin = mean_K - se_K, ymax = mean_K + se_K),
              alpha = 0.2, fill = "steelblue") +
  geom_line(size = 1, color = "steelblue") +
  geom_point(size = 3, color = "steelblue") +
  scale_x_log10(labels = comma) +
  labs(
    title = "James Law: K Statistic vs Window Size",
    subtitle = "Gray ribbon: ±1 SE. Dashed red line: K = 1.0 (perfect fit)",
    x = "Window Size (bytes, log scale)",
    y = "Mean K Statistic"
  ) +
  theme_minimal()

ggsave(file.path(SVG_DIR, "window_K_vs_window_size.svg"), p2,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 3: Stability (CV) by Window Size
# ============================================================================

cat("  [3] Stability by window size\n")

stability_summary <- df %>%
  group_by(window_size) %>%
  summarise(
    mean_cv = mean(cv_interval, na.rm = TRUE),
    sd_cv = sd(cv_interval, na.rm = TRUE),
    n = n(),
    .groups = "drop"
  )

p3 <- ggplot(stability_summary, aes(x = window_size, y = mean_cv)) +
  geom_line(size = 1, color = "darkblue") +
  geom_point(size = 3, color = "darkblue") +
  scale_x_log10(labels = comma) +
  labs(
    title = "Stability: CV of Tick Intervals by Window Size",
    subtitle = "Lower CV = more stable performance",
    x = "Window Size (bytes, log scale)",
    y = "Mean CV (%)"
  ) +
  theme_minimal()

ggsave(file.path(SVG_DIR, "window_stability_cv.svg"), p3,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 4: K Deviation by Window Size
# ============================================================================

cat("  [4] K deviation by window size\n")

deviation_summary <- df %>%
  group_by(window_size) %>%
  summarise(
    mean_deviation = mean(K_deviation, na.rm = TRUE),
    sd_deviation = sd(K_deviation, na.rm = TRUE),
    .groups = "drop"
  )

p4 <- ggplot(deviation_summary, aes(x = window_size, y = mean_deviation)) +
  geom_hline(yintercept = 0, linetype = "dashed", color = "green", size = 1) +
  geom_line(size = 1, color = "darkred") +
  geom_point(size = 3, color = "darkred") +
  scale_x_log10(labels = comma) +
  labs(
    title = "James Law Deviation: |K - 1.0| by Window Size",
    subtitle = "Lower deviation = better fit to James Law. Green line: perfect fit",
    x = "Window Size (bytes, log scale)",
    y = "|K - 1.0|"
  ) +
  theme_minimal()

ggsave(file.path(SVG_DIR, "window_K_deviation.svg"), p4,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Chart 5: Frequency (ω₀) vs Window Size
# ============================================================================

cat("  [5] Frequency vs window size\n")

freq_summary <- df %>%
  group_by(window_size) %>%
  summarise(
    mean_freq = mean(freq_hz, na.rm = TRUE),
    sd_freq = sd(freq_hz, na.rm = TRUE),
    cv_freq = sd_freq / mean_freq * 100,
    .groups = "drop"
  )

p5 <- ggplot(freq_summary, aes(x = window_size, y = mean_freq)) +
  geom_line(size = 1, color = "purple") +
  geom_point(size = 3, color = "purple") +
  scale_x_log10(labels = comma) +
  labs(
    title = "Fundamental Frequency (ω₀) vs Window Size",
    subtitle = "Testing invariance of ω₀ across window sizes",
    x = "Window Size (bytes, log scale)",
    y = "Mean Frequency (Hz)"
  ) +
  theme_minimal()

ggsave(file.path(SVG_DIR, "window_frequency_vs_size.svg"), p5,
       width = 12, height = 8, device = "svg")
svg_count <- svg_count + 1

# ============================================================================
# Summary
# ============================================================================

cat("\n================================================================================\n")
cat(sprintf("  REGENERATION COMPLETE: %d WINDOW SCALING SVG CHARTS GENERATED\n", svg_count))
cat("================================================================================\n")
cat(sprintf("\nOutput directory: %s\n", SVG_DIR))
cat("\nCharts generated:\n")
cat("  1. window_K_distribution_by_size.svg - K distribution by window size\n")
cat("  2. window_K_vs_window_size.svg - K statistic vs window size\n")
cat("  3. window_stability_cv.svg - Stability (CV) vs window size\n")
cat("  4. window_K_deviation.svg - James Law deviation by window size\n")
cat("  5. window_frequency_vs_size.svg - Fundamental frequency vs window size\n")
cat("\nAll charts regenerated from heartbeat CSV data (NO PNG conversion)\n")
cat("\n")
