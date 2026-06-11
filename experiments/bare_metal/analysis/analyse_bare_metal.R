#!/usr/bin/env Rscript
# analyse_bare_metal.R
# StarForth LithosAnanke — Multi-Architecture Bare-Metal DoE Analysis
#
# Key insight from data exploration:
#  - tick_interval_ns is constant at 10000 (QEMU deterministic 10µs timer).
#    HR-based phase portraits (as in l8_attractor_map) do not apply here.
#  - The interesting dynamics are in:
#      window_width  — adaptive rolling window BREATHES 256→512→1024→2048→4096→...
#      avg_word_heat_q48 — monotonically rising thermal accumulation
#      hot_word_count — convergent excited-state count
#      time_trust_q48 / variance_q48 — arch-specific timer quality metrics
#  - Rows 1~26251 are IDENTICAL across all three ISAs (deterministic boot/POST).
#  - Divergence starts at row ~26252 in arch-specific timer metrics.
#
# The centrepiece chart: window_width[n] vs window_width[n+1] (Poincaré map).
# The adaptive window orbits a fixed cycle through powers of 2 — this IS the
# compudynamics attractor for bare-metal execution.

suppressPackageStartupMessages({
  library(ggplot2)
  library(svglite)
  library(dplyr)
  library(tidyr)
  library(scales)
  library(patchwork)
  library(viridis)
})

# ── paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR <- tryCatch(
  dirname(normalizePath(sys.frames()[[1]]$ofile)),
  error = function(e) getwd()
)
BASE_DIR   <- normalizePath(file.path(SCRIPT_DIR, ".."))
DATA_DIR   <- file.path(BASE_DIR, "latest")
OUT_CHARTS <- file.path(SCRIPT_DIR, "charts")
OUT_TABLES <- file.path(SCRIPT_DIR, "tables")
dir.create(OUT_CHARTS, showWarnings = FALSE, recursive = TRUE)
dir.create(OUT_TABLES, showWarnings = FALSE, recursive = TRUE)

cat("══════════════════════════════════════════════════════════════════\n")
cat("  StarForth LithosAnanke — Bare-Metal DoE Analysis\n")
cat("══════════════════════════════════════════════════════════════════\n\n")

Q48 <- 65536.0

load_arch <- function(arch) {
  path <- file.path(DATA_DIR, paste0(arch, ".csv"))
  cat(sprintf("  Loading %s ... ", arch))
  df <- read.csv(path, stringsAsFactors = FALSE)
  cat(sprintf("%d rows\n", nrow(df)))
  df$arch <- arch

  # Guard: remove degenerate rows
  df <- df[!is.na(df$tick_interval_ns) & df$tick_interval_ns > 0, ]

  # Q48.16 decode
  df$heat       <- df$avg_word_heat_q48 / Q48
  df$variance   <- df$variance_q48      / Q48
  df$time_trust <- df$time_trust_q48    / Q48

  # Elapsed in seconds
  df$elapsed_s  <- df$elapsed_ns / 1e9

  # Lag-1 window step for Poincaré map
  df$win_next   <- c(df$window_width[-1], NA_real_)
  df$delta_win  <- c(diff(df$window_width), NA_real_)
  df$delta_heat <- c(diff(df$heat), NA_real_)

  df
}

archs    <- c("amd64", "aarch64", "riscv64")
all_data <- lapply(archs, load_arch)
names(all_data) <- archs
df_all <- bind_rows(all_data)
df_all$arch <- factor(df_all$arch, levels = archs)

cat(sprintf("\n  Total rows: %d\n\n", nrow(df_all)))

# ── summary table ─────────────────────────────────────────────────────────────
cat("Computing per-arch summary...\n")

