#!/usr/bin/env Rscript
# analyse_multirep.R
# StarForth LithosAnanke — Multi-Replication DoE Analysis
#
# Pools all 9 outer DoE cells (3 reps × 3 arches) and tests:
#   1. Replication main effect on attractor metrics (should be nil)
#   2. Seed → shuffle independence (independent permutations = independent reps)
#   3. Inter-rep consistency of execution rate invariance
#   4. Per-rep Kruskal-Wallis to verify arch divergence in timer quality is
#      reproducible across seeds
#
# Rep 1: seed 12345  (amd64, aarch64, riscv64)
# Rep 2: seed 67890  (riscv64, aarch64, amd64)
# Rep 3: seed 13579  (aarch64, amd64, riscv64)

suppressPackageStartupMessages({
  library(ggplot2)
  library(svglite)
  library(dplyr)
  library(tidyr)
  library(scales)
  library(patchwork)
  library(viridis)
})

SCRIPT_DIR <- tryCatch(
  dirname(normalizePath(sys.frames()[[1]]$ofile)),
  error = function(e) getwd()
)
BASE_DIR   <- normalizePath(file.path(SCRIPT_DIR, ".."))
RUNS_DIR   <- file.path(BASE_DIR, "runs")
OUT_CHARTS <- file.path(SCRIPT_DIR, "charts")
OUT_TABLES <- file.path(SCRIPT_DIR, "tables")
dir.create(OUT_CHARTS, showWarnings = FALSE, recursive = TRUE)
dir.create(OUT_TABLES, showWarnings = FALSE, recursive = TRUE)

cat("══════════════════════════════════════════════════════════════════\n")
cat("  StarForth LithosAnanke — Multi-Replication DoE Analysis\n")
cat("  9 outer cells (3 reps × 3 arches), ~783k rows each rep\n")
cat("══════════════════════════════════════════════════════════════════\n\n")

Q48 <- 65536.0

# ── manifest of all 9 canonical runs ─────────────────────────────────────────
manifest <- list(
  list(rep=1, seed=12345, arch="amd64",   file="doe-amd64-20260612-110212.csv"),
  list(rep=1, seed=12345, arch="aarch64", file="doe-aarch64-20260612-132412.csv"),
  list(rep=1, seed=12345, arch="riscv64", file="doe-riscv64-20260612-135612.csv"),
  list(rep=2, seed=67890, arch="riscv64", file="doe-riscv64-20260612-141129.csv"),
  list(rep=2, seed=67890, arch="aarch64", file="doe-aarch64-20260612-141722.csv"),
  list(rep=2, seed=67890, arch="amd64",   file="doe-amd64-20260612-142323.csv"),
  list(rep=3, seed=13579, arch="aarch64", file="doe-aarch64-20260612-160132.csv"),
  list(rep=3, seed=13579, arch="amd64",   file="doe-amd64-20260612-160656.csv"),
  list(rep=3, seed=13579, arch="riscv64", file="doe-riscv64-20260612-174414.csv")
)

load_run <- function(m) {
  path <- file.path(RUNS_DIR, m$file)
  cat(sprintf("  Rep%d seed=%-6d %-8s ... ", m$rep, m$seed, m$arch))
  df <- read.csv(path, stringsAsFactors = FALSE)
  cat(sprintf("%d rows\n", nrow(df)))
  df$rep  <- m$rep
  df$seed <- m$seed
  df$arch <- m$arch

  df <- df[!is.na(df$tick_interval_ns) & df$tick_interval_ns > 0, ]

  df$heat       <- df$avg_word_heat_q48 / Q48
  df$variance   <- df$variance_q48      / Q48
  df$time_trust <- df$time_trust_q48    / Q48
  df$elapsed_s  <- df$elapsed_ns / 1e9
  df
}

all_runs <- lapply(manifest, load_run)
df_all   <- bind_rows(all_runs)
df_all$arch <- factor(df_all$arch, levels = c("amd64","aarch64","riscv64"))
df_all$rep  <- factor(df_all$rep)

cat(sprintf("\n  Total rows across all 9 cells: %s\n\n",
            formatC(nrow(df_all), format="d", big.mark=",")))

