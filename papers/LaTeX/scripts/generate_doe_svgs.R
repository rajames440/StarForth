#!/usr/bin/env Rscript

############################################################
# StarForth DOE — SVG Generation Script
# Generates standalone SVG figures from experimental data
############################################################

suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
})

# Set working directory to DoE results
setwd("/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_30reps_RAJ_2025.11.22_1700hrs")

# Output directory for SVGs
svg_dir <- "/home/rajames/CLionProjects/StarForth/papers/LaTeX/figures"
dir.create(svg_dir, showWarnings = FALSE, recursive = TRUE)

cat("═══════════════════════════════════════════════════════════════\n")
cat("    StarForth DOE SVG Generation\n")
cat("═══════════════════════════════════════════════════════════════\n\n")

# Modified save function for SVG output
save_plot_svg <- function(name, plot_obj, width = 10, height = 6) {
  file <- file.path(svg_dir, paste0(name, ".svg"))

  if (inherits(plot_obj, "ggplot")) {
    ggsave(file, plot_obj, width = width, height = height, device = "svg")
  } else {
    svg(file, width = width, height = height)
    eval(plot_obj)
    dev.off()
  }

  cat(sprintf("  ✓ Saved: %s\n", basename(file)))
  return(file)
}

# Load data
results_file <- "doe_results_20251122_085656.csv"

if (!file.exists(results_file)) {
  stop(paste("Missing:", results_file))
}

results <- read.csv(results_file, stringsAsFactors = FALSE)
cat(sprintf("Loaded: %d rows, %d columns\n\n", nrow(results), ncol(results)))

# Define factors
factor_cols <- c(
  "L1_heat_tracking",
  "L2_rolling_window",
  "L3_linear_decay",
  "L4_pipelining_metrics",
  "L5_window_inference",
  "L6_decay_inference",
  "L7_adaptive_heartrate"
)

for (fc in factor_cols) {
  results[[fc]] <- as.factor(results[[fc]])
}

# Compute ns_per_word if needed
if (!"ns_per_word" %in% names(results)) {
  results <- results %>% mutate(
    ns_per_word = workload_ns_q48 / words_exec
  )
}

cat("Generating SVG plots...\n\n")

# PLOT 1: Performance distribution
p <- ggplot(results, aes(x = ns_per_word)) +
  geom_histogram(bins = 50, fill = "steelblue", alpha = 0.7) +
  labs(title = "Performance Distribution (DoE 2×7)",
       x = "ns/word", y = "Count") +
  theme_minimal(base_size = 12)
save_plot_svg("doe_dist_ns_per_word", p)

# PLOT 2-8: Box plots for each loop
for (loop in factor_cols) {
  p <- ggplot(results, aes_string(x = loop, y = "ns_per_word", fill = loop)) +
    geom_boxplot(alpha = 0.7) +
    labs(title = paste("Performance by", gsub("_", " ", loop)),
         x = gsub("_", " ", loop), y = "ns/word") +
    theme_minimal(base_size = 12) +
    theme(legend.position = "none")

  safe_name <- paste0("doe_box_", loop)
  save_plot_svg(safe_name, p)
}

# PLOT 9-15: Facet plots for each loop
for (loop in factor_cols) {
  p <- ggplot(results, aes_string(x = "ns_per_word", fill = loop)) +
    geom_density(alpha = 0.6) +
    facet_wrap(as.formula(paste("~", loop)), ncol = 1) +
    labs(title = paste("Performance Distribution by", gsub("_", " ", loop)),
         x = "ns/word") +
    theme_minimal(base_size = 12)

  safe_name <- paste0("doe_facet_", loop)
  save_plot_svg(safe_name, p, width = 10, height = 8)
}

# PLOT 16: Correlation matrix
metrics <- results %>%
  select_if(is.numeric) %>%
  select(-matches("run_id|replicate"))

if (ncol(metrics) > 1) {
  cor_mat <- cor(metrics, use = "pairwise.complete.obs")

  p <- ggplot(reshape2::melt(cor_mat), aes(Var1, Var2, fill = value)) +
    geom_tile() +
    scale_fill_gradient2(low = "blue", mid = "white", high = "red", midpoint = 0) +
    labs(title = "Metric Correlation Matrix") +
    theme_minimal(base_size = 10) +
    theme(axis.text.x = element_text(angle = 45, hjust = 1))

  save_plot_svg("doe_corr_metrics", p, width = 12, height = 10)
}

cat("\n═══════════════════════════════════════════════════════════════\n")
cat("SVG generation complete!\n")
cat(sprintf("Files saved to: %s\n", svg_dir))
cat("═══════════════════════════════════════════════════════════════\n")
