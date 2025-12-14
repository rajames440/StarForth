# StarForth Phase 2 Heartbeat DoE – CORRECTED AND FINALIZED

**Date:** November 20, 2025
**Status:** ✅ DESIGN COMPLETE (Corrected)
**Critical Insight:** Heartbeat rate is variable (tick_ns), not fixed!

---

## Executive Summary

The initial Phase 2 design was **incomplete** because it measured "jitter in a fixed heartbeat" rather than "**adaptive heartbeat rate modulation**".

**Critical Discovery:** The heartbeat isn't a fixed 1ms tick. It's a **variable rate** (tick_ns) that the inference engine adjusts in response to workload. Different configurations will modulate this rate **differently**, and that's what we should measure.

**Corrected Design:** Phase 2 now focuses on measuring **load-to-heartrate correlation** — how well does each configuration's heartbeat respond to workload changes?

---

## The Physics Story

### How Heartbeat Adaptation Works

```
High workload detected by inference engine
  ↓
Inference decides: "System is busy, increase thinking time"
  ↓
Increases tick_ns (e.g., 1ms → 2ms → 1.5ms)
  ↓
Heartbeat thread sleeps longer (runs less frequently)
  ↓
Gives inference engine more CPU time to tune physics parameters
  ↓
System adapts and performance improves
  ↓
Eventually converges to optimal heartbeat rate for that workload
```

### What We Should Measure

**Primary Metric: Load-to-Heartrate Correlation**

```
correlation(workload_intensity, heartrate_hz)

For each heartbeat tick:
  workload = dictionary lookups executed this tick
  heartrate = 1e9 / tick_ns (conversion to Hz)

Expected GOOD config:
  ✓ Correlation > 0.75 (heartrate strongly responds to load)
  ✓ Smooth modulation (no wild oscillations in tick_ns)
  ✓ Fast convergence (settles within 1000 ticks)
  ✓ Performance improvement (adaptation makes system better)

Expected BAD config:
  ✗ Correlation ≈ 0 (heartrate doesn't respond)
  ✗ Oscillations (tick_ns bounces erratically)
  ✗ No convergence (never settles)
  ✗ Performance degradation (adaptation hurts performance)
```

---

## Why Initial Design Was Wrong

### Previous Approach (INCOMPLETE)

```
"Measure jitter in heartbeat tick intervals"
├─ Assumed tick interval was ~1ms and fixed
├─ Measured variance in that fixed interval
├─ Didn't measure tick_ns CHANGES
└─ Missed the entire adaptive behavior story
```

**Problem:** This measures steady-state "smoothness" but misses the critical metric: **Does the system respond to load?**

### Correct Approach (THIS DESIGN)

```
"Measure heartbeat rate modulation in response to load"
├─ Recognize tick_ns is variable
├─ Capture tick_ns TRAJECTORY (per tick)
├─ Measure correlation with workload intensity
├─ Measure convergence time and smoothness
└─ Identifies configs that ADAPT BEST to load
```

**Benefit:** This measures **adaptive physics behavior** — the real story.

---

## The 5 Elite Configurations

From Stage 1 baseline (3200 runs), these 5 showed best performance:

| Config | Meaning | Expected Behavior |
|--------|---------|-------------------|
| `1_0_1_1_1_0` | Heat + Decay + Pipelining + Window Inference | **Most likely WINNER** — best balanced, responsive |
| `1_0_1_1_1_1` | Above + Decay Inference | Full inference — may be over-tuned |
| `1_1_0_1_1_1` | Heat + Window + Pipelining + Both Inferences | Alternative path — skip decay loop |
| `1_0_1_0_1_0` | Heat + Decay + Window (lean) | Minimal overhead — may be sluggish |
| `0_1_1_0_1_1` | Window + Decay + Inferences (no heat) | Baseline — may not adapt |

**Golden Config Expected:** The one with **highest load-heartrate correlation** + **smoothest modulation** + **fastest convergence**.

---

## What Needs to Be Implemented

### Phase A: Code Changes (2-3 hours)

**1. Add HeartbeatRateSample struct** (include/vm.h)
```c
typedef struct {
    uint64_t tick_number;
    uint64_t tick_ns;              /* ← THE HEARTBEAT RATE */
    uint64_t workload_ops_this_tick;
    uint64_t timestamp_ns;
    uint64_t hot_word_count;
    uint64_t total_heat;
} HeartbeatRateSample;
```

**2. Extend VM struct** (include/vm.h)
```c
HeartbeatRateSample* heartbeat_rate_samples;
uint64_t heartbeat_rate_sample_count;
```

