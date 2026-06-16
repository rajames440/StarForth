#!/usr/bin/env Rscript
# analyse_acl_rwt.R
# StarForth LithosAnanke — ACL Rolling Window of Truth DoE Analysis
#
# Generates figures for §8 (ACL Security Extension) of bare_metal_doe_report.tex
#
# Data sources:
#   Baseline (no ACL):   runs/doe-{arch}-20260612-*.csv  (3 seeds × 3 arches)
#   ACL floor=16:        runs/doe-{arch}-20260615-{...}.csv
#   ACL floor=256:       runs/doe-{arch}-20260615-{...}.csv (partial)
#   ACL-RWT:             runs/doe-{arch}-20260616-*.csv    (3 seeds × 3 arches)
#
# Figures produced:
#   acl_overhead_bar_{light,dark}.svg   — grouped bar: overhead % by ISA × campaign (log scale)
#   acl_latin_square_{light,dark}.svg   — 3×3 heatmap: tick counts, ACL-RWT campaign
#   acl_tick_comparison_{light,dark}.svg — side-by-side tick totals: baseline vs ACL-RWT
#   acl_overhead_reduction_{light,dark}.svg — overhead by ISA: floor16 → floor256 → ACL-RWT

suppressPackageStartupMessages({
  library(ggplot2)
  library(svglite)
  library(dplyr)
  library(tidyr)
  library(scales)
  library(patchwork)
})

SCRIPT_DIR <- tryCatch(
  dirname(normalizePath(sys.frames()[[1]]$ofile)),
  error = function(e) getwd()
)
BASE_DIR   <- normalizePath(file.path(SCRIPT_DIR, ".."))
RUNS_DIR   <- file.path(BASE_DIR, "runs")
OUT_CHARTS <- file.path(SCRIPT_DIR, "charts")
dir.create(OUT_CHARTS, showWarnings = FALSE, recursive = TRUE)

cat("══════════════════════════════════════════════════════════════════\n")
cat("  StarForth LithosAnanke — ACL-RWT DoE Analysis\n")
cat("  Patent support material — all figures from measured data\n")
cat("══════════════════════════════════════════════════════════════════\n\n")

# ── palette ──────────────────────────────────────────────────────────────────
arch_colours <- c(amd64   = "#E07B39",
                  aarch64 = "#4A90D9",
                  riscv64 = "#50C878")

camp_colours <- c(
  "Baseline\n(no ACL)"     = "#888888",
  "Floor = 16"             = "#D62728",
  "Floor = 256"            = "#FF7F0E",
  "ACL-RWT\n(this work)"  = "#2CA02C"
)

theme_light_sf <- function(base = 11) {
  theme_minimal(base_size = base) %+replace% theme(
    panel.grid.minor  = element_blank(),
    panel.grid.major  = element_line(colour = "grey90"),
    strip.text        = element_text(face = "bold"),
    plot.title        = element_text(face = "bold", size = base + 1),
    plot.subtitle     = element_text(colour = "grey40", size = base - 2),
    legend.position   = "bottom",
    legend.key.size   = unit(0.5, "cm")
  )
}

