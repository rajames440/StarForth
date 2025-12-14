# StarForth Heartbeat DoE Experiment – Execution Guide

**Date:** November 20, 2025
**Purpose:** Execute Phase 2 factorial DoE with heartbeat stability metrics
**Duration:** ~2-4 hours (250 runs = 5 configs × 50 runs each)

---

## Quick Start

```bash
# Navigate to repo root
cd /home/rajames/CLionProjects/StarForth

# Run the experiment (interactive prompt before execution)
./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5

# Optional: Use custom run count
./scripts/run_factorial_doe_with_heartbeat.sh --runs-per-config 100 HEARTBEAT_EXTENDED

# Analyze results_run_01_2025_12_08 (when complete)
Rscript scripts/analyze_heartbeat_stability.R

# Or with explicit path:
Rscript scripts/analyze_heartbeat_stability.R ~/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5/experiment_results_heartbeat.csv
```

---

## What This Experiment Does

### Phase 2: Focus on Heartbeat Stability

Building on Stage 1 (3200-run baseline without heartbeat), this experiment:

1. **Focuses on 5 elite configurations** (not all 64)
2. **Re-enables heartbeat thread** (HEARTBEAT_THREAD_ENABLED=1)
3. **Captures per-tick dynamics:**
   - Heartbeat interval timing (jitter)
   - Cache hit stability (rolling window)
   - Decay slope convergence
   - Load-response coupling
4. **Measures stability characteristics:**
   - Coefficient of variation in tick interval
   - Outlier ratio (3σ threshold)
   - Convergence rate to steady state
   - Correlation between workload and heartbeat frequency

### The 5 Elite Configurations

| # | Config | Description | Key Strengths |
|---|--------|-------------|---|
| 1 | `1_0_1_1_1_0` | Heat + Decay + Pipelining + Window Inference | Minimal overhead, strong performance |
| 2 | `1_0_1_1_1_1` | Config #1 + Decay Inference | Full inference pipeline |
| 3 | `1_1_0_1_1_1` | Heat + Window + Pipelining + Both Inferences | Alternative path (skip decay loop) |
| 4 | `1_0_1_0_1_0` | Heat + Decay + Window Inference only | Lean configuration |
| 5 | `0_1_1_0_1_1` | Window + Decay + Both Inferences | Contrast: no heat tracking |

**Factor codes:**
- L1 = ENABLE_LOOP_1_HEAT_TRACKING
- L2 = ENABLE_LOOP_2_ROLLING_WINDOW
- L3 = ENABLE_LOOP_3_LINEAR_DECAY
- L4 = ENABLE_LOOP_4_PIPELINING_METRICS
- L5 = ENABLE_LOOP_5_WINDOW_INFERENCE
- L6 = ENABLE_LOOP_6_DECAY_INFERENCE

---

## Pre-Execution Checklist

### 1. Build System Ready

```bash
# Verify build system works
cd /home/rajames/CLionProjects/StarForth
make clean
make fastest

# Should complete without errors
./build/amd64/fastest/starforth -c "1 2 + . BYE"
```

If build fails, troubleshoot:
- Check compiler: `gcc --version`
- Verify dependencies: `make check-deps`
- Clean and retry: `make clean && make fastest`

### 2. Storage Space

The experiment will generate:
- **CSV file:** ~50 MB (250 runs × 60+ columns)
- **Run logs:** ~500 MB (250 separate run logs)
- **Total:** ~600 MB required

Verify space:
```bash
mkdir -p /home/rajames/CLionProjects/StarForth-DoE/experiments
df -h /home/rajames/CLionProjects/StarForth-DoE/
# Should show at least 1 GB available
```

### 3. System Stability

The experiment will run continuously for 2-4 hours. Minimize background load:

```bash
# Check current load
uptime

# Close unnecessary applications
# - Disable automatic updates
# - Close heavy browser tabs
# - Consider running on a dedicated machine

# Optional: Run with reduced CPU frequency for more stable heartbeat
# (Requires sudo; skip if not needed)
# sudo cpupower frequency-set --governor powersave
```

### 4. Heartbeat Thread Configuration

Verify heartbeat is compiled in:

```bash
make clean
make fastest HEARTBEAT_THREAD_ENABLED=1

# Check binary
nm ./build/amd64/fastest/starforth | grep -i heartbeat
# Should show heartbeat symbols
```

