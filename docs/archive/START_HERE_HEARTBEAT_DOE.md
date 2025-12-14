# START HERE – Heartbeat DoE Phase 2

**Status:** ✅ All implementation complete and ready to run

---

## What You Asked For

> This is the next step. Conduct the factorial WITH heartbeat data and we'll write a new experiment to include heartbeat data.

## What Was Delivered

**Complete Phase 2 experimental infrastructure:**
- ✅ Optimized DoE script for 5 elite configs (250 runs)
- ✅ Heartbeat metrics collection architecture (20+ new metrics)
- ✅ Production R analysis pipeline (automated rankings + 6 visualizations)
- ✅ Comprehensive execution guide (5 min checklist to results)
- ✅ Quick-start reference (TL;DR for impatient users)

---

## Execute in 3 Steps

### Step 1: Launch Experiment (5 sec)

```bash
cd /home/rajames/CLionProjects/StarForth
./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
```

You'll see a prompt:
```
Press ENTER to begin execution (Ctrl+C to abort):
```

**Press ENTER** to start. The experiment will run for 2-4 hours automatically.

### Step 2: Wait (2-4 hours)

You'll see progress like:
```
→ Run 1/250 - config 1_0_1_0_1_0 #47...
✓ Build successful for 1_0_1_0_1_0
✓ Run 1/250 completed (12s)
→ Run 2/250 - config 0_1_1_0_1_1 #13...
✓ Run 2/250 completed (11s)
...
```

**What's happening:** Building each config once, then running 50 iterations. Each run takes ~12 seconds.

### Step 3: Analyze Results (30 min)

When complete, run:

```bash
Rscript scripts/analyze_heartbeat_stability.R
```

**Output:** You'll get:
- `stability_rankings.csv` – Detailed metrics per config
- 6 PNG files – Visualizations (boxplots, heatmaps, trade-offs)
- Console output with golden configuration recommendation

---

## What Gets Measured

The experiment measures **stability** (not just speed):

| Metric | What It Measures | Target |
|--------|------------------|--------|
| **Jitter** | How steady is the heartbeat? | CV < 0.15 |
| **Convergence** | How fast does tuning settle? | < 5000 ticks |
| **Load Coupling** | Does heartbeat respond to workload? | Correlation > 0.75 |
| **Overall Score** | Composite stability | > 75 / 100 |

**Golden Configuration:** The one with highest overall stability score.

---

## The 5 Configurations Being Tested

From Stage 1 baseline (3200 runs), these 5 stood out:

1. **1_0_1_1_1_0** – Heat + Decay + Pipelining + Window Inference (minimal overhead)
2. **1_0_1_1_1_1** – Same + Decay Inference (full pipeline)
3. **1_1_0_1_1_1** – Alternative: skip decay loop, use window instead
4. **1_0_1_0_1_0** – Lean: heat + decay + window only
5. **0_1_1_0_1_1** – Contrast: window + decay + inference without heat

**Why?** Stage 1 showed these 5 have the best performance. Stage 2 finds which is most *stable*.

---

## Files That Were Created

```
StarForth/
├── scripts/
│   ├── run_factorial_doe_with_heartbeat.sh       ← Main DoE script
│   └── analyze_heartbeat_stability.R             ← Analysis script
├── docs/
│   ├── DOE_HEARTBEAT_EXPERIMENT_DESIGN.md        ← Full design (theory)
│   └── HEARTBEAT_EXPERIMENT_EXECUTION_GUIDE.md   ← Detailed guide
├── QUICK_START_HEARTBEAT_DOE.md                  ← One-pager TL;DR
└── HEARTBEAT_DOE_IMPLEMENTATION_SUMMARY.md       ← What was built

Output will be written to:
/home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_20_HEARTBEAT_TOP5/
```

---

## Pre-Flight Checklist (5 min)

Before running, verify:

```bash
# ✓ Build system works
cd /home/rajames/CLionProjects/StarForth
make clean && make fastest
./build/amd64/fastest/starforth -c "1 2 + . BYE"
# Should output: 3

# ✓ Disk space available
df -h /home/rajames/CLionProjects/StarForth-DoE
# Need: ~1 GB

# ✓ R is installed (optional, but recommended)
Rscript -e "library(tidyverse); print('OK')"
# Should print: [1] "OK"
```

---

## What Happens Next (After Results)

Once golden config is identified:

1. **Document:** Add to CLAUDE.md
2. **Lock:** Use as default in Makefile
3. **Archive:** Save results for future reference
4. **Use:** Baseline for Phase 3 (MamaForth experiments)

---

## Expected Results

Golden configuration will likely be one of these:
- **1_0_1_1_1_0** – Most likely (minimal overhead, strong performance)
- **1_0_1_1_1_1** – Possibly (full inference)
- **1_1_0_1_1_1** – Maybe (alternative path)

The winner will have:
- **Stability Score:** 75+ / 100 (target)
- **Jitter (CV):** < 0.15 (steady heartbeat)
- **Convergence:** Settle within 5000 ticks
- **Load Coupling:** > 0.75 correlation

---

## Detailed References

For **full details**, see:

1. **Quick overview:** `QUICK_START_HEARTBEAT_DOE.md` (10 min read)
2. **Design details:** `docs/DOE_HEARTBEAT_EXPERIMENT_DESIGN.md` (30 min read)
3. **Execution help:** `docs/HEARTBEAT_EXPERIMENT_EXECUTION_GUIDE.md` (troubleshooting)
4. **Implementation:** `HEARTBEAT_DOE_IMPLEMENTATION_SUMMARY.md` (what was built)

---

## Troubleshooting

### Build fails
```bash
cat run_logs/build_[CONFIG].log
# Fix issue, retry (experiment auto-resumes)
```

### Analysis fails (missing R packages)
```bash
R --quiet --no-save <<'EOF'
install.packages(c("tidyverse", "ggplot2", "gridExtra"))
quit()
EOF
```

### Results look wrong
```bash
# Check heartbeat is enabled
nm ./build/amd64/fastest/starforth | grep heartbeat
# Should show symbols
```

For more help, see execution guide or implementation summary.

---

## Success Criteria

✅ **Done when:**
1. Experiment runs to completion (250 runs)
2. R analysis generates visualizations
3. Stability rankings are clear
4. Golden config identified with p < 0.05

---

## Timeline

| Time | Activity |
|------|----------|
| 0:00 | Run experiment |
| 0:05 | Press ENTER, start |
| 2:30 | ~125 runs complete (halfway) |
| 4:30 | All 250 runs done |
| 5:00 | Run R analysis |
| 5:30 | Results ready (rankings + plots) |

**Total:** ~3.5-5 hours

---

## One Last Thing

This is a **focused, optimized** version of the full factorial:

- **Stage 1:** 3200 runs (2^6 = 64 configs × 50 runs) – found top 5
- **Stage 2:** 250 runs (5 configs × 50 runs) – finds best stability ← **YOU ARE HERE**
- **Stage 3:** TBD (MamaForth baseline using golden config)

---

## Ready?

```bash
cd /home/rajames/CLionProjects/StarForth
./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
```

**That's it!** Press ENTER when prompted and let it run. 🚀

---

## Questions?

- **How long?** 2-4 hours to run, 30 min to analyze
- **What if I stop?** Results saved incrementally, can resume
- **Can I monitor?** Yes, progress printed continuously
- **What's next?** Document golden config, use for Phase 3

---

**Created:** November 20, 2025
**Status:** Ready for execution
**Next:** Run the experiment!