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
