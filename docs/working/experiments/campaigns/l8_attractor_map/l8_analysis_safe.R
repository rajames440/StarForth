#!/usr/bin/env Rscript
library(readr)
library(ggplot2)

dir.create("results_20251209_145907/analysis_output", showWarnings = FALSE, recursive = TRUE)
dir.create("analysis_output/charts", showWarnings = FALSE)
dir.create("analysis_output/tables", showWarnings = FALSE)

cat("═══════════════════════════════════════════════════════════════\n")
cat("    L8 Attractor Heartbeat Analysis\n")
cat("═══════════════════════════════════════════════════════════════\n\n")

results_dir <- "results_20251209_145907"

cat("Loading datasets...\n")
heartbeat <- as.data.frame(read_csv(file.path(results_dir, "heartbeat_all.csv"), show_col_types = FALSE))
workload_map <- as.data.frame(read_csv(file.path(results_dir, "workload_mapping.csv"), show_col_types = FALSE))

cat(sprintf("  Heartbeat rows (incl header): %d\n", nrow(heartbeat)))
cat(sprintf("  Mapping entries: %d\n", nrow(workload_map)))

# Merge data - row-by-row matching with bounds checking
cat("\nMerging datasets...\n")
heartbeat$row_num <- 1:nrow(heartbeat)
heartbeat$run_id <- NA_integer_
heartbeat$init_script <- NA_character_
heartbeat$replicate <- NA_integer_

max_row <- nrow(heartbeat)

for (i in 1:nrow(workload_map)) {
  start_row <- workload_map$hb_start_row[i]
  end_row <- min(workload_map$hb_end_row[i], max_row)  # Bounds check
  
  if (start_row <= max_row) {
    heartbeat$run_id[start_row:end_row] <- workload_map$run_id[i]
    heartbeat$init_script[start_row:end_row] <- workload_map$init_script[i]
    heartbeat$replicate[start_row:end_row] <- workload_map$replicate[i]
  }
}

heartbeat_merged <- heartbeat[!is.na(heartbeat$run_id), ]
cat(sprintf("  Merged rows: %d\n", nrow(heartbeat_merged)))
cat(sprintf("  Runs represented: %d\n", length(unique(heartbeat_merged$run_id))))

# Per-run summary
cat("\nComputing per-run statistics...\n")

