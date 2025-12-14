# Experimental Iteration Runner - Usage Guide

## Overview

**One Experiment = (30 × iterations) × 4 builds = Total Runs**

The `run_experimental_iteration.sh` script conducts empirical testing across four build configurations:
1. **A_BASELINE** - No optimizations
2. **A_B_CACHE** - Static cache only
3. **A_C_FULL** - Pipelining only
4. **A_B_C_FULL** - Full cache + pipelining

All runs are randomized and aggregated into ONE unified dataset.

---

## Usage

### Interactive Mode (Manual Execution)

```bash
# Default (30 × 1 × 4 = 120 runs, ~12-15 hours)
./scripts/run_doe.sh ./my_results

# With explicit iterations (30 × 2 × 4 = 240 runs, ~24-30 hours)
./scripts/run_doe.sh --exp-iterations 2 ./my_results

# Push harder (30 × 3 × 4 = 360 runs, ~36-45 hours)
./scripts/run_doe.sh --exp-iterations 3 ./my_results

# Maximum data (30 × 4 × 4 = 480 runs, ~48-60 hours)
./scripts/run_doe.sh --exp-iterations 4 ./my_results
```

**Interactive prompts appear for:**
- Iteration description/purpose ("Why are we running this?")
- Tuning parameter changes from previous iteration
- Expected outcome hypothesis
- Previous iteration observations

**What happens:**
1. Prompts capture experimental context
2. Complete test matrix generated (all N × 4 runs randomized)
3. Test matrix preview shown (first 20 runs)
4. Script waits for your ENTER confirmation before executing
5. Runs execute in randomized order
6. Results saved to CSV with complete audit trail

---

### CI/CD Mode (Nightly Builds, Automated)

Set environment variables instead of prompting:

```bash
# Example: Nightly run with Isabelle/HOL proofs
export ITERATION_NOTES="Nightly baseline validation"
export TUNING_CHANGES="decay_rate_q16=1 (unchanged)"
export EXPECTED_OUTCOME="Baseline determinism: variance < 2%"
export PREVIOUS_OBSERVATIONS="Iteration 1 showed stable cache promotion"

./scripts/run_doe.sh --exp-iterations 2 ./nightly_results
```

**Behavior in CI/CD mode:**
- No interactive prompts (checks if ITERATION_NOTES is set)
- Skips "Press ENTER to begin" confirmation
- Test matrix generated and immediately executed
- All context captured in experiment_notes.txt

**Environment Variables:**
```bash
ITERATION_NOTES           # Description of iteration purpose
TUNING_CHANGES            # What parameters changed (or "none")
EXPECTED_OUTCOME          # Hypothesis for expected results_run_01_2025_12_08
PREVIOUS_OBSERVATIONS     # Lessons from last iteration
```

---

## Output Structure

```
OUTPUT_DIR/
├── experiment_results.csv      (N rows + header, 33 columns)
├── experiment_summary.txt      (metadata, runtime, parameters)
├── experiment_notes.txt        (audit trail + iteration context)
├── test_matrix.txt             (randomized execution order, N lines)
└── run_logs/
    ├── A_BASELINE_run_1.log
    ├── A_BASELINE_run_2.log
    ├── ... (30 × iterations A_BASELINE logs)
    ├── A_B_CACHE_run_1.log
    ├── ... (30 × iterations A_B_CACHE logs)
    ├── A_C_FULL_run_1.log
    ├── ... (30 × iterations A_C_FULL logs)
    ├── A_B_C_FULL_run_1.log
    └── ... (30 × iterations A_B_C_FULL logs)
```

---

## experiment_notes.txt Format

**Automatically captured:**

```
═══════════════════════════════════════════════════════════════════════════════
EXPERIMENTAL ITERATION NOTES
═══════════════════════════════════════════════════════════════════════════════

AUDIT TRAIL:
  Run by: rajames@hostname
  Timestamp: 2025-11-07T22:15:30
  Working directory: /home/rajames/CLionProjects/StarForth
  Experiment directory: /my/results/dir

EXPERIMENTAL PARAMETERS:
  --exp-iterations: 2
  Runs per configuration: 60
  Total runs (30 × 2 × 4): 240
  Benchmark iterations per run: 100,000
  Build profile: fastest

ITERATION CONTEXT:
  Description/Purpose:
    Second iteration - validate decay slope improvement

  Tuning Changes from Previous Iteration:
    decay_rate_q16: 1 → 2 (test faster decay)
    rolling_window_size: 4096 (unchanged)

  Expected Outcome Hypothesis:
    Expect lower variance in cache promotion (determinism improved)
    Expect measurable performance improvement in A_B_C_FULL
    Predict Theorem 1 validation: CV < 1.5%

  Previous Iteration Observations:
    Iteration 1 showed ~3% variance, suggests linear decay is too slow
    Cache hit percentages converged after ~40 runs per config
    No thermal throttling detected (temps 45-48°C throughout)

FOUR BUILD CONFIGURATIONS (always constant):
  1. A_BASELINE       (ENABLE_HOTWORDS_CACHE=0, ENABLE_PIPELINING=0)
  2. A_B_CACHE        (ENABLE_HOTWORDS_CACHE=1, ENABLE_PIPELINING=0)
  3. A_C_FULL         (ENABLE_HOTWORDS_CACHE=1, ENABLE_PIPELINING=0)
  4. A_B_C_FULL       (ENABLE_HOTWORDS_CACHE=1, ENABLE_PIPELINING=1)
```