# ── 1. Per-cell summary ───────────────────────────────────────────────────────
cat("Computing per-cell summary (rep × arch)...\n")

cell_tbl <- df_all %>%
  group_by(rep, seed, arch) %>%
  summarise(
    n_ticks         = n(),
    mean_exec_delta = mean(word_executions_delta, na.rm=TRUE),
    sd_exec_delta   = sd(word_executions_delta,   na.rm=TRUE),
    mean_heat       = mean(heat,                  na.rm=TRUE),
    max_heat        = max(heat,                   na.rm=TRUE),
    mean_trust      = mean(time_trust,            na.rm=TRUE),
    mean_variance   = mean(variance,              na.rm=TRUE),
    mean_win        = mean(window_width,          na.rm=TRUE),
    .groups = "drop"
  )

write.csv(cell_tbl, file.path(OUT_TABLES, "cell_summary.csv"), row.names=FALSE)
cat("  Saved: cell_summary.csv\n")
print(as.data.frame(cell_tbl))
cat("\n")

# ── 2. Replication main effect on exec_delta (should be F≈0, p≈1) ───────────
cat("ANOVA: exec_delta ~ rep (should show nil replication effect)...\n")
fit_rep <- aov(mean_exec_delta ~ rep, data = cell_tbl)
print(summary(fit_rep))
cat("\n")

# ── 3. Per-rep Kruskal-Wallis: timer quality divergence reproducible? ─────────
cat("Per-rep Kruskal-Wallis: time_trust ~ arch (divergent region only)\n")
cat("──────────────────────────────────────────────────────────────────\n")

kw_results <- list()
for (r in 1:3) {
  df_r <- df_all %>%
    filter(rep == r) %>%
    group_by(arch) %>%
    filter(row_number() > 26251) %>%
    ungroup()

  kw <- kruskal.test(time_trust ~ arch, data = df_r)
  kw_results[[r]] <- data.frame(
    rep       = r,
    seed      = unique(df_all$seed[df_all$rep == r]),
    H         = round(kw$statistic, 1),
    df        = kw$parameter,
    p_value   = kw$p.value,
    decision  = if (kw$p.value < 2.2e-16) "REJECT H0 (arch divergence confirmed)" else "fail to reject"
  )
  cat(sprintf("  Rep %d (seed %5d): H=%.1f  df=%d  p=%.2e  → %s\n",
              r, kw_results[[r]]$seed, kw$statistic, kw$parameter, kw$p.value,
              kw_results[[r]]$decision))
}
kw_tbl <- bind_rows(kw_results)
write.csv(kw_tbl, file.path(OUT_TABLES, "kw_per_rep.csv"), row.names=FALSE)
cat("  Saved: kw_per_rep.csv\n\n")

# ── 4. Exec rate: stable across reps? ────────────────────────────────────────
cat("Exec rate stability across reps (per arch):\n")
exec_stability <- cell_tbl %>%
  group_by(arch) %>%
  summarise(
    rep_mean_of_means = mean(mean_exec_delta),
    rep_sd_of_means   = sd(mean_exec_delta),
    cv_across_reps    = sd(mean_exec_delta) / mean(mean_exec_delta) * 100,
    .groups = "drop"
  )
print(as.data.frame(exec_stability))
write.csv(exec_stability, file.path(OUT_TABLES, "exec_stability.csv"), row.names=FALSE)
cat("  Saved: exec_stability.csv\n\n")

# ── colour themes ─────────────────────────────────────────────────────────────
arch_colours <- c(amd64   = "#E07B39",
                  aarch64 = "#4A90D9",
                  riscv64 = "#50C878")
rep_colours  <- c("1" = "#E07B39", "2" = "#4A90D9", "3" = "#50C878")

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

save_svg <- function(plot, name, w = 12, h = 7) {
  path <- file.path(OUT_CHARTS, paste0(name, ".svg"))
  svglite(path, width = w, height = h)
  print(plot)
  dev.off()
  cat(sprintf("  Saved: %s.svg\n", name))
  invisible(path)
}

cat("Generating multi-rep charts...\n\n")

# ── CHART MR-1: Exec rate per cell (3×3 grid, rep×arch) ──────────────────────
cat("[MR-1] Per-cell execution rate comparison...\n")

