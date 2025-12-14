#!/usr/bin/env Rscript

############################################################
# StarForth DOE — L1–L7 Behavioral Analysis Report
# Reproducible: Rscript doe_full_report.R
#
# Requires only: doe_results_20251122_085656.csv
############################################################

suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(htmltools)
  library(corrplot)
})

cat("Working directory:", getwd(), "\n")

# ----------------------------------------------------------
# 1. LOAD THE DATA
# ----------------------------------------------------------
results_file <- "./doe_results_20251123_093204.csv"

if (!file.exists(results_file)) {
  stop(paste("Missing:", results_file))
}

results <- read.csv(results_file, stringsAsFactors = FALSE)
cat("Loaded:", nrow(results), "rows,", ncol(results), "columns\n")


# ----------------------------------------------------------
# 2. DEFINE INPUT FACTORS (YOUR L1–L7 CONFIG BITS)
# ----------------------------------------------------------
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
  if (!(fc %in% names(results))) {
    stop(paste("Missing factor column:", fc))
  }
  results[[fc]] <- as.factor(results[[fc]])
}

has_rep <- "replicate" %in% names(results)
if (has_rep) results$replicate <- as.factor(results$replicate)


# ----------------------------------------------------------
# 3. ENSURE ns_per_word EXISTS
# ----------------------------------------------------------
if (!"ns_per_word" %in% names(results)) {
  if (all(c("workload_ns_q48","words_exec") %in% names(results))) {
    results <- results %>% mutate(
      ns_per_word = workload_ns_q48 / words_exec
    )
    cat("Derived ns_per_word.\n")
  } else {
    stop("ns_per_word missing and cannot derive.")
  }
}


# ----------------------------------------------------------
# 4. DETECT METRICS (EXCLUDE FACTORS + replicate + run_id)
# ----------------------------------------------------------
all_numeric <- names(results)[sapply(results, is.numeric)]
design_like <- c("run_id", "replicate", factor_cols)

metric_cols <- setdiff(all_numeric, design_like)
if (!"ns_per_word" %in% metric_cols)
  metric_cols <- c("ns_per_word", metric_cols)

cat("Metrics detected:\n")
print(metric_cols)


# ----------------------------------------------------------
# 5. MISSINGNESS PROFILE
# ----------------------------------------------------------
missing_profile <- tibble(
  metric = metric_cols,
  n      = nrow(results),
  n_na   = sapply(results[metric_cols], function(x) sum(is.na(x))),
  n_nan  = sapply(results[metric_cols], function(x)
    if (is.numeric(x)) sum(is.nan(x)) else NA_integer_),
  na_prop = n_na / n
)


# ----------------------------------------------------------
# 6. ns_per_word SUMMARY + ANOVA
# ----------------------------------------------------------
ns_summary <- summary(results$ns_per_word)
ns_sd <- sd(results$ns_per_word, na.rm = TRUE)

main_terms <- paste(factor_cols, collapse = " + ")
int_terms  <- paste0("(", main_terms, ")^2")

main_formula_str <- paste(
  "ns_per_word ~", main_terms,
  if (has_rep) " + replicate" else ""
)

int_formula_str <- paste(
  "ns_per_word ~", int_terms,
  if (has_rep) " + replicate" else ""
)

model_main <- lm(as.formula(main_formula_str), data = results)
model_int  <- lm(as.formula(int_formula_str),  data = results)

anova_main <- anova(model_main)
anova_int  <- anova(model_int)


# ----------------------------------------------------------
# 7. PLOT HANDLER
# ----------------------------------------------------------
plot_files <- list()

save_plot <- function(name, plot_obj) {
  file <- paste0(name, ".png")
  png(file, width = 1400, height = 900)
  if (inherits(plot_obj, "ggplot")) {
    print(plot_obj)
  } else {
    # For base R plots passed as expressions
    eval(plot_obj)
  }
  dev.off()
  plot_files[[name]] <<- file
  cat("  Saved:", file, "\n")
}


# ----------------------------------------------------------
# 8. ns_per_word DISTRIBUTION
# ----------------------------------------------------------
save_plot("dist_ns_per_word",
  ggplot(results, aes(x = ns_per_word)) +
    geom_histogram(bins = 40, alpha=0.7) +
    geom_density(linewidth=1.1) +
    theme_bw(base_size = 16) +
    labs(title="Distribution of ns_per_word")
)


# ----------------------------------------------------------
# 9. BOX PLOTS BY L1–L7
# ----------------------------------------------------------
for (fc in factor_cols) {
  p <- ggplot(results, aes(x = .data[[fc]], y = ns_per_word)) +
    geom_boxplot(outlier.alpha = 0.4) +
    theme_bw(base_size = 16) +
    labs(title = paste("ns_per_word by", fc))
  save_plot(paste0("box_", fc), p)
}


