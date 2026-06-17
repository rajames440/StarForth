<!-- Moved from docs/01-getting-started/quick-start/heartbeat-doe.md to docs/working/architecture/getting-started/quick-start/heartbeat-doe.md on 2026-06-16 (docs reorg Phase 2) -->
# StarForth Heartbeat DoE – Quick Start

**Last Updated:** November 20, 2025
**Time to Execute:** 2-4 hours + 30 minutes analysis
**Complexity:** One command (fully automated)

---

## TL;DR – Execute This

```bash
# Navigate to repository
cd /home/rajames/CLionProjects/StarForth

# Run the experiment (will prompt before starting)
./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5

# When complete, analyze results_run_01_2025_12_08
Rscript scripts/analyze_heartbeat_stability.R
```

---

## What This Does

| Step | Command | Time | Output |
|------|---------|------|--------|
| 1. Setup | `./scripts/run_factorial_doe_with_heartbeat.sh LABEL` | 5 min | Prompt to confirm |
| 2. Build & Run | Executes 250 runs | 2-4 hours | Progress log, CSV results |
| 3. Analyze | `Rscript scripts/analyze_heartbeat_stability.R` | 30 min | Rankings + 6 visualizations |
| 4. Review | Open PNG files | 10 min | Visual comparison of configs |

**Total time:** ~3-5 hours (mostly waiting for runs to complete)

---

## Pre-Flight Checklist (5 minutes)

```bash
# ✓ Check disk space
df -h /home/rajames/CLionProjects/StarForth-DoE
# Need: ~1 GB available

# ✓ Test build system
cd /home/rajames/CLionProjects/StarForth
make clean && make fastest
# Should complete without errors

# ✓ Test binary
./build/amd64/fastest/starforth -c "1 2 + . BYE"
# Should output: 3

# ✓ Test R (optional, can skip if confident)
Rscript -e "library(tidyverse); print('OK')"
# Should print: [1] "OK"
```

---

## The Three Commands You Need

### Command 1: Launch Experiment

```bash
cd /home/rajames/CLionProjects/StarForth

./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
```

**What it does:**
- Builds 5 elite configurations
- Runs each 50 times = 250 total runs
- Records heartbeat metrics (jitter, convergence, coupling)
- Creates `experiment_results_heartbeat.csv`

**You'll see:**
```
═══════════════════════════════════════════════════════════
STARFORTH HEARTBEAT DoE - TOP 5 ELITE CONFIGURATIONS
═══════════════════════════════════════════════════════════
→ Configurations: 5 elite (selected from Stage 1 baseline)
→ Runs per config: 50
→ TOTAL RUNS: 250
...
Press ENTER to begin execution (Ctrl+C to abort):
```

**When running:**
```
→ Run 1/250 - config 1_0_1_0_1_0 #47...
✓ Build successful for 1_0_1_0_1_0
✓ Run 1/250 completed (12s)
→ Run 2/250 - config 0_1_1_0_1_1 #13...
[continues for 2-4 hours]
```

**When done:**
```
═══════════════════════════════════════════════════════════
EXPERIMENT COMPLETE
═══════════════════════════════════════════════════════════
✓ All runs completed!
✓ Results: .../experiment_results_heartbeat.csv
```

### Command 2: Analyze Results

```bash
Rscript scripts/analyze_heartbeat_stability.R
```

**What it does:**
- Loads CSV from current directory
- Ranks configs by stability score
- Generates 6 visualizations (PNG files)
- Identifies golden configuration

**You'll see:**
```
Loading data from: experiment_results_heartbeat.csv

=== STABILITY RANKINGS ===
configuration mean_jitter_cv rank_stability
1_0_1_1_1_0      0.0842           1
1_0_1_1_1_1      0.0891           2
...

═══════════════════════════════════════════════════════════
GOLDEN CONFIGURATION RECOMMENDATION
═══════════════════════════════════════════════════════════

✓ SELECTED: 1_0_1_1_1_0

  Stability Metrics:
    • Overall Stability Score: 82.45 / 100
    • Heartbeat Jitter (CV): 0.0842 (target < 0.15)
    • Load Coupling: 0.8234 (target > 0.7)
```

**Generated files:**
- `stability_rankings.csv` – Detailed metrics per config
- `01_stability_scores.png` – Main comparison plot
- `02_jitter_control.png` – Jitter by config
- `03_convergence_speed.png` – Convergence comparison
- `04_load_coupling.png` – Load-response strength
- `05_metrics_heatmap.png` – All metrics heatmap
- `06_tradeoff_jitter_vs_convergence.png` – Trade-off plot

### Command 3: Use Golden Configuration