summary_tbl <- df_all %>%
  group_by(arch) %>%
  summarise(
    n_ticks          = n(),
    elapsed_s        = max(elapsed_s, na.rm = TRUE),
    mean_exec_delta  = mean(word_executions_delta, na.rm = TRUE),
    sd_exec_delta    = sd(word_executions_delta,   na.rm = TRUE),
    mean_hot_words   = mean(hot_word_count,         na.rm = TRUE),
    mean_heat        = mean(heat,                   na.rm = TRUE),
    max_heat         = max(heat,                    na.rm = TRUE),
    mean_win         = mean(window_width,           na.rm = TRUE),
    mean_trust       = mean(time_trust,             na.rm = TRUE),
    mean_variance    = mean(variance,               na.rm = TRUE),
    .groups = "drop"
  )

write.csv(summary_tbl, file.path(OUT_TABLES, "arch_summary.csv"), row.names = FALSE)
cat("  Saved: arch_summary.csv\n")
print(as.data.frame(summary_tbl))

# Statistical tests on the divergent region (rows after ~26251)
# Use time_trust and variance which differ per arch.
# If run length == 26251 (3-rep standard sweep) df_tail will be empty;
# fall back to the last 10% of each arch's data.
df_tail <- df_all %>%
  group_by(arch) %>%
  filter(row_number() > 26251) %>%
  ungroup()

if (nrow(df_tail) == 0) {
  cat("  NOTE: No rows beyond 26251 — using last 10% per arch as divergent region\n")
  df_tail <- df_all %>%
    group_by(arch) %>%
    slice_tail(prop = 0.10) %>%
    ungroup()
}

n_groups_trust <- df_tail %>% pull(time_trust) %>% unique() %>% length()
n_groups_var   <- df_tail %>% pull(variance)   %>% unique() %>% length()

kw_note <- ""
if (n_groups_trust < 2) {
  kw_note <- "SKIP — all time_trust values identical across architectures (algorithmic variance = 0)"
  cat(sprintf("\nKruskal-Wallis: time_trust_q48 ~ architecture: %s\n", kw_note))
  kw_trust <- list(statistic = 0, parameter = 0, p.value = 1)
} else {
  cat("\nKruskal-Wallis: time_trust_q48 ~ architecture (divergent region)\n")
  kw_trust <- kruskal.test(time_trust ~ arch, data = df_tail)
  cat(sprintf("  H=%.4f  df=%d  p=%.4e\n", kw_trust$statistic, kw_trust$parameter, kw_trust$p.value))
}

if (n_groups_var < 2) {
  kw_var_note <- "SKIP — all variance values identical across architectures (algorithmic variance = 0)"
  cat(sprintf("Kruskal-Wallis: variance_q48 ~ architecture: %s\n\n", kw_var_note))
  kw_var <- list(statistic = 0, parameter = 0, p.value = 1)
} else {
  cat("Kruskal-Wallis: variance_q48 ~ architecture (divergent region)\n")
  kw_var <- kruskal.test(variance ~ arch, data = df_tail)
  cat(sprintf("  H=%.4f  df=%d  p=%.4e\n\n", kw_var$statistic, kw_var$parameter, kw_var$p.value))
}

sink(file.path(OUT_TABLES, "kw_results.txt"))
cat("Kruskal-Wallis tests (divergent region)\n")
cat("════════════════════════════════════════\n\n")
if (nzchar(kw_note)) {
  cat(sprintf("time_trust_q48 ~ architecture: %s\n", kw_note))
} else {
  cat("time_trust_q48 ~ architecture\n"); print(kw_trust)
}
cat("\n")
if (nzchar(kw_note)) {
  cat(sprintf("variance_q48 ~ architecture: %s\n", kw_note))
} else {
  cat("variance_q48 ~ architecture\n"); print(kw_var)
}
sink()
cat("  Saved: kw_results.txt\n\n")

# ── colour themes ─────────────────────────────────────────────────────────────
arch_colours <- c(amd64   = "#E07B39",
                  aarch64 = "#4A90D9",
                  riscv64 = "#50C878")

theme_light_report <- function() {
  theme_minimal(base_size = 11) %+replace%
    theme(
      panel.grid.minor = element_blank(),
      panel.grid.major = element_line(colour = "grey90"),
      strip.text       = element_text(face = "bold"),
      plot.title       = element_text(face = "bold", size = 12),
      plot.subtitle    = element_text(colour = "grey40", size = 9),
      legend.position  = "bottom"
    )
}

