#!/usr/bin/env Rscript

# This script takes NO arguments.
# It:
#   - finds experiments/l8_attractor_map
#   - reads .latest_results_dir
#   - writes l8_attractor_run_matrix.csv into that results dir

# --------------------------------------------------------------------
# Locate experiment root (.. from scripts/generate_l8_attractor_matrix.R)
# --------------------------------------------------------------------
get_script_dir <- function() {
  args <- commandArgs(trailingOnly = FALSE)
  file_arg <- "--file="
  match <- grep(file_arg, args)
  if (length(match) > 0) {
    normalizePath(dirname(sub(file_arg, "", args[match])))
  } else {
    # fallback: working dir if run interactively
    getwd()
  }
}

SCRIPT_DIR      <- get_script_dir()
EXPERIMENT_ROOT <- normalizePath(file.path(SCRIPT_DIR, ".."))
LATEST_FILE     <- file.path(EXPERIMENT_ROOT, ".latest_results_dir")

if (!file.exists(LATEST_FILE)) {
  cat("ERROR: .latest_results_dir not found.\n")
  cat("Run ./scripts/setup.sh first.\n")
  quit(status = 1)
}

RESULTS_DIR <- normalizePath(readLines(LATEST_FILE, warn = FALSE)[1])
MATRIX_OUT  <- file.path(RESULTS_DIR, "l8_attractor_run_matrix.csv")

# --------------------------------------------------------------------
# Build run matrix
# --------------------------------------------------------------------
# FIXED SEED FOR REPRODUCIBILITY
set.seed(20251208)

# L8 workloads
workloads <- c(
  "init-l8-diverse.4th",
  "init-l8-stable.4th",
  "init-l8-temporal.4th",
  "init-l8-transition.4th",
  "init-l8-volatile.4th",
  "init-l8-omni.4th"
)

replicates <- 1:30

# Construct full grid
df <- expand.grid(
  init_script = workloads,
  replicate   = replicates
)

# Shuffle rows
df <- df[sample(nrow(df)), ]
df$run_id <- seq_len(nrow(df))

# Reorder columns
df <- df[, c("run_id", "init_script", "replicate")]

# Write CSV
dir.create(RESULTS_DIR, showWarnings = FALSE, recursive = TRUE)
write.csv(df, MATRIX_OUT, row.names = FALSE)

cat("Wrote matrix to:\n  ", MATRIX_OUT, "\n", sep = "")
