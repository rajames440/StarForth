# StarForth Optimization DoE Guide

**Date:** 2025-11-19
**Purpose:** Run progressive Design of Experiments (DoE) for each optimization opportunity in sequence

---

## Overview

The `run_optimization_doe.sh` script executes progressive experiments to test each optimization opportunity from `OPTIMIZATION_OPPORTUNITIES.md`. Each experiment:

- Tests 3-4 parameter variations
- Runs **minimal 2 iterations** (60 samples per configuration) for fast feedback
- All metrics in **Q48.16 fixed-point integer format** (no floating-point)
- Results inform next optimization opportunity

---

## Q48.16 Fixed-Point Format

All metrics use Q48.16 fixed-point representation:
- **48 bits:** Integer part
- **16 bits:** Fractional part (65536ths)
- **Why:** Deterministic, no floating-point errors, verifiable via formal methods

### Common Conversions

```
Decimal Value          Q48.16 Integer      How to Convert
──────────────────────────────────────────────────────────
0.2 (decay slope)      13107.2             0.2 × 65536 = 13107.2 → use 13107
0.33 (decay slope)     21626.88            0.33 × 65536 = 21626.88 → use 21627
0.5 (decay slope)      32768               0.5 × 65536 = 32768
0.7 (decay slope)      45875.2             0.7 × 65536 = 45875.2 → use 45875

To read from CSV:
  vm_workload_duration_ns_q48 = 315797667840
  Divide by 65536:  315797667840 / 65536 = 4,822,021 nanoseconds (decimal)
```

---

## Running Opportunity Experiments

### Quick Start

```bash
cd /home/rajames/CLionProjects/StarForth

# Opportunity #1: Decay Slope Inference (4 configs, 2 iterations = 240 total runs)
./scripts/run_optimization_doe.sh --opportunity 1 OPP_01_DECAY_SLOPE

# Opportunity #2: Window Width Tuning (3 configs, 2 iterations = 180 total runs)
./scripts/run_optimization_doe.sh --opportunity 2 OPP_02_WINDOW_WIDTH

# And so on...
./scripts/run_optimization_doe.sh --opportunity 3 OPP_03_DECAY_RATE
./scripts/run_optimization_doe.sh --opportunity 4 OPP_04_WINDOW_SIZING
./scripts/run_optimization_doe.sh --opportunity 5 OPP_05_THRESHOLD
```

### Command-line Options

```bash
./scripts/run_optimization_doe.sh --opportunity N [--exp-iterations M] LABEL

--opportunity N         Which optimization opportunity (1-5)
--exp-iterations M      Number of iterations (default: 2, meaning 60 samples per config)
LABEL                   Experiment label (e.g., OPP_01_BASELINE)
```

### Example: Run Opportunity #1 with 3 Iterations

```bash
./scripts/run_optimization_doe.sh --opportunity 1 --exp-iterations 3 OPP_01_EXTENDED
```

This will:
- Test 4 decay slope values (0.2, 0.33, 0.5, 0.7)
- Run 3 iterations × 30 samples = 90 samples per config
- Total: 360 runs
- Estimated time: ~2 minutes

---

## Opportunity Details

### Opportunity #1: Decay Slope Inference

**What it tests:**
- 4 decay slope values: 0.2, 0.33 (baseline), 0.5, 0.7
- Decay slope = how fast execution heat fades from cache

**Expected impact:** 8-15% performance improvement

**Configurations tested:**
```
1. DECAY_SLOPE_0.2    (decay_slope_q48=13107)   - Fast decay
2. DECAY_SLOPE_0.33   (decay_slope_q48=21627)   - Baseline
3. DECAY_SLOPE_0.5    (decay_slope_q48=32768)   - Medium decay
4. DECAY_SLOPE_0.7    (decay_slope_q48=45875)   - Slow decay
```

**Metrics to examine:**
- `cache_hit_percent`: Should vary based on which words stay hot
- `hot_word_count`: Should vary with decay strategy
- `vm_workload_duration_ns_q48`: Lower is better (faster execution)

**Decision rule:**
- Pick decay slope with highest `cache_hit_percent` and lowest workload duration
- Lock this value for Opportunity #2

---