# ----------------------------------------------------------
# 10. METRIC CORRELATION HEATMAP
# ----------------------------------------------------------
metric_nonconst <- metric_cols[
  sapply(results[metric_cols], function(x) sd(x, na.rm=TRUE) > 0)
]

corr_matrix <- NULL
if (length(metric_nonconst) > 1) {
  corr_matrix <- cor(results[metric_nonconst], use="pairwise.complete.obs")
  save_plot("corr_metrics", quote({
    corrplot(corr_matrix, method="color", type="upper",
             tl.cex=0.7, mar=c(0,0,1,0))
  }))
}


# ----------------------------------------------------------
# 11. FACTOR → METRIC EFFECT TABLE
# ----------------------------------------------------------
effect_rows <- list()

for (fc in factor_cols) {
  lv <- levels(results[[fc]])
  if (length(lv) != 2) next

  for (m in metric_cols) {
    m0 <- results[[m]][results[[fc]] == lv[1]]
    m1 <- results[[m]][results[[fc]] == lv[2]]
    effect_rows[[length(effect_rows)+1]] <- data.frame(
      factor = fc,
      metric = m,
      effect = mean(m1,na.rm=TRUE) - mean(m0,na.rm=TRUE),
      stringsAsFactors = FALSE
    )
  }
}

effect_df <- if (length(effect_rows)>0) bind_rows(effect_rows) else NULL


# --------------------------
# FIXED BLOCK FOR FAILING slice_head()
# --------------------------
if (!is.null(effect_df)) {
  top_metrics_tbl <- effect_df %>%
    group_by(metric) %>%
    summarise(max_abs_effect = max(abs(effect), na.rm=TRUE), .groups="drop") %>%
    arrange(desc(max_abs_effect))

  # CORRECT FIX:
  N_top <- min(30, nrow(top_metrics_tbl))

  top_metrics <- top_metrics_tbl %>%
    slice_head(n = N_top) %>%
    pull(metric)

  effect_df_top <- effect_df %>%
    filter(metric %in% top_metrics)

  p <- ggplot(effect_df_top, aes(x=factor, y=metric, fill=effect)) +
    geom_tile() +
    scale_fill_gradient2(low="blue", high="red", mid="white") +
    theme_bw(base_size=12) +
    labs(title="Factor → Metric Effects")
  save_plot("effect_heatmap", p)
}


# ----------------------------------------------------------
# 12. FULL 128-CONFIGURATION ANALYSIS
# ----------------------------------------------------------
# Create binary configuration string for each row
results <- results %>%
  mutate(
    config_bits = paste0(
      L1_heat_tracking, L2_rolling_window, L3_linear_decay,
      L4_pipelining_metrics, L5_window_inference, L6_decay_inference,
      L7_adaptive_heartrate
    ),
    config_decimal = as.integer(L1_heat_tracking) * 64 +
                     as.integer(L2_rolling_window) * 32 +
                     as.integer(L3_linear_decay) * 16 +
                     as.integer(L4_pipelining_metrics) * 8 +
                     as.integer(L5_window_inference) * 4 +
                     as.integer(L6_decay_inference) * 2 +
                     as.integer(L7_adaptive_heartrate) * 1
  )

# Summary stats for all 128 configurations
config_summary <- results %>%
  group_by(config_bits, config_decimal,
           L1_heat_tracking, L2_rolling_window, L3_linear_decay,
           L4_pipelining_metrics, L5_window_inference, L6_decay_inference,
           L7_adaptive_heartrate) %>%
  summarise(
    n_reps = n(),
    mean_ns = mean(ns_per_word, na.rm = TRUE),
    sd_ns = sd(ns_per_word, na.rm = TRUE),
    se_ns = sd_ns / sqrt(n_reps),
    min_ns = min(ns_per_word, na.rm = TRUE),
    max_ns = max(ns_per_word, na.rm = TRUE),
    cv_pct = 100 * sd_ns / mean_ns,
    .groups = "drop"
  ) %>%
  arrange(config_decimal)

# Mark best/worst configurations
config_summary <- config_summary %>%
  mutate(
    rank = rank(mean_ns),
    is_best = rank == min(rank),
    is_worst = rank == max(rank)
  )

cat("\n=== ALL 128 CONFIGURATIONS ===\n")
cat("Best config:", config_summary$config_bits[config_summary$is_best],
    "mean =", format(config_summary$mean_ns[config_summary$is_best], scientific=TRUE), "\n")
cat("Worst config:", config_summary$config_bits[config_summary$is_worst],
    "mean =", format(config_summary$mean_ns[config_summary$is_worst], scientific=TRUE), "\n")

# Write full config table to CSV for patent documentation
write.csv(config_summary, "all_128_configurations.csv", row.names = FALSE)
cat("Saved: all_128_configurations.csv\n")

