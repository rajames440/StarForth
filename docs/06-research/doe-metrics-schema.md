# StarForth Design of Experiments (DoE) Metrics Schema

**Version:** 1.0
**Date:** 2025-11-19
**Target:** R statistical analysis (ready for tidyverse, ggplot2, lm models)

---

## CSV Format

Each DoE run produces a **single CSV row** with the following structure:

```
timestamp,configuration,run_number,<35 metrics from C API>
```

### Metadata Columns (3)

| Column | Type | Description |
|--------|------|-------------|
| `timestamp` | ISO 8601 | When the run started (YYYY-MM-DDTHH:MM:SS) |
| `configuration` | string | Build configuration (e.g., "A_BASELINE__HB_ON") |
| `run_number` | integer | Sequential run number within configuration (1-N) |

### Metrics Columns (35)

All metrics are extracted directly from the VM at the end of each test run via `metrics_from_vm()`.

---

## Metrics Breakdown by Category

### 1. Cache Metrics (8 columns)

| Column | Type | Range | Description |
|--------|------|-------|-------------|
| `total_lookups` | uint32_t | 0+ | Total dictionary lookups during run |
| `cache_hits` | uint64_t | 0+ | Cache hits (if ENABLE_HOTWORDS_CACHE=1) |
| `cache_hit_percent` | double | 0-100 | Hit percentage (formula: 100 * hits / total_lookups) |
| `bucket_hits` | uint64_t | 0+ | Bucket search hits (after cache miss) |
| `bucket_hit_percent` | double | 0-100 | Bucket hit percentage (formula: 100 * hits / total_lookups) |
| `cache_hit_latency_ns` | int64_t | 0+ | Average cache hit latency (nanoseconds) |
| `cache_hit_stddev_ns` | int64_t | 0+ | Cache hit latency standard deviation |
| `bucket_search_latency_ns` | int64_t | 0+ | Average bucket search latency (ns) |
| `bucket_search_stddev_ns` | int64_t | 0+ | Bucket search latency standard deviation |

**Notes:**
- All latencies converted from internal Q48.16 fixed-point to nanoseconds
- If `ENABLE_HOTWORDS_CACHE=0`, cache_hits and cache_hit_percent will be 0

### 2. Pipelining Metrics (3 columns)

| Column | Type | Range | Description |
|--------|------|-------|-------------|
| `context_predictions_total` | uint64_t | 0+ | Total prefetch predictions made |
| `context_correct` | uint64_t | 0+ | Successful predictions |
| `context_accuracy_percent` | double | 0-100 | Prediction accuracy (formula: 100 * correct / total) |

**Notes:**
- Only populated if `ENABLE_PIPELINING=1`
- Measures Loop #4 (word transition prediction) effectiveness
- Formula: `context_accuracy_percent = (context_correct / context_predictions_total) * 100`

### 3. Physics & Adaptive Tuning Metrics (8 columns)

| Column | Type | Range | Description |
|--------|------|-------|-------------|
| `rolling_window_width` | uint32_t | 256-4096 | Current effective rolling window size |
| `decay_slope` | double | 0-1.0 | Exponential decay slope (converted from Q48.16) |
| `hot_word_count` | uint64_t | 0+ | Words above heat threshold (>HOTWORDS_EXECUTION_HEAT_THRESHOLD) |
| `stale_word_ratio` | double | 0-1.0 | Ratio of stale words (execution_heat < 10) to total words |
| `avg_word_heat` | double | 0+ | Average execution_heat across all dictionary entries |
| `prefetch_accuracy_percent` | double | 0-100 | Speculative prefetch success rate |
| `prefetch_attempts` | uint64_t | 0+ | Total prefetch attempts made |
| `prefetch_hits` | uint64_t | 0+ | Successful prefetch hits |

**Notes:**
- `rolling_window_width` = running window size for pattern recognition (adaptive)
- `decay_slope` = Q48.16 → double conversion (slope = q48_value / 65536)
- `hot_word_count` + `stale_word_ratio` measure Heartbeat Loop #3 effectiveness (decay)
- `prefetch_*` metrics measure Loop #5 (window tuning) effectiveness

### 4. Window Tuning Metrics (2 columns)

| Column | Type | Range | Description |
|--------|------|-------|-------------|
| `window_tuning_checks` | uint64_t | 0+ | Number of adaptive window size adjustments |
| `final_effective_window_size` | uint32_t | 256+ | Final window size after all adjustments |

**Notes:**
- Indicates how many times the adaptive shrinking logic ran
- Shows result of adaptive tuning (window may have shrunk from initial size)

