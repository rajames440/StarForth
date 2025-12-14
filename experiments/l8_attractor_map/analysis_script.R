#!/usr/bin/env Rscript

# L8 Attractor Heartbeat Analysis
# Rigorous statistical analysis with publication-quality outputs

library(tidyverse)
library(ggplot2)
library(viridis)

# Create output directories
dir.create("results_20251209_145907/analysis_output", showWarnings = FALSE)
dir.create("analysis_output/charts", showWarnings = FALSE)
dir.create("analysis_output/tables", showWarnings = FALSE)
dir.create("analysis_output/report", showWarnings = FALSE)

cat("═══════════════════════════════════════════════════════════════\n")
cat("    L8 Attractor Heartbeat Analysis\n")
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

# 2. MERGE DATA - Assign run_id to each heartbeat tick
cat("\nMerging datasets...\n")

# Create row numbers for heartbeat data (1-indexed)
heartbeat$row_num <- 1:nrow(heartbeat)

# Join: for each heartbeat row, find which run it belongs to
heartbeat_merged <- heartbeat %>%
  left_join(
    workload_map %>% 
      select(run_id, init_script, replicate, hb_start_row, hb_end_row),
    by = join_by(row_num >= hb_start_row, row_num <= hb_end_row)
  ) %>%
  filter(!is.na(run_id))  # Remove header row

cat(sprintf("  Merged rows: %d\n", nrow(heartbeat_merged)))

# 3. PER-RUN SUMMARY STATISTICS
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
    window_width_constant = length(unique(window_width)) == 1,
    .groups = "drop"
  )

# Save per-run summary
write_csv(per_run_summary, "analysis_output/tables/per_run_summary.csv")
cat(sprintf("  Saved: analysis_output/tables/per_run_summary.csv\n"))

# 4. WORKLOAD-LEVEL SUMMARY
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
cat(sprintf("  Saved: analysis_output/tables/workload_summary.csv\n"))

print(workload_summary)

# 5. ANOVA TESTS
cat("\n\n═══════════════════════════════════════════════════════════════\n")
cat("    ANOVA: Tick Count ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")

anova_ticks <- aov(tick_count ~ init_script, data = per_run_summary)
print(summary(anova_ticks))

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    ANOVA: Duration ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")

anova_duration <- aov(duration_ns ~ init_script, data = per_run_summary)
print(summary(anova_duration))

# Save ANOVA results
sink("analysis_output/tables/anova_results.txt")
cat("ANOVA: Tick Count ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
print(summary(anova_ticks))
cat("\n\nANOVA: Duration ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
print(summary(anova_duration))
sink()

cat("\nAll analyses complete. Generating charts...\n\n")