# PLOT: All 128 configurations ranked by performance
config_summary_sorted <- config_summary %>% arrange(mean_ns)
config_summary_sorted$config_bits <- factor(config_summary_sorted$config_bits,
                                            levels = config_summary_sorted$config_bits)

p <- ggplot(config_summary_sorted, aes(x = config_bits, y = mean_ns)) +
  geom_bar(stat = "identity", fill = "steelblue", alpha = 0.8) +
  geom_errorbar(aes(ymin = mean_ns - se_ns, ymax = mean_ns + se_ns),
                width = 0.3, color = "darkred") +
  theme_bw(base_size = 10) +
  theme(axis.text.x = element_text(angle = 90, hjust = 1, vjust = 0.5, size = 6)) +
  labs(title = "All 128 Configurations: L1L2L3L4L5L6L7",
       subtitle = "Sorted by mean ns_per_word (lower = faster)",
       x = "Configuration (L1-L7 bits)",
       y = "Mean ns_per_word")
save_plot("all_128_configs_ranked", p)

# PLOT: Heatmap-style grid of all configurations
# Create a 16x8 grid (L1L2L3L4 on y-axis, L5L6L7 on x-axis)
config_summary <- config_summary %>%
  mutate(
    L1234 = paste0(L1_heat_tracking, L2_rolling_window, L3_linear_decay, L4_pipelining_metrics),
    L567 = paste0(L5_window_inference, L6_decay_inference, L7_adaptive_heartrate)
  )

p <- ggplot(config_summary, aes(x = L567, y = L1234, fill = mean_ns)) +
  geom_tile(color = "white") +
  geom_text(aes(label = sprintf("%.1e", mean_ns)), size = 2.5) +
  scale_fill_gradient2(low = "darkgreen", mid = "yellow", high = "darkred",
                       midpoint = median(config_summary$mean_ns),
                       name = "ns/word") +
  theme_bw(base_size = 12) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "128-Configuration Heatmap",
       subtitle = "L1234 (rows) vs L567 (cols) - Green=Fast, Red=Slow",
       x = "L5 L6 L7",
       y = "L1 L2 L3 L4")
save_plot("config_heatmap_128", p)

# PLOT: CV% heatmap (variance analysis)
p <- ggplot(config_summary, aes(x = L567, y = L1234, fill = cv_pct)) +
  geom_tile(color = "white") +
  geom_text(aes(label = sprintf("%.1f%%", cv_pct)), size = 2.5) +
  scale_fill_gradient(low = "white", high = "purple", name = "CV%") +
  theme_bw(base_size = 12) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "128-Configuration Variance (CV%)",
       subtitle = "Coefficient of Variation across 30 replicates",
       x = "L5 L6 L7",
       y = "L1 L2 L3 L4")
save_plot("config_variance_128", p)

# ----------------------------------------------------------
# 12b. OPTIMAL STATE ANALYSIS (Speed + Stability)
# ----------------------------------------------------------
# The optimal performer is NOT just fastest - it's fastest + steady state
# that represents true optimal dynamic system behavior.

# Normalize both metrics to [0,1] where lower is better
config_summary <- config_summary %>%
  mutate(
    # Normalize speed (lower ns = better, so invert)
    speed_norm = (mean_ns - min(mean_ns)) / (max(mean_ns) - min(mean_ns)),
    # Normalize stability (lower CV = better)
    stability_norm = (cv_pct - min(cv_pct)) / (max(cv_pct) - min(cv_pct)),
    # Combined optimality score (equal weight speed + stability)
    # Lower = better (Pareto-optimal direction)
    optimality_score = sqrt(speed_norm^2 + stability_norm^2),
    # Alternative: weighted score favoring stability for steady-state
    steady_state_score = 0.4 * speed_norm + 0.6 * stability_norm
  )

# Rank by optimality
config_summary <- config_summary %>%
  arrange(optimality_score) %>%
  mutate(
    optimality_rank = row_number(),
    optimality_percentile = 100 * (1 - (optimality_rank - 1) / n()),
    is_top5pct = optimality_percentile >= 95
  )

# Top 5% optimal configurations
top5pct <- config_summary %>% filter(is_top5pct)
cat("\n=== TOP 5% OPTIMAL CONFIGURATIONS (Speed + Stability) ===\n")
cat("Criteria: Pareto-optimal balance of execution speed AND low variance\n")
cat(sprintf("Top 5%% threshold: %d configurations\n", nrow(top5pct)))
print(top5pct %>% select(config_bits, mean_ns, cv_pct, optimality_score, optimality_percentile))

# Save optimal configs for patent
write.csv(config_summary %>% arrange(optimality_rank),
          "optimal_configurations_ranked.csv", row.names = FALSE)
cat("Saved: optimal_configurations_ranked.csv\n")