theme_dark_report <- function() {
  theme_minimal(base_size = 11) %+replace%
    theme(
      panel.background = element_rect(fill = "#0d0d0d", colour = NA),
      plot.background  = element_rect(fill = "#0d0d0d", colour = NA),
      panel.grid.major = element_line(colour = "#1e1e1e"),
      panel.grid.minor = element_blank(),
      axis.text        = element_text(colour = "#aaaaaa"),
      axis.title       = element_text(colour = "#cccccc"),
      strip.text       = element_text(colour = "white", face = "bold"),
      plot.title       = element_text(colour = "white", face = "bold", size = 12),
      plot.subtitle    = element_text(colour = "#666666", size = 9),
      legend.text      = element_text(colour = "#aaaaaa"),
      legend.title     = element_text(colour = "#cccccc"),
      legend.background = element_rect(fill = "#0d0d0d", colour = NA),
      legend.position  = "bottom"
    )
}

save_svg <- function(plot, name, w = 12, h = 7) {
  path <- file.path(OUT_CHARTS, paste0(name, ".svg"))
  svglite(path, width = w, height = h)
  print(plot)
  dev.off()
  cat(sprintf("  Saved: %s.svg\n", name))
  invisible(path)
}

cat("Generating charts...\n\n")

# ══════════════════════════════════════════════════════════════════════════════
# CHART 1+2: Window Poincaré Map — THE CENTREPIECE
# window_width[n] vs window_width[n+1], density-coloured
# Shows the adaptive window's fixed-cycle attractor
# ══════════════════════════════════════════════════════════════════════════════
cat("[1+2] Window Poincaré map (centrepiece — light + dark)...\n")

df_poin <- df_all %>%
  filter(!is.na(win_next), window_width > 0, win_next > 0)

NBINS <- 60   # coarser bins since window_width has only 5 discrete levels

make_poincare <- function(dark = FALSE) {
  bg       <- if (dark) "#0d0d0d" else "white"
  lo       <- if (dark) "#0d0d0d"  else "white"
  mid      <- if (dark) "#003366"  else "#ffcc00"
  hi       <- if (dark) "#00e5ff"  else "#cc0000"
  vhigh    <- if (dark) "white"    else "#1a0000"
  grid_col <- if (dark) "#1a1a2e"  else "grey88"
  txt_col  <- if (dark) "#aaaaaa"  else "grey30"
  ttl_col  <- if (dark) "white"    else "black"

  # Log-scale window for better visual separation of powers-of-2
  ggplot(df_poin, aes(x = log2(window_width), y = log2(win_next))) +
    stat_bin_2d(bins = NBINS, aes(fill = after_stat(count))) +
    scale_fill_gradientn(
      colours = c(lo, mid, hi, vhigh),
      values  = scales::rescale(c(0, 0.05, 0.3, 1)),
      name    = "Density",
      trans   = "sqrt",
      labels  = scales::comma
    ) +
    geom_abline(slope = 1, intercept = 0,
                colour = if (dark) "#334455" else "grey75",
                linewidth = 0.5, linetype = "dashed") +
    scale_x_continuous(
      breaks = log2(c(256, 512, 1024, 2048, 4096)),
      labels = c("256","512","1024","2048","4096")
    ) +
    scale_y_continuous(
      breaks = log2(c(256, 512, 1024, 2048, 4096)),
      labels = c("256","512","1024","2048","4096")
    ) +
    facet_wrap(~ arch, ncol = 3) +
    labs(
      title    = "Adaptive Window Poincaré Map — Bare-Metal Multi-Architecture",
      subtitle = "window_width[n] vs window_width[n+1]; diagonal = identity; orbit = adaptive breathing cycle",
      x        = expression(W[n]~"(words, log"[2]*" scale)"),
      y        = expression(W[n+1]~"(words, log"[2]*" scale)")
    ) +
    theme_minimal(base_size = 11) %+replace%
    theme(
      panel.background  = element_rect(fill = bg, colour = NA),
      plot.background   = element_rect(fill = bg, colour = NA),
      panel.grid.major  = element_line(colour = grid_col),
      panel.grid.minor  = element_blank(),
      axis.text         = element_text(colour = txt_col, size = 8),
      axis.title        = element_text(colour = txt_col),
      strip.text        = element_text(colour = ttl_col, face = "bold"),
      plot.title        = element_text(colour = ttl_col, face = "bold", size = 13),
      plot.subtitle     = element_text(colour = if(dark) "#888888" else "grey40", size = 9),
      legend.position   = "right",
      legend.background = element_rect(fill = bg, colour = NA),
      legend.text       = element_text(colour = txt_col),
      legend.title      = element_text(colour = txt_col)
    )
}

