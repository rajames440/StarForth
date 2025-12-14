# StarForth Heartbeat DoE – CORRECTED Design (Heartrate Focus)

**Date:** November 20, 2025
**CORRECTION:** Previous design missing critical metric: **HEARTBEAT RATE (tick_ns)**
**Status:** Ready for implementation

---

## The Critical Insight You Caught

The heartbeat isn't just a **fixed tick**. The system has a **variable heartbeat rate** controlled by:

```c
/* HeartbeatWorker.tick_ns — this is DYNAMIC, not constant! */
uint64_t tick_ns;  /* Nanoseconds between ticks — VARIES with load! */
```

**What this means:**
- Default: `HEARTBEAT_TICK_NS` (~1 millisecond)
- **But:** The inference engine can adaptively adjust `tick_target_ns`
- Different configurations may modulate heartbeat frequency differently
- **This is the KEY physics behavior we must measure!**

---

## Stage 1 Problem (What We Got Wrong)

Stage 1 baseline ran **without heartbeat thread** (HEARTBEAT_THREAD_ENABLED=0):
- Fixed 1ms tick (no modulation)
- No adaptive heartrate
- Doesn't reflect StarForth's intended physics-driven behavior

**Stage 1 conclusion:** "System is deterministic, no heartbeat variation"

**Stage 1 was incomplete** because it disabled the very mechanism that makes the system adaptive!

---

## Stage 2 Correction (What We Should Do)

### Primary Hypothesis

The 5 elite configurations differ in **how they modulate heartbeat rate**:

| Config | Expected Behavior |
|--------|-------------------|
| 1_0_1_1_1_0 | Tight, responsive heartrate (low jitter, fast modulation) |
| 1_0_1_1_1_1 | Similar, with decay inference slowing response? |
| 1_1_0_1_1_1 | Window-driven, may have different modulation pattern |
| 1_0_1_0_1_0 | Lean, may have sluggish response |
| 0_1_1_0_1_1 | Window-without-heat, baseline behavior |

### Key Metrics for Heartrate (Not Just Intervals)

**We need to measure:**

1. **Heartbeat Rate Modulation (NOT just interval jitter)**
   ```
   heartbeat_rate_trajectory = [tick_ns_t0, tick_ns_t1, tick_ns_t2, ...]

   What we measure:
   - Mean heartbeat rate (expected: ~1,000,000 ns = 1 kHz baseline)
   - Responsiveness to load (does rate slow under heavy load?)
   - Convergence time (how long until rate stabilizes?)
   - Modulation range (min to max tick_ns observed)
   ```

2. **Heartrate vs. Workload Coupling**
   ```
   For each tick:
     workload_intensity = lookups_this_tick / lookups_per_second_baseline
     heartrate_hz = 1_000_000_000 / tick_ns

   Measure: correlation(workload_intensity, heartrate_hz)
   - Positive correlation: heartrate slows under load (good physics)
   - Negative correlation: heartrate speeds up under load (bad physics)
   - No correlation: heartrate doesn't respond to load (broken physics)
   ```

3. **Heartrate Stability (Not "jitter" but "rate consistency")**
   ```
   - Does heartrate converge to a steady state?
   - How quickly does it adapt to workload changes?
   - Does it oscillate or settle smoothly?
   ```

4. **Heartrate Efficiency**
   ```
   - Is the system running faster/slower with adapted heartrate?
   - Workload duration: should improve with optimized heartrate
   - Memory: should be more efficient with tuned heartrate
   ```

---

## Corrected Metrics Collection

### Per-Tick Collection (During Execution)

```c
/* In heartbeat_thread_main(), before nanosleep() */
struct HeartbeatTickData {
    uint64_t tick_number;
    uint64_t tick_ns;                   /* THIS IS THE HEARTBEAT RATE! */
    uint64_t workload_ops_this_tick;    /* Dictionary lookups in this tick */
    uint64_t wall_clock_ns;             /* Actual wall-clock time */
    uint64_t hot_word_count;            /* Physics state */
    uint64_t total_heat;                /* Physics state */
};

/* Store in circular buffer or rolling window */
```

### Post-Run Analysis Metrics

