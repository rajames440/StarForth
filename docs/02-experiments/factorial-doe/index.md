# StarForth Complete 2^6 Factorial DoE - Documentation Index

## Overview

This is a **one-and-done comprehensive factorial design of experiments** to systematically test all 64 possible combinations of 6 feedback loops.

**Key insight:** Don't toggle back-and-forth. Instead, systematically test ALL 2^6 combinations in a single experiment, then analyze the results afterward.

## Documentation Structure

### 1. **START HERE** 📋
- **File:** `docs/QUICK_START_FACTORIAL_DoE.md`
- **Purpose:** Entry point for quick reference
- **Read time:** 5 minutes
- **Contains:**
  - 3 command options (quick/standard/high-precision)
  - Time estimates
  - What to expect during execution
  - Basic troubleshooting

### 2. **Complete Guide** 📚
- **File:** `docs/FACTORIAL_DoE_GUIDE.md`
- **Purpose:** Full design documentation and rationale
- **Read time:** 20-30 minutes
- **Contains:**
  - Why factorial DoE (vs. incremental tuning)
  - 6 feedback loop definitions
  - Configuration naming scheme (0_0_0_0_0_0 → 1_1_1_1_1_1)
  - Design choices explained
  - Expected results and patterns
  - Troubleshooting guide
  - Performance notes

### 3. **Analysis Workflow** 🔍
- **File:** `docs/FACTORIAL_DoE_ANALYSIS_WORKFLOW.md`
- **Purpose:** End-to-end pipeline from collection to insights
- **Read time:** 15-20 minutes (or 1-2 hours if coding along)
- **Contains:**
  - Phase 1: Data collection (run script)
  - Phase 2: Data transfer (to analysis repo)
  - Phase 3: Statistical analysis (main effects, interactions)
  - Phase 4: Reporting (generating insights)
  - Python code examples:
    * Load data and validate
    * Compute main effects (individual loop impacts)
    * Detect interactions (loop synergies)
    * Optimize configuration subset
    * Visualization code
  - Example report structure
  - Checklist for completion

### 4. **Main Script** ⚙️
- **File:** `scripts/run_factorial_doe.sh`
- **Purpose:** Executable for running the entire experiment
- **Read time:** 10 minutes (code + comments)
- **Contains:**
  - Generates all 64 configurations
  - Rebuilds for each unique configuration
  - Randomizes execution order (1,920+ runs)
  - Collects metrics to CSV
  - Full instrumentation and progress reporting

---

## Reading Guide by Use Case

### "I just want to run the experiment"
1. Read: `QUICK_START_FACTORIAL_DoE.md` (5 min)
2. Execute: Choose command option (quick/standard/high-precision)
3. Wait: 30 min - 12 hours depending on option
4. Done!

### "I want to understand the design before running"
1. Read: `QUICK_START_FACTORIAL_DoE.md` (5 min)
2. Read: `FACTORIAL_DoE_GUIDE.md` (30 min)
3. Execute: Run with confidence
4. Analyze: Follow `FACTORIAL_DoE_ANALYSIS_WORKFLOW.md`

### "I need to analyze the results"
1. Read: `QUICK_START_FACTORIAL_DoE.md` → understand naming
2. Read: `FACTORIAL_DoE_ANALYSIS_WORKFLOW.md` → statistical methods
3. Code along: Use Python examples in Analysis Workflow
4. Interpret: Reference `FACTORIAL_DoE_GUIDE.md` for expected patterns

### "I want complete understanding"
Read in order:
1. `QUICK_START_FACTORIAL_DoE.md` (overview)
2. `FACTORIAL_DoE_GUIDE.md` (design rationale)
3. `scripts/run_factorial_doe.sh` (implementation)
4. `FACTORIAL_DoE_ANALYSIS_WORKFLOW.md` (analysis methods)

---

## Quick Command Reference

### Run Experiment
```bash
# Option A: Quick validation (30 min)
./scripts/run_factorial_doe.sh --runs-per-config 1 TEST_RUN

# Option B: Standard production (2-4 hours)
./scripts/run_factorial_doe.sh --runs-per-config 30 2025_11_19_FULL_FACTORIAL

# Option C: High-precision overnight (6-12 hours)
./scripts/run_factorial_doe.sh --runs-per-config 100 HIGH_PRECISION_FACTORIAL
```

### Monitor Progress
```bash
# Check results_run_01_2025_12_08 CSV growth
watch -n 5 'wc -l /path/to/experiment_results.csv'

# View summary
tail /path/to/experiment_summary.txt

# Count completed configurations
ls /path/to/run_logs/build_*.log | wc -l
```

