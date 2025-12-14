# StarForth Factorial DoE with Heartbeat Integration

**Date:** November 20, 2025
**Status:** Design Phase
**Experiment:** 2^6 Factorial with Heartbeat-Aware Metrics Collection

---

## Overview

This document describes the **second-generation factorial DoE** that incorporates heartbeat observability into metric collection. Building on the baseline DOE (Stage 1: 3200 runs, 6 factors, no heartbeat), this experiment captures **per-tick heartbeat dynamics** to identify configurations with optimal stability characteristics.

### Key Innovation

Instead of measuring static performance, this experiment measures **temporal stability**:
- How quickly does the system converge under load changes?
- How much does heartbeat interval modulate in response to workload?
- What is the jitter envelope after settling?
- Which configuration achieves smoothest load→heartbeat coupling?

---

## Design Points (Top 5 Configurations from Stage 1)

Based on Stage 1 analysis, the elite configurations are:

1. **Config 1:** `1_0_1_1_1_0` (F1=ON, F2=OFF, F3=ON, F4=ON, F5=ON, F6=OFF)
2. **Config 2:** `1_0_1_1_1_1` (F1=ON, F2=OFF, F3=ON, F4=ON, F5=ON, F6=ON)
3. **Config 3:** `1_1_0_1_1_1` (F1=ON, F2=ON, F3=OFF, F4=ON, F5=ON, F6=ON)
4. **Config 4:** `1_0_1_0_1_0` (F1=ON, F2=OFF, F3=ON, F4=OFF, F5=ON, F6=OFF)
5. **Config 5:** `0_1_1_0_1_1` (F1=OFF, F2=ON, F3=ON, F4=OFF, F5=ON, F6=ON)

**Factors Mapping:**
- F1 = ENABLE_LOOP_1_HEAT_TRACKING
- F2 = ENABLE_LOOP_2_ROLLING_WINDOW
- F3 = ENABLE_LOOP_3_LINEAR_DECAY
- F4 = ENABLE_LOOP_4_PIPELINING_METRICS
- F5 = ENABLE_LOOP_5_WINDOW_INFERENCE
- F6 = ENABLE_LOOP_6_DECAY_INFERENCE

---

## New Heartbeat Metrics (Per-Tick Collection)

### 1. Heartbeat Interval Dynamics

```c
struct HeartbeatTickMetrics {
    uint64_t tick_count;                    /* Absolute tick number */
    uint64_t tick_interval_ns;              /* Wall-clock duration of this tick */
    uint64_t instantaneous_workload_ops;    /* Dictionary lookups in this tick */
    double   instantaneous_load_ratio;      /* (ops this tick) / (avg ops/tick) */
};
```

**Purpose:** Capture how heartbeat frequency modulates in response to workload intensity.

### 2. Cache Performance (Per-Tick Window)

```c
struct CacheWindowMetrics {
    uint64_t tick_start;
    uint64_t cache_hits_window;             /* Hits in last 100 ticks */
    uint64_t cache_attempts_window;         /* Attempts in last 100 ticks */
    double   cache_hit_percent_window;      /* Rolling window %, 0-100 */
};
```

**Purpose:** Measure cache stability, not just final hit rate.

### 3. Rolling Window Diagnostics

```c
struct WindowDiagnostics {
    uint32_t effective_window_size;         /* Current adaptive size */
    uint64_t pattern_diversity;             /* Unique word IDs observed */
    double   diversity_growth_rate;         /* % new patterns per 1000 executions */
    int      is_warm;                       /* 1 if window is representative */
};
```

**Purpose:** Track window convergence and saturation.

### 4. Jitter Envelope

```c
struct JitterMetrics {
    double   tick_interval_mean_ns;         /* Average heartbeat interval */
    double   tick_interval_stddev_ns;       /* StdDev of interval */
    double   tick_interval_cv;              /* Coefficient of variation (σ/μ) */
    uint64_t outlier_count;                 /* Ticks >3σ from mean */
    double   outlier_ratio;                 /* Outliers / total ticks */
};
```

**Purpose:** Measure temporal smoothness and jitter control.

### 5. Decay Slope Tracking

