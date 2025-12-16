#!/usr/bin/env Rscript
################################################################################
# REGENERATE ALL TABLES - DUAL FORMAT (src + HTML)
# Generates both src and standalone HTML versions
################################################################################

.libPaths('~/R/library')
suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(readr)
  library(xtable)
  library(knitr)
  library(kableExtra)
})

# Paths
TABLE_DIR <- "/home/rajames/CLionProjects/StarForth/papers/LaTeX/tables"
dir.create(TABLE_DIR, showWarnings = FALSE, recursive = TRUE)

cat("================================================================================\n")
cat("  REGENERATING ALL TABLES (LaTeX + HTML)\n")
cat("================================================================================\n\n")

table_count <- 0
table_metadata <- list()

# Helper function to save both formats
save_table_dual <- function(df, name, caption, label, digits = 2) {
  # Save src version
  tex <- xtable(df,
                caption = caption,
                label = label,
                digits = digits)

  sink(file.path(TABLE_DIR, paste0(name, ".tex")))
  print(tex, include.rownames = FALSE, caption.placement = "top",
        sanitize.text.function = function(x) x)
  sink()

  # Save standalone HTML version
  html_content <- paste0(
    '<!DOCTYPE html>\n',
    '<html lang="en">\n',
    '<head>\n',
    '    <meta charset="UTF-8">\n',
    '    <meta name="viewport" content="width=device-width, initial-scale=1.0">\n',
    '    <title>', caption, '</title>\n',
    '    <style>\n',
    '        body {\n',
    '            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;\n',
    '            max-width: 1200px;\n',
    '            margin: 40px auto;\n',
    '            padding: 20px;\n',
    '            background: #f5f5f5;\n',
    '        }\n',
    '        .container {\n',
    '            background: white;\n',
    '            padding: 40px;\n',
    '            border-radius: 8px;\n',
    '            box-shadow: 0 2px 10px rgba(0,0,0,0.1);\n',
    '        }\n',
    '        h1 {\n',
    '            color: #1e40af;\n',
    '            border-bottom: 3px solid #2563eb;\n',
    '            padding-bottom: 15px;\n',
    '            margin-bottom: 30px;\n',
    '        }\n',
    '        table {\n',
    '            width: 100%;\n',
    '            border-collapse: collapse;\n',
    '            margin: 20px 0;\n',
    '        }\n',
    '        th, td {\n',
    '            padding: 12px 15px;\n',
    '            text-align: left;\n',
    '            border-bottom: 1px solid #e5e7eb;\n',
    '        }\n',
    '        th {\n',
    '            background: #1e40af;\n',
    '            color: white;\n',
    '            font-weight: 600;\n',
    '            text-transform: uppercase;\n',
    '            font-size: 0.85em;\n',
    '            letter-spacing: 0.5px;\n',
    '        }\n',
    '        tr:hover {\n',
    '            background: #f9fafb;\n',
    '        }\n',
    '        tr:nth-child(even) {\n',
    '            background: #f8fafc;\n',
    '        }\n',
    '        .footer {\n',
    '            margin-top: 40px;\n',
    '            padding-top: 20px;\n',
    '            border-top: 2px solid #e5e7eb;\n',
    '            color: #64748b;\n',
    '            font-size: 0.9em;\n',
    '        }\n',
    '        .label {\n',
    '            background: #eff6ff;\n',
    '            color: #1e40af;\n',
    '            padding: 4px 12px;\n',
    '            border-radius: 4px;\n',
    '            font-family: monospace;\n',
    '            font-size: 0.85em;\n',
    '            margin-top: 10px;\n',
    '            display: inline-block;\n',
    '        }\n',
    '    </style>\n',
    '</head>\n',
    '<body>\n',
    '    <div class="container">\n',
    '        <h1>', caption, '</h1>\n',
    '        <div class="label">LaTeX Label: ', label, '</div>\n'
  )

  # Generate HTML table using kable
  html_table <- kable(df, format = "html", escape = FALSE) %>%
    kable_styling(bootstrap_options = c("striped", "hover"),
                  full_width = TRUE,
                  position = "left")

  html_content <- paste0(
    html_content,
    '\n        ', html_table, '\n',
    '        <div class="footer">\n',
    '            <p>Generated from real experimental data | StarForth Project | December 2025</p>\n',
    '            <p>LaTeX file: <code>', name, '.tex</code> | ',
    'Usage: <code>\\input{tables/', name, '.tex}</code></p>\n',
    '        </div>\n',
    '    </div>\n',
    '</body>\n',
    '</html>\n'
  )

  writeLines(html_content, file.path(TABLE_DIR, paste0(name, ".html")))

  table_count <<- table_count + 1

  # Store metadata for gallery
  table_metadata[[length(table_metadata) + 1]] <<- list(
    name = name,
    caption = caption,
    label = label,
    rows = nrow(df),
    cols = ncol(df)
  )
}

