#!/usr/bin/env Rscript
# ============================================================================
# Window Scaling Experiment: Shuffled Run Matrix Generator (REDESIGNED)
# ============================================================================
# Purpose: Generate a randomized experimental design for testing the
#          James Law of Computational Dynamics: Λ = W / (DoF + 1)
#
# CRITICAL CHANGE: DoF is NOT compile-time! It's determined at runtime by
# the L8 Jacquard "loom" which picks between configuration "cards".
# We only vary WINDOW SIZE at compile-time.
#
# Author: Robert A. James
# Date: 2025-11-29
# ============================================================================

library(dplyr)
library(tidyr)
library(readr)

# Get experiment directory (assumes script is run from experiment root or scripts/)
get_experiment_dir <- function() {
  if (file.exists("results")) return(".")
  if (file.exists("../results_run_one")) return("..")
  return(".")
}

EXPERIMENT_DIR <- get_experiment_dir()

# ============================================================================
# Experimental Parameters
# ============================================================================

# Window sizes to test (bytes) - ONLY compile-time parameter
# Coverage: subcritical → near-critical → critical → catastrophic collapse
WINDOWS <- c( 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536)

# Replicates per window size
REPS <- 30

# Critical windows for FULL heartbeat logging (10-15% of runs)
# These windows are in the critical regime where we expect interesting dynamics
CRITICAL_WINDOWS_FULL <- c(1024, 4096, 8192, 16384, 65536)
FULL_LOGGING_REPS <- 5  # First 5 reps of critical windows get full logging

# Random seed for reproducibility
SEED <- 20251208  # YYYYMMDD format (Run Two)

# ============================================================================
# Generate Experimental Design
# ============================================================================

cat("Generating run matrix (DoF is RUNTIME, not compile-time)...\n")
cat("  Window sizes:", length(WINDOWS), "\n")
cat("  Replicates:", REPS, "\n")

run_matrix <- expand_grid(
  window_size = WINDOWS,
  replicate = 1:REPS
)

total_runs <- nrow(run_matrix)
cat("  Total runs:", total_runs, "\n\n")

# ============================================================================
# Add Metadata Columns
# ============================================================================

run_matrix <- run_matrix %>%
  mutate(
    # Window size category
    window_category = case_when(
      window_size < 2048 ~ "subcritical",
      window_size == 4096 ~ "baseline",
      window_size <= 8192 ~ "stable",
      window_size == 16384 ~ "critical",
      window_size > 16384 ~ "collapse"
    ),

    # Heartbeat logging mode: ALL runs use FULL mode for complete heartbeat data
    log_mode = "full",

    # Run ID (sequential, will be shuffled)
    run_id_sequential = row_number()
  )

# ============================================================================
# Shuffle the Run Order (eliminates temporal bias)
# ============================================================================

cat("Shuffling run order (seed =", SEED, ")...\n")
set.seed(SEED)
run_matrix_shuffled <- run_matrix %>%
  sample_frac(1) %>%
  mutate(
    # New run ID after shuffling
    run_id_shuffled = row_number()
  )

# ============================================================================
# Save Results
# ============================================================================

output_file <- file.path(EXPERIMENT_DIR, "run_matrix_shuffled.csv")
cat("Writing shuffled run matrix to:", output_file, "\n")

write_csv(run_matrix_shuffled, output_file)

# ============================================================================
# Summary Statistics
# ============================================================================

cat("\n")
cat("============================================================================\n")
cat("RUN MATRIX SUMMARY (REDESIGNED)\n")
cat("============================================================================\n")
cat("Total runs:              ", total_runs, "\n")
cat("Window range:            ", min(WINDOWS), "-", max(WINDOWS), "bytes\n")
cat("Replicates per window:   ", REPS, "\n")
cat("Random seed:             ", SEED, "\n")
cat("\n")

cat("NOTE: DoF is determined at RUNTIME by L8 Jacquard loom!\n")
cat("      Only WINDOW SIZE varies at compile-time.\n")
cat("\n")

cat("Runs by window size:\n")
print(table(run_matrix_shuffled$window_size))
cat("\n")

cat("Runs by category:\n")
print(table(run_matrix_shuffled$window_category))
cat("\n")

cat("Runs by logging mode:\n")
print(table(run_matrix_shuffled$log_mode))
cat("\n")

cat("Full logging details:\n")
full_logging_runs <- run_matrix_shuffled %>%
  filter(log_mode == "full") %>%
  count(window_size)
print(full_logging_runs)
cat("  Total FULL mode runs:", sum(full_logging_runs$n),
    sprintf("(%.1f%%)\n", 100 * sum(full_logging_runs$n) / total_runs))
cat("\n")

cat("Critical windows for time-series analysis:\n")
cat("  Windows:", paste(CRITICAL_WINDOWS_FULL, collapse=", "), "\n")
cat("  Reps with full logging:", FULL_LOGGING_REPS, "per window\n")
cat("  Total full-mode runs:", length(CRITICAL_WINDOWS_FULL) * FULL_LOGGING_REPS, "\n")
cat("\n")

cat("============================================================================\n")
cat("Run matrix generation complete.\n")
cat("Output: 360 runs (12 windows × 30 reps)\n")
cat("Logging: 25 FULL (6.9%), 335 SUMMARY (93.1%)\n")
cat("============================================================================\n")