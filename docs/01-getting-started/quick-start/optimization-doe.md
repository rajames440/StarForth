# Quick Start: Optimization DoE

## TL;DR - Run These Commands

```bash
cd /home/rajames/CLionProjects/StarForth

# Opportunity #1: Decay Slope (4 configs, ~2 min)
echo "" | ./scripts/run_optimization_doe.sh --opportunity 1 OPP_01_DECAY_SLOPE

# Opportunity #2: Window Width (3 configs, ~1.5 min)
echo "" | ./scripts/run_optimization_doe.sh --opportunity 2 OPP_02_WINDOW_WIDTH

# Opportunity #3: Decay Rate (3 configs, ~1.5 min)
echo "" | ./scripts/run_optimization_doe.sh --opportunity 3 OPP_03_DECAY_RATE

# Opportunity #4: Window Sizing (4 configs, ~2 min)
echo "" | ./scripts/run_optimization_doe.sh --opportunity 4 OPP_04_WINDOW_SIZING

# Opportunity #5: Threshold (4 configs, ~2 min)
echo "" | ./scripts/run_optimization_doe.sh --opportunity 5 OPP_05_THRESHOLD
```

Total time: ~9 minutes for all 5 experiments

---

## What's Happening

Each script:
1. ✅ Tests 3-4 parameter variations
2. ✅ Runs 2 iterations × 30 samples per config (60 samples total)
3. ✅ Outputs metrics in Q48.16 fixed-point (integer math)
4. ✅ Generates CSV results in `/home/rajames/CLionProjects/StarForth-DoE/experiments/OPP_XX_*/`

---

## After Each Run

### View Results
```bash
# Last 10 rows of results_run_01_2025_12_08
tail -10 /home/rajames/CLionProjects/StarForth-DoE/experiments/OPP_01_DECAY_SLOPE/experiment_results.csv

# Sort by workload duration (column 27) - lower is better
tail -n +2 /home/rajames/CLionProjects/StarForth-DoE/experiments/OPP_01_DECAY_SLOPE/experiment_results.csv | \
  sort -t',' -k27 -n | head -5
```

### Analyze with R
```bash
Rscript /home/rajames/CLionProjects/StarForth-DoE/R/analysis/01_load_and_explore.R \
    /home/rajames/CLionProjects/StarForth-DoE/experiments/OPP_01_DECAY_SLOPE/experiment_results.csv
```

### Pick the Winner

For each opportunity, look for the configuration with:
- **Highest cache_hit_percent** AND
- **Lowest vm_workload_duration_ns_q48**

Example:
```
OPP_01: DECAY_SLOPE_0.33 wins (baseline was right, or try next if 0.5 better)
OPP_02: WINDOW_SIZE_4096 wins (baseline holds)
(etc.)
```

---

## Q48.16 Quick Reference

All metrics are **Q48.16 fixed-point integers** (48-bit integer, 16-bit fraction).

**To convert to decimal:** Divide by 65536

Examples from actual runs:
```
vm_workload_duration_ns_q48 = 315797667840
Decimal ns = 315797667840 / 65536 = 4,822,021 nanoseconds

cpu_freq_delta_mhz_q48 = -6356992
Decimal MHz = -6356992 / 65536 = -97.07 MHz
```

---

## If Something Goes Wrong

### Experiment Hangs at "Press ENTER..."
```bash
# Just press ENTER or use echo:
echo "" | ./scripts/run_optimization_doe.sh --opportunity 1 OPP_01
```

### Binary Not Found
```bash
cd /home/rajames/CLionProjects/StarForth
make test  # Rebuild if needed
```

### No CSV Data
```bash
# Check logs
tail -50 /home/rajames/CLionProjects/StarForth-DoE/experiments/OPP_01_DECAY_SLOPE/run_logs/*.log
```

---

## Sequencing Recommendation

Run them in order #1→#5, waiting a few minutes between each to review results:

```
OPP #1 (Decay Slope)     → Pick winner → Lock value ✓
OPP #2 (Window Width)    → Pick winner → Lock value ✓
OPP #3 (Decay Rate)      → Pick winner → Lock value ✓
OPP #4 (Window Sizing)   → Analyze interactions ✓
OPP #5 (Threshold)       → Final optimization ✓
```

Then aggregate findings into optimized baseline.

---

## Expected Results

Based on OPTIMIZATION_OPPORTUNITIES.md:

| Opp | Impact | Expected Winner |
|-----|--------|-----------------|
| #1 | 8-15% | DECAY_SLOPE_0.33 or 0.5 |
| #2 | 6-12% | WINDOW_SIZE_4096 or 8192 |
| #3 | 3-6% | Likely NORMAL (baseline) |
| #4 | 5-8% | Likely WINDOW_8K_DECAY_0.33 |
| #5 | 2-4% | Likely THRESHOLD_10 (baseline) |

---

## Full Documentation

For detailed information:
- See `OPTIMIZATION_DoE_GUIDE.md` for complete guide
- See `OPTIMIZATION_OPPORTUNITIES.md` for analysis of each opportunity
