# SVG Generation Summary

**Total SVG Files:** 112
**Generation Date:** December 15, 2025
**Method:** Regenerated from real experimental CSV data (NO PNG conversion)

## Overview

All SVG charts have been regenerated from real StarForth experimental data using R/ggplot2. These are:
- ✅ **Standalone**: Open directly in web browsers
- ✅ **Reusable**: Embed in LaTeX, HTML, Markdown, presentations
- ✅ **Scalable**: Vector graphics, infinite resolution
- ✅ **Data-driven**: Generated from real experimental CSV files

## Chart Categories

### 0. Window Scaling Experiment (5 new)

**Data Source:** `experiments/window_scaling/results_run_one/raw/hb/*.csv` (355 heartbeat files)
**Design:** 12 window sizes (512-65536 bytes), ~30 replicates per window

| File | Description |
|------|-------------|
| `window_K_distribution_by_size.svg` | K distribution by window size (violin + box plots) (12×8) |
| `window_K_vs_window_size.svg` | K statistic vs window size with error bands (12×8) |
| `window_stability_cv.svg` | Stability (CV) by window size (12×8) |
| `window_K_deviation.svg` | James Law deviation \|K - 1.0\| by window size (12×8) |
| `window_frequency_vs_size.svg` | Fundamental frequency ω₀ vs window size (12×8) |

### 1. L8 Attractor Analysis Charts (11 existing)

**Data Source:** `experiments/l8_attractor_map/results_20251209_145907/analysis_output/charts/`  
**Original Generation:** December 9, 2025

| File | Description |
|------|-------------|
| `binary_2cycle_analysis.svg` | Binary 2-cycle workload temporal analysis (1800×1200) |
| `duration_vs_ticks.svg` | Run duration vs tick count scatter (720×432) |
| `heat_timeseries_runNA.svg` | Word heat evolution over time (720×360) |
| `heat_vs_jitter.svg` | Heat vs jitter correlation (720×432) |
| `hotwords_timeseries_runNA.svg` | Hot words count evolution (720×360) |
| `hotwords_vs_interval.svg` | Hot words vs tick interval (720×432) |
| `interval_hist_all.svg` | Tick interval distribution, all runs (720×432) |
| `interval_hist_by_workload.svg` | Tick interval by workload (864×576) |
| `jitter_hist_by_workload.svg` | Jitter distribution by workload (864×576) |
| `jitter_timeseries_runNA.svg` | Jitter evolution over time (720×360) |
| `tick_count_boxplot.svg` | Tick count comparison by workload (720×432) |

### 2. L8 Attractor Report Charts (11 new)

**Data Source:** `experiments/l8_attractor_map/results_20251209_145907/heartbeat_all.csv` (4,351 rows)  
**Workload Mapping:** `workload_mapping.csv` (180 experimental runs)

| File | Description |
|------|-------------|
| `l8_boxplot_comparison.svg` | Tick count by workload comparison (10×6) |
| `l8_violin_plot.svg` | Tick interval distribution violin plot (10×6) |
| `l8_distributions_by_workload.svg` | Tick interval density distributions (12×8) |
| `l8_heart_rate_evolution.svg` | Heart rate temporal evolution (12×6) |
| `l8_heart_rate_variability.svg` | Heart rate variability box plot (10×6) |
| `l8_heat_correlation.svg` | Heat vs tick interval correlation (10×6) |
| `l8_phase_space_portrait.svg` | Phase space: heat vs interval (10×8) |
| `l8_phase_space_portrait_II.svg` | Phase space: hot words vs heat (10×8) |
| `l8_timeseries_runs.svg` | Heat time series, first 10 runs (12×6) |
| `l8_word_heat_temporal.svg` | Word heat temporal evolution (12×6) |
| `l8_autocorrelation.svg` | Autocorrelation function, run 180 (10×6) |

### 3. DoE 30 Reps Experiment (38 charts)

**Data Source:** `experiments/doe_2x7/results_30reps_RAJ_2025.11.22_1700hrs/doe_results_20251122_085656.csv`  
**Rows:** 3,840 experimental runs  
**Design:** 2^7 factorial design, 30 replicates

#### Distribution (1)
- `doe30_dist_ns_per_word.svg` - Performance distribution histogram

#### Box Plots by Loop (7)
- `doe30_box_L1_heat_tracking.svg`
- `doe30_box_L2_rolling_window.svg`
- `doe30_box_L3_linear_decay.svg`
- `doe30_box_L4_pipelining_metrics.svg`
- `doe30_box_L5_window_inference.svg`
- `doe30_box_L6_decay_inference.svg`
- `doe30_box_L7_adaptive_heartrate.svg`