# ============================================================================
# DOE 30 REPS TABLES
# ============================================================================

cat("Processing DoE 30 Reps experiment...\n")

doe30_file <- "/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_30reps_RAJ_2025.11.22_1700hrs/doe_results_20251122_085656.csv"
doe30 <- read_csv(doe30_file, show_col_types = FALSE) %>%
  mutate(ns_per_word = (runtime_ms * 1e6) / words_exec)

# Table 1: Main Effects
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
) %>%
  mutate(
    Effect = ON_mean - OFF_mean,
    `Effect %` = sprintf("%.2f%%", (Effect / OFF_mean) * 100),
    Direction = ifelse(Effect < 0, "Beneficial", "Harmful")
  ) %>%
  arrange(Effect) %>%
  select(Loop, OFF_mean, ON_mean, Effect, `Effect %`, Direction)

save_table_dual(main_effects_30, "doe30_main_effects",
                "DoE 30 Reps: Main Effects of Feedback Loops on Performance (ns/word)",
                "tab:doe30_main_effects", digits = 2)

# Table 2: Top 10 Configurations
cat("  [2] DoE 30 Reps: Top 10 configurations\n")

top_configs_30 <- doe30 %>%
  group_by(L1_heat_tracking, L2_rolling_window, L3_linear_decay,
           L4_pipelining_metrics, L5_window_inference, L6_decay_inference,
           L7_adaptive_heartrate) %>%
  summarise(
    mean_ns = mean(ns_per_word),
    cv = sd(ns_per_word) / mean_ns * 100,
    n = n(),
    .groups = "drop"
  ) %>%
  arrange(mean_ns) %>%
  head(10) %>%
  mutate(
    Rank = row_number(),
    Config = paste0(L1_heat_tracking, L2_rolling_window, L3_linear_decay,
                   L4_pipelining_metrics, L5_window_inference,
                   L6_decay_inference, L7_adaptive_heartrate)
  ) %>%
  select(Rank, Config, `Mean (ns/word)` = mean_ns, `CV %` = cv, N = n)

save_table_dual(top_configs_30, "doe30_top_configs",
                "DoE 30 Reps: Top 10 Configurations Ranked by Performance",
                "tab:doe30_top_configs", digits = 2)

# ============================================================================
# DOE 300 REPS TABLES
# ============================================================================

cat("Processing DoE 300 Reps experiment...\n")

doe300_file <- "/home/rajames/CLionProjects/StarForth/experiments/doe_2x7/results_300_reps_RAJ_2025.11.26.10:30hrs/doe_results_20251123_093204.csv"
doe300 <- read_csv(doe300_file, show_col_types = FALSE) %>%
  mutate(ns_per_word = (runtime_ms * 1e6) / words_exec)