### Analyze Results
```bash
# Load and analyze in Python
cd /path/to/StarForth-DoE-Analysis
python3 << 'EOF'
import pandas as pd
df = pd.read_csv('data/2025_11_19_FULL_FACTORIAL/experiment_results.csv')
print(f"Loaded {len(df)} runs")
# See FACTORIAL_DoE_ANALYSIS_WORKFLOW.md for full analysis code
EOF
```

---

## Key Concepts

### 64 Configurations
Each configuration is a unique combination of 6 binary factors:
- **0_0_0_0_0_0** = Baseline (all loops OFF)
- **1_0_0_0_0_0** = Only heat tracking
- **1_1_0_0_0_0** = Heat + rolling window
- ...
- **1_1_1_1_1_1** = All loops ON (current optimal)

### Randomized Execution
All 1,920+ runs (64 configs × 30 samples) are randomized into a single execution matrix.
- Eliminates temporal bias
- Prevents thermal ramp (if CPU heats during night)
- Proper DoE methodology

### One-and-Done Collection
Collect all data in single experiment, analyze separately.
- No tuning during collection (avoid bias)
- Single pass through parameter space
- Separates measurement from analysis

### Main Effects
Individual impact of each loop:
```
Loop #1: Loop turns ON, average metric changes by X%
Loop #2: Loop turns ON, average metric changes by Y%
... etc
```

### Interactions
How loops work together (amplify or suppress each other):
```
Loop #1 + Loop #2: Do they work better together than independently?
```

---

## Files at a Glance

| File | Size | Purpose |
|------|------|---------|
| `scripts/run_factorial_doe.sh` | 19 KB | Main executable script |
| `docs/QUICK_START_FACTORIAL_DoE.md` | 4.9 KB | Quick reference |
| `docs/FACTORIAL_DoE_GUIDE.md` | 8.0 KB | Design documentation |
| `docs/FACTORIAL_DoE_ANALYSIS_WORKFLOW.md` | 9.6 KB | Analysis pipeline |
| `docs/FACTORIAL_DoE_INDEX.md` | This file | Documentation index |

---

## Execution Timeline

### Phase 1: Preparation (5-10 min)
- Read `QUICK_START_FACTORIAL_DoE.md`
- Choose execution option
- Run command

### Phase 2: Collection (30 min - 12 hours)
- Script builds all 64 configs (as needed)
- Randomized execution of 1,920+ runs
- Metrics collected to CSV
- Progress shown every run

### Phase 3: Transfer (5 min)
- Copy results to analysis repo
- Verify file integrity

### Phase 4: Analysis (1-2 hours)
- Load CSV in Python
- Compute main effects
- Detect interactions
- Find optimal configurations
- Generate visualizations

### Phase 5: Reporting (1-2 hours)
- Summarize findings
- Create report
- Recommendations for production

**Total time:** Collection (2-4 hrs) + Analysis (2-4 hrs) = **4-8 hours**

---

## The Design Philosophy

### Problem Statement
How do we systematically understand the performance contribution and interactions of 6 interdependent feedback loops?

### Solution
**Complete 2^6 factorial design of experiments**

### Why This Approach
1. **Complete:** Tests all 64 combinations (no gaps)
2. **Systematic:** Proper experimental design methodology
3. **Separable:** Collection ≠ Analysis (reduces bias)
4. **Reproducible:** Randomized execution, full logging
5. **Scalable:** Single script, extensible to more factors

### Alternatives Rejected
- ❌ Incremental tuning ("baseline → +loop1 → +loop1+2"): Misses interactions
- ❌ Dynamic toggle without rebuild: Introduces state contamination
- ❌ Partial factorial: Misses key interactions
- ❌ One-at-a-time testing: Expensive and confounded

---

## Next Steps

1. **Read** `QUICK_START_FACTORIAL_DoE.md` (5 minutes)
2. **Decide** which execution option (quick/standard/precise)
3. **Run** the command and let it collect data
4. **Monitor** progress in another terminal
5. **Analyze** results using `FACTORIAL_DoE_ANALYSIS_WORKFLOW.md`
6. **Report** findings to stakeholders

---

## Questions?

- **"How long will this take?"** → See `QUICK_START_FACTORIAL_DoE.md`
- **"What does configuration X mean?"** → See `FACTORIAL_DoE_GUIDE.md`
- **"How do I analyze the results?"** → See `FACTORIAL_DoE_ANALYSIS_WORKFLOW.md`
- **"Why this approach?"** → See `FACTORIAL_DoE_GUIDE.md` - "The Why"
- **"How do I run it?"** → `./scripts/run_factorial_doe.sh`

---

**Version:** 1.0
**Created:** 2025-11-19
**Status:** Ready for production use