#### Facet Plots by Loop (7)
- `doe30_facet_L1_heat_tracking.svg`
- `doe30_facet_L2_rolling_window.svg`
- `doe30_facet_L3_linear_decay.svg`
- `doe30_facet_L4_pipelining_metrics.svg`
- `doe30_facet_L5_window_inference.svg`
- `doe30_facet_L6_decay_inference.svg`
- `doe30_facet_L7_adaptive_heartrate.svg`

#### Interaction Plots (21)
- `doe30_interaction_01_L1_heat_tracking_x_L2_rolling_window.svg`
- `doe30_interaction_02_L1_heat_tracking_x_L3_linear_decay.svg`
- ... (19 more interaction plots)
- `doe30_interaction_21_L6_decay_inference_x_L7_adaptive_heartrate.svg`

#### Summary Charts (2)
- `doe30_corr_metrics.svg` - Correlation matrix heatmap
- `doe30_effect_heatmap.svg` - Main effects heatmap

### 4. DoE 300 Reps Experiment (39 charts)

**Data Source:** `experiments/doe_2x7/results_300_reps_RAJ_2025.11.26.10:30hrs/doe_results_20251123_093204.csv`
**Rows:** 38,400 experimental runs
**Design:** 2^7 factorial design, 300 replicates

### 5. Window Scaling Experiment (5 charts)

**Data Source:** `experiments/window_scaling/results_run_one/raw/hb/run-*.csv`
**Files:** 355 individual heartbeat CSV files
**Design:** 12 window sizes (512, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 16384, 32769, 52153, 65536 bytes)

#### All Charts (5)
- `window_K_distribution_by_size.svg` - K distribution by window size
- `window_K_vs_window_size.svg` - K statistic vs window size
- `window_stability_cv.svg` - Stability (CV) by window size
- `window_K_deviation.svg` - James Law deviation by window size
- `window_frequency_vs_size.svg` - Fundamental frequency vs window size

### 6. L8 Validation Experiment (8 charts)

**Data Source:** `experiments/l8_validation/l8_validation_20251208_102205/l8_validation_results.csv`
**Rows:** 1,813 experimental runs
**Design:** 8 control strategies × 5 workload types (STABLE, DIVERSE, VOLATILE, TEMPORAL, TRANSITION)

#### All Charts (8)
- `l8_validation_performance_by_strategy.svg` - Performance box plots by strategy and workload
- `l8_validation_interaction_plot.svg` - Strategy × workload interaction plot
- `l8_validation_stability_cv.svg` - Stability (CV) by strategy and workload
- `l8_validation_strategy_ranking.svg` - Strategy ranking per workload
- `l8_validation_performance_distribution.svg` - Overall performance distribution
- `l8_validation_pareto_frontier.svg` - Speed vs stability Pareto frontier
- `l8_validation_l8_margin.svg` - L8_ADAPTIVE performance margin per workload
- `l8_validation_heatmap.svg` - Performance heatmap (strategy × workload)

#### Distribution (1)
- `doe300_dist_ns_per_word.svg` - Performance distribution histogram

#### Box Plots by Loop (7)
- `doe300_box_L1_heat_tracking.svg`
- `doe300_box_L2_rolling_window.svg`
- `doe300_box_L3_linear_decay.svg`
- `doe300_box_L4_pipelining_metrics.svg`
- `doe300_box_L5_window_inference.svg`
- `doe300_box_L6_decay_inference.svg`
- `doe300_box_L7_adaptive_heartrate.svg`

#### Facet Plots by Loop (7)
- `doe300_facet_L1_heat_tracking.svg`
- `doe300_facet_L2_rolling_window.svg`
- `doe300_facet_L3_linear_decay.svg`
- `doe300_facet_L4_pipelining_metrics.svg`
- `doe300_facet_L5_window_inference.svg`
- `doe300_facet_L6_decay_inference.svg`
- `doe300_facet_L7_adaptive_heartrate.svg`

#### Interaction Plots (21)
- `doe300_interaction_01_L1_heat_tracking_x_L2_rolling_window.svg`
- `doe300_interaction_02_L1_heat_tracking_x_L3_linear_decay.svg`
- ... (19 more interaction plots)
- `doe300_interaction_21_L6_decay_inference_x_L7_adaptive_heartrate.svg`