# Table 3: Main Effects
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
) %>%
  mutate(
    Effect = ON_mean - OFF_mean,
    `Effect %` = sprintf("%.2f%%", (Effect / OFF_mean) * 100),
    Direction = ifelse(Effect < 0, "Beneficial", "Harmful")
  ) %>%
  arrange(Effect) %>%
  select(Loop, OFF_mean, ON_mean, Effect, `Effect %`, Direction)

save_table_dual(main_effects_300, "doe300_main_effects",
                "DoE 300 Reps: Main Effects of Feedback Loops on Performance (ns/word)",
                "tab:doe300_main_effects", digits = 2)

# Table 4: Top 10 Configurations
cat("  [4] DoE 300 Reps: Top 10 configurations\n")

top_configs_300 <- doe300 %>%
  group_by(L1_heat_tracking, L2_rolling_window, L3_linear_decay,
           L4_pipelining_metrics, L5_window_inference, L6_decay_inference,
           L7_adaptive_heartrate) %>%
  summarise(
    mean_ns = mean(ns_per_word),
    cv = sd(ns_per_word) / mean_ns * 100,
    n = n(),
    .groups = "drop"
  ) %>%
  arrange(mean_ns) %>%
  head(10) %>%
  mutate(
    Rank = row_number(),
    Config = paste0(L1_heat_tracking, L2_rolling_window, L3_linear_decay,
                   L4_pipelining_metrics, L5_window_inference,
                   L6_decay_inference, L7_adaptive_heartrate)
  ) %>%
  select(Rank, Config, `Mean (ns/word)` = mean_ns, `CV %` = cv, N = n)

save_table_dual(top_configs_300, "doe300_top_configs",
                "DoE 300 Reps: Top 10 Configurations Ranked by Performance",
                "tab:doe300_top_configs", digits = 2)

# ============================================================================
# WINDOW SCALING TABLE
# ============================================================================

cat("Processing Window Scaling experiment...\n")
cat("  [5] Window Scaling: Summary statistics\n")

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

window_summary <- window_df %>%
  group_by(window_size) %>%
  summarise(
    N = n(),
    `Mean K` = mean(mean_K),
    `SD K` = sd(mean_K),
    `|K-1.0|` = abs(`Mean K` - 1.0),
    `CV %` = mean(cv_interval),
    `Freq (Hz)` = mean(freq_hz),
    .groups = "drop"
  ) %>%
  arrange(window_size) %>%
  rename(`Window Size` = window_size)

save_table_dual(window_summary, "window_scaling_summary",
                "Window Scaling: Summary Statistics by Window Size",
                "tab:window_scaling_summary", digits = 3)

# ============================================================================
# L8 VALIDATION TABLES
# ============================================================================

cat("Processing L8 Validation experiment...\n")

l8_file <- "/home/rajames/CLionProjects/StarForth/experiments/l8_validation/l8_validation_20251208_102205/l8_validation_results.csv"
l8 <- read_csv(l8_file, show_col_types = FALSE) %>%
  mutate(
    ns_per_word = runtime_ns / 1000,
    runtime_ms = runtime_ns / 1e6
  )

# Table 6: Strategy Performance Summary
cat("  [6] L8 Validation: Strategy performance summary\n")

strategy_summary <- l8 %>%
  group_by(strategy) %>%
  summarise(
    N = n(),
    `Mean Runtime (ms)` = mean(runtime_ms),
    `SD (ms)` = sd(runtime_ms),
    `CV %` = `SD (ms)` / `Mean Runtime (ms)` * 100,
    .groups = "drop"
  ) %>%
  arrange(`Mean Runtime (ms)`) %>%
  mutate(Rank = row_number()) %>%
  select(Rank, Strategy = strategy, `Mean Runtime (ms)`, `CV %`, N)

save_table_dual(strategy_summary, "l8_strategy_summary",
                "L8 Validation: Overall Strategy Performance (all workloads combined)",
                "tab:l8_strategy_summary", digits = 2)

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
  ungroup() %>%
  select(Workload = workload_type, Rank = rank, Strategy = strategy,
         `Runtime (ms)` = mean_runtime)