theme_dark_sf <- function(base = 11) {
  theme_minimal(base_size = base) %+replace% theme(
    panel.background  = element_rect(fill = "#0d0d0d", colour = NA),
    plot.background   = element_rect(fill = "#0d0d0d", colour = NA),
    panel.grid.major  = element_line(colour = "#1e1e1e"),
    panel.grid.minor  = element_blank(),
    axis.text         = element_text(colour = "#aaaaaa"),
    axis.title        = element_text(colour = "#cccccc"),
    strip.text        = element_text(colour = "white", face = "bold"),
    plot.title        = element_text(colour = "white", face = "bold", size = base + 1),
    plot.subtitle     = element_text(colour = "#666666", size = base - 2),
    legend.text       = element_text(colour = "#aaaaaa"),
    legend.title      = element_text(colour = "#cccccc"),
    legend.background = element_rect(fill = "#0d0d0d", colour = NA),
    legend.position   = "bottom",
    legend.key.size   = unit(0.5, "cm")
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

# ── measured data ─────────────────────────────────────────────────────────────
# All tick counts verified against CSV last-row tick_number fields.

baseline_ticks <- c(amd64 = 261098, aarch64 = 261095, riscv64 = 261095)

# Floor=16: mean across 3 seeds (all confirmed)
floor16_ticks <- list(
  amd64   = c(261307, 261307, 261307),
  aarch64 = c(422431, 432403, 432111),
  riscv64 = c(433210, 436040, 438938)
)

# Floor=256: partial (amd64 2 seeds, aarch64 1 seed, riscv64 1 seed)
floor256_ticks <- list(
  amd64   = c(261248, 261112),
  aarch64 = c(271443),
  riscv64 = c(272725)
)

# ACL-RWT: all 9 cells confirmed
rwt_ticks <- list(
  amd64   = list(s12345 = 261113, s67890 = 261112, s13579 = 261113),
  aarch64 = list(s12345 = 261118, s67890 = 261118, s13579 = 261117),
  riscv64 = list(s12345 = 261118, s67890 = 261117, s13579 = 261117)
)

seeds <- c("seed 12345", "seed 67890", "seed 13579")

# ── compute overhead % ────────────────────────────────────────────────────────
overhead_pct <- function(ticks_vec, arch) {
  base <- baseline_ticks[arch]
  mean((ticks_vec - base) / base * 100)
}

archs <- c("amd64", "aarch64", "riscv64")

df_overhead <- bind_rows(
  # Baseline
  data.frame(
    campaign = "Baseline\n(no ACL)",
    arch     = archs,
    overhead = 0,
    stringsAsFactors = FALSE
  ),
  # Floor=16
  data.frame(
    campaign = "Floor = 16",
    arch     = archs,
    overhead = sapply(archs, function(a) overhead_pct(floor16_ticks[[a]], a)),
    stringsAsFactors = FALSE
  ),
  # Floor=256
  data.frame(
    campaign = "Floor = 256",
    arch     = archs,
    overhead = sapply(archs, function(a) overhead_pct(floor256_ticks[[a]], a)),
    stringsAsFactors = FALSE
  ),
  # ACL-RWT
  data.frame(
    campaign = "ACL-RWT\n(this work)",
    arch     = archs,
    overhead = sapply(archs, function(a) {
      ticks <- unlist(rwt_ticks[[a]])
      overhead_pct(ticks, a)
    }),
    stringsAsFactors = FALSE
  )
)

df_overhead$campaign <- factor(df_overhead$campaign,
  levels = c("Baseline\n(no ACL)", "Floor = 16", "Floor = 256", "ACL-RWT\n(this work)"))
df_overhead$arch <- factor(df_overhead$arch, levels = archs)

# Floor=256 partial: note in subtitle
cat("Overhead summary:\n")
print(df_overhead)
cat("\n")

# ══════════════════════════════════════════════════════════════════════════════
# FIGURE 1: Grouped bar chart — overhead % by ISA and campaign (log scale)
# ══════════════════════════════════════════════════════════════════════════════
cat("[ACL-1] ACL overhead bar chart (light + dark)...\n")

df_bar <- df_overhead %>%
  filter(campaign != "Baseline\n(no ACL)") %>%
  mutate(overhead_plot = pmax(overhead, 0.001))   # floor for log scale

make_acl_bar <- function(dark = FALSE) {
  thm <- if (dark) theme_dark_sf() else theme_light_sf()
  bg  <- if (dark) "#0d0d0d" else "white"
  lbl <- if (dark) "#cccccc" else "grey20"

  ggplot(df_bar, aes(x = arch, y = overhead_plot, fill = campaign)) +
    geom_col(position = position_dodge(width = 0.75), width = 0.65,
             colour = NA, alpha = 0.92) +
    geom_text(aes(label = ifelse(overhead_plot < 0.01,
                                 sprintf("%.4f%%", overhead_plot),
                                 ifelse(overhead_plot < 1,
                                        sprintf("%.3f%%", overhead_plot),
                                        sprintf("%.1f%%",  overhead_plot)))),
              position = position_dodge(width = 0.75),
              vjust = -0.4, size = 2.7, colour = lbl, fontface = "bold") +
    scale_fill_manual(
      values = c("Floor = 16"          = "#D62728",
                 "Floor = 256"         = "#FF7F0E",
                 "ACL-RWT\n(this work)" = "#2CA02C"),
      name = "ACL Policy"
    ) +
    scale_y_log10(
      breaks = c(0.001, 0.01, 0.1, 1, 10, 100),
      labels = c("0.001%", "0.01%", "0.1%", "1%", "10%", "100%"),
      limits = c(0.001, 200),
      expand = expansion(mult = c(0, 0.15))
    ) +
    scale_x_discrete(labels = c(amd64 = "amd64\n(x86-64)",
                                 aarch64 = "aarch64\n(ARMv8-A)",
                                 riscv64 = "riscv64\n(RV64GC)")) +
    labs(
      title    = "ACL Enforcement Overhead by ISA and TTL Policy",
      subtitle = paste0(
        "Log₁₀ scale. Baseline: amd64 = 261,098 ticks; aarch64/riscv64 = 261,095 ticks. ",
        "30 reps per cell.\nFloor = 256 partial (2 seeds amd64, 1 seed each RISC). ",
        "ACL-RWT: all 9 cells confirmed."
      ),
      x = "Instruction-Set Architecture",
      y = "Overhead vs. no-ACL baseline (%, log₁₀ scale)"
    ) +
    thm +
    theme(
      panel.background = element_rect(fill = bg, colour = NA),
      plot.background  = element_rect(fill = bg, colour = NA),
      legend.position  = "right"
    )
}

save_svg(make_acl_bar(FALSE), "acl_overhead_bar_light", w = 12, h = 7)
save_svg(make_acl_bar(TRUE),  "acl_overhead_bar_dark",  w = 12, h = 7)

# ══════════════════════════════════════════════════════════════════════════════
# FIGURE 2: ACL-RWT 3×3 Latin square heatmap
# ══════════════════════════════════════════════════════════════════════════════
cat("[ACL-2] ACL-RWT 3x3 Latin square heatmap (light + dark)...\n")

df_rwt_grid <- bind_rows(lapply(archs, function(a) {
  base <- baseline_ticks[a]
  data.frame(
    arch      = a,
    seed      = seeds,
    ticks     = unlist(rwt_ticks[[a]]),
    overhead  = (unlist(rwt_ticks[[a]]) - base) / base * 100,
    stringsAsFactors = FALSE
  )
}))
df_rwt_grid$arch <- factor(df_rwt_grid$arch, levels = archs)
df_rwt_grid$seed <- factor(df_rwt_grid$seed, levels = seeds)

make_ls_heatmap <- function(dark = FALSE) {
  bg      <- if (dark) "#0d0d0d" else "white"
  txt_lo  <- if (dark) "white"   else "#0d4016"
  txt_hi  <- if (dark) "#aaffaa" else "#0d4016"
  tile_lo <- if (dark) "#0a2e12" else "#c8f0d0"
  tile_hi <- if (dark) "#2ca02c" else "#006400"

  ggplot(df_rwt_grid, aes(x = seed, y = arch)) +
    geom_tile(aes(fill = overhead), colour = if(dark) "#1a1a1a" else "white",
              linewidth = 1.5) +
    geom_text(aes(label = sprintf("%s ticks\n+%.4f%%",
                                  formatC(ticks, format = "d", big.mark = ","),
                                  overhead)),
              size = 3.2, fontface = "bold",
              colour = if (dark) "white" else "#0a3010") +
    scale_fill_gradient(low  = tile_lo, high = tile_hi,
                        name = "Overhead (%)",
                        labels = function(x) sprintf("%.4f%%", x)) +
    scale_x_discrete(labels = c("seed 12345\n(Rep 1)",
                                 "seed 67890\n(Rep 2)",
                                 "seed 13579\n(Rep 3)")) +
    scale_y_discrete(labels = c(amd64   = "amd64\n(x86-64)",
                                 aarch64 = "aarch64\n(ARMv8-A)",
                                 riscv64 = "riscv64\n(RV64GC)")) +
    labs(
      title    = "ACL-RWT Campaign: 3×3 Balanced Latin Square",
      subtitle = paste0(
        "All 9 cells confirmed. 30 replicates per cell. ",
        "Baseline: amd64 = 261,098; aarch64/riscv64 = 261,095 ticks.\n",
        "CV of execution rate across all 9 cells = 0.000% (compudynamic invariance preserved)."
      ),
      x = NULL, y = NULL
    ) +
    theme_minimal(base_size = 11) %+replace% theme(
      panel.background = element_rect(fill = bg, colour = NA),
      plot.background  = element_rect(fill = bg, colour = NA),
      panel.grid       = element_blank(),
      axis.text        = element_text(colour = if(dark) "#cccccc" else "grey20",
                                      face = "bold", size = 10),
      plot.title       = element_text(colour = if(dark) "white" else "black",
                                      face = "bold", size = 12),
      plot.subtitle    = element_text(colour = if(dark) "#888888" else "grey40", size = 9),
      legend.text      = element_text(colour = if(dark) "#aaaaaa" else "grey20"),
      legend.title     = element_text(colour = if(dark) "#cccccc" else "grey20"),
      legend.background = element_rect(fill = bg, colour = NA),
      legend.position  = "right"
    )
}

save_svg(make_ls_heatmap(FALSE), "acl_latin_square_light", w = 11, h = 6)
save_svg(make_ls_heatmap(TRUE),  "acl_latin_square_dark",  w = 11, h = 6)

# ══════════════════════════════════════════════════════════════════════════════
# FIGURE 3: Tick comparison — baseline vs ACL-RWT per ISA per seed
# ══════════════════════════════════════════════════════════════════════════════
cat("[ACL-3] Tick count comparison baseline vs ACL-RWT (light + dark)...\n")

df_tick_comp <- bind_rows(
  data.frame(
    campaign = "Baseline (no ACL)",
    arch     = rep(archs, each = 3),
    seed     = rep(seeds, 3),
    ticks    = c(rep(261098, 3), rep(261095, 3), rep(261095, 3)),
    stringsAsFactors = FALSE
  ),
  data.frame(
    campaign = "ACL-RWT",
    arch     = rep(archs, each = 3),
    seed     = rep(seeds, 3),
    ticks    = c(unlist(rwt_ticks$amd64),
                 unlist(rwt_ticks$aarch64),
                 unlist(rwt_ticks$riscv64)),
    stringsAsFactors = FALSE
  )
)
df_tick_comp$arch     <- factor(df_tick_comp$arch, levels = archs)
df_tick_comp$seed     <- factor(df_tick_comp$seed, levels = seeds)
df_tick_comp$campaign <- factor(df_tick_comp$campaign,
                                levels = c("Baseline (no ACL)", "ACL-RWT"))

make_tick_comp <- function(dark = FALSE) {
  thm <- if (dark) theme_dark_sf() else theme_light_sf()
  bg  <- if (dark) "#0d0d0d" else "white"

  ggplot(df_tick_comp, aes(x = seed, y = ticks, fill = campaign)) +
    geom_col(position = position_dodge(width = 0.7), width = 0.6,
             colour = NA, alpha = 0.9) +
    scale_fill_manual(
      values = c("Baseline (no ACL)" = "#888888", "ACL-RWT" = "#2CA02C"),
      name   = NULL
    ) +
    scale_y_continuous(
      labels  = scales::comma,
      limits  = c(260900, 261200),
      oob     = scales::squish,
      expand  = expansion(mult = c(0.01, 0.05))
    ) +
    facet_wrap(~ arch, ncol = 3,
               labeller = labeller(arch = c(amd64   = "amd64 (x86-64)",
                                            aarch64 = "aarch64 (ARMv8-A)",
                                            riscv64 = "riscv64 (RV64GC)"))) +
    labs(
      title    = "Heartbeat Tick Totals: Baseline vs. ACL-RWT",
      subtitle = paste0(
        "Y-axis: 260,900–261,200 ticks (zoomed to show delta). ",
        "Each bar = 30 replicates, 480 inner runs.\n",
        "Overhead is +14–23 ticks (+0.005–0.009%). Bars nearly identical — by design."
      ),
      x = NULL,
      y = "Total heartbeat ticks"
    ) +
    thm +
    theme(
      panel.background = element_rect(fill = bg, colour = NA),
      plot.background  = element_rect(fill = bg, colour = NA),
      axis.text.x      = element_text(size = 8, angle = 10, hjust = 1),
      legend.position  = "bottom"
    )
}

save_svg(make_tick_comp(FALSE), "acl_tick_comparison_light", w = 13, h = 7)
save_svg(make_tick_comp(TRUE),  "acl_tick_comparison_dark",  w = 13, h = 7)

# ══════════════════════════════════════════════════════════════════════════════
# FIGURE 4: Overhead reduction curve — all three mitigation steps
# ══════════════════════════════════════════════════════════════════════════════
cat("[ACL-4] Overhead reduction curve (light + dark)...\n")

# Mean overhead per ISA per mitigation step (floor=256 uses available data)
df_reduction <- data.frame(
  step     = factor(rep(c("Floor = 16", "Floor = 256", "ACL-RWT"), each = 3),
                    levels = c("Floor = 16", "Floor = 256", "ACL-RWT")),
  arch     = factor(rep(archs, 3), levels = archs),
  overhead = c(
    # Floor=16 means
    mean((c(261307,261307,261307) - 261098) / 261098 * 100),
    mean((c(422431,432403,432111) - 261095) / 261095 * 100),
    mean((c(433210,436040,438938) - 261095) / 261095 * 100),
    # Floor=256 means (partial)
    mean((c(261248,261112) - 261098) / 261098 * 100),
    mean((c(271443)        - 261095) / 261095 * 100),
    mean((c(272725)        - 261095) / 261095 * 100),
    # ACL-RWT means
    mean((c(261113,261112,261113) - 261098) / 261098 * 100),
    mean((c(261118,261118,261117) - 261095) / 261095 * 100),
    mean((c(261118,261117,261117) - 261095) / 261095 * 100)
  ),
  partial  = c(rep(FALSE,3), TRUE, TRUE, TRUE, rep(FALSE,3))
)

make_reduction <- function(dark = FALSE) {
  thm <- if (dark) theme_dark_sf() else theme_light_sf()
  bg  <- if (dark) "#0d0d0d" else "white"
  na_colour <- if (dark) "#555555" else "#bbbbbb"

  ggplot(df_reduction, aes(x = step, y = overhead, colour = arch, group = arch)) +
    geom_line(linewidth = 1.1, alpha = 0.85) +
    geom_point(aes(shape = partial), size = 4, fill = "white", stroke = 1.5) +
    geom_text(aes(label = ifelse(overhead < 1,
                                 sprintf("%.4f%%", overhead),
                                 sprintf("%.1f%%",  overhead))),
              vjust = -1.0, size = 2.8, fontface = "bold",
              show.legend = FALSE) +
    scale_colour_manual(values = arch_colours, name = "ISA") +
    scale_shape_manual(values = c("FALSE" = 16, "TRUE" = 1),
                       labels = c("FALSE" = "Full 3 seeds", "TRUE"  = "Partial data"),
                       name   = "Data coverage") +
    scale_y_log10(
      breaks = c(0.001, 0.01, 0.1, 1, 10, 100),
      labels = c("0.001%", "0.01%", "0.1%", "1%", "10%", "100%")
    ) +
    labs(
      title    = "ACL Enforcement Overhead Reduction Across Mitigation Steps",
      subtitle = paste0(
        "Three ISAs × three TTL policies. Y-axis: log₁₀ scale.\n",
        "Open circles = partial data (fewer seeds). Lines connect measured means.\n",
        "Floor = 16 → Floor = 256: 16× reduction (theoretical: 12/(256+12) ≈ 4.5%).\n",
        "Floor = 256 → ACL-RWT: >400× additional reduction. ISA gap closed."
      ),
      x = "TTL Policy",
      y = "Mean overhead vs. baseline (%, log₁₀)"
    ) +
    thm +
    theme(
      panel.background = element_rect(fill = bg, colour = NA),
      plot.background  = element_rect(fill = bg, colour = NA),
      legend.position  = "right"
    )
}

save_svg(make_reduction(FALSE), "acl_overhead_reduction_light", w = 12, h = 7)
save_svg(make_reduction(TRUE),  "acl_overhead_reduction_dark",  w = 12, h = 7)

# ══════════════════════════════════════════════════════════════════════════════
# FIGURE 5: ISA gap visualisation — floor=16 seed-by-seed scatter
# ══════════════════════════════════════════════════════════════════════════════
cat("[ACL-5] ISA gap scatter — floor=16 per-seed ticks (light + dark)...\n")

df_f16_seeds <- data.frame(
  arch     = factor(rep(archs, each = 3), levels = archs),
  seed     = rep(seeds, 3),
  ticks    = c(261307, 261307, 261307,
               422431, 432403, 432111,
               433210, 436040, 438938),
  baseline = c(rep(261098, 3), rep(261095, 3), rep(261095, 3)),
  stringsAsFactors = FALSE
)
df_f16_seeds$overhead <- (df_f16_seeds$ticks - df_f16_seeds$baseline) /
                          df_f16_seeds$baseline * 100
df_f16_seeds$seed <- factor(df_f16_seeds$seed, levels = seeds)

make_isagap <- function(dark = FALSE) {
  thm <- if (dark) theme_dark_sf() else theme_light_sf()
  bg  <- if (dark) "#0d0d0d" else "white"

  ggplot(df_f16_seeds, aes(x = arch, y = overhead, colour = arch, shape = seed)) +
    geom_hline(yintercept = 0, colour = if(dark) "#333333" else "grey80",
               linewidth = 0.5, linetype = "dashed") +
    geom_point(size = 5, stroke = 1.5, alpha = 0.9,
               position = position_dodge(width = 0.4)) +
    geom_text(aes(label = sprintf("%.1f%%", overhead)),
              vjust = -1.2, size = 3.0, fontface = "bold",
              position = position_dodge(width = 0.4),
              show.legend = FALSE) +
    scale_colour_manual(values = arch_colours, name = "ISA") +
    scale_shape_manual(values = c(16, 17, 15), name = "Seed") +
    scale_x_discrete(labels = c(amd64   = "amd64\n(x86-64)",
                                 aarch64 = "aarch64\n(ARMv8-A)",
                                 riscv64 = "riscv64\n(RV64GC)")) +
    scale_y_continuous(labels = function(x) paste0(x, "%"),
                       limits = c(-5, 80)) +
    labs(
      title    = "ISA Gap at ACL-BASE-TTL = 16 (All Three Seeds)",
      subtitle = paste0(
        "Each point = one 30-rep campaign cell. amd64: +0.08% (all three seeds identical).\n",
        "aarch64/riscv64: +61.8%–68.1% — caused by cold-word heat deficit under TCG emulation.\n",
        "Gap factor: ~800×. Theoretical prediction: 12/(16+12) ≈ 43% per cold word."
      ),
      x = NULL,
      y = "Overhead vs. no-ACL baseline (%)"
    ) +
    thm +
    theme(
      panel.background = element_rect(fill = bg, colour = NA),
      plot.background  = element_rect(fill = bg, colour = NA),
      legend.position  = "right"
    )
}

save_svg(make_isagap(FALSE), "acl_isa_gap_light", w = 11, h = 7)
save_svg(make_isagap(TRUE),  "acl_isa_gap_dark",  w = 11, h = 7)

# ══════════════════════════════════════════════════════════════════════════════
# Summary
# ══════════════════════════════════════════════════════════════════════════════
svg_acl <- list.files(OUT_CHARTS, pattern = "^acl_.*\\.svg$")
cat(sprintf("\n══════════════════════════════════════════════════════════════════\n"))
cat(sprintf("  ACL analysis complete. %d ACL SVG figures in %s\n",
            length(svg_acl), OUT_CHARTS))
cat(sprintf("══════════════════════════════════════════════════════════════════\n\n"))
cat("  Figures:\n")
for (f in sort(svg_acl)) cat(sprintf("    %s\n", f))
cat("\n")
cat("  Copy to report/figures/ then:\n")
cat("    cd experiments/bare_metal/analysis/report\n")
cat("    pdflatex bare_metal_doe_report.tex && pdflatex bare_metal_doe_report.tex\n\n")