### Opportunity #2: Variance-Based Window Width Tuning

**What it tests:**
- 3 rolling window sizes: 2048, 4096 (baseline), 8192
- Window size = how many executions we track for adaptive decisions

**Expected impact:** 6-12% performance improvement

**Configurations tested:**
```
1. WINDOW_SIZE_2048   - Small window (less history)
2. WINDOW_SIZE_4096   - Baseline window
3. WINDOW_SIZE_8192   - Large window (more history)
```

**Metrics to examine:**
- `rolling_window_width`: Should match configured value
- `context_accuracy_percent`: Should vary (larger window = better patterns)
- `vm_workload_duration_ns_q48`: Lower is better

**Decision rule:**
- Pick window size with best balance of accuracy and performance
- Lock this value for Opportunity #4

---

### Opportunity #3: Decay Rate Parameter Tuning

**What it tests:**
- Decay interval (ns) and adaptive shrink rate combinations
- How aggressively do we age hot-words vs. shrink the window?

**Expected impact:** 3-6% performance improvement

**Configurations tested:**
```
1. DECAY_FAST_SHRINK_FAST       (interval=500ns, shrink=50)
2. DECAY_NORMAL_SHRINK_NORMAL   (interval=1000ns, shrink=75) - Baseline
3. DECAY_SLOW_SHRINK_SLOW       (interval=2000ns, shrink=100)
```

**Metrics to examine:**
- `bucket_hit_percent`: Should vary based on decay aggressiveness
- `final_effective_window_size`: Should vary based on shrink rate
- Cache efficiency metrics

---

### Opportunity #4: Rolling Window Sizing Experiment

**What it tests:**
- 2×2 factorial: window size (2048, 8192) × decay slope (0.33, 0.5)
- Reveals interaction effects between parameters

**Expected impact:** 5-8% performance improvement

**Configurations tested:**
```
1. WINDOW_2K_DECAY_0.33   - Small window, normal decay
2. WINDOW_2K_DECAY_0.5    - Small window, slower decay
3. WINDOW_8K_DECAY_0.33   - Large window, normal decay
4. WINDOW_8K_DECAY_0.5    - Large window, slower decay
```

**Analysis approach:**
- Look for main effects (window size matters? decay slope matters?)
- Look for interactions (do they reinforce or compete?)

---

### Opportunity #5: Hotwords Cache Threshold Optimization

**What it tests:**
- 4 cache promotion thresholds: 5, 10 (baseline), 20, 50
- When does a word get hot enough to promote to cache?

**Expected impact:** 2-4% performance improvement

**Configurations tested:**
```
1. THRESHOLD_AGGRESSIVE_5       - Low threshold (many words cached)
2. THRESHOLD_BASELINE_10        - Default threshold
3. THRESHOLD_CONSERVATIVE_20    - High threshold (few words cached)
4. THRESHOLD_VERY_CONSERVATIVE_50
```

**Metrics to examine:**
- `cache_hits` and `cache_hit_percent`: How many lookups hit cache
- `hot_word_count`: How many words are actually hot
- Memory pressure (if cache is too aggressive, thrashing may occur)

---

## Reading the Results

### CSV Output Structure

Each row contains:

```
timestamp,configuration,run_number,
total_lookups,cache_hits,cache_hit_percent,bucket_hits,bucket_hit_percent,
cache_hit_latency_ns,cache_hit_stddev_ns,bucket_search_latency_ns,bucket_search_stddev_ns,
context_predictions_total,context_correct,context_accuracy_percent,
rolling_window_width,decay_slope,hot_word_count,stale_word_ratio,avg_word_heat,
prefetch_accuracy_percent,prefetch_attempts,prefetch_hits,window_tuning_checks,final_effective_window_size,
vm_workload_duration_ns_q48,cpu_temp_delta_c_q48,cpu_freq_delta_mhz_q48,
decay_rate_q16,decay_min_interval_ns,rolling_window_size,adaptive_shrink_rate,heat_cache_demotion_threshold,
enable_hotwords_cache,enable_pipelining
```

### Key Metrics (Q48.16 format)

