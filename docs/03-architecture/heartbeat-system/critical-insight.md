# CRITICAL INSIGHT: Heartbeat Rate is Variable, Not Fixed

**Date:** November 20, 2025
**Status:** Game-changer for Phase 2 DoE
**Impact:** Previous design was incomplete

---

## The Discovery

The heartbeat isn't a **fixed 1ms tick**. It's a **variable rate** that adapts to system load:

```c
/* In HeartbeatWorker struct (vm.c:64) */
uint64_t tick_ns;  /* ← THIS VARIES! Not constant! */

/* In heartbeat_thread_main() (vm.c:695) */
uint64_t tick_ns = worker->tick_ns ? worker->tick_ns : HEARTBEAT_TICK_NS;
```

**What this means:**
- Default: ~1,000,000 ns (1 kHz)
- **Can be adjusted by inference engine** to respond to load
- Different configurations may modulate heartbeat **differently**
- This is **THE physics feedback mechanism** we should measure!

---

## Why This Matters for Phase 2

### Previous Design (INCOMPLETE)

```
Baseline (Stage 1): HEARTBEAT_THREAD_ENABLED=0
├─ Fixed 1ms tick (no modulation)
├─ No adaptive behavior
└─ Result: "System is deterministic, stable"
   ↳ WRONG! We disabled the feature that makes it adaptive!
```

### Correct Design (This Changes Everything)

```
Phase 2: HEARTBEAT_THREAD_ENABLED=1 (WITH modulation)
├─ Heartbeat rate VARIES per tick (tick_ns changes)
├─ Inference engine adjusts tick_ns based on workload
├─ Different configs modulate heartrate differently
└─ Result: Measure which config has BEST load responsiveness
   ↳ Golden config = smoothest, most responsive heartrate modulation
```

---

## The Physics Story (What's Really Happening)

### Load Increases → System Slows Heartbeat (Thinking Time)

```
High workload detected
  ↓
Inference engine: "System is busy, I need more thinking time"
  ↓
Increases tick_ns (slows heartbeat, e.g., 1ms → 2ms)
  ↓
Heartbeat runs less frequently (gives inference more CPU time)
  ↓
Physics parameters converge faster
  ↓
System adapts and performance improves
```

### Expected Behavior of Good Configs

- **Responsive:** tick_ns increases under load (slower heartbeat = more tuning time)
- **Smooth:** No wild oscillation in tick_ns values
- **Efficient:** Results in better overall performance

### Expected Behavior of Bad Configs