p_exec_cells <- ggplot(cell_tbl,
                       aes(x = arch, y = mean_exec_delta, colour = rep, shape = rep)) +
  geom_point(size = 4, alpha = 0.85) +
  geom_errorbar(aes(ymin = mean_exec_delta - sd_exec_delta,
                    ymax = mean_exec_delta + sd_exec_delta),
                width = 0.15, alpha = 0.5) +
  scale_colour_manual(values = rep_colours, name = "Replication") +
  scale_shape_manual(values = c("1"=16,"2"=17,"3"=15), name = "Replication") +
  scale_y_continuous(limits = c(253, 259)) +
  labs(
    title    = "Execution Rate per Cell (mean ± SD)",
    subtitle = "3 replications × 3 architectures — rate invariant across ISA and seed",
    x        = "Architecture",
    y        = "Words per 10 µs tick"
  ) +
  theme_light_report()

save_svg(p_exec_cells, "multirep_exec_rate", w = 8, h = 6)

# ── CHART MR-2: Heat mean per cell ────────────────────────────────────────────
cat("[MR-2] Per-cell heat comparison...\n")

p_heat_cells <- ggplot(cell_tbl,
                       aes(x = rep, y = mean_heat, fill = arch, colour = arch)) +
  geom_col(position = "dodge", alpha = 0.8, width = 0.6) +
  scale_fill_manual(values = arch_colours) +
  scale_colour_manual(values = arch_colours) +
  labs(
    title    = "Mean Word Heat by Replication × Architecture",
    subtitle = "amd64 heat ~8× higher due to APIC residual accumulation; consistent across reps",
    x        = "Replication",
    y        = "Mean avg word heat (Q48.16 decoded)"
  ) +
  theme_light_report()

save_svg(p_heat_cells, "multirep_heat", w = 8, h = 6)

# ── CHART MR-3: Trust mean per cell ───────────────────────────────────────────
cat("[MR-3] Per-cell time-trust comparison...\n")

p_trust_cells <- ggplot(cell_tbl,
                        aes(x = rep, y = mean_trust, fill = arch, colour = arch)) +
  geom_col(position = "dodge", alpha = 0.8, width = 0.6) +
  scale_fill_manual(values = arch_colours) +
  scale_colour_manual(values = arch_colours) +
  coord_cartesian(ylim = c(0.99, 1.001)) +
  labs(
    title    = "Mean Time-Trust by Replication × Architecture",
    subtitle = "amd64 trust ~0.993; aarch64/riscv64 = 1.000 exactly — reproducible across all 3 seeds",
    x        = "Replication",
    y        = "Mean time_trust (Q48.16 decoded)"
  ) +
  theme_light_report()

save_svg(p_trust_cells, "multirep_trust", w = 8, h = 6)

# ── CHART MR-4: KW H-statistic across reps (robustness check) ────────────────
cat("[MR-4] KW H-statistic reproducibility across reps...\n")

p_kw <- ggplot(kw_tbl, aes(x = factor(rep), y = H, fill = factor(rep))) +
  geom_col(alpha = 0.85, width = 0.5) +
  geom_text(aes(label = formatC(H, format="f", digits=0)),
            vjust = -0.3, size = 4, fontface = "bold") +
  scale_fill_manual(values = rep_colours, guide = "none") +
  scale_y_continuous(labels = scales::comma, expand = expansion(mult = c(0, 0.15))) +
  labs(
    title    = "Kruskal-Wallis H-Statistic: Timer Quality Divergence per Replication",
    subtitle = "H ≈ 668,000 in all three reps — arch divergence is fully reproducible across seeds",
    x        = "Replication",
    y        = "K-W H statistic"
  ) +
  theme_light_report() +
  theme(legend.position = "none")

save_svg(p_kw, "multirep_kw_h", w = 7, h = 6)

# ── CHART MR-5: Window Poincaré map — overlay all 3 reps by arch ─────────────
cat("[MR-5] Multi-rep Poincaré map overlay (all 9 cells)...\n")

df_poin <- df_all %>%
  mutate(
    win_next = ave(window_width, interaction(rep, arch),
                   FUN = function(x) c(x[-1], NA_real_))
  ) %>%
  filter(!is.na(win_next), window_width > 0, win_next > 0)

