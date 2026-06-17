#!/usr/bin/env Rscript

################################################################################
# Live Experiment Monitor - James Law Window Scaling
################################################################################
#
# Real-time dashboard showing:
#   1. Experiment progress (runs completed)
#   2. K statistic accumulation and trends
#   3. Live heartbeat traces from current run
#   4. Performance metrics evolution
#
# Usage:
#   ./live_monitor.R [update_interval_seconds]
#
# Example:
#   ./live_monitor.R 10    # Update every 10 seconds
################################################################################

suppressPackageStartupMessages({
  library(readr)
  library(dplyr)
  library(ggplot2)
  library(tidyr)
})

# Configuration
args <- commandArgs(trailingOnly = TRUE)
UPDATE_INTERVAL <- if(length(args) > 0) as.numeric(args[1]) else 15  # seconds
MAX_ITERATIONS <- 1000  # Safety limit

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
    args <- commandArgs(trailingOnly = FALSE)
    file_arg <- grep("^--file=", args, value = TRUE)
    if (length(file_arg) > 0) {
      script_dir <- dirname(sub("^--file=", "", file_arg))
      return(file.path(script_dir, ".."))
    }
    return(".")
  }
}

EXPERIMENT_DIR <- get_experiment_dir()
setwd(EXPERIMENT_DIR)

# Paths (relative to experiment directory)
RESULTS_CSV <- "results/raw/window_sweep_results.csv"
HB_DIR <- "results/raw/hb"
LIVE_OUTPUT_DIR <- "results/live"
dir.create(LIVE_OUTPUT_DIR, showWarnings = FALSE, recursive = TRUE)

# ANSI color codes for terminal
RESET <- "\033[0m"
BOLD <- "\033[1m"
GREEN <- "\033[32m"
YELLOW <- "\033[33m"
BLUE <- "\033[34m"
CYAN <- "\033[36m"

cat_color <- function(color, ...) {
  cat(color, ..., RESET, sep = "")
}

clear_screen <- function() {
  cat("\033[2J\033[H")  # Clear screen and move cursor to home
}

################################################################################
# Data Loading Functions
################################################################################

load_summary_data <- function() {
  if (!file.exists(RESULTS_CSV)) {
    return(NULL)
  }

  tryCatch({
    df <- read_csv(RESULTS_CSV, show_col_types = FALSE,
                   col_types = cols(.default = col_guess()))

    # Filter out invalid runs (skip header-only or malformed rows)
    if (nrow(df) > 0) {
      df <- df %>% filter(!is.na(window_size))
    }

    return(df)
  }, error = function(e) {
    return(NULL)
  })
}

load_latest_heartbeat <- function() {
  if (!dir.exists(HB_DIR)) {
    return(NULL)
  }

  # Find most recent heartbeat file
  hb_files <- list.files(HB_DIR, pattern = "^run-.*\\.csv$", full.names = TRUE)
  if (length(hb_files) == 0) {
    return(NULL)
  }

  # Get most recent by modification time
  latest_file <- hb_files[which.max(file.mtime(hb_files))]

  tryCatch({
    hb_data <- read_csv(latest_file, show_col_types = FALSE)
    return(list(
      file = basename(latest_file),
      data = hb_data
    ))
  }, error = function(e) {
    return(NULL)
  })
}

################################################################################
# Visualization Functions
################################################################################

plot_k_accumulation <- function(df) {
  if (is.null(df) || nrow(df) == 0) {
    return(NULL)
  }

  # Calculate cumulative K statistics
  df_plot <- df %>%
    arrange(timestamp) %>%
    mutate(
      run_number = row_number(),
      K_stat = K_statistic,
      K_cumulative_mean = cummean(K_stat),
      K_cumulative_sd = cumsd(K_stat)
    )

  p <- ggplot(df_plot, aes(x = run_number)) +
    geom_ribbon(aes(ymin = K_cumulative_mean - K_cumulative_sd,
                    ymax = K_cumulative_mean + K_cumulative_sd),
                alpha = 0.2, fill = "blue") +
    geom_line(aes(y = K_stat), alpha = 0.3, color = "gray50") +
    geom_line(aes(y = K_cumulative_mean), color = "blue", size = 1.2) +
    geom_hline(yintercept = 1.0, linetype = "dashed", color = "red") +
    labs(
      title = "K Statistic Accumulation",
      subtitle = sprintf("Current: %.4f | Mean: %.4f | Target: 1.0",
                         tail(df_plot$K_stat, 1),
                         tail(df_plot$K_cumulative_mean, 1)),
      x = "Run Number",
      y = "K Statistic"
    ) +
    theme_minimal() +
    theme(plot.title = element_text(face = "bold"))

  return(p)
}