---

## Iteration Workflow

**Example: Two-iteration study**

### Iteration 1: Baseline Observation
```bash
./scripts/run_doe.sh --exp-iterations 1 ./iter1_results
# Prompts: "First iteration - establish baseline"
# Output: 120 rows in CSV, experiment_notes.txt with observations
```

**After Iteration 1:**
- Analyze: `python3 scripts/analyze_physics_results.py iter1_results/experiment_results.csv`
- Observe: Where does it break? Variance metrics? Performance trends?
- Document: Tuning adjustments needed for Iteration 2

### Iteration 2: Refined Tuning
```bash
./scripts/run_doe.sh --exp-iterations 2 ./iter2_results
# Prompts: "Iteration 2 - test decay_rate improvements"
#          "decay_rate_q16: 1 → 2 (faster decay)"
#          "Expect determinism: CV < 1.5%"
#          "Iter 1 showed ~3% variance, suggests linear decay too slow"
# Output: 240 rows in CSV (aggregating iter 1 & 2), experiment_notes.txt
```

**The key:** Each iteration's experiment_notes.txt documents the scientific reasoning, creating an audit trail for the nightly build and future reference.

---

## CI/CD Integration Example (Jenkins)

```groovy
// Nightly physics experiment pipeline
stage('Physics Iteration 2') {
    steps {
        script {
            withEnv([
                'ITERATION_NOTES=Nightly iteration 2 - decay rate tuning',
                'TUNING_CHANGES=decay_rate_q16: 1 → 2 (faster linear decay)',
                'EXPECTED_OUTCOME=Determinism improved (CV < 1.5%)',
                'PREVIOUS_OBSERVATIONS=Iter 1 showed 3% variance'
            ]) {
                sh '''
                    ./scripts/run_experimental_iteration.sh --exp-iterations 2 ./nightly_physics_results
                '''
            }
        }
    }
    post {
        always {
            archiveArtifacts 'nightly_physics_results/**'
            publishHTML([
                reportDir: 'nightly_physics_results',
                reportFiles: 'experiment_notes.txt',
                reportName: 'Physics Experiment Notes'
            ])
        }
    }
}
```

---

## Analysis After Experiments

### Manual Statistical Analysis

```python
import pandas as pd
import numpy as np
from scipy import stats

# Load results_run_01_2025_12_08 from iteration
df = pd.read_csv('experiment_results.csv')

# Theorem 1: Determinism - low variance in cache hit %
baseline = df[df['configuration'] == 'A_BASELINE']['cache_hit_percent']
cv = baseline.std() / baseline.mean() * 100
print(f"Cache hit % CV: {cv:.2f}% (expect <2%)")

# Theorem 2: Performance - C_FULL faster than baseline
full = df[df['configuration'] == 'A_B_C_FULL']['total_runtime_ms']
baseline = df[df['configuration'] == 'A_BASELINE']['total_runtime_ms']
t_stat, p_val = stats.ttest_ind(full, baseline)
print(f"Full vs Baseline: t={t_stat:.3f}, p={p_val:.4f}")

# Theorem 3: Robustness - ANOVA
configs = df.groupby('configuration')['total_runtime_ms']
f_stat, p_val = stats.f_oneway(*[group.values for name, group in configs])
print(f"ANOVA: F={f_stat:.3f}, p={p_val:.4f}")

# Theorem 4: Predictability - tight CI
mu = baseline.mean()
ci = 1.96 * baseline.std() / np.sqrt(len(baseline))
rel_width = (2 * ci) / mu * 100
print(f"95% CI relative width: {rel_width:.1f}% (expect <10%)")
```

---

## Troubleshooting

### Script fails to build
- Check: `run_logs/build_A_BASELINE.log`
- Retry: `make clean && make fastest`

### Prompt not appearing in Jenkins
- Set env var: `export ITERATION_NOTES="..."`
- Script will skip prompts automatically

### Test matrix wrong
- Review: `run_logs/test_matrix.txt`
- Verify: All 4 configs present with correct counts
- Total lines should equal: `30 × iterations × 4`

### Results CSV has missing rows
- Check: `ls -l run_logs/ | wc -l` should match total runs
- Review: Last run logs for errors

---

## References

- FINAL_REPORT/03-methodology.adoc - Experimental design
- FINAL_REPORT/02-formal-theorems.adoc - Four theorems to validate
- scripts/analyze_physics_results.py - Analysis script
- experiment_notes.txt - Audit trail within each experiment

---

**Ready to run your first iteration!**

Start with `--exp-iterations 1` to establish baseline, then scale up based on observations.