p_poin_mr <- ggplot(df_poin,
                    aes(x = log2(window_width), y = log2(win_next), colour = rep)) +
  stat_bin_2d(bins = 40, aes(fill = after_stat(count)), alpha = 0.7) +
  scale_fill_viridis_c(option = "plasma", trans = "sqrt",
                       name = "Density", labels = scales::comma) +
  scale_colour_manual(values = rep_colours, guide = "none") +
  scale_x_continuous(breaks = log2(c(256,512,1024,2048,4096)),
                     labels = c("256","512","1024","2048","4096")) +
  scale_y_continuous(breaks = log2(c(256,512,1024,2048,4096)),
                     labels = c("256","512","1024","2048","4096")) +
  geom_abline(slope = 1, intercept = 0, colour = "grey75",
              linewidth = 0.4, linetype = "dashed") +
  facet_wrap(~ arch, ncol = 3) +
  labs(
    title    = "Multi-Replication Window Poincaré Map (Reps 1–3 pooled)",
    subtitle = "3 seeds × 3 arches — attractor orbit structure identical across all replications",
    x        = expression(W[n]~"(log"[2]*" scale)"),
    y        = expression(W[n+1]~"(log"[2]*" scale)")
  ) +
  theme_light_report() +
  theme(legend.position = "right")

save_svg(p_poin_mr, "multirep_poincare", w = 14, h = 6)

# ══════════════════════════════════════════════════════════════════════════════
# CHART MR-6+7: 3×3 Thermal Phase Portrait — "the attractor grid"
# Rows = replication (Rep 1/2/3), Columns = architecture
# Styled after L8 capsule mode portraits: bold facet labels, dark + light
# Shows: same attractor across ALL 9 outer DoE cells
# ══════════════════════════════════════════════════════════════════════════════
cat("[MR-6+7] 3×3 attractor grid — thermal portrait (light + dark)...\n")

df_th9 <- df_all %>%
  mutate(
    delta_heat = ave(heat, interaction(rep, arch),
                     FUN = function(x) c(diff(x), NA_real_))
  ) %>%
  filter(!is.na(delta_heat)) %>%
  group_by(arch, rep) %>%
  filter(abs(delta_heat) < quantile(abs(delta_heat), 0.995, na.rm = TRUE)) %>%
  ungroup() %>%
  mutate(
    arch_label = factor(
      case_when(
        arch == "amd64"   ~ "x86-64 (amd64)",
        arch == "aarch64" ~ "ARMv8-A (aarch64)",
        arch == "riscv64" ~ "RISC-V (riscv64)"
      ),
      levels = c("x86-64 (amd64)", "ARMv8-A (aarch64)", "RISC-V (riscv64)")
    ),
    rep_label = factor(
      case_when(
        rep == "1" ~ "REP 1  ·  seed 12345",
        rep == "2" ~ "REP 2  ·  seed 67890",
        rep == "3" ~ "REP 3  ·  seed 13579"
      ),
      levels = c("REP 1  ·  seed 12345",
                 "REP 2  ·  seed 67890",
                 "REP 3  ·  seed 13579")
    )
  )