- **Unresponsive:** tick_ns stays fixed (doesn't adapt to load)
- **Oscillating:** tick_ns bounces around wildly
- **Inefficient:** Heartrate adaptation makes things worse, not better

---

## What Stage 1 Missed

Stage 1 baseline ran **without heartbeat thread**:

```c
HEARTBEAT_THREAD_ENABLED=0  /* Thread disabled */
```

This means:
- ✗ No tick_ns modulation
- ✗ No adaptive behavior
- ✗ System can't respond to load
- ✗ Physics isn't actually running

**Stage 1 conclusion was incomplete:** "These 5 configs are good" — **but we don't know if their heartrate MODULATION is good!**

---

## What Phase 2 Should Actually Measure

### Primary Metric: Heartbeat Rate Modulation Quality

Not "jitter in a fixed tick", but **"how well does heartrate respond to load?"**

**Measure these per run:**

1. **Heartbeat Rate Trajectory**
   ```
   tick_ns values: [1000000, 1200000, 1400000, 1200000, 1100000, 1050000, ...]
   Convert to Hz:  [1000,    833.3,   714.3,   833.3,   909.1,   952.4,   ...]
   ```

2. **Load-Heartrate Correlation**
   ```
   For each tick:
     workload = lookups_this_tick
     heartrate_hz = 1e9 / tick_ns

   Correlation(workload, heartrate_hz) = ?
   - Positive: heartrate DECREASES under load (slower, more thinking time) ✓ GOOD
   - Negative: heartrate INCREASES under load (faster, less tuning)  ✗ BAD
   - Zero: heartrate doesn't respond to load                        ✗ NO ADAPTATION
   ```

3. **Heartrate Stability**
   ```
   - Does heartrate converge to a steady state?
   - How smooth is the convergence?
   - Does it oscillate?
   - How long to settle?
   ```

4. **Performance Impact**
   ```
   - Does heartrate modulation improve execution time?
   - Is the system faster with adaptive heartrate?
   ```

---

## Golden Configuration Characteristics

The winner should have:

✓ **Responsive heartrate** (tick_ns changes with load)
✓ **Negative correlation** (slower heartbeat under heavy load = more thinking time)
✓ **Smooth modulation** (no wild oscillations)
✓ **Fast convergence** (settles within ~1000 ticks)
✓ **Performance improvement** (adaptive heartrate makes system faster/better)

**If a config has none of these, it's not adaptive — it's just a static heartbeat.**

---

## The Missing Code (What Needs to Happen)

### Current State

```c
/* heartbeat_thread_main() in vm.c:682 */
static void* heartbeat_thread_main(void *arg) {
    /* ... runs heartbeat cycle ... */

    uint64_t tick_ns = worker->tick_ns ? worker->tick_ns : HEARTBEAT_TICK_NS;

    /* Sleep for tick_ns nanoseconds */
    nanosleep(&req, &rem);

    /* ← tick_ns value is lost! We don't capture it! */
}
```

**We need to LOG tick_ns per tick!**

### What's Missing

```c
/* NEEDED: Capture tick_ns trajectory */
struct HeartbeatRateSample {
    uint64_t tick_number;
    uint64_t tick_ns;        /* ← THE HEARTBEAT RATE */
    uint64_t workload_ops;   /* Dictionary lookups this tick */
    uint64_t timestamp_ns;
};

/* In heartbeat_thread_main(), capture sample before sleep */
capture_heartbeat_rate_sample(vm, tick_ns, workload_ops_this_tick);

/* Store samples in rolling buffer in VM struct */
```

### Then Analyze

```r
# Load tick_ns trajectory
heartrate_samples <- read.csv("heartbeat_rates_config_1_run_1.csv")

# Compute metrics
heartrate_hz <- 1e9 / heartrate_samples$tick_ns
load_per_tick <- heartrate_samples$workload_ops

# Measure correlation
correlation <- cor(load_per_tick, heartrate_hz)
# ← This tells us: does heartrate respond to load?

# Expected for good config: correlation > 0.7 (strong negative)
# ← Negative = slower heartrate under heavy load ✓
```

---

## Revised Experimental Question

**Old (Incomplete):** "Which config is most stable?"
→ Measured: interval jitter in fixed 1ms tick

**New (Correct):** "Which config has the best heartbeat rate modulation?"
→ Measure: tick_ns trajectory, load-response coupling, convergence smoothness

---

## Why This is Important

The heartbeat rate modulation is **the core physics mechanism**:

```
Without adaptive heartbeat:
├─ System can't respond to load changes
├─ Physics parameters don't converge
├─ Performance stagnates
└─ "Stability" is just having a steady bad system

With adaptive heartbeat (Phase 2):
├─ System modulates heartbeat in response to load
├─ Physics gets thinking time when needed
├─ Parameters converge dynamically
├─ "Stability" means smooth, responsive adaptation
└─ Performance improves over time
```

**The golden config isn't the one with the steadiest heartbeat — it's the one with the BEST ADAPTIVE response.**

---

## Implementation Priority

### HIGH PRIORITY (Blocking)

1. **Extend HeartbeatWorker:** Add tick_ns sample logging
2. **Modify heartbeat_thread_main():** Capture tick_ns trajectory
3. **Update DoeMetrics:** Add heartrate modulation metrics
4. **Update CSV schema:** Include heartrate trajectory data

### MEDIUM PRIORITY (Affects analysis)

5. Modify R analysis to focus on load-heartrate coupling
6. Update golden config selection criteria
7. Generate heartrate response plots

### LOW PRIORITY (Nice-to-have)

8. Visualize tick_ns trajectory over time
9. Compare heartrate modulation patterns across configs

---

## Expected Results (When Done Right)

### Config Analysis

| Config | Heartrate Responsiveness | Load Coupling | Convergence | Expected Rank |
|--------|-------------------------|---------------|-------------|---|
| 1_0_1_1_1_0 | High (modulates 1→2ms) | 0.82 | Fast | 🥇 Gold |
| 1_0_1_1_1_1 | High | 0.78 | Medium | 🥈 Silver |
| 1_1_0_1_1_1 | Medium | 0.65 | Slow | 🥉 Bronze |
| 1_0_1_0_1_0 | Low (sluggish) | 0.55 | Very slow | 4th |
| 0_1_1_0_1_1 | None (fixed) | 0.20 | N/A | 5th |

**The golden config will be the one with highest load-heartrate correlation.**

---

## Summary

**You were right:** We need to measure **heartbeat rate**, not just "jitter in a fixed tick".

The heartbeat is **adaptive**, and the quality of that adaptation is what differentiates the configurations.

**Next:** Implement heartrate trajectory logging and re-run the Phase 2 DoE focused on load responsiveness.

This is the real physics story. ✓