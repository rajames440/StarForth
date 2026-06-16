#!/usr/bin/env Rscript

# L8 Attractor Heartbeat Analysis - SVG Output
# Publication-quality statistical analysis

library(dplyr)
library(readr)
library(ggplot2)
library(tidyr)

# Create output directories
dir.create("results_20251209_145907/analysis_output", showWarnings = FALSE)
dir.create("analysis_output/charts", showWarnings = FALSE)
dir.create("analysis_output/tables", showWarnings = FALSE)
dir.create("analysis_output/report", showWarnings = FALSE)

cat("═══════════════════════════════════════════════════════════════\n")
cat("    L8 Attractor Heartbeat Analysis (SVG Output)\n")
cat("═══════════════════════════════════════════════════════════════\n\n")

# 1. LOAD DATA
cat("Loading datasets...\n")
results_dir <- "results_20251209_145907"

heartbeat <- read_csv(file.path(results_dir, "heartbeat_all.csv"), 
                      show_col_types = FALSE)
run_matrix <- read_csv(file.path(results_dir, "l8_attractor_run_matrix.csv"),
                       show_col_types = FALSE)
workload_map <- read_csv(file.path(results_dir, "workload_mapping.csv"),
                         show_col_types = FALSE)

cat(sprintf("  Heartbeat rows: %d\n", nrow(heartbeat)))
cat(sprintf("  Runs: %d\n", nrow(run_matrix)))
cat(sprintf("  Mapping entries: %d\n", nrow(workload_map)))

# 2. MERGE DATA
cat("\nMerging datasets...\n")

# Add row numbers
heartbeat$row_num <- 1:nrow(heartbeat)

# Merge by row range
heartbeat_merged <- heartbeat %>%
  left_join(
    workload_map %>% 
      select(run_id, init_script, replicate, hb_start_row, hb_end_row),
    by = join_by(row_num >= hb_start_row, row_num <= hb_end_row)
  ) %>%
  filter(!is.na(run_id))

cat(sprintf("  Merged rows: %d\n", nrow(heartbeat_merged)))

# 3. PER-RUN SUMMARY
cat("\nComputing per-run summary statistics...\n")

per_run_summary <- heartbeat_merged %>%
  group_by(run_id, init_script, replicate) %>%
  summarise(
    tick_count = n(),
    duration_ns = max(elapsed_ns) - min(elapsed_ns),
    mean_interval_ns = mean(tick_interval_ns, na.rm = TRUE),
    sd_interval_ns = sd(tick_interval_ns, na.rm = TRUE),
    cv_interval = sd_interval_ns / mean_interval_ns,
    mean_heat = mean(avg_word_heat, na.rm = TRUE),
    max_heat = max(avg_word_heat, na.rm = TRUE),
    mean_hot_words = mean(hot_word_count, na.rm = TRUE),
    max_hot_words = max(hot_word_count, na.rm = TRUE),
    mean_jitter_ns = mean(estimated_jitter_ns, na.rm = TRUE),
    sd_jitter_ns = sd(estimated_jitter_ns, na.rm = TRUE),
    .groups = "drop"
  )

write_csv(per_run_summary, "analysis_output/tables/per_run_summary.csv")
cat("  Saved: analysis_output/tables/per_run_summary.csv\n")

# 4. WORKLOAD SUMMARY
cat("\nComputing workload-level statistics...\n")

workload_summary <- per_run_summary %>%
  group_by(init_script) %>%
  summarise(
    n_runs = n(),
    mean_tick_count = mean(tick_count),
    sd_tick_count = sd(tick_count),
    mean_duration_ns = mean(duration_ns),
    sd_duration_ns = sd(duration_ns),
    mean_heat = mean(mean_heat),
    sd_heat = sd(mean_heat),
    mean_jitter = mean(mean_jitter_ns),
    sd_jitter = sd(mean_jitter_ns),
    .groups = "drop"
  )

write_csv(workload_summary, "analysis_output/tables/workload_summary.csv")
cat("  Saved: analysis_output/tables/workload_summary.csv\n\n")
print(workload_summary)

# 5. ANOVA
cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    ANOVA: Tick Count ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
anova_ticks <- aov(tick_count ~ init_script, data = per_run_summary)
print(summary(anova_ticks))

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    ANOVA: Duration ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
anova_duration <- aov(duration_ns ~ init_script, data = per_run_summary)
print(summary(anova_duration))

# Save ANOVA
sink("analysis_output/tables/anova_results.txt")
cat("ANOVA: Tick Count ~ Workload\n═══════════════════════════════════════════════════════════════\n")
print(summary(anova_ticks))
cat("\n\nANOVA: Duration ~ Workload\n═══════════════════════════════════════════════════════════════\n")
print(summary(anova_duration))
sink()

cat("\n\nGenerating SVG charts...\n")

# 6. CHART 1: Tick Count by Workload (Boxplot)
cat("  → Tick count boxplot...\n")
p1 <- ggplot(per_run_summary, aes(x = init_script, y = tick_count, fill = init_script)) +
  geom_boxplot(alpha = 0.7) +
  geom_jitter(width = 0.2, alpha = 0.3) +
  labs(title = "Tick Count by Workload",
       x = "Workload", y = "Tick Count") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1),
        legend.position = "none")
ggsave("analysis_output/charts/tick_count_boxplot.svg", p1, width = 10, height = 6)