```c
struct DecayTrajectory {
    uint64_t tick_sample_count;             /* How many ticks sampled */
    double   decay_slope_observed;          /* Fitted slope from observations */
    double   slope_convergence_rate;        /* Speed of slope convergence */
    int      slope_direction;               /* -1=decreasing, 0=stable, +1=increasing */
};
```

**Purpose:** Monitor whether decay slope inference is tracking correctly.

### 6. Load-Response Coupling

```c
struct LoadResponseCoupling {
    double   correlation_load_to_interval;  /* Correlation(workload, heartbeat_interval) */
    double   response_latency_ticks;        /* Ticks until interval reacts to load change */
    double   overshoot_ratio;               /* Peak response / steady-state response */
    double   settling_time_ticks;           /* Ticks to settle within 10% of target */
};
```

**Purpose:** Measure how quickly and smoothly heartbeat responds to workload shifts.

---

## Extended DoeMetrics Struct

Add to `include/doe_metrics.h`:

```c
typedef struct {
    /* === Original Metrics (preserved) === */
    /* ... existing 35+ metrics ... */

    /* === NEW: Heartbeat Stability Metrics === */

    /* Tick dynamics */
    uint64_t total_heartbeat_ticks;         /* Total ticks during run */
    double   tick_interval_mean_ns;         /* Average heartbeat interval */
    double   tick_interval_stddev_ns;       /* StdDev of heartbeat interval */
    double   tick_interval_cv;              /* Coefficient of variation */
    uint64_t tick_outlier_count_3sigma;     /* Ticks >3σ from mean */
    double   tick_outlier_ratio;            /* Outliers / total ticks */

    /* Cache stability window */
    double   cache_hit_percent_rolling_mean;     /* Rolling avg, last 100 ticks */
    double   cache_hit_percent_rolling_stddev;   /* Rolling stddev */
    double   cache_stability_index;              /* How stable is cache hit %? */

    /* Window convergence */
    uint32_t window_final_effective_size;   /* Final adaptive window size */
    double   pattern_diversity_final;       /* Final unique patterns observed */
    double   diversity_growth_final;        /* Growth rate at end */
    int      window_warmth_achieved;        /* 1 = warm, 0 = still cold */

    /* Decay slope convergence */
    double   decay_slope_fitted_final;      /* Final fitted decay slope */
    double   decay_slope_convergence_rate;  /* How fast did slope converge? */
    int      decay_slope_stable;            /* 1 = stable direction, 0 = oscillating */

    /* Load-response */
    double   load_interval_correlation;     /* Correlation(load, heartbeat_interval) */
    double   response_latency_ticks;        /* Ticks until response to workload change */
    double   overshoot_ratio;               /* Peak response / steady-state */
    double   settling_time_ticks;           /* Time to 10% settling band */

    /* Aggregate stability score */
    double   overall_stability_score;       /* Composite metric: (low jitter + fast convergence + tight correlation) */

} DoeMetrics;  /* Now extends to ~65+ columns */
```

---

## Heartbeat Data Collection Architecture

### Per-Tick Logging (Lightweight)

During VM execution, capture minimal per-tick data:

```c
/* In vm.c vm_tick_heartbeat_worker() */
void vm_tick_heartbeat_worker(VM *vm) {
    HeartbeatSnapshot snapshot = {
        .published_tick = vm->heartbeat.tick_count,
        .published_ns = platform_monotonic_ns(),
        .window_width = vm->rolling_window.effective_window_size,
        .decay_slope_q48 = vm->decay_slope_q48,
        .hot_word_count = vm->hot_word_count_at_check,
        .stale_word_count = vm->stale_word_count_at_check,
        .total_heat = vm->total_heat_at_last_check,
    };

    /* Publish snapshot (double-buffered, lock-free read) */
    int next_slot = 1 - vm->heartbeat.snapshot_index;
    vm->heartbeat.snapshots[next_slot] = snapshot;
    vm->heartbeat.snapshot_index = next_slot;
}
```

### Post-Run Analysis (Batch Processing)

After test execution completes, analyze collected tick data:

```c
/* In src/doe_metrics.c metrics_from_vm() */
void analyze_heartbeat_trajectory(VM *vm, DoeMetrics *metrics) {
    HeartbeatSnapshot *snapshots = vm->heartbeat.snapshots;
    uint64_t total_ticks = vm->heartbeat.tick_count;

    /* 1. Compute tick interval statistics */
    double intervals[total_ticks];
    for (size_t i = 1; i < total_ticks; i++) {
        intervals[i] = snapshots[i].published_ns - snapshots[i-1].published_ns;
    }
    metrics->tick_interval_mean_ns = compute_mean(intervals, total_ticks);
    metrics->tick_interval_stddev_ns = compute_stddev(intervals, total_ticks);
    metrics->tick_interval_cv = metrics->tick_interval_stddev_ns / metrics->tick_interval_mean_ns;

    /* 2. Compute outlier ratio (ticks >3σ from mean) */
    double threshold_3sigma = metrics->tick_interval_mean_ns + 3.0 * metrics->tick_interval_stddev_ns;
    uint64_t outliers = 0;
    for (size_t i = 0; i < total_ticks; i++) {
        if (intervals[i] > threshold_3sigma) outliers++;
    }
    metrics->tick_outlier_ratio = (double)outliers / total_ticks;

    /* 3. Compute decay slope convergence */
    double slopes[total_ticks];
    for (size_t i = 100; i < total_ticks; i++) {  /* Skip warm-up */
        slopes[i] = snapshots[i].decay_slope_q48 / 65536.0;
    }
    metrics->decay_slope_fitted_final = slopes[total_ticks - 1];
    metrics->decay_slope_convergence_rate = compute_convergence_rate(slopes, total_ticks);

    /* 4. Compute overall stability score */
    double stability = 0.0;
    stability += (100.0 - metrics->tick_interval_cv);           /* Lower CV = higher score */
    stability += (100.0 - metrics->tick_outlier_ratio * 100.0); /* Fewer outliers = higher */
    stability += metrics->decay_slope_convergence_rate;         /* Fast convergence = higher */
    metrics->overall_stability_score = stability / 3.0;
}
```

---

## CSV Output Schema (Extended)

**New columns (per heartbeat experiment run):**

```
timestamp,configuration,run_number,
[... original 38 columns ...],
total_heartbeat_ticks,
tick_interval_mean_ns,tick_interval_stddev_ns,tick_interval_cv,
tick_outlier_count_3sigma,tick_outlier_ratio,
cache_hit_percent_rolling_mean,cache_hit_percent_rolling_stddev,cache_stability_index,
window_final_effective_size,pattern_diversity_final,diversity_growth_final,window_warmth_achieved,
decay_slope_fitted_final,decay_slope_convergence_rate,decay_slope_stable,
load_interval_correlation,response_latency_ticks,overshoot_ratio,settling_time_ticks,
overall_stability_score
```

**Total:** ~60 columns per run

---

## R Analysis Template