# PLOT: Pareto frontier (Speed vs Stability scatter)
p <- ggplot(config_summary, aes(x = mean_ns, y = cv_pct)) +
  geom_point(aes(color = optimality_percentile, size = is_top5pct), alpha = 0.7) +
  geom_point(data = top5pct, aes(x = mean_ns, y = cv_pct),
             color = "red", size = 4, shape = 1, stroke = 2) +
  geom_text(data = top5pct, aes(label = config_bits),
            hjust = -0.1, vjust = 0, size = 3, color = "darkred") +
  scale_color_gradient(low = "red", high = "darkgreen", name = "Optimality %ile") +
  scale_size_manual(values = c(2, 4), guide = "none") +
  theme_bw(base_size = 14) +
  labs(title = "Pareto Frontier: Speed vs Stability",
       subtitle = "Top 5% optimal configs circled in red (lower-left = better)",
       x = "Mean ns_per_word (lower = faster)",
       y = "CV% (lower = more stable)")
save_plot("pareto_speed_stability", p)

# PLOT: Optimality score distribution with top 5% highlighted
p <- ggplot(config_summary, aes(x = optimality_score)) +
  geom_histogram(aes(fill = is_top5pct), bins = 30, alpha = 0.8, color = "white") +
  geom_vline(xintercept = max(top5pct$optimality_score),
             linetype = "dashed", color = "red", linewidth = 1) +
  scale_fill_manual(values = c("gray50", "darkgreen"),
                    labels = c("Other", "Top 5%"), name = "") +
  theme_bw(base_size = 14) +
  labs(title = "Distribution of Optimality Scores",
       subtitle = sprintf("Top 5%% threshold: score ≤ %.3f", max(top5pct$optimality_score)),
       x = "Optimality Score (lower = better)",
       y = "Count")
save_plot("optimality_distribution", p)

# PLOT: Bell curve fit with top 5% shaded
# Fit normal distribution to optimality scores
opt_mean <- mean(config_summary$optimality_score)
opt_sd <- sd(config_summary$optimality_score)
top5_threshold <- max(top5pct$optimality_score)

x_seq <- seq(min(config_summary$optimality_score) - 0.1,
             max(config_summary$optimality_score) + 0.1, length.out = 200)
bell_df <- data.frame(x = x_seq, y = dnorm(x_seq, mean = opt_mean, sd = opt_sd))

p <- ggplot() +
  geom_histogram(data = config_summary, aes(x = optimality_score, y = after_stat(density)),
                 bins = 25, fill = "steelblue", alpha = 0.5, color = "white") +
  geom_line(data = bell_df, aes(x = x, y = y), color = "darkblue", linewidth = 1.2) +
  geom_area(data = bell_df %>% filter(x <= top5_threshold),
            aes(x = x, y = y), fill = "darkgreen", alpha = 0.4) +
  geom_vline(xintercept = top5_threshold, linetype = "dashed", color = "red", linewidth = 1) +
  annotate("text", x = top5_threshold, y = max(bell_df$y) * 0.9,
           label = "Top 5%", hjust = 1.1, color = "darkgreen", fontface = "bold") +
  theme_bw(base_size = 14) +
  labs(title = "Optimality Score: Normal Distribution Fit",
       subtitle = sprintf("μ = %.3f, σ = %.3f | Top 5%% ≤ %.3f", opt_mean, opt_sd, top5_threshold),
       x = "Optimality Score (Speed + Stability)",
       y = "Density")
save_plot("optimality_bell_curve", p)

# PLOT: Top 5% configs detail view
top5pct_long <- top5pct %>%
  select(config_bits, mean_ns, cv_pct, optimality_score) %>%
  mutate(config_bits = factor(config_bits, levels = config_bits[order(optimality_score)])) %>%
  pivot_longer(cols = c(mean_ns, cv_pct, optimality_score),
               names_to = "metric", values_to = "value")

# Normalize for comparison
top5pct_normalized <- top5pct %>%
  mutate(
    speed_pct = 100 * (1 - speed_norm),  # Higher = faster
    stability_pct = 100 * (1 - stability_norm),  # Higher = more stable
    overall_pct = optimality_percentile
  ) %>%
  select(config_bits, speed_pct, stability_pct, overall_pct) %>%
  pivot_longer(cols = c(speed_pct, stability_pct, overall_pct),
               names_to = "metric", values_to = "percentile")

p <- ggplot(top5pct_normalized, aes(x = reorder(config_bits, -percentile),
                                     y = percentile, fill = metric)) +
  geom_bar(stat = "identity", position = "dodge", alpha = 0.8) +
  scale_fill_manual(values = c("overall_pct" = "purple",
                               "speed_pct" = "steelblue",
                               "stability_pct" = "darkgreen"),
                    labels = c("Overall", "Speed", "Stability")) +
  theme_bw(base_size = 12) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "Top 5% Optimal Configurations: Component Scores",
       subtitle = "Higher percentile = better performance in that dimension",
       x = "Configuration (L1L2L3L4L5L6L7)",
       y = "Percentile Score",
       fill = "Metric")