| Metric | Q48.16 Format | Interpretation |
|--------|---------------|-----------------|
| `vm_workload_duration_ns_q48` | Integer (÷65536) | Workload execution time in ns (lower is better) |
| `cpu_temp_delta_c_q48` | Integer (÷65536) | CPU temperature change in °C |
| `cpu_freq_delta_mhz_q48` | Integer (÷65536) | CPU frequency change in MHz |
| `decay_rate_q16` | Integer (÷65536) | Actual decay rate used (Q16 format) |

### Quick Analysis in R

```bash
# Load and analyze results_run_01_2025_12_08
Rscript /home/rajames/CLionProjects/StarForth-DoE/R/analysis/01_load_and_explore.R \
    /home/rajames/CLionProjects/StarForth-DoE/experiments/OPP_01_DECAY_SLOPE/experiment_results.csv
```

This will:
- Load the CSV
- Convert Q48.16 metrics to decimals
- Show summary statistics by configuration
- Identify which configuration performs best

---

## Decision Framework

### For Each Opportunity:

1. **Run the experiment** (2 iterations, ~2-10 minutes depending on configs)
2. **Analyze results:**
   ```bash
   # View top performers
   tail -n +2 experiment_results.csv | sort -t',' -k27 -n | head -10
   # This sorts by vm_workload_duration_ns_q48 (column 27) in ascending order
   ```
3. **Choose winner:**
   - Highest `cache_hit_percent` OR
   - Lowest `vm_workload_duration_ns_q48` OR
   - Best balance based on context
4. **Lock the value** for next experiment
5. **Move to next opportunity**

### When to Repeat an Opportunity

Repeat if:
- Results are inconclusive (all configs perform similarly)
- Need more statistical power (run with `--exp-iterations 3` or `4`)
- Want to test refined parameter ranges around the winner

### When to Skip an Opportunity

Skip if:
- Results show no meaningful improvement over baseline
- Implementation effort outweighs benefit
- Next opportunity has higher ROI

---

## Expected Runtime

| Opportunity | Configs | Total Runs (iter=2) | Time |
|-------------|---------|-------------------|------|
| #1 (Decay Slope) | 4 | 240 | ~2 min |
| #2 (Window Width) | 3 | 180 | ~1.5 min |
| #3 (Decay Rate) | 3 | 180 | ~1.5 min |
| #4 (Window Sizing) | 4 | 240 | ~2 min |
| #5 (Threshold) | 4 | 240 | ~2 min |

**Total for all 5:** ~9 minutes

---

## Troubleshooting

### Experiment hangs at "Press ENTER..."
- Press ENTER to confirm and start execution
- Or Ctrl+C to abort

### CSV has no data rows
- Check that the binary compiled correctly: `make test`
- Check run logs: `tail -20 run_logs/*.log`
- Verify `--doe-experiment` flag is working

### All configurations perform identically
- Test workload (936 tests) is deterministic → same patterns every run
- Try higher iteration count: `--exp-iterations 4` for more precision
- Or move to next opportunity (diminishing returns)

### Metrics look like garbage (huge numbers)
- These are likely Q48.16 integers, not broken data
- Divide by 65536 to see decimal values
- E.g., 315797667840 ÷ 65536 = 4,822,021 nanoseconds

---

## Next Steps

### Sequencing

```
Day 1:
  Run OPP_01 (Decay Slope)        → Pick winner → Lock value
  Run OPP_02 (Window Width)       → Pick winner → Lock value

Day 2:
  Run OPP_03 (Decay Rate)         → Pick winner → Lock value
  Run OPP_04 (Window Sizing)      → Analyze interaction

Day 3:
  Run OPP_05 (Threshold)          → Pick winner
  Aggregate results → Overall tuning strategy
```

### After All Opportunities

- **Create tuning baseline:** Lock all 5 winners into Makefile defaults
- **Validate:** Run TST_BASELINE_OPTIMIZED to confirm improvements
- **Compare:** Against original TST_03 (pre-optimization)
- **Document:** Expected performance gain (could be 15-30% cumulative)

---

## References

- `OPTIMIZATION_OPPORTUNITIES.md` - Detailed opportunity analysis
- `docs/src/physics_runtime/` - Physics runtime documentation
- `docs/` - Additional architecture and tuning guides