# 7. CHART 2: Duration vs Tick Count (Scatter)
cat("  → Duration vs tick count scatter...\n")
p2 <- ggplot(per_run_summary, aes(x = tick_count, y = duration_ns / 1e9, color = init_script)) +
  geom_point(alpha = 0.6, size = 2) +
  geom_smooth(method = "lm", se = FALSE, linewidth = 0.5) +
  labs(title = "Run Duration vs Tick Count",
       x = "Tick Count", y = "Duration (seconds)",
       color = "Workload") +
  theme_minimal()
ggsave("analysis_output/charts/duration_vs_ticks.svg", p2, width = 10, height = 6)

# 8. CHART 3: Tick Interval Distribution (Histogram)
cat("  → Tick interval histograms...\n")
p3 <- ggplot(heartbeat_merged, aes(x = tick_interval_ns / 1000)) +
  geom_histogram(bins = 50, fill = "steelblue", alpha = 0.7) +
  labs(title = "Tick Interval Distribution (All Runs)",
       x = "Tick Interval (μs)", y = "Count") +
  theme_minimal()
ggsave("analysis_output/charts/interval_hist_all.svg", p3, width = 10, height = 6)

p4 <- ggplot(heartbeat_merged, aes(x = tick_interval_ns / 1000, fill = init_script)) +
  geom_histogram(bins = 30, alpha = 0.7) +
  facet_wrap(~ init_script, ncol = 2) +
  labs(title = "Tick Interval Distribution by Workload",
       x = "Tick Interval (μs)", y = "Count") +
  theme_minimal() +
  theme(legend.position = "none")
ggsave("analysis_output/charts/interval_hist_by_workload.svg", p4, width = 12, height = 8)

# 9. CHART 4: Jitter Distribution
cat("  → Jitter histograms...\n")
p5 <- ggplot(heartbeat_merged, aes(x = estimated_jitter_ns / 1000, fill = init_script)) +
  geom_histogram(bins = 30, alpha = 0.7) +
  facet_wrap(~ init_script, ncol = 2) +
  labs(title = "Jitter Distribution by Workload",
       x = "Jitter (μs)", y = "Count") +
  theme_minimal() +
  theme(legend.position = "none")
ggsave("analysis_output/charts/jitter_hist_by_workload.svg", p5, width = 12, height = 8)

# 10. CHART 5-7: Time Series for Selected Runs
cat("  → Time series plots for selected runs...\n")

# Select 3 representative runs (one from each major workload type)
selected_runs <- per_run_summary %>%
  filter(init_script %in% c('"init-l8-diverse.4th"', '"init-l8-omni.4th"', '"init-l8-stable.4th"')) %>%
  group_by(init_script) %>%
  slice(1) %>%
  pull(run_id)

for (rid in selected_runs) {
  run_data <- heartbeat_merged %>% filter(run_id == rid)
  workload_name <- unique(run_data$init_script)
  
  # Heat over time
  p_heat <- ggplot(run_data, aes(x = elapsed_ns / 1e9, y = avg_word_heat)) +
    geom_line(color = "darkred", linewidth = 0.8) +
    labs(title = sprintf("Heat Evolution - %s (Run %d)", workload_name, rid),
         x = "Elapsed Time (s)", y = "Avg Word Heat") +
    theme_minimal()
  ggsave(sprintf("analysis_output/charts/heat_timeseries_run%d.svg", rid), p_heat, width = 10, height = 5)
  
  # Hot words over time
  p_hotw <- ggplot(run_data, aes(x = elapsed_ns / 1e9, y = hot_word_count)) +
    geom_line(color = "darkorange", linewidth = 0.8) +
    labs(title = sprintf("Hot Words - %s (Run %d)", workload_name, rid),
         x = "Elapsed Time (s)", y = "Hot Word Count") +
    theme_minimal()
  ggsave(sprintf("analysis_output/charts/hotwords_timeseries_run%d.svg", rid), p_hotw, width = 10, height = 5)
  
  # Jitter over time
  p_jit <- ggplot(run_data, aes(x = elapsed_ns / 1e9, y = estimated_jitter_ns / 1000)) +
    geom_line(color = "steelblue", linewidth = 0.8) +
    labs(title = sprintf("Jitter Evolution - %s (Run %d)", workload_name, rid),
         x = "Elapsed Time (s)", y = "Jitter (μs)") +
    theme_minimal()
  ggsave(sprintf("analysis_output/charts/jitter_timeseries_run%d.svg", rid), p_jit, width = 10, height = 5)
}

# 11. CHART 8: Heat vs Jitter (Scatter)
cat("  → Heat vs jitter scatter...\n")
p8 <- ggplot(heartbeat_merged, aes(x = avg_word_heat, y = estimated_jitter_ns / 1000, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  labs(title = "Heat vs Jitter Relationship",
       x = "Avg Word Heat", y = "Jitter (μs)",
       color = "Workload") +
  theme_minimal()
ggsave("analysis_output/charts/heat_vs_jitter.svg", p8, width = 10, height = 6)

# 12. CHART 9: Hot Words vs Interval (Scatter)
cat("  → Hot words vs interval scatter...\n")
p9 <- ggplot(heartbeat_merged, aes(x = hot_word_count, y = tick_interval_ns / 1000, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  labs(title = "Hot Words vs Tick Interval",
       x = "Hot Word Count", y = "Tick Interval (μs)",
       color = "Workload") +
  theme_minimal()
ggsave("analysis_output/charts/hotwords_vs_interval.svg", p9, width = 10, height = 6)

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    Analysis Complete!\n")
cat("═══════════════════════════════════════════════════════════════\n")
cat("\nOutputs:\n")
cat("  - analysis_output/charts/          (9 SVG charts)\n")
cat("  - analysis_output/tables/          (3 CSV + ANOVA results)\n")
cat("\n")