### 5. Performance Metrics (3 columns)

| Column | Type | Range | Description |
|--------|------|-------|-------------|
| `vm_workload_duration_ns_q48` | int64_t | 0+ | VM workload execution time (Q48.16 nanoseconds) |
| `cpu_temp_delta_c_q48` | int64_t | -100 to +100 | CPU temperature change (Q48.16 degrees Celsius) |
| `cpu_freq_delta_mhz_q48` | int64_t | -4000 to +4000 | CPU frequency change (Q48.16 MHz) |

**Notes:**
- All three values are in **Q48.16 fixed-point format**
- To convert: `double_value = q48_value / 65536.0`
- Temperature/frequency deltas indicate system thermal behavior during test
- Extracted from `/sys/class/thermal/` and `/sys/devices/system/cpu/cpufreq/`

### 6. Configuration/Tuning Knobs (5 columns)

| Column | Type | Range | Description |
|--------|------|-------|-------------|
| `decay_rate_q16` | uint32_t | 1+ | Execution heat decay rate (Q16 fixed-point) |
| `decay_min_interval_ns` | uint32_t | 1+ | Minimum decay interval (nanoseconds) |
| `rolling_window_size` | uint32_t | 256-8192 | Initial rolling window size (Makefile knob #7) |
| `adaptive_shrink_rate` | uint32_t | 50-95 | Shrinking percentage (Makefile knob #8) |
| `heat_cache_demotion_threshold` | uint32_t | 1-100 | Cache demotion threshold |

**Notes:**
- These are Makefile-controlled tuning knobs
- Captured to enable reproducibility analysis
- `decay_rate_q16` is in Q16 fixed-point (divide by 65536 for decimal)

### 7. Feature Flags (2 columns)

| Column | Type | Values | Description |
|--------|------|--------|-------------|
| `enable_hotwords_cache` | int | 0, 1 | Whether physics-driven cache enabled |
| `enable_pipelining` | int | 0, 1 | Whether word transition prediction enabled |

**Notes:**
- `0` = disabled, `1` = enabled
- Used to identify which optimization was active
- Crucial for factorial DoE analysis (A, B, C, AB combinations)

---

## CSV Example Row

```
2025-11-19T10:30:45,A_B_C_FULL__HB_ON,1,5420,2341,43.21,234,4.32,125,85,340,120,1200,1050,87.50,2048,0.0015,45,0.12,38.5,87.50,1200,1050,32,512,15000000000,0,-50,1,1000,4096,75,10,1,1
```

**Breakdown:**
- `timestamp`: 2025-11-19T10:30:45 (when run started)
- `configuration`: A_B_C_FULL__HB_ON (all optimizations + threaded heartbeat)
- `run_number`: 1 (first run of this config)
- `total_lookups`: 5420 (dictionary lookups)
- `cache_hits`: 2341 (43.21% hit rate)
- `bucket_hits`: 234 (4.32% after cache miss)
- ... (30+ more metrics)
- `enable_hotwords_cache`: 1 (yes)
- `enable_pipelining`: 1 (yes)

---

## R Analysis Template

### Load Data
```r
library(tidyverse)

doe_data <- read.csv("experiment_results.csv") %>%
  mutate(
    configuration = as.factor(configuration),
    timestamp = as.POSIXct(timestamp),
    # Convert Q48.16 values to decimal
    vm_workload_duration_ms = vm_workload_duration_ns_q48 / 65536 / 1e6,
    cpu_temp_delta_c = cpu_temp_delta_c_q48 / 65536,
    cpu_freq_delta_mhz = cpu_freq_delta_mhz_q48 / 65536,
    decay_rate = decay_rate_q16 / 65536
  )
```

### Exploratory Analysis
```r
# Summarize by configuration
doe_data %>%
  group_by(configuration) %>%
  summarize(
    n_runs = n(),
    mean_cache_hit_pct = mean(cache_hit_percent),
    sd_cache_hit_pct = sd(cache_hit_percent),
    mean_pipeline_accuracy = mean(context_accuracy_percent),
    sd_pipeline_accuracy = sd(context_accuracy_percent),
    mean_workload_ms = mean(vm_workload_duration_ms),
    sd_workload_ms = sd(vm_workload_duration_ms)
  )
```

### Factorial Analysis (2^2 DoE)
```r
# Model: metrics ~ enable_hotwords_cache * enable_pipelining
lm_cache <- lm(cache_hit_percent ~ as.factor(enable_hotwords_cache), data = doe_data)
lm_pipeline <- lm(context_accuracy_percent ~ as.factor(enable_pipelining), data = doe_data)
lm_interaction <- lm(vm_workload_duration_ms ~
                      as.factor(enable_hotwords_cache) *
                      as.factor(enable_pipelining),
                      data = doe_data)

summary(lm_interaction)  # Show main effects and interaction
```

### Visualization
```r
# Cache performance by configuration
ggplot(doe_data, aes(x = configuration, y = cache_hit_percent, fill = configuration)) +
  geom_boxplot() +
  facet_wrap(~enable_pipelining, labeller = labeller(enable_pipelining = c("0" = "Pipeline OFF", "1" = "Pipeline ON"))) +
  theme_minimal() +
  labs(title = "Cache Hit Rate by Configuration",
       x = "Configuration",
       y = "Cache Hit %")

# Performance comparison
ggplot(doe_data, aes(x = configuration, y = vm_workload_duration_ms, color = as.factor(enable_hotwords_cache))) +
  geom_jitter(width = 0.2, alpha = 0.6) +
  geom_boxplot(alpha = 0.1) +
  theme_minimal() +
  labs(title = "Workload Duration by Config",
       color = "Cache Enabled")
```

---

## Statistical Validation

### Before Analysis
1. **Check for missing values**: `sum(is.na(doe_data))`
2. **Verify factor balance**: Each config should have equal runs
3. **Check for outliers**: Use `boxplot()` on continuous metrics
4. **Normality test**: `shapiro.test()` on residuals for ANOVA

### Assumptions
- **Independence**: Runs are randomized (shell script does this)
- **Normality**: Metrics approximate normal distribution (CLT helps)
- **Homogeneity**: Variance similar across groups (Levene's test: `leveneTest()`)

### Power Analysis
- Minimum runs per config: 30 (per DoE design in shell script)
- Confidence level: 95% (standard)
- Effect size detectable: ~5% change in metrics

---

## Troubleshooting

### Metrics All Zeros or Constant
- **Cache metrics zero**: `ENABLE_HOTWORDS_CACHE` was 0 during build
- **Pipeline metrics zero**: `ENABLE_PIPELINING` was 0 during build
- **All zero**: VM didn't run properly (check run logs)

### CSV Parsing Issues in R
```r
# If timestamp won't parse:
doe_data$timestamp <- strptime(doe_data$timestamp, "%Y-%m-%dT%H:%M:%S")

# If too many columns:
# Check that CSV header matches metrics_write_csv_row() output
# Run: head -1 experiment_results.csv | tr ',' '\n' | nl
```

### Unexpected Variance
- Check heartbeat enabled/disabled (HB_ON vs HB_OFF in configuration)
- Verify CPU was not throttling (`cpu_freq_delta_mhz_q48` should be small)
- Check test matrix was properly randomized (`cat test_matrix.txt`)

---

## Future Enhancements

### 12 Math Optimizations (Planned)
As we implement stack batching, dictionary heat-aware lookup, tail-call optimization, etc., each will add new metrics columns:

| Optimization | New Metrics |
|---|---|
| Stack Operation Batching | `batch_operations_applied`, `batch_reduction_percent` |
| Heat-Aware Dictionary Lookup | `lookup_reorder_events`, `lookup_cache_line_hits` |
| Tail-Call Optimization | `tail_calls_detected`, `frame_overhead_saved_ns` |
| Decay Slope Adaptation | `per_word_decay_fitted`, `decay_convergence_iterations` |
| Control Flow Prediction | `branch_prediction_accuracy_percent` |
| Literal Inlining | `literals_inlined`, `bytes_saved_percent` |
| Loop Unrolling | `loops_unrolled`, `unroll_factor_applied` |
| Arithmetic Strength Reduction | `strength_reductions_applied` |
| Colon Definition Inlining | `definitions_inlined`, `inline_frame_overhead_saved` |
| Memory Access Prediction | `prefetch_predictions_made`, `prefetch_cache_line_hits` |
| Execution Heat Clustering | `cluster_count`, `cluster_coherence_score` |
| Adaptive Decay Rate | `decay_rate_adjustments_made`, `convergence_rate_ms` |

Each new metric will be:
- Added to `DoeMetrics` struct
- Populated in `metrics_from_vm()` or extraction helper
- Included in CSV header and `metrics_write_csv_row()`
- Documented in this file

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-19 | Initial schema: 35 metrics, 2^2 factorial DoE support |
| TBD | TBD | Add metrics for 12 math optimizations |

---

**Generated for:** StarshipOS / StarForth Physics-Driven VM
**Contact:** Robert A. James (Captain Bob)
**License:** See ../LICENSE