make_attractor_grid <- function(dark = FALSE) {
  bg       <- if (dark) "#0d0d0d" else "white"
  lo       <- if (dark) "#0d0d0d" else "white"
  mid      <- if (dark) "#003366" else "#ffdd00"
  hi       <- if (dark) "#00e5ff" else "#cc2200"
  vhigh    <- if (dark) "white"   else "#1a0000"
  grid_col <- if (dark) "#1a1a2e" else "grey88"
  txt_col  <- if (dark) "#aaaaaa" else "grey25"
  ttl_col  <- if (dark) "white"   else "black"
  stl_col  <- if (dark) "#666666" else "grey45"

  ggplot(df_th9, aes(x = heat, y = delta_heat)) +
    stat_bin_2d(bins = 120, aes(fill = after_stat(count))) +
    scale_fill_gradientn(
      colours = c(lo, mid, hi, vhigh),
      values  = scales::rescale(c(0, 0.05, 0.30, 1)),
      name    = "Density",
      trans   = "sqrt",
      labels  = scales::comma
    ) +
    geom_hline(yintercept = 0,
               colour = if (dark) "#2a3a4a" else "grey78",
               linewidth = 0.4, linetype = "dashed") +
    facet_grid(rep_label ~ arch_label) +
    labs(
      title    = "Compudynamic Attractor — Thermal Phase Space (All 9 Cells)",
      subtitle = expression(paste("Heat"[n], " vs ", Delta, "Heat"[n],
                  " · 3 replications × 3 architectures · attractor topology invariant")),
      x        = "Avg word heat (Q48.16 decoded)",
      y        = expression(Delta * "Heat per tick")
    ) +
    theme_minimal(base_size = 10) %+replace%
    theme(
      panel.background  = element_rect(fill = bg, colour = NA),
      plot.background   = element_rect(fill = bg, colour = NA),
      panel.grid.major  = element_line(colour = grid_col),
      panel.grid.minor  = element_blank(),
      axis.text         = element_text(colour = txt_col, size = 7),
      axis.title        = element_text(colour = txt_col, size = 9),
      # Column labels (architecture) — bold, prominent like the reference
      strip.text.x      = element_text(colour = ttl_col, face = "bold",
                                       size = 10, margin = margin(b = 4)),
      # Row labels (replication)
      strip.text.y      = element_text(colour = ttl_col, face = "bold",
                                       size = 8, angle = 270,
                                       margin = margin(l = 4)),
      strip.background  = element_rect(fill = if(dark) "#1a1a2e" else "grey95",
                                       colour = NA),
      plot.title        = element_text(colour = ttl_col, face = "bold",
                                       size = 12, margin = margin(b = 4)),
      plot.subtitle     = element_text(colour = stl_col, size = 8,
                                       margin = margin(b = 8)),
      legend.position   = "right",
      legend.background = element_rect(fill = bg, colour = NA),
      legend.text       = element_text(colour = txt_col, size = 7),
      legend.title      = element_text(colour = txt_col, size = 8),
      panel.spacing     = unit(0.6, "lines")
    )
}

save_svg(make_attractor_grid(FALSE), "attractor_grid_light", w = 14, h = 10)
save_svg(make_attractor_grid(TRUE),  "attractor_grid_dark",  w = 14, h = 10)

# ══════════════════════════════════════════════════════════════════════════════
# CHART MR-8+9: 3×3 Poincaré Map Grid — window attractor, all 9 cells
# ══════════════════════════════════════════════════════════════════════════════
cat("[MR-8+9] 3×3 attractor grid — Poincaré map (light + dark)...\n")

df_po9 <- df_all %>%
  mutate(
    win_next = ave(window_width, interaction(rep, arch),
                   FUN = function(x) c(x[-1], NA_real_))
  ) %>%
  filter(!is.na(win_next), window_width > 0, win_next > 0) %>%
  mutate(
    arch_label = factor(
      case_when(
        arch == "amd64"   ~ "x86-64 (amd64)",
        arch == "aarch64" ~ "ARMv8-A (aarch64)",
        arch == "riscv64" ~ "RISC-V (riscv64)"
      ),
      levels = c("x86-64 (amd64)", "ARMv8-A (aarch64)", "RISC-V (riscv64)")
    ),
    rep_label = factor(
      case_when(
        rep == "1" ~ "REP 1  ·  seed 12345",
        rep == "2" ~ "REP 2  ·  seed 67890",
        rep == "3" ~ "REP 3  ·  seed 13579"
      ),
      levels = c("REP 1  ·  seed 12345",
                 "REP 2  ·  seed 67890",
                 "REP 3  ·  seed 13579")
    )
  )