```c
typedef struct {
    /* === Original Metrics === */
    /* ... existing 38 metrics ... */

    /* === CORRECTED: Heartbeat Rate Metrics === */

    /* Heartbeat rate trajectory */
    uint64_t total_ticks_executed;
    double   mean_heartbeat_rate_hz;              /* Hz = 1e9 / mean_tick_ns */
    double   heartbeat_rate_stddev_hz;            /* Variation in Hz */
    double   heartbeat_rate_cv;                   /* Coefficient of variation */
    uint64_t heartbeat_rate_min_ns;               /* Slowest (highest tick_ns) */
    uint64_t heartbeat_rate_max_ns;               /* Fastest (lowest tick_ns) */
    double   heartbeat_rate_range_hz;             /* max_hz - min_hz */

    /* Heartrate vs Load Coupling */
    double   load_to_heartrate_correlation;       /* Does rate respond to load? */
    int      load_response_direction;             /* +1 = slower under load (good) */
    double   heartrate_adaptation_latency_ticks;  /* Ticks until rate responds */
    double   heartrate_settling_time_ticks;       /* Ticks to settle after change */

    /* Heartrate Convergence */
    double   heartrate_convergence_time_ms;       /* Time to reach stable rate */
    int      heartrate_converged;                 /* 1 = yes, 0 = still oscillating */
    double   heartrate_oscillation_amplitude_hz;  /* Peak-to-peak variation */

    /* Overall Heartrate Stability Score */
    double   heartrate_stability_score;           /* 0-100: how stable is heartrate? */

} DoeMetrics_CORRECTED;
```

---

## The Missing Piece: Heartbeat Rate Logging

**Critical code addition needed:**

```c
/* In vm.c heartbeat_thread_main() - CAPTURE tick_ns BEFORE and AFTER */

static inline void capture_heartbeat_rate_sample(VM *vm, uint64_t tick_ns, uint64_t workload_ops) {
    /* Store sample in rolling buffer */
    if (!vm->heartbeat_rate_samples) {
        vm->heartbeat_rate_samples = calloc(HEARTBEAT_RATE_SAMPLE_MAX, sizeof(HeartbeatRateSample));
    }

    HeartbeatRateSample *sample = &vm->heartbeat_rate_samples[vm->heartbeat_rate_sample_count];
    sample->tick_number = vm->heartbeat.tick_count;
    sample->tick_ns = tick_ns;  /* THE HEARTBEAT RATE! */
    sample->workload_ops = workload_ops;
    sample->timestamp_ns = platform_monotonic_ns();

    vm->heartbeat_rate_sample_count++;
}

/* In heartbeat_thread_main() */
static void* heartbeat_thread_main(void *arg) {
    VM *vm = (VM*)arg;

    while (!vm->heartbeat.worker->stop_requested) {
        uint64_t tick_start_ns = platform_monotonic_ns();

        vm_heartbeat_run_cycle(vm);  /* May adjust tick_ns */

        uint64_t tick_ns = vm->heartbeat.worker->tick_ns;
        uint64_t workload_ops = vm->execution_heat_current_tick;  /* Need to track */

        /* CAPTURE THE HEARTBEAT RATE! */
        capture_heartbeat_rate_sample(vm, tick_ns, workload_ops);

        /* Sleep */
        nanosleep(...);
    }
}
```

---

## Corrected Analysis (R Script)

```r
# Load heartbeat rate samples from CSV
doe_data <- read.csv("experiment_results_heartbeat.csv") %>%
  mutate(
    # Convert heartbeat rates (tick_ns → Hz)
    mean_heartbeat_hz = 1e9 / mean_heartbeat_rate_ns,
    heartrate_stability = 1 / (1 + heartbeat_rate_cv),  # Lower CV = higher score
  )

# PRIMARY ANALYSIS: Heartrate Stability
stability_by_heartrate <- doe_data %>%
  group_by(configuration) %>%
  summarize(
    mean_heartrate_hz = mean(mean_heartbeat_hz),
    mean_heartrate_stability = mean(heartrate_stability),
    mean_load_coupling = mean(load_to_heartrate_correlation),
    mean_workload_ms = mean(vm_workload_duration_ns_q48 / 1e6)
  ) %>%
  mutate(
    # Golden config = best heartrate stability + strongest load coupling
    golden_score = (heartrate_stability * 0.5) + (load_coupling * 0.3) + (workload_improvement * 0.2)
  )

# Golden Config = Highest golden_score
```