---

## Execution Steps

### Step 1: Launch the Experiment

```bash
cd /home/rajames/CLionProjects/StarForth

./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
```

**Output:**
```
═══════════════════════════════════════════════════════════
STARFORTH HEARTBEAT DoE - TOP 5 ELITE CONFIGURATIONS
═══════════════════════════════════════════════════════════
→ Factors: 6 feedback loops (LOOP_1 through LOOP_6)
→ Configurations: 5 elite (selected from Stage 1 baseline)
→ Runs per config: 50
→ TOTAL RUNS: 250
→ Heartbeat metrics: YES (per-tick jitter, convergence, coupling)
→ Workload: Complete test harness (936+ FORTH tests)
→ Output: /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5

>>> Generating randomized test matrix...
✓ Test matrix generated: .../test_matrix.txt

Test Matrix Preview (first 10 of 250 runs):
  1_0_1_0_1_0,47
  0_1_1_0_1_1,13
  1_1_0_1_1_1,35
  ...

→ Complete test matrix saved: .../test_matrix.txt
Press ENTER to begin execution (Ctrl+C to abort):
```

**At this point:**
- Review the configuration manifest
- Verify test matrix looks reasonable (random order)
- Press ENTER to start

### Step 2: Monitor Execution

The script will print progress:

```
>>> RANDOMIZED HEARTBEAT EXPERIMENT (250 runs)
→ Run 1/250 - config 1_0_1_0_1_0 #47...
  [Building configuration...]
✓ Build successful for 1_0_1_0_1_0
✓ Run 1/250 completed (12s)

→ Run 2/250 - config 0_1_1_0_1_1 #13...
✓ Run 2/250 completed (11s)
...
```

**Expected timing:**
- ~10-15 seconds per run (test harness + heartbeat collection)
- ~250-375 minutes total (4-6 hours)
- Builds are cached, only rebuild when config changes

**What to monitor:**
- Are builds succeeding? (OK status)
- Are runs completing without crashes? (Log lines appearing)
- Elapsed time tracking (should be ~12s per run on average)

### Step 3: Completion

When finished:

```
═══════════════════════════════════════════════════════════
EXPERIMENT COMPLETE
═══════════════════════════════════════════════════════════
✓ All runs completed!
✓ Start time: 2025-11-20T14:30:45
✓ End time: 2025-11-20T18:15:20
✓ Total runtime: 3h 44m 35s
✓ Results: .../experiment_results_heartbeat.csv
✓ Configs: .../configuration_manifest.txt
✓ Logs: .../run_logs/

CSV Results Preview (first 5 data rows):
  [CSV data shown]

→ Next steps:
  1. Download results to analysis repository
  2. Run R analysis: Rscript analyze_heartbeat_stability.R
  3. Identify golden configuration by stability score
  4. Use golden config as baseline for MamaForth/PapaForth
```

---

## Post-Execution Analysis

### Step 1: Verify CSV Output

```bash
cd /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5

# Check file
ls -lh experiment_results_heartbeat.csv

# Verify header
head -1 experiment_results_heartbeat.csv

# Count rows (should be 251 = header + 250 data rows)
wc -l experiment_results_heartbeat.csv
```

Expected header includes:
```
timestamp,configuration,run_number,...,total_heartbeat_ticks,tick_interval_mean_ns,tick_interval_stddev_ns,tick_interval_cv,...,overall_stability_score
```

### Step 2: Run R Analysis

```bash
cd /home/rajames/CLionProjects/StarForth

# Run analysis with explicit path
Rscript scripts/analyze_heartbeat_stability.R \
  /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5/experiment_results_heartbeat.csv

# Or copy CSV to local directory and run with default
cp /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5/experiment_results_heartbeat.csv .
Rscript scripts/analyze_heartbeat_stability.R
```