```r
library(tidyverse)
library(ggplot2)
library(gridExtra)

# Load heartbeat DoE data
doe_data <- read.csv("experiment_results_with_heartbeat.csv") %>%
  mutate(
    configuration = as.factor(configuration),
    timestamp = as.POSIXct(timestamp)
  )

# ========== MAIN ANALYSIS ==========

# 1. Stability Ranking
stability_rank <- doe_data %>%
  group_by(configuration) %>%
  summarize(
    mean_stability_score = mean(overall_stability_score),
    mean_jitter_cv = mean(tick_interval_cv),
    mean_convergence_rate = mean(decay_slope_convergence_rate),
    mean_settling_time = mean(settling_time_ticks),
    n_runs = n()
  ) %>%
  mutate(
    rank = rank(-mean_stability_score, ties.method = "min")
  ) %>%
  arrange(rank)

print("=== STABILITY RANKINGS ===")
print(stability_rank)

# 2. Visualization: Stability Scores
ggplot(doe_data, aes(x = reorder(configuration, overall_stability_score, FUN=median),
                      y = overall_stability_score,
                      fill = configuration)) +
  geom_boxplot(alpha = 0.7) +
  geom_jitter(width = 0.2, alpha = 0.4) +
  coord_flip() +
  theme_minimal() +
  labs(title = "Overall Stability Scores by Configuration",
       x = "Configuration",
       y = "Stability Score (0-100)")
ggsave("01_stability_scores.png", width = 10, height = 6)

# 3. Visualization: Jitter Control (CV)
ggplot(doe_data, aes(x = reorder(configuration, tick_interval_cv, FUN=median),
                      y = tick_interval_cv,
                      fill = configuration)) +
  geom_boxplot(alpha = 0.7) +
  coord_flip() +
  theme_minimal() +
  labs(title = "Heartbeat Jitter (Coefficient of Variation)",
       x = "Configuration",
       y = "CV (lower = less jitter)")
ggsave("02_jitter_control.png", width = 10, height = 6)

# 4. Visualization: Convergence Speed
ggplot(doe_data, aes(x = reorder(configuration, decay_slope_convergence_rate, FUN=median),
                      y = decay_slope_convergence_rate,
                      fill = configuration)) +
  geom_boxplot(alpha = 0.7) +
  coord_flip() +
  theme_minimal() +
  labs(title = "Decay Slope Convergence Speed",
       x = "Configuration",
       y = "Convergence Rate (ticks)")
ggsave("03_convergence_speed.png", width = 10, height = 6)

# 5. Correlation: Load Response Coupling
ggplot(doe_data, aes(x = load_interval_correlation,
                      y = overall_stability_score,
                      color = configuration)) +
  geom_point(alpha = 0.6, size = 2) +
  facet_wrap(~configuration) +
  theme_minimal() +
  labs(title = "Load-Response Coupling vs Stability",
       x = "Correlation(load, interval)",
       y = "Overall Stability Score")
ggsave("04_load_response_coupling.png", width = 12, height = 8)

# 6. Statistical Test: Is top config significantly better?
top_config <- stability_rank$configuration[1]
other_configs <- stability_rank$configuration[-1]

top_data <- doe_data %>% filter(configuration == top_config) %>% pull(overall_stability_score)
other_data <- doe_data %>% filter(configuration %in% other_configs) %>% pull(overall_stability_score)

t_test <- t.test(top_data, other_data)
print(sprintf("\nT-test: %s vs others\np-value: %.4f", top_config, t_test$p.value))

# 7. Golden Configuration Summary
cat("\n=== GOLDEN CONFIGURATION CANDIDATE ===\n")
golden <- stability_rank %>% slice(1)
print(golden)

cat("\nInterpretation:")
cat(sprintf("\n- Lowest jitter (CV): %.4f", golden$mean_jitter_cv))
cat(sprintf("\n- Fastest convergence: %.1f ticks", golden$mean_convergence_rate))
cat(sprintf("\n- Settling time: %.1f ticks", golden$mean_settling_time))
cat(sprintf("\n- Recommendation: Use '%s' as golden configuration\n", golden$configuration))
```

---

## Execution Plan

### Phase 1: Implementation (3-4 hours)
1. Extend `DoeMetrics` struct with heartbeat columns
2. Implement `analyze_heartbeat_trajectory()` in `doe_metrics.c`
3. Capture snapshot data during heartbeat ticks
4. Update CSV schema

### Phase 2: Experiment Execution (2-4 hours)
1. Create `run_factorial_doe_with_heartbeat.sh` script
2. Run 5 elite configurations × 50 runs each = 250 total runs
3. Collect heartbeat + performance data

### Phase 3: Analysis (2-3 hours)
1. Load CSV in R
2. Compute stability rankings
3. Generate visualizations
4. Identify golden configuration

---

## Expected Outcomes

**Primary Question:** Which of the 5 elite configurations exhibits:
- Lowest heartbeat jitter (CV)?
- Fastest convergence to steady state?
- Strongest load-response correlation?
- Cleanest temporal signature?

**Expected Winner:** Configuration with balanced F-factors, likely:
- F1=1 (heat tracking required)
- F3=1 or F5=1 (decay or inference, but not both)
- F6=0 or F6=1 (decay inference optional)

**Golden Configuration:** Becomes baseline physics for MamaForth & PapaForth downstream work.

---

## Success Criteria

✓ All 5 configurations build and run without segfault
✓ Heartbeat data collected for all 250 runs
✓ Stability rankings computed and visualized
✓ Golden configuration identified with p < 0.05 significance
✓ R analysis report generated with 5+ plots

---

**Version:** 1.0
**Status:** Ready for implementation
**Next Step:** Implement heartbeat metrics capture and extended DoE script