# Helper function for cumulative SD
cumsd <- function(x) {
  n <- seq_along(x)
  sqrt((cumsum(x^2) - (cumsum(x)^2 / n)) / (n - 1))
}

plot_heartbeat_trace <- function(hb_info) {
  if (is.null(hb_info)) {
    return(NULL)
  }

  hb <- hb_info$data

  # Create time series plots
  p <- ggplot(hb, aes(x = tick_number)) +
    geom_line(aes(y = K_approx, color = "K"), size = 0.8) +
    geom_line(aes(y = K_deviation * 10, color = "K deviation (×10)"),
              size = 0.8, alpha = 0.6) +
    geom_hline(yintercept = 1.0, linetype = "dashed", color = "red", alpha = 0.5) +
    labs(
      title = sprintf("Live Heartbeat Trace: %s", hb_info$file),
      subtitle = sprintf("%d ticks | Latest K: %.4f",
                         nrow(hb),
                         tail(hb$K_approx, 1)),
      x = "Tick Number",
      y = "Value",
      color = "Metric"
    ) +
    scale_color_manual(values = c("K" = "blue", "K deviation (×10)" = "orange")) +
    theme_minimal() +
    theme(
      plot.title = element_text(face = "bold"),
      legend.position = "bottom"
    )

  return(p)
}

plot_window_performance <- function(df) {
  if (is.null(df) || nrow(df) == 0) {
    return(NULL)
  }

  # Aggregate by window size
  df_summary <- df %>%
    group_by(window_size) %>%
    summarise(
      mean_K = mean(K_statistic, na.rm = TRUE),
      se_K = sd(K_statistic, na.rm = TRUE) / sqrt(n()),
      n = n(),
      .groups = "drop"
    ) %>%
    filter(n >= 3)  # Only show windows with 3+ replicates

  if (nrow(df_summary) == 0) {
    return(NULL)
  }

  p <- ggplot(df_summary, aes(x = window_size, y = mean_K)) +
    geom_point(aes(size = n), color = "blue", alpha = 0.6) +
    geom_errorbar(aes(ymin = mean_K - se_K, ymax = mean_K + se_K),
                  width = 0.1, alpha = 0.5) +
    geom_hline(yintercept = 1.0, linetype = "dashed", color = "red") +
    scale_x_log10() +
    labs(
      title = "K by Window Size",
      subtitle = sprintf("%d windows tested", nrow(df_summary)),
      x = "Window Size (bytes, log scale)",
      y = "Mean K Statistic",
      size = "Replicates"
    ) +
    theme_minimal() +
    theme(plot.title = element_text(face = "bold"))

  return(p)
}

################################################################################
# Dashboard Display
################################################################################

