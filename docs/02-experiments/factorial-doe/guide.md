# StarForth Complete 2^6 Factorial Design of Experiments

## Overview

**One-and-done comprehensive factorial testing** - collect all data systematically, analyze separately.

This guide describes how to run a **complete 2^6 factorial design of experiments** across all 6 feedback loops, yielding:
- **64 unique configurations** (all binary combinations)
- **1,920+ runs** (30 samples per configuration)
- **Single unified dataset** for factorial analysis

## The Why: Factorial DoE

Instead of incrementally tuning parameters, we collect **comprehensive experimental data** in one go:

### Traditional Approach (Old)
- Run baseline
- Run with Loop #1
- Run with Loops #1-2
- Run with Loops #1-3
- ... (sequential, incremental)
- **Problem:** Can't see interactions, trade-offs, or surprises

### Factorial Approach (New)
- Run ALL 2^6 = 64 combinations in randomized order
- Collect all data at once
- Analyze factorial effects, interactions, and main effects
- **Advantage:** Complete picture of how loops interact

## The 6 Feedback Loops

Each loop is a binary toggle in the Makefile:

| Loop | Makefile Flag | Purpose |
|------|---|---|
| **#1** | `ENABLE_LOOP_1_HEAT_TRACKING` | Count word execution frequency |
| **#2** | `ENABLE_LOOP_2_ROLLING_WINDOW` | Capture execution history (circular buffer) |
| **#3** | `ENABLE_LOOP_3_LINEAR_DECAY` | Age words by exponential decay over time |
| **#4** | `ENABLE_LOOP_4_PIPELINING_METRICS` | Track word-to-word transitions |
| **#5** | `ENABLE_LOOP_5_WINDOW_INFERENCE` | Infer optimal window width (Levene's test) |
| **#6** | `ENABLE_LOOP_6_DECAY_INFERENCE` | Infer decay slope via linear regression |

## Configuration Naming

Each of 64 configurations is named by its loop status:

```
L1_L2_L3_L4_L5_L6

Where each digit is 0 (OFF) or 1 (ON)

Examples:
  000000 = Baseline (all loops OFF - pure FORTH-79)
  000001 = Only heat tracking
  111110 = All except decay inference
  111111 = All loops ON (likely optimal)
  101010 = Alternating pattern (for research)
```

## Quick Start

### Minimal Test (validate script)
```bash
./scripts/run_factorial_doe.sh --runs-per-config 1 TEST_RUN
# 64 builds × 1 run = 64 total runs
# Runtime: ~30-45 minutes
# Output: /home/rajames/CLionProjects/StarForth-DoE/experiments/TEST_RUN/
```

### Standard Comprehensive Run
```bash
./scripts/run_factorial_doe.sh --runs-per-config 30 2025_11_19_FACTORIAL
# 64 builds × 30 runs = 1,920 total runs
# Runtime: ~2-4 hours
# Output: /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FACTORIAL/
```

### High-Precision Run
```bash
./scripts/run_factorial_doe.sh --runs-per-config 100 HIGH_PRECISION_DOE
# 64 builds × 100 runs = 6,400 total runs
# Runtime: ~6-12 hours
# Output: /home/rajames/CLionProjects/StarForth-DoE/experiments/HIGH_PRECISION_DOE/
```

## Output Structure

```
/home/rajames/CLionProjects/StarForth-DoE/experiments/<EXPERIMENT_LABEL>/
├── experiment_results.csv           # All measurements (1920+ rows)
├── experiment_summary.txt           # Timing, metadata, statistics
├── configuration_manifest.txt       # All 64 configs documented
├── test_matrix.txt                  # Randomized execution order
└── run_logs/
    ├── 000000_run_1.log
    ├── 000000_run_2.log
    ├── 000001_run_1.log
    ├── ...
    └── 111111_run_30.log            # 1,920 total log files
```

## CSV Schema

Each row in `experiment_results.csv` contains:

```
timestamp,configuration,run_number,
  <35+ metrics from test harness>,
  enable_loop_1_heat_tracking,
  enable_loop_2_rolling_window,
  enable_loop_3_linear_decay,
  enable_loop_4_pipelining_metrics,
  enable_loop_5_window_inference,
  enable_loop_6_decay_inference
```

This lets you correlate every metric against every loop combination.

## Analysis Strategy

After collection, analyze in the separate `StarForth-DoE` repository:

### Step 1: Load Data
```python
import pandas as pd
df = pd.read_csv("experiment_results.csv")
print(df.shape)  # Should be (1920, 38+)
print(df.columns)
```

### Step 2: Separate Metrics from Factors
```python
factor_cols = ['enable_loop_1_heat_tracking', ..., 'enable_loop_6_decay_inference']
metric_cols = ['total_lookups', 'cache_hit_percent', ..., 'vm_workload_duration_ns_q48']

factors = df[factor_cols]
metrics = df[metric_cols]
```

### Step 3: Factorial Analysis
```python
# Main effects: What's the average change when Loop X goes from 0→1?
main_effects = {}
for loop in factor_cols:
    main_effects[loop] = metrics[factors[loop]==1].mean() - metrics[factors[loop]==0].mean()

# Interactions: Do loops work better/worse together?
# Can use statsmodels.formula.api for ANOVA/regression

# Optimal subset: Which combinations of loops give best throughput?
best_configs = df.nlargest(10, 'vm_workload_duration_ns_q48')
```

## Key Design Choices

### 1. **Rebuild Every Pass** ✓
- Each configuration requires a `make clean && make`
- Ensures clean, isolated state
- No cross-contamination
- True ground truth for each variant

### 2. **Randomized Execution**
- All 1,920 runs randomized into single matrix (not 64 separate runs)
- Prevents thermal ramp, temporal bias, or systematic trends
- DoE principle: randomization eliminates confounds

### 3. **Full Factorial** (not fractional)
- Testing ALL 2^6 = 64 combinations
- Can detect unexpected interactions
- Most expensive, most informative

### 4. **One-and-Done Collection**
- Single experiment, all data
- No progressive tuning during collection
- Separates measurement from analysis (good practice)

## Expected Results

### Hypothetical Output
Assuming typical performance distribution:

```
Configuration | Avg Time (ns) | Speed vs Baseline
─────────────────────────────────────────────────
000000 (base) | 10,000,000    | 1.0x
000001 (L1)   | 9,950,000     | 1.005x
111111 (all)  | 8,500,000     | 1.176x  ← Likely best
```

### Expected Patterns
1. **Loop #1 (heat tracking)**: ~2-5% improvement alone
2. **Loop #2 (rolling window)**: ~1-3% improvement (depends on L1)
3. **Loops #3-6**: Increasingly synergistic (work better together)
4. **Interactions**: Loop #5 + #6 likely synergistic (inference loops)

## Troubleshooting

### Build Fails for a Configuration
```bash
# Check specific config build log
cat /home/rajames/CLionProjects/StarForth-DoE/experiments/<LABEL>/run_logs/build_<CONFIG>.log
```

### Test Harness Crashes on Some Configs
- Check if loop combination is invalid (e.g., Loop #5 without Loop #2)
- Verify Makefile flags are correctly parsed
- See: `include/vm.h` for interdependencies

### Results Look Suspicious
- Check `test_matrix.txt` - ensure randomization is present
- Verify CSV row counts match expected total
- Check for missing runs in log directory

## Performance Notes

### Timing Estimates
- **Minimal (1 run/config)**: 30-45 minutes
- **Standard (30 runs/config)**: 2-4 hours
- **High-precision (100 runs/config)**: 6-12 hours
- **Depends on:** CPU speed, disk I/O, test harness overhead

### Optimization
- Run overnight for high-precision experiments
- Use `screen` or `tmux` to keep session alive
- Monitor disk space (~100 MB for results + logs)

## Next Steps

1. **Run the experiment:**
   ```bash
   ./scripts/run_factorial_doe.sh 2025_11_19_COMPLETE_FACTORIAL
   ```

2. **Monitor progress:**
   ```bash
   tail -f /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_COMPLETE_FACTORIAL/experiment_summary.txt
   ```

3. **Move to analysis repo** (once complete):
   ```bash
   cp -r /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_COMPLETE_FACTORIAL \
         /home/rajames/CLionProjects/StarForth-DoE-Analysis/data/
   cd /home/rajames/CLionProjects/StarForth-DoE-Analysis
   python3 analyze_factorial.py data/2025_11_19_COMPLETE_FACTORIAL/experiment_results.csv
   ```

4. **Generate reports:**
   - Main effects plots
   - Interaction matrices
   - Pareto optimal configurations
   - Recommendations for production builds

---

**Version:** 1.0
**Last Updated:** 2025-11-19
**Author:** Claude Code (with Robert A. James)