```bash
# Document the golden config
cat >> CLAUDE.md <<'EOF'
## Golden Configuration (Phase 2 Heartbeat DoE)

**Config:** [WINNER from analysis]
**Date:** 2025-11-20
**Stability Score:** [SCORE] / 100

Use this as default for all future builds.
EOF

# Or lock it in the build system
make ENABLE_LOOP_1_HEAT_TRACKING=1 ENABLE_LOOP_3_LINEAR_DECAY=1 ...
```

---

## The 5 Configurations (What We're Testing)

```
Config 1: 1_0_1_1_1_0  ← Minimal (heat + decay + pipelining + window inference)
Config 2: 1_0_1_1_1_1  ← Config 1 + decay inference
Config 3: 1_1_0_1_1_1  ← Alternative (skip decay loop)
Config 4: 1_0_1_0_1_0  ← Lean (heat + decay + window only)
Config 5: 0_1_1_0_1_1  ← Contrast (no heat tracking)
```

**Why these 5?** They were the best performers from Stage 1 (3200-run baseline). Now we measure their *temporal stability* instead of just final performance.

---

## Key Metrics Explained (In 10 Seconds)

| Metric | What | Target |
|--------|------|--------|
| **Stability Score** | Overall goodness (0-100) | > 75 (gold), > 70 (ok) |
| **Jitter (CV)** | Steadiness of heartbeat | < 0.15 (±15% variation ok) |
| **Convergence** | Speed of tuning | Converge within 5000 ticks |
| **Load Coupling** | Heartbeat follows workload | > 0.75 correlation |

**Golden Config:** Has lowest jitter + fastest convergence + best coupling

---

## What If Something Goes Wrong?

### Build Fails
```bash
# Check error
cat run_logs/build_1_1_0_1_1_1.log
# Fix, then retry (experiment auto-resumes)
```

### Analysis Fails (R error)
```bash
# Install packages
R --quiet --no-save <<'EOF'
install.packages(c("tidyverse", "ggplot2", "gridExtra"))
quit()
EOF
# Retry
```

### Results Look Wrong (All configs equal)
```bash
# Verify heartbeat was enabled
nm ./build/amd64/fastest/starforth | grep heartbeat
# Should show symbols

# Check a run log
tail -1 run_logs/1_0_1_1_1_0_run_1.log | wc -w
# Should show ~60 columns
```

**Full troubleshooting:** See `docs/HEARTBEAT_EXPERIMENT_EXECUTION_GUIDE.md`

---

## Expected Timeline

| Time | Activity |
|------|----------|
| 0:00 | Run experiment command |
| 0:05 | Review config manifest, press ENTER |
| 0:10 | Builds start (5 configs build in sequence) |
| 0:30 | First runs completing |
| 2:30 | ~125 runs done (halfway) |
| 4:30 | All 250 runs complete |
| 5:00 | Run R analysis |
| 5:30 | Results ready (rankings + visualizations) |

---

## Files You Need to Know About

```
StarForth/
├── scripts/
│   ├── run_factorial_doe_with_heartbeat.sh         ← Main command
│   └── analyze_heartbeat_stability.R               ← Analysis command
├── docs/
│   ├── DOE_HEARTBEAT_EXPERIMENT_DESIGN.md          ← Full design
│   └── HEARTBEAT_EXPERIMENT_EXECUTION_GUIDE.md     ← Detailed guide
└── HEARTBEAT_DOE_IMPLEMENTATION_SUMMARY.md         ← What was built
```

---

## Golden Configuration Once Found

**Next steps:**
1. **Document:** Add to CLAUDE.md
2. **Lock:** Use in Makefile as default
3. **Archive:** Save results for reference
4. **Use:** Baseline for Phase 3 (MamaForth)

---

## One-Pager: What's Happening

**Stage 1 (Completed):**
- Ran 3200 tests (2^6 factorial, 50 reps each)
- Found 5 best configurations

**Stage 2 (This Experiment):**
- Focus on 5 best configs only
- Add heartbeat observability (measuring stability)
- Identify golden config by stability metrics
- **~250 runs total (not 3200)**

**Stage 3 (Next):**
- Use golden config as baseline
- Run MamaForth experiments
- Further optimization rounds

---

## Questions?

- **How long?** 2-4 hours to run, 30 min to analyze
- **What if I need to stop?** Ctrl+C, can resume (results saved incrementally)
- **Will it use CPU?** Yes, fully (disable other apps)
- **Can I watch?** Yes, progress printed continuously
- **What's the goal?** Find most stable configuration

---

## Let's Go

```bash
cd /home/rajames/CLionProjects/StarForth
./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
```

**Good luck!** 🚀

---

*For full documentation, see HEARTBEAT_DOE_IMPLEMENTATION_SUMMARY.md or DOE_HEARTBEAT_EXPERIMENT_DESIGN.md*