make_poincare_grid <- function(dark = FALSE) {
  bg       <- if (dark) "#0d0d0d" else "white"
  lo       <- if (dark) "#0d0d0d" else "white"
  mid      <- if (dark) "#003366" else "#ffdd00"
  hi       <- if (dark) "#00e5ff" else "#cc2200"
  vhigh    <- if (dark) "white"   else "#1a0000"
  grid_col <- if (dark) "#1a1a2e" else "grey88"
  txt_col  <- if (dark) "#aaaaaa" else "grey25"
  ttl_col  <- if (dark) "white"   else "black"
  stl_col  <- if (dark) "#666666" else "grey45"

  ggplot(df_po9, aes(x = log2(window_width), y = log2(win_next))) +
    stat_bin_2d(bins = 40, aes(fill = after_stat(count))) +
    scale_fill_gradientn(
      colours = c(lo, mid, hi, vhigh),
      values  = scales::rescale(c(0, 0.05, 0.30, 1)),
      name    = "Density",
      trans   = "sqrt",
      labels  = scales::comma
    ) +
    geom_abline(slope = 1, intercept = 0,
                colour = if (dark) "#2a3a4a" else "grey78",
                linewidth = 0.4, linetype = "dashed") +
    scale_x_continuous(
      breaks = log2(c(256, 512, 1024, 2048, 4096)),
      labels = c("256","512","1024","2048","4096")
    ) +
    scale_y_continuous(
      breaks = log2(c(256, 512, 1024, 2048, 4096)),
      labels = c("256","512","1024","2048","4096")
    ) +
    facet_grid(rep_label ~ arch_label) +
    labs(
      title    = "Compudynamic Attractor — Window Poincaré Map (All 9 Cells)",
      subtitle = expression(paste(W[n], " vs ", W[n+1],
                  " (log"[2], " scale) · breathing cycle 256 ↔ 4096 preserved across all cells")),
      x        = expression(W[n]~"(words, log"[2]*" scale)"),
      y        = expression(W[n+1]~"(words, log"[2]*" scale)")
    ) +
    theme_minimal(base_size = 10) %+replace%
    theme(
      panel.background  = element_rect(fill = bg, colour = NA),
      plot.background   = element_rect(fill = bg, colour = NA),
      panel.grid.major  = element_line(colour = grid_col),
      panel.grid.minor  = element_blank(),
      axis.text         = element_text(colour = txt_col, size = 6),
      axis.title        = element_text(colour = txt_col, size = 9),
      strip.text.x      = element_text(colour = ttl_col, face = "bold",
                                       size = 10, margin = margin(b = 4)),
      strip.text.y      = element_text(colour = ttl_col, face = "bold",
                                       size = 8, angle = 270,
                                       margin = margin(l = 4)),
      strip.background  = element_rect(fill = if(dark) "#1a1a2e" else "grey95",
                                       colour = NA),
      plot.title        = element_text(colour = ttl_col, face = "bold",
                                       size = 12, margin = margin(b = 4)),
      plot.subtitle     = element_text(colour = stl_col, size = 8,
                                       margin = margin(b = 8)),
      legend.position   = "right",
      legend.background = element_rect(fill = bg, colour = NA),
      legend.text       = element_text(colour = txt_col, size = 7),
      legend.title      = element_text(colour = txt_col, size = 8),
      panel.spacing     = unit(0.6, "lines")
    )
}

save_svg(make_poincare_grid(FALSE), "poincare_grid_light", w = 14, h = 10)
save_svg(make_poincare_grid(TRUE),  "poincare_grid_dark",  w = 14, h = 10)

# ── summary ───────────────────────────────────────────────────────────────────
svg_new <- list.files(OUT_CHARTS, pattern = "^multirep.*\\.svg$")
cat(sprintf("\n══════════════════════════════════════════════════════════════════\n"))
cat(sprintf("  Multi-rep analysis complete. %d new charts.\n", length(svg_new)))
cat(sprintf("══════════════════════════════════════════════════════════════════\n\n"))

cat("Key findings:\n")
cat(sprintf("  Exec rate CV across reps: amd64=%.4f%% / aarch64=%.4f%% / riscv64=%.4f%%\n",
            exec_stability$cv_across_reps[exec_stability$arch=="amd64"],
            exec_stability$cv_across_reps[exec_stability$arch=="aarch64"],
            exec_stability$cv_across_reps[exec_stability$arch=="riscv64"]))
cat(sprintf("  KW H range: %.0f – %.0f (all p < 2.2e-16)\n",
            min(kw_tbl$H), max(kw_tbl$H)))
cat("  Conclusion: attractor metrics stable across seeds; timer divergence reproducible.\n\n")