save_svg(make_poincare(dark = FALSE), "poincare_light", w = 14, h = 6)
save_svg(make_poincare(dark = TRUE),  "poincare_dark",  w = 14, h = 6)

# ══════════════════════════════════════════════════════════════════════════════
# CHART 3+4: Window width time series — shows the breathing oscillation
# ══════════════════════════════════════════════════════════════════════════════
cat("[3+4] Window width time series (light + dark)...\n")

df_win_ts <- df_all %>%
  group_by(arch) %>%
  slice(seq(1, n(), by = 3)) %>%
  ungroup()

make_window_ts <- function(dark = FALSE) {
  base <- ggplot(df_win_ts, aes(x = elapsed_s, y = window_width, colour = arch)) +
    geom_line(alpha = 0.65, linewidth = 0.35) +
    scale_colour_manual(values = arch_colours) +
    scale_y_log10(
      breaks = c(256, 512, 1024, 2048, 4096),
      labels = c("256","512","1024","2048","4096")
    ) +
    scale_x_continuous(labels = scales::comma) +
    facet_wrap(~ arch, ncol = 1, scales = "free_x") +
    labs(
      title    = "Adaptive Window Width Over Session",
      subtitle = "Log₂ scale; breathing cycle: 256→4096→256 repeating — the adaptive attractor",
      x        = "Elapsed time (s)",
      y        = "Window width (words)"
    )
  if (dark) base + theme_dark_report() + theme(legend.position = "none")
  else      base + theme_light_report() + theme(legend.position = "none")
}

save_svg(make_window_ts(FALSE), "window_ts_light", w = 12, h = 9)
save_svg(make_window_ts(TRUE),  "window_ts_dark",  w = 12, h = 9)

# ══════════════════════════════════════════════════════════════════════════════
# CHART 5+6: Thermal accumulation — avg_word_heat time series
# ══════════════════════════════════════════════════════════════════════════════
cat("[5+6] Thermal accumulation time series (light + dark)...\n")

df_heat_ts <- df_all %>%
  group_by(arch) %>%
  slice(seq(1, n(), by = 5)) %>%
  ungroup()

make_heat_ts <- function(dark = FALSE) {
  base <- ggplot(df_heat_ts, aes(x = elapsed_s, y = heat, colour = arch)) +
    geom_line(alpha = 0.6, linewidth = 0.35) +
    scale_colour_manual(values = arch_colours) +
    scale_x_continuous(labels = scales::comma) +
    facet_wrap(~ arch, ncol = 1, scales = "free_x") +
    labs(
      title    = "Average Word Heat Accumulation Over Session",
      subtitle = "Q48.16 decoded; monotonic rise during DOE-WORK; arch-specific divergence in later phase",
      x        = "Elapsed time (s)",
      y        = "Avg word heat"
    )
  if (dark) base + theme_dark_report() + theme(legend.position = "none")
  else      base + theme_light_report() + theme(legend.position = "none")
}

save_svg(make_heat_ts(FALSE), "heat_ts_light", w = 12, h = 9)
save_svg(make_heat_ts(TRUE),  "heat_ts_dark",  w = 12, h = 9)