save_plot("top5pct_breakdown", p)

# PLOT: Heatmap showing which loops are ON in top 5%
top5_loop_freq <- top5pct %>%
  summarise(
    L1 = mean(as.numeric(as.character(L1_heat_tracking))),
    L2 = mean(as.numeric(as.character(L2_rolling_window))),
    L3 = mean(as.numeric(as.character(L3_linear_decay))),
    L4 = mean(as.numeric(as.character(L4_pipelining_metrics))),
    L5 = mean(as.numeric(as.character(L5_window_inference))),
    L6 = mean(as.numeric(as.character(L6_decay_inference))),
    L7 = mean(as.numeric(as.character(L7_adaptive_heartrate)))
  ) %>%
  pivot_longer(everything(), names_to = "loop", values_to = "freq_on")

# Compare to overall frequency (should be 0.5 if uniform)
all_loop_freq <- config_summary %>%
  summarise(
    L1 = mean(as.numeric(as.character(L1_heat_tracking))),
    L2 = mean(as.numeric(as.character(L2_rolling_window))),
    L3 = mean(as.numeric(as.character(L3_linear_decay))),
    L4 = mean(as.numeric(as.character(L4_pipelining_metrics))),
    L5 = mean(as.numeric(as.character(L5_window_inference))),
    L6 = mean(as.numeric(as.character(L6_decay_inference))),
    L7 = mean(as.numeric(as.character(L7_adaptive_heartrate)))
  ) %>%
  pivot_longer(everything(), names_to = "loop", values_to = "baseline")

loop_comparison <- left_join(top5_loop_freq, all_loop_freq, by = "loop") %>%
  mutate(enrichment = freq_on - baseline)

p <- ggplot(loop_comparison, aes(x = loop, y = freq_on, fill = enrichment)) +
  geom_bar(stat = "identity", alpha = 0.8) +
  geom_hline(yintercept = 0.5, linetype = "dashed", color = "gray50") +
  scale_fill_gradient2(low = "red", mid = "white", high = "darkgreen", midpoint = 0,
                       name = "Enrichment\nvs baseline") +
  theme_bw(base_size = 14) +
  labs(title = "Loop Activation Frequency in Top 5% Optimal Configs",
       subtitle = "Dashed line = expected 50% if random | Green = enriched, Red = depleted",
       x = "Feedback Loop",
       y = "Fraction ON in Top 5%")
save_plot("top5pct_loop_enrichment", p)

# ----------------------------------------------------------
# 12b-ii. TOP 5% vs BASELINE (0000000) COMPARISON
# ----------------------------------------------------------
baseline_config <- config_summary %>% filter(config_bits == "0000000")
baseline_mean <- baseline_config$mean_ns
baseline_cv <- baseline_config$cv_pct

cat("\n=== BASELINE CONFIG (0000000) ===\n")
cat(sprintf("Mean ns_per_word: %.2e\n", baseline_mean))
cat(sprintf("CV%%: %.2f%%\n", baseline_cv))

# Get raw data for baseline
baseline_raw <- results %>% filter(config_bits == "0000000")

# Compare each top 5% config to baseline with t-tests
top5_vs_baseline <- data.frame()
for (i in seq_len(nrow(top5pct))) {
  cfg <- top5pct$config_bits[i]
  cfg_raw <- results %>% filter(config_bits == cfg)

  t_result <- t.test(cfg_raw$ns_per_word, baseline_raw$ns_per_word)

  top5_vs_baseline <- bind_rows(top5_vs_baseline, data.frame(
    config_bits = cfg,
    mean_ns = top5pct$mean_ns[i],
    cv_pct = top5pct$cv_pct[i],
    baseline_mean = baseline_mean,
    baseline_cv = baseline_cv,
    speedup_vs_baseline = (baseline_mean - top5pct$mean_ns[i]) / baseline_mean * 100,
    cv_improvement = baseline_cv - top5pct$cv_pct[i],
    t_statistic = t_result$statistic,
    t_pvalue = t_result$p.value,
    t_significant = t_result$p.value < 0.05,
    optimality_score = top5pct$optimality_score[i],
    optimality_percentile = top5pct$optimality_percentile[i],
    stringsAsFactors = FALSE
  ))
}

cat("\n=== TOP 5% vs BASELINE (0000000) ===\n")
print(top5_vs_baseline %>% select(config_bits, mean_ns, speedup_vs_baseline,
                                   cv_pct, cv_improvement, t_pvalue, t_significant))