---

## Why This Matters

### Stage 1 Got Lucky
Stage 1 found 5 good configs **by accident** because it measured final performance, not physics.

### Stage 2 Should Be Different
Stage 2 should measure **whether the physics is working correctly**:

- **Can the system modulate heartbeat rate** in response to load?
- **Is the modulation smooth** or does it oscillate?
- **Does heartrate modulation improve overall performance?**
- **Which configuration has the most responsive, stable heartrate?**

**The golden configuration should be the one where heartbeat rate modulation is:**
1. ✓ **Responsive** (correlates with workload)
2. ✓ **Smooth** (doesn't oscillate wildly)
3. ✓ **Efficient** (results in best overall performance)

---

## Revised DoE Experiment Design

### What Actually Gets Measured

Instead of just "jitter in fixed 1ms tick intervals", measure:

**Per-Run Heartbeat Rate Trajectory:**
```
Tick 0:    tick_ns = 1,000,000  (1 ms baseline)
Tick 1:    tick_ns = 1,500,000  (slowing under load)
Tick 2:    tick_ns = 2,000,000  (further slowdown)
Tick 10:   tick_ns = 1,200,000  (adapting back)
Tick 100:  tick_ns = 1,050,000  (converged to steady state)
```

**What we analyze:**
1. Did heartrate change at all? (some configs might keep it fixed)
2. How much did it vary? (CV of tick_ns, not CV of fixed intervals)
3. Did it respond to load? (correlation with workload)
4. Did it converge smoothly? (no oscillation)
5. Did it improve performance? (faster execution with adaptive heartrate?)

---

## Implementation Checklist (Revised)

- [ ] Add `HeartbeatRateSample` struct to capture `tick_ns` per tick
- [ ] Modify `heartbeat_thread_main()` to log `tick_ns` trajectory
- [ ] Extend `DoeMetrics` with heartrate-specific fields (not just interval jitter)
- [ ] Update CSV schema to include heartrate trajectory metrics
- [ ] Modify R analysis to focus on **heartrate modulation patterns**
- [ ] Update golden config selection: prioritize **load responsiveness**

---

## Expected Results (Corrected)

### Hypothesis: Configs Differ in Heartrate Modulation

**Config 1 (1_0_1_1_1_0) - Expected WINNER:**
- Heartrate slowly increases under load (tick_ns increases, Hz decreases)
- Smooth adaptation (no oscillation)
- Converges within 1000 ticks
- Performance improves (faster overall execution)
- Load coupling: 0.8+ (strong)

**Config 5 (0_1_1_0_1_1) - Expected LOSER:**
- Heartrate doesn't respond to load (fixed tick_ns)
- No correlation with workload
- Performance degrades (why? system can't adapt)

---

## Key Realization

The heartbeat isn't just a **timer**. It's a **feedback mechanism**:

```
Load increases
  ↓
Inference engine detects change
  ↓
Adjusts tick_target_ns (slower heartbeat = more thinking time)
  ↓
Heartbeat rate modulates (tick_ns increases)
  ↓
Physics engine gets more time to tune parameters
  ↓
System adapts and performance improves
```

**We need to measure whether this feedback loop is working!**

---

## Next Steps

1. **Extend HeartbeatWorker:** Add rate sample collection
2. **Modify heartbeat_thread_main():** Log `tick_ns` per tick
3. **Update DoeMetrics:** Heartrate trajectory analysis
4. **Rewrite CSV schema:** Focus on heartrate modulation
5. **Rewrite R analysis:** Measure load-response coupling and heartrate stability
6. **Re-run experiment:** Focus on configs that modulate heartrate best

---

**This changes the entire experiment focus.**

Instead of measuring "jitter in a fixed heartbeat", we're measuring "**how well does the system modulate its heartbeat in response to load?**"

That's the real physics story.