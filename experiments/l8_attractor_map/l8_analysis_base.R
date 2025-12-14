#!/usr/bin/env Rscript

# L8 Attractor Analysis - Base R + ggplot2 only
library(readr)
library(ggplot2)

# Create output directories
dir.create("results_20251209_145907/analysis_output", showWarnings = FALSE, recursive = TRUE)
dir.create("analysis_output/charts", showWarnings = FALSE)
dir.create("analysis_output/tables", showWarnings = FALSE)

cat("═══════════════════════════════════════════════════════════════\n")
cat("    L8 Attractor Heartbeat Analysis\n")
cat("═══════════════════════════════════════════════════════════════\n\n")

# Load data
cat("Loading datasets...\n")
results_dir <- "results_20251209_145907"

heartbeat <- read_csv(file.path(results_dir, "heartbeat_all.csv"), show_col_types = FALSE)
workload_map <- read_csv(file.path(results_dir, "workload_mapping.csv"), show_col_types = FALSE)

cat(sprintf("  Heartbeat rows: %d\n", nrow(heartbeat)))
cat(sprintf("  Mapping entries: %d\n", nrow(workload_map)))

# Merge data manually
cat("\nMerging datasets...\n")
heartbeat$row_num <- 1:nrow(heartbeat)

# Assign run_id to each heartbeat row
for (i in 1:nrow(workload_map)) {
  start_row <- workload_map$hb_start_row[i]
  end_row <- workload_map$hb_end_row[i]
  
  heartbeat$run_id[start_row:end_row] <- workload_map$run_id[i]
  heartbeat$init_script[start_row:end_row] <- workload_map$init_script[i]
  heartbeat$replicate[start_row:end_row] <- workload_map$replicate[i]
}

heartbeat_merged <- heartbeat[!is.na(heartbeat$run_id), ]
cat(sprintf("  Merged rows: %d\n", nrow(heartbeat_merged)))

# Per-run summary
cat("\nComputing per-run statistics...\n")

compute_run_stats <- function(df) {
  data.frame(
    tick_count = nrow(df),
    duration_ns = max(df$elapsed_ns) - min(df$elapsed_ns),
    mean_interval_ns = mean(df$tick_interval_ns, na.rm = TRUE),
    sd_interval_ns = sd(df$tick_interval_ns, na.rm = TRUE),
    mean_heat = mean(df$avg_word_heat, na.rm = TRUE),
    max_heat = max(df$avg_word_heat, na.rm = TRUE),
    mean_hot_words = mean(df$hot_word_count, na.rm = TRUE),
    max_hot_words = max(df$hot_word_count, na.rm = TRUE),
    mean_jitter_ns = mean(df$estimated_jitter_ns, na.rm = TRUE),
    sd_jitter_ns = sd(df$estimated_jitter_ns, na.rm = TRUE)
  )
}

per_run_list <- lapply(split(heartbeat_merged, heartbeat_merged$run_id), compute_run_stats)
per_run_summary <- do.call(rbind, per_run_list)
per_run_summary$run_id <- as.integer(rownames(per_run_summary))
per_run_summary$init_script <- sapply(split(heartbeat_merged$init_script, heartbeat_merged$run_id), function(x) x[1])
per_run_summary$replicate <- sapply(split(heartbeat_merged$replicate, heartbeat_merged$run_id), function(x) x[1])
per_run_summary$cv_interval <- per_run_summary$sd_interval_ns / per_run_summary$mean_interval_ns

write.csv(per_run_summary, "analysis_output/tables/per_run_summary.csv", row.names = FALSE)
cat("  Saved: analysis_output/tables/per_run_summary.csv\n")

# Workload summary
cat("\nComputing workload statistics...\n")
workload_stats <- aggregate(
  cbind(tick_count, duration_ns, mean_heat, mean_jitter_ns) ~ init_script,
  data = per_run_summary,
  FUN = function(x) c(mean = mean(x), sd = sd(x), n = length(x))
)

workload_summary <- data.frame(
  init_script = workload_stats$init_script,
  n_runs = workload_stats$tick_count[, 3],
  mean_tick_count = workload_stats$tick_count[, 1],
  sd_tick_count = workload_stats$tick_count[, 2],
  mean_duration_ns = workload_stats$duration_ns[, 1],
  sd_duration_ns = workload_stats$duration_ns[, 2],
  mean_heat = workload_stats$mean_heat[, 1],
  sd_heat = workload_stats$mean_heat[, 2],
  mean_jitter = workload_stats$mean_jitter_ns[, 1],
  sd_jitter = workload_stats$mean_jitter_ns[, 2]
)

write.csv(workload_summary, "analysis_output/tables/workload_summary.csv", row.names = FALSE)
cat("  Saved: analysis_output/tables/workload_summary.csv\n\n")
print(workload_summary)