# Save comparison table
write.csv(top5_vs_baseline, "top5pct_vs_baseline.csv", row.names = FALSE)
cat("Saved: top5pct_vs_baseline.csv\n")

# PLOT: Top 5% vs baseline bar comparison
comparison_data <- bind_rows(
  baseline_config %>%
    select(config_bits, mean_ns, cv_pct) %>%
    mutate(group = "Baseline"),
  top5pct %>%
    select(config_bits, mean_ns, cv_pct) %>%
    mutate(group = "Top 5%")
)

comparison_data$config_bits <- factor(comparison_data$config_bits,
                                       levels = c("0000000", top5pct$config_bits))

p <- ggplot(comparison_data, aes(x = config_bits, y = mean_ns, fill = group)) +
  geom_bar(stat = "identity", alpha = 0.8) +
  geom_hline(yintercept = baseline_mean, linetype = "dashed", color = "red", linewidth = 1) +
  scale_fill_manual(values = c("Baseline" = "gray50", "Top 5%" = "darkgreen")) +
  theme_bw(base_size = 14) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "Top 5% Optimal Configs vs Baseline (0000000)",
       subtitle = sprintf("Baseline mean = %.2e ns/word (red dashed line)", baseline_mean),
       x = "Configuration (L1L2L3L4L5L6L7)",
       y = "Mean ns_per_word",
       fill = "")
save_plot("top5pct_vs_baseline_speed", p)

# PLOT: Speedup percentage vs baseline
p <- ggplot(top5_vs_baseline, aes(x = reorder(config_bits, -speedup_vs_baseline),
                                   y = speedup_vs_baseline,
                                   fill = t_significant)) +
  geom_bar(stat = "identity", alpha = 0.8) +
  geom_hline(yintercept = 0, color = "black") +
  geom_text(aes(label = sprintf("%.1f%%\np=%.2e", speedup_vs_baseline, t_pvalue)),
            vjust = -0.3, size = 3) +
  scale_fill_manual(values = c("FALSE" = "gray70", "TRUE" = "steelblue"),
                    labels = c("Not Significant", "p < 0.05"), name = "T-test") +
  theme_bw(base_size = 14) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "Speedup vs Baseline (0000000) with Statistical Significance",
       subtitle = "Positive = faster than baseline | T-test p-values shown",
       x = "Configuration",
       y = "Speedup (%)")
save_plot("top5pct_speedup_vs_baseline", p)

# PLOT: Dual metric comparison (speed + stability vs baseline)
dual_comparison <- top5_vs_baseline %>%
  select(config_bits, speedup_vs_baseline, cv_improvement) %>%
  pivot_longer(cols = c(speedup_vs_baseline, cv_improvement),
               names_to = "metric", values_to = "improvement") %>%
  mutate(metric = ifelse(metric == "speedup_vs_baseline", "Speed (%)", "Stability (CV% reduction)"))

p <- ggplot(dual_comparison, aes(x = config_bits, y = improvement, fill = metric)) +
  geom_bar(stat = "identity", position = "dodge", alpha = 0.8) +
  geom_hline(yintercept = 0, color = "black", linetype = "dashed") +
  scale_fill_manual(values = c("Speed (%)" = "steelblue", "Stability (CV% reduction)" = "darkgreen")) +
  theme_bw(base_size = 12) +
  theme(axis.text.x = element_text(angle = 45, hjust = 1)) +
  labs(title = "Top 5% Improvement vs Baseline (0000000)",
       subtitle = "Speed = % faster | Stability = CV% reduction (positive = better)",
       x = "Configuration",
       y = "Improvement vs Baseline",
       fill = "Metric")
save_plot("top5pct_dual_improvement", p)

# Statistical test: which loops are significantly enriched/depleted in top 5%?
cat("\n=== LOOP ENRICHMENT IN TOP 5% (Chi-squared tests) ===\n")
for (loop_col in factor_cols) {
  loop_short <- gsub("_.*", "", gsub("L([0-9])_.*", "L\\1", loop_col))
  top5_on <- sum(as.numeric(as.character(top5pct[[loop_col]])))
  top5_n <- nrow(top5pct)
  all_on <- sum(as.numeric(as.character(config_summary[[loop_col]])))
  all_n <- nrow(config_summary)

  # Proportion test
  test <- prop.test(c(top5_on, all_on - top5_on), c(top5_n, all_n - top5_n))
  enriched <- if (top5_on/top5_n > 0.5) "ENRICHED" else "DEPLETED"
  sig <- if (test$p.value < 0.05) "*" else ""

  cat(sprintf("%s: %.1f%% ON in top 5%% vs 50%% baseline | %s %s (p=%.3f)\n",
              loop_short, 100*top5_on/top5_n, enriched, sig, test$p.value))
}