#### Summary Charts (3)
- `doe300_corr_metrics.svg` - Correlation matrix heatmap
- `doe300_effect_heatmap.svg` - Main effects heatmap
- `doe_comparison_30v300.svg` - Effect size comparison: 30 vs 300 reps

## Regeneration Scripts

### Main Script

**Location:** `papers/LaTeX/scripts/regenerate_all_svgs.R`
**Total Lines:** 460+
**Dependencies:** R packages (dplyr, tidyr, ggplot2, readr, reshape2, svglite)

```bash
cd /home/rajames/CLionProjects/StarForth/papers/LaTeX/scripts
Rscript regenerate_all_svgs.R
```

Output: 88 SVG files in `papers/LaTeX/figures/` (plus 11 existing = 99 total)

### Window Scaling Script

**Location:** `papers/LaTeX/scripts/regenerate_window_scaling_svgs.R`
**Total Lines:** 295
**Dependencies:** R packages (dplyr, tidyr, ggplot2, readr, scales)

```bash
cd /home/rajames/CLionProjects/StarForth/papers/LaTeX/scripts
Rscript regenerate_window_scaling_svgs.R
```

Output: 5 SVG files in `papers/LaTeX/figures/` (window scaling charts)

### L8 Validation Script

**Location:** `papers/LaTeX/scripts/regenerate_l8_validation_svgs.R`
**Total Lines:** 299
**Dependencies:** R packages (dplyr, tidyr, ggplot2, readr, scales)

```bash
cd /home/rajames/CLionProjects/StarForth/papers/LaTeX/scripts
Rscript regenerate_l8_validation_svgs.R
```

Output: 8 SVG files in `papers/LaTeX/figures/` (L8 validation charts)

### Total

**112 SVG files** = 11 (L8 existing) + 11 (L8 regenerated) + 38 (DoE 30) + 39 (DoE 300) + 5 (Window Scaling) + 8 (L8 Validation)

## Data Provenance

All charts are generated from **real experimental data** collected during StarForth VM experiments:

1. **L8 Attractor Experiment**
   - Date: December 9, 2025, 14:59:07
   - Runs: 360 experimental runs (6 workloads × 60 replicates)
   - Data: `heartbeat_all.csv` (4,351 heartbeat ticks)

2. **DoE 30 Reps Experiment**
   - Date: November 22, 2025, 08:56:56
   - Runs: 3,840 experimental runs (128 configurations × 30 replicates)
   - Design: 2^7 full factorial

3. **DoE 300 Reps Experiment**
   - Date: November 23, 2025, 09:32:04
   - Runs: 38,400 experimental runs (128 configurations × 300 replicates)
   - Design: 2^7 full factorial

4. **Window Scaling Experiment**
   - Date: December 15, 2025, 12:53 (SVG generation)
   - Runs: 355 experimental runs (12 window sizes × ~30 replicates)
   - Design: Single-factor window size variation (512-65536 bytes)
   - Data: Individual heartbeat CSV files per run

5. **L8 Validation Experiment**
   - Date: December 8, 2025, 10:22:05 (experiment run)
   - Runs: 1,813 experimental runs (8 strategies × 5 workloads)
   - Design: 8×5 factorial (L8_ADAPTIVE + 7 static configs × 5 workload types)
   - Data: Single CSV file with all runs

## File Sizes

```bash
Total size: ~13.9 MB
Average per chart: ~127 KB

Largest files:
- doe300_corr_metrics.svg (594K)
- binary_2cycle_analysis.svg (245K)
- l8_validation_performance_distribution.svg (178K)
- doe30_corr_metrics.svg (178K)

Smallest files:
- l8_validation_l8_margin.svg (6.1K)
- window_frequency_vs_size.svg (7.1K)
- window_stability_cv.svg (7.2K)
- window_K_deviation.svg (7.3K)
```

## Usage Examples

See [README.md](README.md) for detailed usage examples for:
- Standalone viewing in browsers
- Embedding in HTML
- Including in LaTeX documents
- Markdown integration
- PowerPoint/Google Slides

## Notes

- All charts use real experimental data (NO synthetic/mock data)
- NO PNG → SVG conversion was performed
- All charts are pure data-driven regenerations
- SVG format ensures publication-quality resolution
- Charts are both standalone AND reusable artifacts
- Window scaling charts validate James Law: K = Λ × (DoF + 1) / W ≈ 1.0
- L8 validation charts test hypothesis that L8_ADAPTIVE matches or exceeds best static config per workload