# ANOVA
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
cat("ANOVA: Tick Count ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
print(summary(anova_ticks))
cat("\n\nANOVA: Duration ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
print(summary(anova_duration))
sink()

cat("\n\nGenerating SVG charts...\n")

# Chart 1: Tick Count Boxplot
cat("  → Tick count boxplot\n")
svg("analysis_output/charts/tick_count_boxplot.svg", width = 10, height = 6)
p1 <- ggplot(per_run_summary, aes(x = init_script, y = tick_count, fill = init_script)) +
  geom_boxplot(alpha = 0.7) +
  geom_jitter(width = 0.2, alpha = 0.3) +
  labs(title = "Tick Count by Workload", x = "Workload", y = "Tick Count") +
  theme_minimal() +
  theme(axis.text.x = element_text(angle = 45, hjust = 1), legend.position = "none")
print(p1)
dev.off()

# Chart 2: Duration vs Ticks
cat("  → Duration vs ticks scatter\n")
svg("analysis_output/charts/duration_vs_ticks.svg", width = 10, height = 6)
p2 <- ggplot(per_run_summary, aes(x = tick_count, y = duration_ns / 1e9, color = init_script)) +
  geom_point(alpha = 0.6, size = 2) +
  geom_smooth(method = "lm", se = FALSE, linewidth = 0.5) +
  labs(title = "Run Duration vs Tick Count", x = "Tick Count", y = "Duration (s)", color = "Workload") +
  theme_minimal()
print(p2)
dev.off()

# Chart 3: Tick Interval Histogram (all runs)
cat("  → Interval histogram (all)\n")
svg("analysis_output/charts/interval_hist_all.svg", width = 10, height = 6)
p3 <- ggplot(heartbeat_merged, aes(x = tick_interval_ns / 1000)) +
  geom_histogram(bins = 50, fill = "steelblue", alpha = 0.7) +
  labs(title = "Tick Interval Distribution", x = "Interval (μs)", y = "Count") +
  theme_minimal()
print(p3)
dev.off()

# Chart 4: Interval by Workload (faceted)
cat("  → Interval histograms by workload\n")
svg("analysis_output/charts/interval_hist_by_workload.svg", width = 12, height = 8)
p4 <- ggplot(heartbeat_merged, aes(x = tick_interval_ns / 1000, fill = init_script)) +
  geom_histogram(bins = 30, alpha = 0.7) +
  facet_wrap(~ init_script, ncol = 2) +
  labs(title = "Tick Interval by Workload", x = "Interval (μs)", y = "Count") +
  theme_minimal() +
  theme(legend.position = "none")
print(p4)
dev.off()

# Chart 5: Jitter by Workload
cat("  → Jitter histograms\n")
svg("analysis_output/charts/jitter_hist_by_workload.svg", width = 12, height = 8)
p5 <- ggplot(heartbeat_merged, aes(x = estimated_jitter_ns / 1000, fill = init_script)) +
  geom_histogram(bins = 30, alpha = 0.7) +
  facet_wrap(~ init_script, ncol = 2) +
  labs(title = "Jitter Distribution by Workload", x = "Jitter (μs)", y = "Count") +
  theme_minimal() +
  theme(legend.position = "none")
print(p5)
dev.off()

# Chart 6-8: Time series for selected runs
cat("  → Time series plots\n")
selected_runs <- c(
  per_run_summary$run_id[per_run_summary$init_script == '"init-l8-diverse.4th"'][1],
  per_run_summary$run_id[per_run_summary$init_script == '"init-l8-omni.4th"'][1],
  per_run_summary$run_id[per_run_summary$init_script == '"init-l8-stable.4th"'][1]
)

for (rid in selected_runs) {
  run_data <- heartbeat_merged[heartbeat_merged$run_id == rid, ]
  workload <- unique(run_data$init_script)
  
  # Heat
  svg(sprintf("analysis_output/charts/heat_timeseries_run%d.svg", rid), width = 10, height = 5)
  p <- ggplot(run_data, aes(x = elapsed_ns / 1e9, y = avg_word_heat)) +
    geom_line(color = "darkred") +
    labs(title = sprintf("Heat - %s (Run %d)", workload, rid), x = "Time (s)", y = "Heat") +
    theme_minimal()
  print(p)
  dev.off()
  
  # Hot words
  svg(sprintf("analysis_output/charts/hotwords_timeseries_run%d.svg", rid), width = 10, height = 5)
  p <- ggplot(run_data, aes(x = elapsed_ns / 1e9, y = hot_word_count)) +
    geom_line(color = "darkorange") +
    labs(title = sprintf("Hot Words - %s (Run %d)", workload, rid), x = "Time (s)", y = "Count") +
    theme_minimal()
  print(p)
  dev.off()
  
  # Jitter
  svg(sprintf("analysis_output/charts/jitter_timeseries_run%d.svg", rid), width = 10, height = 5)
  p <- ggplot(run_data, aes(x = elapsed_ns / 1e9, y = estimated_jitter_ns / 1000)) +
    geom_line(color = "steelblue") +
    labs(title = sprintf("Jitter - %s (Run %d)", workload, rid), x = "Time (s)", y = "Jitter (μs)") +
    theme_minimal()
  print(p)
  dev.off()
}

# Chart 9: Heat vs Jitter
cat("  → Heat vs jitter scatter\n")
svg("analysis_output/charts/heat_vs_jitter.svg", width = 10, height = 6)
p9 <- ggplot(heartbeat_merged, aes(x = avg_word_heat, y = estimated_jitter_ns / 1000, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  labs(title = "Heat vs Jitter", x = "Heat", y = "Jitter (μs)", color = "Workload") +
  theme_minimal()
print(p9)
dev.off()

# Chart 10: Hot Words vs Interval
cat("  → Hot words vs interval scatter\n")
svg("analysis_output/charts/hotwords_vs_interval.svg", width = 10, height = 6)
p10 <- ggplot(heartbeat_merged, aes(x = hot_word_count, y = tick_interval_ns / 1000, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  labs(title = "Hot Words vs Tick Interval", x = "Hot Words", y = "Interval (μs)", color = "Workload") +
  theme_minimal()
print(p10)
dev.off()

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    Analysis Complete!\n")
cat("═══════════════════════════════════════════════════════════════\n")
cat("\nOutputs:\n")
cat("  - analysis_output/charts/      (13 SVG files)\n")
cat("  - analysis_output/tables/      (3 CSV + ANOVA)\n\n")