save_table_dual(best_per_workload, "l8_best_per_workload",
                "L8 Validation: Top 3 Strategies per Workload Type",
                "tab:l8_best_per_workload", digits = 2)

# Table 8: L8_ADAPTIVE Margin
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
  select(Workload = workload_type, `L8 Runtime (ms)` = mean_runtime,
         `Best Runtime (ms)` = best_runtime, `Margin %` = margin_pct) %>%
  ungroup()

save_table_dual(l8_margin, "l8_adaptive_margin",
                "L8 Validation: L8_ADAPTIVE Performance Margin vs Best Static Config",
                "tab:l8_adaptive_margin", digits = 2)

# ============================================================================
# L8 ATTRACTOR TABLE
# ============================================================================

cat("Processing L8 Attractor experiment...\n")
cat("  [9] L8 Attractor: Overall summary statistics\n")

l8_attractor_file <- "/home/rajames/CLionProjects/StarForth/experiments/l8_attractor_map/results_20251209_145907/heartbeat_all.csv"
l8_attractor <- read_csv(l8_attractor_file, show_col_types = FALSE)

attractor_summary <- data.frame(
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

save_table_dual(attractor_summary, "l8_attractor_summary",
                "L8 Attractor: Overall Summary Statistics (all workloads combined)",
                "tab:l8_attractor_summary", digits = 2)

# ============================================================================
# CREATE HTML GALLERY INDEX
# ============================================================================

cat("\nGenerating HTML gallery index...\n")

gallery_html <- '<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StarForth Tables Gallery</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background: #f5f5f5;
            padding: 20px;
        }

        .container {
            max-width: 1400px;
            margin: 0 auto;
            background: white;
            padding: 40px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }

        header {
            border-bottom: 3px solid #2563eb;
            padding-bottom: 20px;
            margin-bottom: 40px;
        }

        h1 {
            font-size: 2.5em;
            color: #1e40af;
            margin-bottom: 10px;
        }

        .subtitle {
            font-size: 1.2em;
            color: #64748b;
        }

        .badge {
            display: inline-block;
            padding: 5px 12px;
            background: #10b981;
            color: white;
            border-radius: 4px;
            font-size: 0.9em;
            margin-right: 10px;
            margin-top: 10px;
        }

        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }

        .stat-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 25px;
            border-radius: 8px;
            text-align: center;
        }

        .stat-value {
            font-size: 2.5em;
            font-weight: bold;
            display: block;
            margin-bottom: 5px;
        }

        .stat-label {
            font-size: 0.9em;
            opacity: 0.9;
        }

        .gallery {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
            gap: 25px;
            margin: 30px 0;
        }

        .table-card {
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            overflow: hidden;
            background: white;
            transition: transform 0.2s, box-shadow 0.2s;
        }

        .table-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.1);
        }

        .table-header {
            background: #1e40af;
            color: white;
            padding: 20px;
        }

        .table-header h3 {
            margin-bottom: 8px;
        }

        .table-header p {
            font-size: 0.9em;
            opacity: 0.9;
        }

        .table-meta {
            padding: 15px 20px;
            background: #f9fafb;
            border-top: 1px solid #e5e7eb;
            font-size: 0.85em;
            color: #64748b;
        }

        .table-actions {
            padding: 20px;
            display: flex;
            gap: 10px;
        }

        .btn {
            flex: 1;
            padding: 10px 20px;
            text-decoration: none;
            text-align: center;
            border-radius: 6px;
            font-weight: 500;
            transition: all 0.2s;
        }

        .btn-view {
            background: #2563eb;
            color: white;
        }

        .btn-view:hover {
            background: #1d4ed8;
        }

        .btn-download {
            background: #10b981;
            color: white;
        }

        .btn-download:hover {
            background: #059669;
        }

        .category-section {
            margin: 50px 0;
        }

        .category-section h2 {
            color: #1e40af;
            margin-bottom: 10px;
            border-bottom: 2px solid #e5e7eb;
            padding-bottom: 10px;
        }

        .category-info {
            color: #64748b;
            margin-bottom: 20px;
            font-size: 0.95em;
        }

        footer {
            margin-top: 60px;
            padding-top: 30px;
            border-top: 2px solid #e5e7eb;
            text-align: center;
            color: #64748b;
        }

        code {
            background: #f1f5f9;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: "Courier New", monospace;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>StarForth Tables Gallery</h1>
            <p class="subtitle">9 Standalone & Reusable Tables from Real Experimental Data</p>
            <div>
                <span class="badge">✓ Standalone HTML</span>
                <span class="badge">✓ Reusable LaTeX</span>
                <span class="badge">✓ Real Data</span>
            </div>
        </header>

        <div class="stats">
            <div class="stat-card">
                <span class="stat-value">9</span>
                <span class="stat-label">Tables</span>
            </div>
            <div class="stat-card" style="background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);">
                <span class="stat-value">46K+</span>
                <span class="stat-label">Experimental Runs</span>
            </div>
            <div class="stat-card" style="background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);">
                <span class="stat-value">18</span>
                <span class="stat-label">Files (9×2)</span>
            </div>
        </div>

        <div class="category-section">
            <h2>DoE 30 Reps Experiment (2 tables)</h2>
            <p class="category-info">3,840 experimental runs from 2^7 factorial design with 30 replicates</p>
            <div class="gallery">
                <div class="table-card">
                    <div class="table-header">
                        <h3>Main Effects</h3>
                        <p>Impact of 7 feedback loops on performance</p>
                    </div>
                    <div class="table-meta">
                        7 rows × 6 columns | Label: <code>tab:doe30_main_effects</code>
                    </div>
                    <div class="table-actions">
                        <a href="doe30_main_effects.html" class="btn btn-view">View Table</a>
                        <a href="doe30_main_effects.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
                <div class="table-card">
                    <div class="table-header">
                        <h3>Top 10 Configurations</h3>
                        <p>Best performing loop configurations</p>
                    </div>
                    <div class="table-meta">
                        10 rows × 5 columns | Label: <code>tab:doe30_top_configs</code>
                    </div>
                    <div class="table-actions">
                        <a href="doe30_top_configs.html" class="btn btn-view">View Table</a>
                        <a href="doe30_top_configs.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
            </div>
        </div>

        <div class="category-section">
            <h2>DoE 300 Reps Experiment (2 tables)</h2>
            <p class="category-info">38,400 experimental runs from 2^7 factorial design with 300 replicates</p>
            <div class="gallery">
                <div class="table-card">
                    <div class="table-header">
                        <h3>Main Effects</h3>
                        <p>Impact of 7 feedback loops on performance</p>
                    </div>
                    <div class="table-meta">
                        7 rows × 6 columns | Label: <code>tab:doe300_main_effects</code>
                    </div>
                    <div class="table-actions">
                        <a href="doe300_main_effects.html" class="btn btn-view">View Table</a>
                        <a href="doe300_main_effects.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
                <div class="table-card">
                    <div class="table-header">
                        <h3>Top 10 Configurations</h3>
                        <p>Best performing loop configurations</p>
                    </div>
                    <div class="table-meta">
                        10 rows × 5 columns | Label: <code>tab:doe300_top_configs</code>
                    </div>
                    <div class="table-actions">
                        <a href="doe300_top_configs.html" class="btn btn-view">View Table</a>
                        <a href="doe300_top_configs.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
            </div>
        </div>

        <div class="category-section">
            <h2>Window Scaling Experiment (1 table)</h2>
            <p class="category-info">355 runs across 12 window sizes validating James Law</p>
            <div class="gallery">
                <div class="table-card">
                    <div class="table-header">
                        <h3>Summary Statistics</h3>
                        <p>K statistic, CV, and frequency by window size</p>
                    </div>
                    <div class="table-meta">
                        12 rows × 7 columns | Label: <code>tab:window_scaling_summary</code>
                    </div>
                    <div class="table-actions">
                        <a href="window_scaling_summary.html" class="btn btn-view">View Table</a>
                        <a href="window_scaling_summary.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
            </div>
        </div>

        <div class="category-section">
            <h2>L8 Validation Experiment (3 tables)</h2>
            <p class="category-info">1,813 runs testing L8 Jacquard mode selector (8 strategies × 5 workloads)</p>
            <div class="gallery">
                <div class="table-card">
                    <div class="table-header">
                        <h3>Strategy Performance</h3>
                        <p>Overall strategy ranking across workloads</p>
                    </div>
                    <div class="table-meta">
                        8 rows × 5 columns | Label: <code>tab:l8_strategy_summary</code>
                    </div>
                    <div class="table-actions">
                        <a href="l8_strategy_summary.html" class="btn btn-view">View Table</a>
                        <a href="l8_strategy_summary.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
                <div class="table-card">
                    <div class="table-header">
                        <h3>Best per Workload</h3>
                        <p>Top 3 strategies for each workload type</p>
                    </div>
                    <div class="table-meta">
                        15 rows × 4 columns | Label: <code>tab:l8_best_per_workload</code>
                    </div>
                    <div class="table-actions">
                        <a href="l8_best_per_workload.html" class="btn btn-view">View Table</a>
                        <a href="l8_best_per_workload.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
                <div class="table-card">
                    <div class="table-header">
                        <h3>L8_ADAPTIVE Margin</h3>
                        <p>Performance margin vs best static config</p>
                    </div>
                    <div class="table-meta">
                        5 rows × 4 columns | Label: <code>tab:l8_adaptive_margin</code>
                    </div>
                    <div class="table-actions">
                        <a href="l8_adaptive_margin.html" class="btn btn-view">View Table</a>
                        <a href="l8_adaptive_margin.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
            </div>
        </div>

        <div class="category-section">
            <h2>L8 Attractor Experiment (1 table)</h2>
            <p class="category-info">4,351 heartbeat ticks from L8 attractor mapping</p>
            <div class="gallery">
                <div class="table-card">
                    <div class="table-header">
                        <h3>Summary Statistics</h3>
                        <p>Overall statistics across all workloads</p>
                    </div>
                    <div class="table-meta">
                        6 rows × 2 columns | Label: <code>tab:l8_attractor_summary</code>
                    </div>
                    <div class="table-actions">
                        <a href="l8_attractor_summary.html" class="btn btn-view">View Table</a>
                        <a href="l8_attractor_summary.tex" class="btn btn-download" download>Download LaTeX</a>
                    </div>
                </div>
            </div>
        </div>

        <footer>
            <p>Generated from real experimental data | StarForth Project | December 2025</p>
            <p style="margin-top: 10px;">
                LaTeX Usage: <code>\\input{tables/tablename.tex}</code><br>
                All tables regenerated from CSV data using R/xtable
            </p>
        </footer>
    </div>
</body>
</html>'

writeLines(gallery_html, file.path(TABLE_DIR, "index.html"))

# ============================================================================
# SUMMARY
# ============================================================================

cat("\n================================================================================\n")
cat(sprintf("  REGENERATION COMPLETE: %d TABLES (DUAL FORMAT)\n", table_count))
cat("================================================================================\n")
cat(sprintf("\nOutput directory: %s\n", TABLE_DIR))
cat("\nGenerated files:\n")
cat("  - 9 LaTeX files (.tex)\n")
cat("  - 9 standalone HTML files (.html)\n")
cat("  - 1 gallery index (index.html)\n")
cat("  = 19 total files\n\n")
cat("Usage:\n")
cat("  LaTeX: \\input{tables/tablename.tex}\n")
cat("  HTML:  Open tables/index.html in browser\n")
cat("\n")