**Expected output:**
```
Loading data from: .../experiment_results_heartbeat.csv

=== DATA SUMMARY ===
Total runs: 250
Configurations: 5
Runs per config: 50
Columns: 60

=== STABILITY RANKINGS ===
configuration mean_jitter_cv mean_convergence_rate ... rank_stability
1_0_1_1_1_0      0.0842         123.45         ...           1
1_0_1_1_1_1      0.0891         118.23         ...           2
1_1_0_1_1_1      0.1024          95.67         ...           3
...

[Visualizations generated]

═══════════════════════════════════════════════════════════
GOLDEN CONFIGURATION RECOMMENDATION
═══════════════════════════════════════════════════════════

✓ SELECTED: 1_0_1_1_1_0

  Stability Metrics:
    • Overall Stability Score: 82.45 / 100
    • Heartbeat Jitter (CV): 0.0842 (target < 0.15)
    • Outlier Ratio: 2.34%
    • Convergence Rate: 123.45 (higher = faster)
    • Load Coupling: 0.8234 (target > 0.7)
    • Settling Time: 1245.3 ticks
    • Mean Workload Duration: 12.34 ms
```

### Step 3: Review Generated Files

The analysis will create:

```
stability_rankings.csv              # Detailed rankings by config
01_stability_scores.png             # Main boxplot comparison
02_jitter_control.png               # CV by config
03_convergence_speed.png            # Convergence comparison
04_load_coupling.png                # Load-response strength
05_metrics_heatmap.png              # Normalized metrics heatmap
06_tradeoff_jitter_vs_convergence.png  # Trade-off visualization
```

**View the visualizations:**
```bash
# In terminal or file viewer
ls -lh *.png
eog 01_stability_scores.png  # Or use your image viewer
```

---

## Interpreting Results

### Stability Score (Primary Metric)

**Range:** 0-100

- **0-50:** Unstable, high jitter, poor convergence
- **50-70:** Acceptable, moderate stability
- **70-85:** Good, reliable for production
- **85-100:** Excellent, reference-quality stability

The golden configuration should score **≥75**.

### Jitter (CV – Coefficient of Variation)

**Measure:** Standard deviation / Mean of heartbeat interval

**Interpretation:**
- **CV < 0.10:** Excellent (steady heartbeat, ±10% variation)
- **0.10-0.15:** Good (±10-15% variation)
- **0.15-0.20:** Acceptable (±15-20% variation)
- **> 0.20:** Poor (erratic heartbeat)

**Goal:** The golden configuration should have **CV < 0.15**.

### Convergence Rate

**Measure:** How quickly decay slope fitting reaches steady state

**Interpretation:**
- **Higher values:** Faster convergence
- **Lower values:** Slower settling, may oscillate

The golden configuration should converge within the first 5000 heartbeat ticks.

### Load-Response Coupling

**Measure:** Correlation between workload intensity and heartbeat interval

**Interpretation:**
- **Correlation > 0.8:** Strong coupling, heartbeat responds proportionally to load
- **0.7-0.8:** Good coupling, minor lag
- **< 0.7:** Weak coupling, heartbeat may be unresponsive to load

**Goal:** The golden configuration should have **correlation > 0.75**.

---

## Troubleshooting

### Problem: Builds Fail for Certain Configs

**Symptom:** Error message during build phase
```
✗ Build failed for 1_1_0_1_1_1
```

**Solution:**
1. Check the build log:
   ```bash
   cat run_logs/build_1_1_0_1_1_1.log
   ```
2. Look for compilation errors or missing definitions
3. Common causes:
   - Missing header includes
   - Circular dependencies between features
   - Undefined macros

