# LaTeX Tables Summary

**Total Tables:** 9
**Generation Date:** December 15, 2025
**Method:** Generated from real experimental CSV data using R/xtable

## Overview

All tables are generated from real StarForth experimental data and formatted as publication-ready LaTeX tables. These can be directly included in LaTeX documents using `\input{tables/tablename.tex}`.

## Table Catalog

### DoE 30 Reps Experiment (2 tables)

**Data Source:** `experiments/doe_2x7/results_30reps_RAJ_2025.11.22_1700hrs/doe_results_20251122_085656.csv`
**Runs:** 3,840 experimental runs

| File | Description | Label |
|------|-------------|-------|
| `doe30_main_effects.tex` | Main effects of 7 feedback loops on performance | `tab:doe30_main_effects` |
| `doe30_top_configs.tex` | Top 10 configurations ranked by performance | `tab:doe30_top_configs` |

### DoE 300 Reps Experiment (2 tables)

**Data Source:** `experiments/doe_2x7/results_300_reps_RAJ_2025.11.26.10:30hrs/doe_results_20251123_093204.csv`
**Runs:** 38,400 experimental runs

| File | Description | Label |
|------|-------------|-------|
| `doe300_main_effects.tex` | Main effects of 7 feedback loops on performance | `tab:doe300_main_effects` |
| `doe300_top_configs.tex` | Top 10 configurations ranked by performance | `tab:doe300_top_configs` |

### Window Scaling Experiment (1 table)

**Data Source:** `experiments/window_scaling/results_run_one/raw/hb/*.csv`
**Runs:** 355 experimental runs

| File | Description | Label |
|------|-------------|-------|
| `window_scaling_summary.tex` | Summary statistics by window size (K, CV, frequency) | `tab:window_scaling_summary` |

### L8 Validation Experiment (3 tables)

**Data Source:** `experiments/l8_validation/l8_validation_20251208_102205/l8_validation_results.csv`
**Runs:** 1,813 experimental runs

| File | Description | Label |
|------|-------------|-------|
| `l8_strategy_summary.tex` | Overall strategy performance across all workloads | `tab:l8_strategy_summary` |
| `l8_best_per_workload.tex` | Top 3 strategies per workload type | `tab:l8_best_per_workload` |
| `l8_adaptive_margin.tex` | L8_ADAPTIVE performance margin vs best static config | `tab:l8_adaptive_margin` |

### L8 Attractor Experiment (1 table)

**Data Source:** `experiments/l8_attractor_map/results_20251209_145907/heartbeat_all.csv`
**Ticks:** 4,351 heartbeat ticks

| File | Description | Label |
|------|-------------|-------|
| `l8_attractor_summary.tex` | Overall summary statistics (all workloads combined) | `tab:l8_attractor_summary` |

## Usage in LaTeX Documents

### Basic Usage

```latex
\documentclass{article}
\usepackage{booktabs}

\begin{document}

% Include a table
\input{tables/doe30_main_effects.tex}

% Reference the table
See Table~\ref{tab:doe30_main_effects} for main effects analysis.

\end{document}
```

### Customizing Tables

Tables can be customized by editing the generated `.tex` files or by modifying the R script before regeneration.

## Regeneration Script

**Location:** `papers/LaTeX/scripts/regenerate_all_tables.R`
**Dependencies:** R packages (dplyr, tidyr, readr, xtable)

### How to Regenerate

```bash
cd /home/rajames/CLionProjects/StarForth/papers/src/scripts
Rscript regenerate_all_tables.R
```

**Output:** 9 LaTeX table files in `papers/LaTeX/tables/`

## File Sizes

```bash
Total size: ~6.0 KB
Average per table: ~667 bytes

Largest tables:
- window_scaling_summary.tex (980 bytes) - 12 window sizes
- doe30_top_configs.tex (935 bytes) - 10 configurations

Smallest tables:
- l8_adaptive_margin.tex (495 bytes) - 5 workloads
- l8_attractor_summary.tex (533 bytes) - 6 metrics
```

## Data Provenance

All tables generated from **real experimental data**:

1. **DoE 30 Reps** - November 22, 2025, 08:56:56 (3,840 runs)
2. **DoE 300 Reps** - November 23, 2025, 09:32:04 (38,400 runs)
3. **Window Scaling** - December 15, 2025, 12:53 (355 runs)
4. **L8 Validation** - December 8, 2025, 10:22:05 (1,813 runs)
5. **L8 Attractor** - December 9, 2025, 14:59:07 (4,351 ticks)

## Notes

- All tables use `xtable` package for LaTeX formatting
- Tables follow standard LaTeX table conventions with `\caption`, `\label`, and `\begin{tabular}`
- Default placement is `[ht]` (here, top)
- All numeric values are formatted with appropriate precision
- Tables are self-contained and can be used independently