# ----------------------------------------------------------
# 12c. ALL 21 PAIRWISE INTERACTIONS
# ----------------------------------------------------------
# Prepare ANOVA interaction data for p-value lookup
anova_int_df <- as.data.frame(anova_int)
anova_int_df$term <- rownames(anova_int_df)

# Generate all C(7,2) = 21 pairwise interaction plots
interaction_pairs <- combn(factor_cols, 2, simplify = FALSE)

for (idx in seq_along(interaction_pairs)) {
  f1 <- interaction_pairs[[idx]][1]
  f2 <- interaction_pairs[[idx]][2]

  # Short names for plot
  f1_short <- gsub("_.*", "", gsub("L([0-9])_.*", "L\\1", f1))
  f2_short <- gsub("_.*", "", gsub("L([0-9])_.*", "L\\1", f2))

  # Compute means for interaction plot
  int_means <- results %>%
    group_by(across(all_of(c(f1, f2)))) %>%
    summarise(mean_ns = mean(ns_per_word, na.rm = TRUE),
              se_ns = sd(ns_per_word, na.rm = TRUE) / sqrt(n()),
              .groups = "drop")

  # Get p-value from ANOVA if available
  term_name <- paste0(f1, ":", f2)
  p_val <- anova_int_df$`Pr(>F)`[anova_int_df$term == term_name]
  if (length(p_val) == 0) p_val <- NA

  p <- ggplot(int_means, aes(x = .data[[f1]], y = mean_ns,
                             color = .data[[f2]], group = .data[[f2]])) +
    geom_point(size = 4) +
    geom_line(linewidth = 1.2) +
    geom_errorbar(aes(ymin = mean_ns - se_ns, ymax = mean_ns + se_ns),
                  width = 0.15, linewidth = 0.8) +
    theme_bw(base_size = 14) +
    labs(title = paste("Interaction:", f1_short, "x", f2_short),
         subtitle = if (!is.na(p_val)) sprintf("ANOVA p = %.2e", p_val) else "p-value N/A",
         x = f1,
         y = "Mean ns_per_word",
         color = f2)

  save_plot(sprintf("interaction_%02d_%s_x_%s", idx, f1_short, f2_short), p)
}

cat("Generated 21 pairwise interaction plots\n")

# Save ANOVA interaction results_run_01_2025_12_08 for patent documentation
interaction_rows <- grep(":", anova_int_df$term)
interaction_anova <- anova_int_df[interaction_rows, ]
interaction_anova <- interaction_anova[order(interaction_anova$`Pr(>F)`), ]
write.csv(interaction_anova, "interaction_anova_results.csv", row.names = FALSE)
cat("Saved: interaction_anova_results.csv\n")

# ----------------------------------------------------------
# 13. KEY METRICS vs FACTOR FACETED BOX PLOTS
# ----------------------------------------------------------
metric_no_ns <- setdiff(metric_cols, "ns_per_word")

cors <- sapply(metric_no_ns, function(m)
  suppressWarnings(cor(results[[m]], results$ns_per_word,
                       use="pairwise.complete.obs")))

cors <- cors[!is.na(cors)]
ord <- sort(abs(cors), decreasing=TRUE)

key_metrics <- names(ord)[seq_len(min(8, length(ord)))]

for (fc in factor_cols) {
  long_df <- results %>%
    select(all_of(c(fc, key_metrics))) %>%
    pivot_longer(all_of(key_metrics),
                 names_to="metric", values_to="value")

  p <- ggplot(long_df, aes(x = .data[[fc]], y = value)) +
    geom_boxplot(outlier.alpha = 0.4) +
    facet_wrap(~metric, scales = "free_y") +
    theme_bw(base_size = 12) +
    labs(title = paste("Key metrics by", fc))
  save_plot(paste0("facet_", fc), p)
}