**3. Capture in heartbeat_thread_main()** (src/vm.c)
```c
/* Before nanosleep(), capture tick_ns */
capture_heartbeat_rate_sample(vm, tick_ns, workload_ops_this_tick);
```

**4. Extend DoeMetrics** (include/doe_metrics.h)
```c
/* Add these fields */
double   mean_heartbeat_rate_hz;
double   load_to_heartrate_correlation;        /* THE KEY METRIC */
double   heartrate_responsiveness_score;
double   heartrate_stability_score;
int      heartrate_converged;
```

**5. Implement analysis** (src/doe_metrics.c)
```c
analyze_heartbeat_rate_trajectory(vm, metrics);
/* Computes correlation, convergence, stability */
```

**6. Update CSV schema** (src/doe_metrics.c)
```c
/* Add heartrate metric columns to CSV header/row */
```

### Phase B: Experiment Execution (2-4 hours)

```bash
./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5
# Runs 250 tests (5 configs × 50 runs)
# Captures tick_ns trajectory per run
# Computes load-heartrate correlation
# Generates CSV with new metrics
```

### Phase C: Analysis (30 minutes)

```bash
Rscript scripts/analyze_heartbeat_stability_REVISED.R
# Focus: load_to_heartrate_correlation (primary metric)
# Rank: by responsiveness, not just stability
# Output: heartrate modulation plots + golden config
```

---

## Expected Results

### Heartbeat Rate Metrics Collected

For each of the 250 runs:

```
mean_heartbeat_rate_hz            = ~1000 Hz (1 kHz baseline)
heartbeat_rate_cv                 = 0.05-0.20 (variation)
load_to_heartrate_correlation     = 0.2-0.9 (GOLDEN DISCRIMINATOR)
heartrate_convergence_time_ms     = 100-5000 ms (settling time)
heartrate_responsiveness_score    = 0-100 (composite: correlation + convergence)
```

### Ranking Example

```
=== HEARTBEAT RESPONSIVENESS RANKING ===

Config 1_0_1_1_1_0 (GOLDEN):
  ├─ load_correlation: 0.82 ✓ Strong
  ├─ responsiveness: 85 / 100 ✓ Excellent
  ├─ convergence: 1245 ms ✓ Fast
  └─ overall_score: 0.847

Config 1_0_1_1_1_1:
  ├─ load_correlation: 0.78
  ├─ responsiveness: 81 / 100
  ├─ convergence: 1523 ms
  └─ overall_score: 0.798

Config 1_1_0_1_1_1:
  ├─ load_correlation: 0.65
  ├─ responsiveness: 72 / 100
  ├─ convergence: 2341 ms
  └─ overall_score: 0.689

Config 1_0_1_0_1_0:
  ├─ load_correlation: 0.45
  ├─ responsiveness: 58 / 100
  ├─ convergence: 4567 ms
  └─ overall_score: 0.516

Config 0_1_1_0_1_1:
  ├─ load_correlation: 0.12 ✗ Weak
  ├─ responsiveness: 28 / 100 ✗ Poor
  ├─ convergence: N/A (no adaptation)
  └─ overall_score: 0.186
```

**Golden Config: 1_0_1_1_1_0** — Best load-heartrate coupling, smoothest adaptation.

---

## R Analysis Focus (Revised)

Instead of measuring "jitter in fixed intervals", measure:

```r
# PRIMARY METRIC: Load-Heartrate Correlation
ggplot(doe_data, aes(x = reorder(configuration, load_to_heartrate_correlation),
                      y = load_to_heartrate_correlation)) +
  geom_boxplot() +
  labs(title = "Load-Heartrate Correlation (Higher = Better)",
       subtitle = "Measures how well heartbeat responds to workload")

# SECONDARY: Responsiveness Score
ggplot(doe_data, aes(x = configuration, y = heartrate_responsiveness_score)) +
  geom_violin() +
  labs(title = "Heartbeat Responsiveness (0-100)")

# RANKING: By overall_score
ranking <- doe_data %>%
  group_by(configuration) %>%
  summarize(
    mean_correlation = mean(load_to_heartrate_correlation),
    mean_responsiveness = mean(heartrate_responsiveness_score),
    golden_score = (mean_correlation * 0.6) + (mean_responsiveness * 0.4)
  ) %>%
  arrange(-golden_score)
```

---

## Critical Implementation Details

### Workload Tracking Per Tick

Need to count dictionary lookups in each tick:

