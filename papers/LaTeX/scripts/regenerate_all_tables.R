#!/usr/bin/env Rscript
################################################################################
# REGENERATE ALL LATEX TABLES FROM EXPERIMENTAL DATA
# Generates publication-ready LaTeX tables from all experiments
################################################################################

.libPaths('~/R/library')
suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(readr)
  library(xtable)
})

# Paths
TABLE_DIR <- "/home/rajames/CLionProjects/StarForth/papers/LaTeX/tables"
dir.create(TABLE_DIR, showWarnings = FALSE, recursive = TRUE)

cat("================================================================================\n")
cat("  REGENERATING ALL LATEX TABLES\n")
cat("================================================================================\n\n")

table_count <- 0

# ============================================================================
# DOE 30 REPS TABLES
# ============================================================================

cat("Processing DoE 30 Reps experiment...\n")

doe30_file <- "/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_30reps_RAJ_2025.11.22_1700hrs/doe_results_20251122_085656.csv"
doe30 <- read_csv(doe30_file, show_col_types = FALSE) %>%
  mutate(ns_per_word = (runtime_ms * 1e6) / words_exec)

# Table 1: Main Effects (DoE 30 Reps)
cat("  [1] DoE 30 Reps: Main effects\n")

main_effects_30 <- data.frame(
  Loop = c("L1: Heat Tracking", "L2: Rolling Window", "L3: Linear Decay",
           "L4: Pipelining Metrics", "L5: Window Inference", "L6: Decay Inference",
           "L7: Adaptive Heartrate"),
  OFF_mean = c(
    mean(doe30$ns_per_word[doe30$L1_heat_tracking == 0]),
    mean(doe30$ns_per_word[doe30$L2_rolling_window == 0]),
    mean(doe30$ns_per_word[doe30$L3_linear_decay == 0]),
    mean(doe30$ns_per_word[doe30$L4_pipelining_metrics == 0]),
    mean(doe30$ns_per_word[doe30$L5_window_inference == 0]),
    mean(doe30$ns_per_word[doe30$L6_decay_inference == 0]),
    mean(doe30$ns_per_word[doe30$L7_adaptive_heartrate == 0])
  ),
  ON_mean = c(
    mean(doe30$ns_per_word[doe30$L1_heat_tracking == 1]),
    mean(doe30$ns_per_word[doe30$L2_rolling_window == 1]),
    mean(doe30$ns_per_word[doe30$L3_linear_decay == 1]),
    mean(doe30$ns_per_word[doe30$L4_pipelining_metrics == 1]),
    mean(doe30$ns_per_word[doe30$L5_window_inference == 1]),
    mean(doe30$ns_per_word[doe30$L6_decay_inference == 1]),
    mean(doe30$ns_per_word[doe30$L7_adaptive_heartrate == 1])
  )
)

main_effects_30 <- main_effects_30 %>%
  mutate(
    Effect = ON_mean - OFF_mean,
    Effect_pct = (Effect / OFF_mean) * 100,
    Direction = ifelse(Effect < 0, "Beneficial", "Harmful")
  ) %>%
  arrange(Effect)

tex <- xtable(main_effects_30[, c("Loop", "OFF_mean", "ON_mean", "Effect", "Effect_pct", "Direction")],
              caption = "DoE 30 Reps: Main Effects of Feedback Loops on Performance (ns/word)",
              label = "tab:doe30_main_effects",
              digits = c(0, 0, 2, 2, 2, 2, 0))