# ----------------------------------------------------------
# 14. HTML REPORT
# ----------------------------------------------------------
html <- tags$html(
  tags$head(tags$title("StarForth DOE Full Report")),
  tags$body(
    h1("StarForth / StarshipOS – Full DOE Report"),
    p("Generated:", as.character(Sys.time())),
    p("Runs:", nrow(results), " | Configurations: 128 | Replicates: 30"),

    h2("1. ns_per_word Distribution"),
    img(src = plot_files[["dist_ns_per_word"]], width="800px"),

    h2("2. ANOVA – Main Effects"),
    tags$pre(paste(capture.output(anova_main), collapse="\n")),

    h2("3. ANOVA – 2-Way Interactions"),
    tags$pre(paste(capture.output(anova_int), collapse="\n")),

    h2("4. All 128 Configurations"),
    h3("4a. Ranked Bar Chart"),
    img(src = plot_files[["all_128_configs_ranked"]], width="1200px"),
    h3("4b. 16x8 Performance Heatmap (L1234 vs L567)"),
    img(src = plot_files[["config_heatmap_128"]], width="900px"),
    h3("4c. Variance Heatmap (CV%)"),
    img(src = plot_files[["config_variance_128"]], width="900px"),
    p("Full data: all_128_configurations.csv"),

    h2("5. OPTIMAL STATE ANALYSIS (Speed + Stability)"),
    p("The optimal performer is NOT just the fastest - it's the configuration that achieves ",
      "both fast execution AND steady-state behavior, representing the true optimal ",
      "dynamic system equilibrium."),
    h3("5a. Pareto Frontier: Speed vs Stability"),
    img(src = plot_files[["pareto_speed_stability"]], width="900px"),
    p("Lower-left corner = Pareto-optimal (fast AND stable). Top 5% configs circled in red."),

    h3("5b. Optimality Score Distribution"),
    img(src = plot_files[["optimality_distribution"]], width="800px"),

    h3("5c. Bell Curve Fit with Top 5% Highlighted"),
    img(src = plot_files[["optimality_bell_curve"]], width="900px"),
    p("Green shaded region shows the top 5% of configurations by optimality score."),

    h3("5d. Top 5% Configuration Breakdown"),
    img(src = plot_files[["top5pct_breakdown"]], width="900px"),
    p("Component scores for each top 5% config: Speed, Stability, and Overall."),

    h3("5e. Loop Enrichment in Top 5%"),
    img(src = plot_files[["top5pct_loop_enrichment"]], width="800px"),
    p("Which feedback loops are over/under-represented in optimal configurations?"),

    h3("5f. Top 5% Optimal Configurations"),
    tags$pre(paste(capture.output(
      top5pct %>% select(config_bits, mean_ns, cv_pct, optimality_score, optimality_percentile)
    ), collapse="\n")),
    p("Full ranking: optimal_configurations_ranked.csv"),

    h3("5g. Top 5% vs Baseline (0000000) Comparison"),
    p(sprintf("Baseline config (0000000): mean = %.2e ns/word, CV = %.2f%%", baseline_mean, baseline_cv)),
    img(src = plot_files[["top5pct_vs_baseline_speed"]], width="900px"),
    h4("Speedup with Statistical Significance (T-test)"),
    img(src = plot_files[["top5pct_speedup_vs_baseline"]], width="900px"),
    h4("Dual Improvement: Speed + Stability"),
    img(src = plot_files[["top5pct_dual_improvement"]], width="900px"),
    h4("Detailed Comparison Table"),
    tags$pre(paste(capture.output(
      top5_vs_baseline %>% select(config_bits, mean_ns, speedup_vs_baseline,
                                   cv_pct, cv_improvement, t_pvalue, t_significant)
    ), collapse="\n")),
    p("Full comparison: top5pct_vs_baseline.csv"),

    h2("6. All 21 Pairwise Interaction Plots"),
    lapply(seq_along(interaction_pairs), function(idx) {
      f1_short <- gsub("_.*", "", gsub("L([0-9])_.*", "L\\1", interaction_pairs[[idx]][1]))
      f2_short <- gsub("_.*", "", gsub("L([0-9])_.*", "L\\1", interaction_pairs[[idx]][2]))
      plot_name <- sprintf("interaction_%02d_%s_x_%s", idx, f1_short, f2_short)
      list(
        h4(paste0(idx, ". ", f1_short, " x ", f2_short)),
        img(src = plot_files[[plot_name]], width = "700px")
      )
    }),
    p("ANOVA results: interaction_anova_results.csv"),

    h2("6. ns_per_word by Individual Factors"),
    lapply(factor_cols, function(fc) {
      list(
        h3(fc),
        img(src = plot_files[[paste0("box_", fc)]], width="700px")
      )
    }),

    h2("7. Metric Correlation Heatmap"),
    if (!is.null(corr_matrix))
      img(src = plot_files[["corr_metrics"]], width="800px")
    else p("Not enough non-constant metrics."),

    h2("8. Factor → Metric Effect Map"),
    if (!is.null(effect_df))
      img(src = plot_files[["effect_heatmap"]], width="800px")
    else p("No effect matrix"),

    h2("9. Key Metrics vs Factors"),
    lapply(factor_cols, function(fc) {
      list(
        h3(fc),
        img(src = plot_files[[paste0("facet_", fc)]], width="800px")
      )
    }),

    h2("10. Missingness Profile"),
    tags$pre(paste(capture.output(missing_profile), collapse="\n")),

    h2("Output Files"),
    tags$ul(
      tags$li("all_128_configurations.csv - Full config summary"),
      tags$li("interaction_anova_results.csv - ANOVA interaction p-values"),
      tags$li("doe_full_report.html - This report")
    )
  )
)

save_html(html, "doe_full_report.html")
cat("\nDONE. Open doe_full_report.html\n")