```c
/* In VM struct */
uint64_t workload_ops_current_tick;

/* In execution loop (word lookup) */
vm->workload_ops_current_tick++;

/* In heartbeat_run_cycle() */
uint64_t lookups_this_tick = vm->workload_ops_current_tick;
vm->workload_ops_current_tick = 0;  /* Reset for next tick */
```

### Correlation Calculation

```c
/* In analyze_heartbeat_rate_trajectory() */
correlation = covariance(workload_ops, heartrate_hz) /
              sqrt(variance(workload_ops) * variance(heartrate_hz))

/* Result interpretation:
   > 0.75: STRONG coupling (heartrate responds well)
   0.5-0.75: MODERATE coupling
   0.2-0.5: WEAK coupling
   < 0.2: NO coupling (heartrate doesn't respond)
*/
```

---

## Files Created (Correction Phase)

```
StarForth/
├── CRITICAL_HEARTBEAT_INSIGHT.md
│   └─ Explains the discovery (tick_ns is variable)
│
├── HEARTBEAT_DOE_CORRECTED_DESIGN.md
│   └─ Revised design focusing on heartrate modulation
│
├── IMPLEMENTATION_PLAN_HEARTBEAT_LOGGING.md
│   └─ Step-by-step code implementation (9 detailed steps)
│
└── HEARTBEAT_DOE_PHASE2_CORRECTED.md
    └─ This file: Executive summary of corrected design
```

---

## Implementation Checklist

- [ ] **Code Changes (2-3 hours)**
  - [ ] Add HeartbeatRateSample struct to include/vm.h
  - [ ] Add heartbeat_rate_samples to VM struct
  - [ ] Add workload_ops_current_tick tracking
  - [ ] Implement capture_heartbeat_rate_sample() in src/vm.c
  - [ ] Call capture in heartbeat_thread_main()
  - [ ] Extend DoeMetrics with heartrate fields
  - [ ] Implement analyze_heartbeat_rate_trajectory()
  - [ ] Update CSV header and row writers
  - [ ] Compile and test: `make clean && make fastest`

- [ ] **Experiment (2-4 hours)**
  - [ ] Run Phase 2 DoE: `./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_HEARTBEAT_TOP5`
  - [ ] Wait for 250 runs to complete

- [ ] **Analysis (30 minutes)**
  - [ ] Run R analysis: `Rscript scripts/analyze_heartbeat_stability_REVISED.R`
  - [ ] Review heartrate responsiveness ranking
  - [ ] Identify golden config (highest load_correlation)

- [ ] **Documentation**
  - [ ] Add golden config to CLAUDE.md
  - [ ] Archive results
  - [ ] Plan Phase 3

---

## Why This Design is Correct

### Measures What Actually Matters

✓ **Adaptive Physics:** Does the system modulate heartbeat in response to load?
✓ **Responsiveness:** How strongly does heartbeat follow workload?
✓ **Convergence:** Does adaptation settle smoothly?
✓ **Efficiency:** Do load-responsive configs perform better?

### Identifies the True Golden Config

✗ **Old approach:** "Most stable" (steady but possibly unresponsive)
✓ **New approach:** "Most adaptive" (responsive, smooth, efficient)

### Tells the Real Physics Story

The heartbeat isn't just a timer. It's a **feedback mechanism** that should:
1. **Detect** workload changes
2. **Respond** by adjusting tick_ns
3. **Converge** to optimal rate
4. **Improve** system performance

This design measures all four steps. ✓

---

## Timeline

| Phase | Duration | Status |
|-------|----------|--------|
| A: Code changes | 2-3 hours | Ready (plan documented) |
| B: Experiment | 2-4 hours | Ready (script available) |
| C: Analysis | 30 min | Ready (R script ready) |
| **Total** | **~6 hours** | **Ready to execute** |

---

## Success Criteria

✓ Code compiles without errors
✓ Heartbeat rate samples captured (sample_count > 0)
✓ CSV contains new heartrate columns
✓ Correlation metrics computed and non-zero
✓ R analysis generates ranking by load_correlation
✓ Golden config identified (correlation > 0.75)
✓ Performance improves with heartrate adaptation

---

## Key Takeaway

The heartbeat rate (tick_ns) is **not fixed** — it's **dynamically modulated** by the inference engine to respond to workload. Phase 2 DoE must measure this modulation quality, not just the jitter in a fixed tick.

**The golden configuration is the one with the best load-heartrate coupling.**

This corrected design measures the actual physics behavior. ✓

---

**Status:** Design finalized. Ready for implementation.
**Next:** Follow IMPLEMENTATION_PLAN_HEARTBEAT_LOGGING.md