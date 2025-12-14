# Quick Start: Complete 2^6 Factorial DoE

## TL;DR

Run a **complete, exhaustive 2^6 = 64 configuration experiment** in one go, collecting all data for later analysis.

## Three Command Options

### Option 1: Quick Validation (30 min)
Test the script and infrastructure:
```bash
./scripts/run_factorial_doe.sh --runs-per-config 1 QUICK_TEST
```
- 64 builds × 1 run = 64 total runs
- Fast feedback on whether setup works
- Output: `/home/rajames/CLionProjects/StarForth-DoE/experiments/QUICK_TEST/`

### Option 2: Standard Full DoE (2-4 hours)
Comprehensive factorial collection:
```bash
./scripts/run_factorial_doe.sh --runs-per-config 30 2025_11_19_FULL_FACTORIAL
```
- 64 builds × 30 runs = 1,920 total runs
- Good statistical power for main effects
- Standard production-grade experiment
- Output: `/home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL/`

### Option 3: High-Precision Run (6-12 hours)
Maximum data for deep analysis:
```bash
./scripts/run_factorial_doe.sh --runs-per-config 100 HIGH_PRECISION_FACTORIAL
```
- 64 builds × 100 runs = 6,400 total runs
- Best for detecting subtle interactions
- Run overnight for best results
- Output: `/home/rajames/CLionProjects/StarForth-DoE/experiments/HIGH_PRECISION_FACTORIAL/`

## What Gets Generated

```
/home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL/
├── experiment_results.csv              ← All 1,920 rows of measurements
├── experiment_summary.txt              ← Timing and metadata
├── configuration_manifest.txt          ← All 64 configs listed
├── test_matrix.txt                     ← Randomized execution order
└── run_logs/
    ├── 0_0_0_0_0_0_run_1.log
    ├── 0_0_0_0_0_0_run_2.log
    ├── ...
    └── 1_1_1_1_1_1_run_30.log
```

## What's Being Tested

6 **independent binary feedback loops**:

| Loop | Toggle | What It Does |
|------|--------|---|
| 1 | `ENABLE_LOOP_1_HEAT_TRACKING` | Count execution frequency |
| 2 | `ENABLE_LOOP_2_ROLLING_WINDOW` | Capture execution history |
| 3 | `ENABLE_LOOP_3_LINEAR_DECAY` | Age words over time |
| 4 | `ENABLE_LOOP_4_PIPELINING_METRICS` | Track word transitions |
| 5 | `ENABLE_LOOP_5_WINDOW_INFERENCE` | Infer window width |
| 6 | `ENABLE_LOOP_6_DECAY_INFERENCE` | Infer decay slope |

**All 2^6 = 64 combinations** tested:
- `0_0_0_0_0_0` = Baseline (all OFF)
- `1_0_0_0_0_0` = Only heat tracking
- `0_1_0_0_0_0` = Only rolling window
- ...
- `1_1_1_1_1_1` = All loops ON (current optimal)

## During Execution

You'll see progress like:
```
═══════════════════════════════════════════════════════════
STARFORTH COMPLETE 2^6 FACTORIAL DoE
═══════════════════════════════════════════════════════════
→ Factors: 6 feedback loops (LOOP_1 through LOOP_6)
→ Configurations: 2^6 = 64
→ Runs per config: 30
→ TOTAL RUNS: 1,920
→ Output: /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL

>>> Generating randomized test matrix...
✓ Test matrix generated

→ Run 1/1920 - config 0_1_0_1_1_0 #14...
✓ Run 1/1920 completed (3s)
→ Run 2/1920 - config 1_1_1_0_0_1 #2...
✓ Run 2/1920 completed (2s)
...
```

The script:
1. **Builds** each of 64 configurations (rebuilds when config changes)
2. **Runs** tests in randomized order (no systematic bias)
3. **Collects** metrics to CSV
4. **Outputs** progress every run

## After Completion

Move to analysis repo to understand results:
```bash
cd /home/rajames/CLionProjects/StarForth-DoE-Analysis
python3 analyze_factorial.py \
  data/2025_11_19_FULL_FACTORIAL/experiment_results.csv
```

Expected analysis outputs:
- **Main effects:** Which individual loops help most?
- **Interactions:** Do loops work better/worse together?
- **Optimal subset:** Best combination of loops?
- **Trade-offs:** Performance vs complexity?

## Key Design Decisions

✅ **Rebuild every pass** - ensures clean, isolated testing
✅ **Randomized execution** - eliminates temporal/thermal bias
✅ **Complete factorial** - captures all interactions
✅ **One-and-done** - collect all data, analyze later

## Estimated Time

| Runs/Config | Total | Time | When |
|---|---|---|---|
| 1 | 64 | 30-45 min | Quick test |
| 30 | 1,920 | 2-4 hours | Standard |
| 100 | 6,400 | 6-12 hours | Overnight |

Times depend on CPU, disk speed, and test harness overhead.

## Example Command

**Run it now:**
```bash
cd /home/rajames/CLionProjects/StarForth
./scripts/run_factorial_doe.sh 2025_11_19_FULL_FACTORIAL
```

**Monitor progress:**
```bash
tail -f /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL/experiment_summary.txt
```

---

**Ready?** Run the command above and let the experiment collect comprehensive data for downstream analysis.