sink(file.path(TABLE_DIR, "doe30_main_effects.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# Table 2: Top 10 Configurations (DoE 30 Reps)
cat("  [2] DoE 30 Reps: Top 10 configurations\n")

config_summary_30 <- doe30 %>%
  group_by(L1_heat_tracking, L2_rolling_window, L3_linear_decay,
           L4_pipelining_metrics, L5_window_inference, L6_decay_inference,
           L7_adaptive_heartrate) %>%
  summarise(
    mean_ns_per_word = mean(ns_per_word),
    sd_ns_per_word = sd(ns_per_word),
    cv = sd_ns_per_word / mean_ns_per_word * 100,
    n = n(),
    .groups = "drop"
  ) %>%
  arrange(mean_ns_per_word) %>%
  head(10) %>%
  mutate(
    rank = row_number(),
    config = paste0("L1=", L1_heat_tracking, ",L2=", L2_rolling_window, ",L3=", L3_linear_decay,
                   ",L4=", L4_pipelining_metrics, ",L5=", L5_window_inference,
                   ",L6=", L6_decay_inference, ",L7=", L7_adaptive_heartrate)
  )

tex <- xtable(config_summary_30[, c("rank", "config", "mean_ns_per_word", "cv", "n")],
              caption = "DoE 30 Reps: Top 10 Configurations Ranked by Performance",
              label = "tab:doe30_top_configs",
              digits = c(0, 0, 0, 2, 2, 0))

sink(file.path(TABLE_DIR, "doe30_top_configs.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# ============================================================================
# DOE 300 REPS TABLES
# ============================================================================

cat("Processing DoE 300 Reps experiment...\n")

doe300_file <- "/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_300_reps_RAJ_2025.11.26.10:30hrs/doe_results_20251123_093204.csv"
doe300 <- read_csv(doe300_file, show_col_types = FALSE) %>%
  mutate(ns_per_word = (runtime_ms * 1e6) / words_exec)

# Table 3: Main Effects (DoE 300 Reps)
cat("  [3] DoE 300 Reps: Main effects\n")

main_effects_300 <- data.frame(
  Loop = c("L1: Heat Tracking", "L2: Rolling Window", "L3: Linear Decay",
           "L4: Pipelining Metrics", "L5: Window Inference", "L6: Decay Inference",
           "L7: Adaptive Heartrate"),
  OFF_mean = c(
    mean(doe300$ns_per_word[doe300$L1_heat_tracking == 0]),
    mean(doe300$ns_per_word[doe300$L2_rolling_window == 0]),
    mean(doe300$ns_per_word[doe300$L3_linear_decay == 0]),
    mean(doe300$ns_per_word[doe300$L4_pipelining_metrics == 0]),
    mean(doe300$ns_per_word[doe300$L5_window_inference == 0]),
    mean(doe300$ns_per_word[doe300$L6_decay_inference == 0]),
    mean(doe300$ns_per_word[doe300$L7_adaptive_heartrate == 0])
  ),
  ON_mean = c(
    mean(doe300$ns_per_word[doe300$L1_heat_tracking == 1]),
    mean(doe300$ns_per_word[doe300$L2_rolling_window == 1]),
    mean(doe300$ns_per_word[doe300$L3_linear_decay == 1]),
    mean(doe300$ns_per_word[doe300$L4_pipelining_metrics == 1]),
    mean(doe300$ns_per_word[doe300$L5_window_inference == 1]),
    mean(doe300$ns_per_word[doe300$L6_decay_inference == 1]),
    mean(doe300$ns_per_word[doe300$L7_adaptive_heartrate == 1])
  )
)

main_effects_300 <- main_effects_300 %>%
  mutate(
    Effect = ON_mean - OFF_mean,
    Effect_pct = (Effect / OFF_mean) * 100,
    Direction = ifelse(Effect < 0, "Beneficial", "Harmful")
  ) %>%
  arrange(Effect)

tex <- xtable(main_effects_300[, c("Loop", "OFF_mean", "ON_mean", "Effect", "Effect_pct", "Direction")],
              caption = "DoE 300 Reps: Main Effects of Feedback Loops on Performance (ns/word)",
              label = "tab:doe300_main_effects",
              digits = c(0, 0, 2, 2, 2, 2, 0))

sink(file.path(TABLE_DIR, "doe300_main_effects.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# Table 4: Top 10 Configurations (DoE 300 Reps)
cat("  [4] DoE 300 Reps: Top 10 configurations\n")

config_summary_300 <- doe300 %>%
  group_by(L1_heat_tracking, L2_rolling_window, L3_linear_decay,
           L4_pipelining_metrics, L5_window_inference, L6_decay_inference,
           L7_adaptive_heartrate) %>%
  summarise(
    mean_ns_per_word = mean(ns_per_word),
    sd_ns_per_word = sd(ns_per_word),
    cv = sd_ns_per_word / mean_ns_per_word * 100,
    n = n(),
    .groups = "drop"
  ) %>%
  arrange(mean_ns_per_word) %>%
  head(10) %>%
  mutate(
    rank = row_number(),
    config = paste0("L1=", L1_heat_tracking, ",L2=", L2_rolling_window, ",L3=", L3_linear_decay,
                   ",L4=", L4_pipelining_metrics, ",L5=", L5_window_inference,
                   ",L6=", L6_decay_inference, ",L7=", L7_adaptive_heartrate)
  )

tex <- xtable(config_summary_300[, c("rank", "config", "mean_ns_per_word", "cv", "n")],
              caption = "DoE 300 Reps: Top 10 Configurations Ranked by Performance",
              label = "tab:doe300_top_configs",
              digits = c(0, 0, 0, 2, 2, 0))

sink(file.path(TABLE_DIR, "doe300_top_configs.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# ============================================================================
# WINDOW SCALING TABLES
# ============================================================================

cat("Processing Window Scaling experiment...\n")

HB_DIR <- "/home/rajames/CLionProjects/StarForth/experiments/window_scaling/results_run_one/raw/hb"
hb_files <- list.files(HB_DIR, pattern = "run-.*\\.csv", full.names = TRUE)

all_runs <- list()
for (hb_file in hb_files) {
  run_id <- gsub("run-|\\.csv", "", basename(hb_file))
  tryCatch({
    hb <- read_csv(hb_file, show_col_types = FALSE)
    if (nrow(hb) == 0) next

    window_size <- as.numeric(hb$window_width[1])
    valid_data <- hb %>%
      filter(tick_number > 0, tick_interval_ns > 0, tick_interval_ns < 1e9)

    if (nrow(valid_data) == 0) next

    intervals <- valid_data$tick_interval_ns
    k_vals <- valid_data %>%
      filter(K_approx > 0, K_approx < 10) %>%
      pull(K_approx)

    if (length(intervals) > 10 && length(k_vals) > 10) {
      run_summary <- data.frame(
        window_size = window_size,
        mean_K = mean(k_vals),
        sd_K = sd(k_vals),
        cv_interval = sd(intervals) / mean(intervals) * 100,
        freq_hz = 1e9 / mean(intervals)
      )
      all_runs[[length(all_runs) + 1]] <- run_summary
    }
  }, error = function(e) {})
}

window_df <- bind_rows(all_runs)

# Table 5: Window Scaling Summary Statistics
cat("  [5] Window Scaling: Summary statistics\n")

window_summary <- window_df %>%
  group_by(window_size) %>%
  summarise(
    n_runs = n(),
    mean_K = mean(mean_K),
    sd_K = sd(mean_K),
    K_deviation = abs(mean_K - 1.0),
    mean_cv = mean(cv_interval),
    mean_freq = mean(freq_hz),
    .groups = "drop"
  ) %>%
  arrange(window_size)

tex <- xtable(window_summary,
              caption = "Window Scaling: Summary Statistics by Window Size",
              label = "tab:window_scaling_summary",
              digits = c(0, 0, 0, 3, 3, 3, 2, 1))

sink(file.path(TABLE_DIR, "window_scaling_summary.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# ============================================================================
# L8 VALIDATION TABLES
# ============================================================================

cat("Processing L8 Validation experiment...\n")

l8_file <- "/home/rajames/CLionProjects/StarForth/experiments/l8_validation/l8_validation_20251208_102205/l8_validation_results.csv"
l8 <- read_csv(l8_file, show_col_types = FALSE)

l8 <- l8 %>%
  mutate(
    ns_per_word = runtime_ns / 1000,
    runtime_ms = runtime_ns / 1e6
  )

# Table 6: Strategy Performance Summary
cat("  [6] L8 Validation: Strategy performance summary\n")

strategy_summary <- l8 %>%
  group_by(strategy) %>%
  summarise(
    n = n(),
    mean_runtime = mean(runtime_ms),
    sd_runtime = sd(runtime_ms),
    cv = sd_runtime / mean_runtime * 100,
    .groups = "drop"
  ) %>%
  arrange(mean_runtime) %>%
  mutate(rank = row_number())

tex <- xtable(strategy_summary[, c("rank", "strategy", "mean_runtime", "cv", "n")],
              caption = "L8 Validation: Overall Strategy Performance (all workloads combined)",
              label = "tab:l8_strategy_summary",
              digits = c(0, 0, 0, 2, 2, 0))

sink(file.path(TABLE_DIR, "l8_strategy_summary.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# Table 7: Best Strategy per Workload
cat("  [7] L8 Validation: Best strategy per workload\n")

best_per_workload <- l8 %>%
  group_by(workload_type, strategy) %>%
  summarise(
    mean_runtime = mean(runtime_ms),
    .groups = "drop"
  ) %>%
  group_by(workload_type) %>%
  arrange(mean_runtime) %>%
  slice(1:3) %>%
  mutate(rank = row_number()) %>%
  ungroup()

best_wide <- best_per_workload %>%
  select(workload_type, rank, strategy, mean_runtime) %>%
  pivot_wider(
    names_from = rank,
    values_from = c(strategy, mean_runtime),
    names_glue = "{.value}_{rank}"
  )

tex <- xtable(best_wide,
              caption = "L8 Validation: Top 3 Strategies per Workload Type",
              label = "tab:l8_best_per_workload",
              digits = 2)

sink(file.path(TABLE_DIR, "l8_best_per_workload.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# Table 8: L8_ADAPTIVE Performance Margin
cat("  [8] L8 Validation: L8_ADAPTIVE margin per workload\n")

l8_margin <- l8 %>%
  group_by(workload_type, strategy) %>%
  summarise(
    mean_runtime = mean(runtime_ms),
    .groups = "drop"
  ) %>%
  group_by(workload_type) %>%
  mutate(
    best_runtime = min(mean_runtime),
    margin_pct = (mean_runtime - best_runtime) / best_runtime * 100
  ) %>%
  filter(strategy == "L8_ADAPTIVE") %>%
  select(workload_type, mean_runtime, best_runtime, margin_pct) %>%
  ungroup()

tex <- xtable(l8_margin,
              caption = "L8 Validation: L8\\_ADAPTIVE Performance Margin vs Best Static Config",
              label = "tab:l8_adaptive_margin",
              digits = c(0, 0, 2, 2, 2))

sink(file.path(TABLE_DIR, "l8_adaptive_margin.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) {
        gsub("L8_ADAPTIVE", "L8\\\\_ADAPTIVE", x, fixed = TRUE)
      })
sink()
table_count <- table_count + 1

# ============================================================================
# L8 ATTRACTOR TABLES
# ============================================================================

cat("Processing L8 Attractor experiment...\n")

l8_attractor_file <- "/home/rajames/CLionProjects/StarForth/experiments/l8_attractor_map/results_20251209_145907/heartbeat_all.csv"
l8_attractor <- read_csv(l8_attractor_file, show_col_types = FALSE)

# Table 9: Overall Summary Statistics
cat("  [9] L8 Attractor: Overall summary statistics\n")

# Calculate overall summary statistics
overall_summary <- data.frame(
  Metric = c("Total Ticks", "Mean Tick Interval (ns)", "CV Tick Interval (%)",
             "Mean Hot Word Count", "Mean Word Heat", "Mean Window Width (bytes)"),
  Value = c(
    nrow(l8_attractor),
    mean(l8_attractor$tick_interval_ns, na.rm = TRUE),
    sd(l8_attractor$tick_interval_ns, na.rm = TRUE) / mean(l8_attractor$tick_interval_ns, na.rm = TRUE) * 100,
    mean(l8_attractor$hot_word_count, na.rm = TRUE),
    mean(l8_attractor$avg_word_heat, na.rm = TRUE),
    mean(l8_attractor$window_width, na.rm = TRUE)
  )
)

tex <- xtable(overall_summary,
              caption = "L8 Attractor: Overall Summary Statistics (all workloads combined)",
              label = "tab:l8_attractor_summary",
              digits = c(0, 0, 2))

sink(file.path(TABLE_DIR, "l8_attractor_summary.tex"))
print(tex, include.rownames = FALSE, caption.placement = "top",
      sanitize.text.function = function(x) x)
sink()
table_count <- table_count + 1

# ============================================================================
# SUMMARY
# ============================================================================

cat("\n================================================================================\n")
cat(sprintf("  REGENERATION COMPLETE: %d LATEX TABLES GENERATED\n", table_count))
cat("================================================================================\n")
cat(sprintf("\nOutput directory: %s\n", TABLE_DIR))
cat("\nTables generated:\n")
cat("  DoE 30 Reps:\n")
cat("    1. doe30_main_effects.tex - Main effects of feedback loops\n")
cat("    2. doe30_top_configs.tex - Top 10 configurations\n")
cat("  DoE 300 Reps:\n")
cat("    3. doe300_main_effects.tex - Main effects of feedback loops\n")
cat("    4. doe300_top_configs.tex - Top 10 configurations\n")
cat("  Window Scaling:\n")
cat("    5. window_scaling_summary.tex - Summary statistics by window size\n")
cat("  L8 Validation:\n")
cat("    6. l8_strategy_summary.tex - Overall strategy performance\n")
cat("    7. l8_best_per_workload.tex - Top 3 strategies per workload\n")
cat("    8. l8_adaptive_margin.tex - L8_ADAPTIVE margin vs best static\n")
cat("  L8 Attractor:\n")
cat("    9. l8_attractor_summary.tex - Overall summary statistics\n")
cat("\nAll tables regenerated from experimental CSV data\n")
cat("Usage: \\input{tables/tablename.tex} in your LaTeX document\n")
cat("\n")