display_dashboard <- function(summary_df, hb_info, iteration) {
  clear_screen()

  cat_color(BOLD, "═══════════════════════════════════════════════════════════════════════\n")
  cat_color(CYAN, "        James Law Window Scaling - LIVE EXPERIMENT MONITOR\n")
  cat_color(BOLD, "═══════════════════════════════════════════════════════════════════════\n\n")

  # Experiment progress
  if (!is.null(summary_df) && nrow(summary_df) > 0) {
    total_runs <- 360
    completed <- nrow(summary_df)
    pct_complete <- (completed / total_runs) * 100

    cat_color(GREEN, sprintf("Progress: %d / %d runs (%.1f%%)\n",
                             completed, total_runs, pct_complete))

    # Progress bar
    bar_width <- 50
    filled <- round(bar_width * completed / total_runs)
    empty <- bar_width - filled
    cat("[", rep("█", filled), rep("░", empty), "]\n\n", sep = "")

    # Current K statistics
    cat_color(YELLOW, "Current Statistics:\n")
    cat(sprintf("  K (latest):     %.6f\n", tail(summary_df$K_statistic, 1)))
    cat(sprintf("  K (mean):       %.6f\n", mean(summary_df$K_statistic, na.rm = TRUE)))
    cat(sprintf("  K (std):        %.6f\n", sd(summary_df$K_statistic, na.rm = TRUE)))
    cat(sprintf("  K (CV):         %.2f%%\n",
                100 * sd(summary_df$K_statistic, na.rm = TRUE) /
                  mean(summary_df$K_statistic, na.rm = TRUE)))

    # Window size distribution
    window_counts <- table(summary_df$window_size)
    cat(sprintf("\nWindow sizes: %d unique (%s)\n",
                length(window_counts),
                paste(names(window_counts), collapse = ", ")))
  } else {
    cat_color(YELLOW, "Waiting for experiment to start...\n")
  }

  # Latest heartbeat info
  cat("\n")
  if (!is.null(hb_info)) {
    cat_color(BLUE, sprintf("Latest Heartbeat: %s\n", hb_info$file))
    cat(sprintf("  Ticks recorded: %d\n", nrow(hb_info$data)))
    cat(sprintf("  Latest K:       %.6f\n", tail(hb_info$data$K_approx, 1)))
    cat(sprintf("  Latest L8 mode: %d\n", tail(hb_info$data$l8_mode, 1)))
  } else {
    cat_color(YELLOW, "No heartbeat data available yet\n")
  }

  cat("\n")
  cat_color(BOLD, "───────────────────────────────────────────────────────────────────────\n")
  cat(sprintf("Update #%d | Interval: %ds | Press Ctrl+C to stop\n",
              iteration, UPDATE_INTERVAL))
  cat_color(BOLD, "═══════════════════════════════════════════════════════════════════════\n")

  # Generate plots (save to files)
  if (!is.null(summary_df) && nrow(summary_df) > 0) {
    tryCatch({
      # K accumulation plot
      p1 <- plot_k_accumulation(summary_df)
      if (!is.null(p1)) {
        ggsave(file.path(LIVE_OUTPUT_DIR, "live_k_accumulation.png"),
               p1, width = 10, height = 6, dpi = 100)
      }

      # Window performance plot
      p2 <- plot_window_performance(summary_df)
      if (!is.null(p2)) {
        ggsave(file.path(LIVE_OUTPUT_DIR, "live_window_performance.png"),
               p2, width = 10, height = 6, dpi = 100)
      }
    }, error = function(e) {
      cat(sprintf("Warning: Plot generation failed: %s\n", e$message))
    })
  }

  # Heartbeat trace plot
  if (!is.null(hb_info)) {
    tryCatch({
      p3 <- plot_heartbeat_trace(hb_info)
      if (!is.null(p3)) {
        ggsave(file.path(LIVE_OUTPUT_DIR, "live_heartbeat_trace.png"),
               p3, width = 10, height = 6, dpi = 100)
      }
    }, error = function(e) {
      cat(sprintf("Warning: Heartbeat plot failed: %s\n", e$message))
    })
  }

  cat(sprintf("\nPlots saved to: %s/\n", LIVE_OUTPUT_DIR))
}

################################################################################
# Main Monitor Loop
################################################################################

cat_color(BOLD, "\nStarting live monitor...\n")
cat(sprintf("Update interval: %d seconds\n", UPDATE_INTERVAL))
cat(sprintf("Results CSV: %s\n", RESULTS_CSV))
cat(sprintf("Heartbeat dir: %s\n", HB_DIR))
cat(sprintf("Live plots: %s/\n\n", LIVE_OUTPUT_DIR))

Sys.sleep(2)

for (i in 1:MAX_ITERATIONS) {
  # Load latest data
  summary_df <- load_summary_data()
  hb_info <- load_latest_heartbeat()

  # Display dashboard
  display_dashboard(summary_df, hb_info, i)

  # Check if experiment is complete
  if (!is.null(summary_df) && nrow(summary_df) >= 360) {
    cat_color(GREEN, "\n\n✓ Experiment complete! All 360 runs finished.\n\n")
    break
  }

  # Wait for next update
  Sys.sleep(UPDATE_INTERVAL)
}

cat_color(BOLD, "\nMonitor stopped.\n")