# ══════════════════════════════════════════════════════════════════════════════
# CHART 7+8: Execution rate distribution (word_executions_delta)
# ══════════════════════════════════════════════════════════════════════════════
cat("[7+8] Execution rate distribution (light + dark)...\n")

df_exec <- df_all %>% filter(word_executions_delta > 0)

make_exec_hist <- function(dark = FALSE) {
  base <- ggplot(df_exec, aes(x = word_executions_delta, fill = arch)) +
    geom_histogram(binwidth = 8, alpha = 0.75, position = "identity") +
    scale_fill_manual(values = arch_colours) +
    scale_y_continuous(labels = scales::comma) +
    facet_wrap(~ arch, ncol = 1) +
    labs(
      title    = "FORTH Execution Rate Distribution",
      subtitle = "Words executed per 10 µs tick; ISA-independent rate convergence expected",
      x        = "Words per tick",
      y        = "Count"
    )
  if (dark) base + theme_dark_report() + theme(legend.position = "none")
  else      base + theme_light_report() + theme(legend.position = "none")
}

save_svg(make_exec_hist(FALSE), "exec_rate_light", w = 10, h = 9)
save_svg(make_exec_hist(TRUE),  "exec_rate_dark",  w = 10, h = 9)

# ══════════════════════════════════════════════════════════════════════════════
# CHART 9+10: time_trust & variance by architecture (divergent region)
# ══════════════════════════════════════════════════════════════════════════════
cat("[9+10] Time-trust & variance density (light + dark)...\n")

df_trust <- df_tail %>% filter(time_trust < 1.01, time_trust > 0.9)

make_trust <- function(dark = FALSE) {
  p_trust <- ggplot(df_trust, aes(x = time_trust, fill = arch, colour = arch)) +
    geom_density(alpha = 0.4, linewidth = 0.6) +
    scale_fill_manual(values = arch_colours) +
    scale_colour_manual(values = arch_colours) +
    labs(title = "Time-Trust Distribution (divergent region)",
         subtitle = "Q48.16 decoded; timer quality diverges per ISA after boot phase",
         x = "Time trust", y = "Density")

  df_vq <- df_tail %>% filter(variance > 0, variance < quantile(variance, 0.99, na.rm=TRUE))
  p_var  <- ggplot(df_vq, aes(x = variance, fill = arch, colour = arch)) +
    geom_density(alpha = 0.4, linewidth = 0.6) +
    scale_fill_manual(values = arch_colours) +
    scale_colour_manual(values = arch_colours) +
    labs(title = "Timing Variance Distribution (divergent region)",
         subtitle = "Q48.16 decoded; lower variance = more stable adaptive timing",
         x = "Variance", y = "Density")

  if (dark) {
    p_trust <- p_trust + theme_dark_report()
    p_var   <- p_var   + theme_dark_report()
  } else {
    p_trust <- p_trust + theme_light_report()
    p_var   <- p_var   + theme_light_report()
  }
  p_trust / p_var
}

save_svg(make_trust(FALSE), "trust_variance_light", w = 10, h = 10)
save_svg(make_trust(TRUE),  "trust_variance_dark",  w = 10, h = 10)

# ══════════════════════════════════════════════════════════════════════════════
# CHART 11+12: Hot word count evolution
# ══════════════════════════════════════════════════════════════════════════════
cat("[11+12] Hot word count evolution (light + dark)...\n")

df_hot_ts <- df_all %>%
  group_by(arch) %>%
  slice(seq(1, n(), by = 5)) %>%
  ungroup()

make_hot_ts <- function(dark = FALSE) {
  base <- ggplot(df_hot_ts, aes(x = elapsed_s, y = hot_word_count, colour = arch)) +
    geom_line(alpha = 0.55, linewidth = 0.35) +
    scale_colour_manual(values = arch_colours) +
    scale_x_continuous(labels = scales::comma) +
    facet_wrap(~ arch, ncol = 1, scales = "free_x") +
    labs(
      title    = "Hot Word Count Over Session",
      subtitle = "Words above heat threshold; converges then differentiates per ISA in DoE phase",
      x        = "Elapsed time (s)",
      y        = "Hot word count"
    )
  if (dark) base + theme_dark_report() + theme(legend.position = "none")
  else      base + theme_light_report() + theme(legend.position = "none")
}