compute_stats <- function(df) {
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

per_run_list <- lapply(split(heartbeat_merged, heartbeat_merged$run_id), compute_stats)
per_run_summary <- do.call(rbind, per_run_list)
per_run_summary$run_id <- as.integer(rownames(per_run_summary))
per_run_summary$init_script <- sapply(split(heartbeat_merged$init_script, heartbeat_merged$run_id), `[`, 1)
per_run_summary$replicate <- sapply(split(heartbeat_merged$replicate, heartbeat_merged$run_id), `[`, 1)
per_run_summary$cv_interval <- per_run_summary$sd_interval_ns / per_run_summary$mean_interval_ns

write.csv(per_run_summary, "analysis_output/tables/per_run_summary.csv", row.names = FALSE)
cat("  Saved: per_run_summary.csv\n")

# Workload summary
cat("\nComputing workload statistics...\n")
ws <- aggregate(cbind(tick_count, duration_ns, mean_heat, mean_jitter_ns) ~ init_script,
                data = per_run_summary,
                FUN = function(x) c(m = mean(x), s = sd(x), n = length(x)))

workload_summary <- data.frame(
  init_script = ws$init_script,
  n_runs = ws$tick_count[,3],
  mean_tick_count = ws$tick_count[,1],
  sd_tick_count = ws$tick_count[,2],
  mean_duration_ns = ws$duration_ns[,1],
  sd_duration_ns = ws$duration_ns[,2],
  mean_heat = ws$mean_heat[,1],
  sd_heat = ws$mean_heat[,2],
  mean_jitter = ws$mean_jitter_ns[,1],
  sd_jitter = ws$mean_jitter_ns[,2]
)

write.csv(workload_summary, "analysis_output/tables/workload_summary.csv", row.names = FALSE)
cat("  Saved: workload_summary.csv\n\n")
print(workload_summary)

# ANOVA
cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    ANOVA: Tick Count ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
anova_t <- aov(tick_count ~ init_script, per_run_summary)
print(summary(anova_t))

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    ANOVA: Duration ~ Workload\n")
cat("═══════════════════════════════════════════════════════════════\n")
anova_d <- aov(duration_ns ~ init_script, per_run_summary)
print(summary(anova_d))

sink("analysis_output/tables/anova_results.txt")
cat("ANOVA: Tick Count ~ Workload\n═══════════════════════════════════════════════════════════════\n")
print(summary(anova_t))
cat("\n\nANOVA: Duration ~ Workload\n═══════════════════════════════════════════════════════════════\n")
print(summary(anova_d))
sink()

cat("\n\nGenerating SVG charts...\n")

# Charts
cat("  [1/10] Tick count boxplot\n")
svg("analysis_output/charts/tick_count_boxplot.svg", width = 10, height = 6)
print(ggplot(per_run_summary, aes(x = init_script, y = tick_count, fill = init_script)) +
  geom_boxplot(alpha = 0.7) + geom_jitter(width = 0.2, alpha = 0.3) +
  labs(title = "Tick Count by Workload", x = "Workload", y = "Tick Count") +
  theme_minimal() + theme(axis.text.x = element_text(angle = 45, hjust = 1), legend.position = "none"))
dev.off()

cat("  [2/10] Duration vs ticks\n")
svg("analysis_output/charts/duration_vs_ticks.svg", width = 10, height = 6)
print(ggplot(per_run_summary, aes(x = tick_count, y = duration_ns/1e9, color = init_script)) +
  geom_point(alpha = 0.6, size = 2) + geom_smooth(method = "lm", se = FALSE) +
  labs(title = "Duration vs Tick Count", x = "Ticks", y = "Duration (s)", color = "Workload") +
  theme_minimal())
dev.off()

cat("  [3/10] Interval histogram (all)\n")
svg("analysis_output/charts/interval_hist_all.svg", width = 10, height = 6)
print(ggplot(heartbeat_merged, aes(x = tick_interval_ns/1000)) +
  geom_histogram(bins = 50, fill = "steelblue", alpha = 0.7) +
  labs(title = "Tick Interval Distribution", x = "Interval (μs)", y = "Count") +
  theme_minimal())
dev.off()

cat("  [4/10] Interval by workload\n")
svg("analysis_output/charts/interval_hist_by_workload.svg", width = 12, height = 8)
print(ggplot(heartbeat_merged, aes(x = tick_interval_ns/1000, fill = init_script)) +
  geom_histogram(bins = 30) + facet_wrap(~ init_script, ncol = 2) +
  labs(title = "Tick Interval by Workload", x = "Interval (μs)", y = "Count") +
  theme_minimal() + theme(legend.position = "none"))
dev.off()

cat("  [5/10] Jitter by workload\n")
svg("analysis_output/charts/jitter_hist_by_workload.svg", width = 12, height = 8)
print(ggplot(heartbeat_merged, aes(x = estimated_jitter_ns/1000, fill = init_script)) +
  geom_histogram(bins = 30) + facet_wrap(~ init_script, ncol = 2) +
  labs(title = "Jitter Distribution", x = "Jitter (μs)", y = "Count") +
  theme_minimal() + theme(legend.position = "none"))
dev.off()

# Time series (3 runs)
cat("  [6-8] Time series plots\n")
sel <- c(
  per_run_summary$run_id[per_run_summary$init_script == '"init-l8-diverse.4th"'][1],
  per_run_summary$run_id[per_run_summary$init_script == '"init-l8-omni.4th"'][1],
  per_run_summary$run_id[per_run_summary$init_script == '"init-l8-stable.4th"'][1]
)

for (rid in sel) {
  rd <- heartbeat_merged[heartbeat_merged$run_id == rid,]
  wl <- unique(rd$init_script)
  
  svg(sprintf("analysis_output/charts/heat_timeseries_run%d.svg", rid), width = 10, height = 5)
  print(ggplot(rd, aes(x = elapsed_ns/1e9, y = avg_word_heat)) +
    geom_line(color = "darkred") +
    labs(title = sprintf("Heat - %s (Run %d)", wl, rid), x = "Time (s)", y = "Heat") +
    theme_minimal())
  dev.off()
  
  svg(sprintf("analysis_output/charts/hotwords_timeseries_run%d.svg", rid), width = 10, height = 5)
  print(ggplot(rd, aes(x = elapsed_ns/1e9, y = hot_word_count)) +
    geom_line(color = "darkorange") +
    labs(title = sprintf("Hot Words - %s (Run %d)", wl, rid), x = "Time (s)", y = "Count") +
    theme_minimal())
  dev.off()
  
  svg(sprintf("analysis_output/charts/jitter_timeseries_run%d.svg", rid), width = 10, height = 5)
  print(ggplot(rd, aes(x = elapsed_ns/1e9, y = estimated_jitter_ns/1000)) +
    geom_line(color = "steelblue") +
    labs(title = sprintf("Jitter - %s (Run %d)", wl, rid), x = "Time (s)", y = "Jitter (μs)") +
    theme_minimal())
  dev.off()
}

cat("  [9/10] Heat vs jitter\n")
svg("analysis_output/charts/heat_vs_jitter.svg", width = 10, height = 6)
print(ggplot(heartbeat_merged, aes(x = avg_word_heat, y = estimated_jitter_ns/1000, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  labs(title = "Heat vs Jitter", x = "Heat", y = "Jitter (μs)", color = "Workload") +
  theme_minimal())
dev.off()

cat("  [10/10] Hot words vs interval\n")
svg("analysis_output/charts/hotwords_vs_interval.svg", width = 10, height = 6)
print(ggplot(heartbeat_merged, aes(x = hot_word_count, y = tick_interval_ns/1000, color = init_script)) +
  geom_point(alpha = 0.2, size = 0.5) +
  labs(title = "Hot Words vs Interval", x = "Hot Words", y = "Interval (μs)", color = "Workload") +
  theme_minimal())
dev.off()

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("    Analysis Complete!\n")
cat("═══════════════════════════════════════════════════════════════\n")
cat("\nOutputs:\n")
cat("  - analysis_output/charts/      (13 SVG files)\n")
cat("  - analysis_output/tables/      (3 CSV + ANOVA)\n\n")