**Next steps:**
- Modify the Makefile or source code to fix incompatibility
- Re-run experiment (builds are skipped if config hasn't changed)

### Problem: Runs Crash Mid-Execution

**Symptom:** Some runs show segfault or crash
```
✗ Configuration 1_1_0_1_1_1 crashed during execution
```

**Solution:**
1. Check individual run log:
   ```bash
   tail -100 run_logs/1_1_0_1_1_1_run_1.log
   ```
2. Look for stack traces or error messages
3. Common causes:
   - Buffer overflow in heartbeat metrics collection
   - Memory leak in rolling window
   - Race condition in heartbeat thread

**Next steps:**
- If crash is in heartbeat code, check for thread safety issues
- Run with GDB if deterministic:
  ```bash
  gdb --args ./build/amd64/fastest/starforth --doe-experiment
  ```

### Problem: Analysis Fails (Missing Dependencies)

**Symptom:** R script errors
```
Error: package 'tidyverse' not found
```

**Solution:**
```bash
# Install required R packages
R --quiet --no-save <<'EOF'
install.packages(c("tidyverse", "ggplot2", "gridExtra"))
quit()
EOF

# Then retry:
Rscript scripts/analyze_heartbeat_stability.R
```

### Problem: Results Look Odd (All Configs Equal)

**Symptom:** All stability scores are identical or very similar

**Possible causes:**
1. Heartbeat data not being collected (check DoeMetrics struct)
2. Heartbeat thread disabled during build (check HEARTBEAT_THREAD_ENABLED=1)
3. Metrics not being extracted from VM (check metrics_from_vm())

**Debug:**
```bash
# Verify heartbeat was enabled in build
nm ./build/amd64/fastest/starforth | grep heartbeat
# Should show symbols

# Check a run log for metrics
tail -1 run_logs/1_0_1_1_1_0_run_1.log | tr ',' '\n' | nl
# Should show 60+ columns with numeric values
```

---

## Golden Configuration Usage

Once identified, the golden configuration should be:

1. **Documented:** Create a reference config file
   ```bash
   cat > docs/GOLDEN_CONFIG_HEARTBEAT.md <<'EOF'
   # Golden Configuration (Phase 2 Heartbeat)

   **Config:** [WINNER_HERE]
   **Date:** [COMPLETION_DATE]
   **Stability Score:** [SCORE]

   Use this configuration as the default for:
   - MamaForth production baseline
   - PapaForth reference physics
   - Future optimization benchmarks
   EOF
   ```

2. **Locked in build system:** Add to Makefile
   ```makefile
   # Golden configuration from Phase 2 DoE (2025-11-20)
   ENABLE_LOOP_1_HEAT_TRACKING ?= 1    # From golden config
   ENABLE_LOOP_2_ROLLING_WINDOW ?= 0
   ENABLE_LOOP_3_LINEAR_DECAY ?= 1
   # ... etc
   ```

3. **Used as baseline:** Future experiments compare against this
   ```bash
   # Experiment Phase 3: Compare new optimizations to golden
   ./scripts/run_comparison_doe.sh --baseline GOLDEN --test NEW_FEATURE
   ```

---

## Archive & Handoff

After analysis:

```bash
# Create archive of results_run_01_2025_12_08
cd /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5

tar czf ../2025_11_20_HEARTBEAT_TOP5.tar.gz \
    experiment_results_heartbeat.csv \
    configuration_manifest.txt \
    experiment_summary.txt

# Create analysis report
cat > ANALYSIS_SUMMARY.txt <<'EOF'
# StarForth Heartbeat DoE - Analysis Summary

**Golden Configuration:** [WINNER]
**Overall Stability Score:** [SCORE] / 100

## Metrics
- Jitter (CV): [VALUE] (target < 0.15)
- Convergence Rate: [VALUE] ticks
- Load Coupling: [VALUE] (target > 0.7)
- Settling Time: [VALUE] ticks

## Interpretation
[Details about why this config is optimal]

## Recommendations
[Future work based on this config]
EOF

# Archive everything
tar czf ../2025_11_20_HEARTBEAT_RESULTS_FINAL.tar.gz \
    *.csv *.png *.txt

echo "Archive ready: ../2025_11_20_HEARTBEAT_RESULTS_FINAL.tar.gz"
```

---

## Next Steps (Phase 3)

Once the golden configuration is identified:

1. **Lock it in code:** Update CLAUDE.md with golden config
2. **Create MamaForth baseline:** Use golden config for next experiment
3. **Plan Phase 3:** Optimization experiments around golden config
4. **Document learnings:** What made this config optimal?

---

## Success Criteria

✓ All 250 runs completed without segfault
✓ CSV file has 251 rows (header + 250 data)
✓ All heartbeat metrics are numeric (not NaN or Inf)
✓ Stability rankings are clear (no tie for first place)
✓ R analysis generates all 6 visualizations
✓ Golden config identified with p < 0.05 significance
✓ Golden config stability score ≥ 75

---

## References

- **Design:** `docs/DOE_HEARTBEAT_EXPERIMENT_DESIGN.md`
- **Script:** `scripts/run_factorial_doe_with_heartbeat.sh`
- **Analysis:** `scripts/analyze_heartbeat_stability.R`
- **Metrics Schema:** `docs/DOE_METRICS_SCHEMA.md`

**Questions?** Review CLAUDE.md or examine run logs for details.