save_svg(make_hot_ts(FALSE), "hot_words_ts_light", w = 12, h = 9)
save_svg(make_hot_ts(TRUE),  "hot_words_ts_dark",  w = 12, h = 9)

# ══════════════════════════════════════════════════════════════════════════════
# CHART 13+14: Execution phase portrait (heat[n] vs delta_heat[n])
# Thermal dynamics — analogous to HR vs ΔHR in l8_attractor_map
# ══════════════════════════════════════════════════════════════════════════════
cat("[13+14] Thermal phase portrait (light + dark)...\n")

df_thermal <- df_all %>%
  filter(!is.na(delta_heat)) %>%
  filter(abs(delta_heat) < quantile(abs(delta_heat), 0.995, na.rm=TRUE))

make_thermal_portrait <- function(dark = FALSE) {
  bg       <- if (dark) "#0d0d0d" else "white"
  lo       <- if (dark) "#0d0d0d"  else "white"
  mid      <- if (dark) "#003366"  else "#ffcc00"
  hi       <- if (dark) "#00e5ff"  else "#cc0000"
  vhigh    <- if (dark) "white"    else "#1a0000"
  grid_col <- if (dark) "#1a1a2e"  else "grey88"
  txt_col  <- if (dark) "#aaaaaa"  else "grey30"
  ttl_col  <- if (dark) "white"    else "black"

  ggplot(df_thermal, aes(x = heat, y = delta_heat)) +
    stat_bin_2d(bins = 200, aes(fill = after_stat(count))) +
    scale_fill_gradientn(
      colours = c(lo, mid, hi, vhigh),
      values  = scales::rescale(c(0, 0.05, 0.3, 1)),
      name    = "Density",
      trans   = "sqrt",
      labels  = scales::comma
    ) +
    geom_hline(yintercept = 0,
               colour = if (dark) "#334455" else "grey75",
               linewidth = 0.4, linetype = "dashed") +
    facet_wrap(~ arch, ncol = 3) +
    labs(
      title    = "Thermal Phase Portrait — Heat[n] vs ΔHeat[n]",
      subtitle = "Thermal analogue of the l8 HR phase portrait; attractor at ΔHeat≈0 as system reaches steady state",
      x        = "Avg word heat (Q48.16 decoded)",
      y        = "ΔHeat per tick"
    ) +
    theme_minimal(base_size = 11) %+replace%
    theme(
      panel.background  = element_rect(fill = bg, colour = NA),
      plot.background   = element_rect(fill = bg, colour = NA),
      panel.grid.major  = element_line(colour = grid_col),
      panel.grid.minor  = element_blank(),
      axis.text         = element_text(colour = txt_col, size = 8),
      axis.title        = element_text(colour = txt_col),
      strip.text        = element_text(colour = ttl_col, face = "bold"),
      plot.title        = element_text(colour = ttl_col, face = "bold", size = 13),
      plot.subtitle     = element_text(colour = if(dark) "#888888" else "grey40", size = 9),
      legend.position   = "right",
      legend.background = element_rect(fill = bg, colour = NA),
      legend.text       = element_text(colour = txt_col),
      legend.title      = element_text(colour = txt_col)
    )
}

save_svg(make_thermal_portrait(FALSE), "thermal_portrait_light", w = 14, h = 6)
save_svg(make_thermal_portrait(TRUE),  "thermal_portrait_dark",  w = 14, h = 6)

# ══════════════════════════════════════════════════════════════════════════════
# done
# ══════════════════════════════════════════════════════════════════════════════
svg_files <- list.files(OUT_CHARTS, pattern = "\\.svg$")
cat(sprintf("\n══════════════════════════════════════════════════════════════════\n"))
cat(sprintf("  Analysis complete. %d SVG charts in %s\n", length(svg_files), OUT_CHARTS))
cat(sprintf("══════════════════════════════════════════════════════════════════\n\n"))
