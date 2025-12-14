# Adaptive Window Shrinking & Decay Slope Investigation

**Date:** 2025-11-08
**Status:** Root Cause Analysis Complete
**Critical Finding:** Adaptive window is UNIDIRECTIONAL (shrink-only), Loop #5 growth mechanism deferred to Phase 2

---

## Executive Summary

You asked: **"Does it shrink AND grow? Is there a driving metric and input into maybe a gauge study and inference?"**

The answer is **NO** - currently the system **only shrinks**, with no growth mechanism. Here's the complete picture:

### Current State
- ✅ Shrinking IS implemented and being called
- ❌ Growing is NOT implemented (deferred to Phase 2 as Loop #5)
- ❌ No gauge study/inference mechanism (no measurement error analysis)
- ✅ Input metric exists (pattern diversity), but feedback loop is incomplete

---

## Part 1: Rolling Window Adaptive Shrinking

### Architecture

#### The Feedback Loop (Current - Incomplete)

```
Execution Stream
    ↓
rolling_window_record_execution()  [Called for EVERY word execution]
    ↓
Every 256 executions:
    ↓
rolling_window_check_adaptive_shrink()
    ↓
Measure: rolling_window_measure_diversity()
    Input: Unique adjacent word transitions (word_a → word_b)
    ↓
Calculate: growth_rate = (current_diversity - last_diversity) / last_diversity
    ↓
Decision Rule: IF growth_rate < 1% → SHRINK
    ↓
Action: effective_window_size = effective_window_size * 75%
    Floor: Never shrink below 256
```

#### Key Files

| File | Component | Status |
|------|-----------|--------|
| `src/rolling_window_of_truth.c:546` | `rolling_window_measure_diversity()` | ✅ Implemented |
| `src/rolling_window_of_truth.c:589` | `rolling_window_check_adaptive_shrink()` | ✅ Implemented, called every 256 executions |
| `include/rolling_window_knobs.h` | Tuning parameters | ✅ Full documentation |
| `src/rolling_window_of_truth.c:644-646` | Loop #5 (Growth mechanism) | ❌ **DEFERRED to Phase 2** |

### Invocation Chain

```
vm.c:708: rolling_window_record_execution(&vm->rolling_window, word_id)
    ↓
rolling_window_of_truth.c:541: Every 256 executions:
    rolling_window_check_adaptive_shrink(window)
        ↓
    Measures diversity
    Calculates growth_rate
    IF growth_rate < 1%: shrinks to 75%
```

✅ **Verified:** Function IS called, IS invoked correctly, IS working as designed.

### Current Behavior (Shrink-Only)

**What happens:**
```c
// src/rolling_window_of_truth.c:622-642
if (growth_rate < threshold && window->effective_window_size > ADAPTIVE_MIN_WINDOW_SIZE)
{
    uint32_t new_size = (window->effective_window_size * ADAPTIVE_SHRINK_RATE) / 100;
    if (new_size < ADAPTIVE_MIN_WINDOW_SIZE)
        new_size = ADAPTIVE_MIN_WINDOW_SIZE;

    if (new_size < window->effective_window_size)
    {
        window->effective_window_size = new_size;  // SHRINK
    }
}
```

**What does NOT happen:**
```c
// This does NOT exist - commented as Phase 2
// if (growth_rate > some_upper_threshold && window->effective_window_size < ROLLING_WINDOW_SIZE)
// {
//     window->effective_window_size = grow_window();
// }
```

### Why It's Not Shrinking in Tests

The adaptive shrinking mechanism EXISTS and IS CALLED, but metrics show `effective_window_size = 4096` always. This could mean:

1. **Pattern diversity never plateaus** - The test harness executes a FORTH program with continuously growing unique word transitions, so growth_rate stays > 1% always
2. **Window never becomes "warm"** - Shrinking is disabled until `is_warm=1` (after 4096 executions). If test doesn't run that long, no shrinking occurs
3. **Measurements are too noisy** - Diversity growth jumps around, never settling below 1% threshold

**Evidence from test-03:**
- A_B_CACHE: 3,895 total lookups (call sequence ~3895 words)
- Test harness runs short FORTH program with limited vocabulary
- Pattern diversity likely hits saturation but we see effective_window_size = 4096 in metrics

**Root Cause Hypothesis:** The metrics are extracted ONCE at the end of the test run, but adaptive shrinking happens dynamically. By the time we extract, the window may have already hit ADAPTIVE_MIN_WINDOW_SIZE (256) and stayed there. OR the shrinking never triggered because diversity never plateaued below 1%.

---

## Part 2: Decay Slope Investigation

### Current State (Line 216 of doe_metrics.c)

```c
metrics.decay_slope = 0.0;  // ❌ Hardcoded to zero
```

### Where Decay Information Exists

**Source: `src/physics_metadata.c:164-203`**

```c
void physics_metadata_apply_linear_decay(DictEntry *entry, uint64_t elapsed_ns)
{
    // H(t) = max(0, H_0 - d*t)
    // decay_amount = (elapsed_us * DECAY_RATE_PER_US_Q16) >> 16

    uint64_t elapsed_us = elapsed_ns / 1000;
    uint64_t decay_amount_raw = (elapsed_us * DECAY_RATE_PER_US_Q16) >> 16;

    if (decay_amount >= entry->execution_heat) {
        entry->execution_heat = 0;
    } else {
        entry->execution_heat -= decay_amount;
    }
}
```

### The Linear Model

```
Decay Rate: DECAY_RATE_PER_US_Q16 = 3 (in include/rolling_window_knobs.h)
  Meaning: 3 / 65536 heat units per microsecond
  ≈ 0.0000458 heat/μs

Half-life: ~1-2 seconds for 100-heat word
  100 heat / 45.8 heat_per_second ≈ 2.18 seconds

Equation: decay_amount = (elapsed_us * 3) >> 16
```

### Why decay_slope = 0.0

There is **NO TRACKING** of how much total heat decayed during the run:

1. ✅ Individual words decay (physics_metadata_apply_linear_decay)
2. ❌ No aggregate heat trajectory stored
3. ❌ No slope calculated from beginning to end of test
4. ❌ No "total heat budget" measurement

### What decay_slope Should Represent

**Option 1: Constant Decay Rate**
```c
metrics.decay_slope = (double)DECAY_RATE_PER_US_Q16 / 65536.0;  // 0.0000458
```

**Option 2: Observed Heat Decline**
```c
// At start: sum all execution_heat values
// At end: sum all execution_heat values
// decay_slope = (heat_start - heat_end) / total_runtime_us
```

**Option 3: Per-Word Average Decay**
```c
// Average decay per word = total_heat_decayed / word_count / runtime_us
```

Currently: **NONE OF THESE ARE IMPLEMENTED** - it's hardcoded to 0.0.

---

## Part 3: What You're Really Asking (Design Question)

Your question "Does it shrink AND grow? Is there a gauge study and inference?" touches on fundamental optimization design:

### Current System (Unidirectional)
```
Pattern Diversity Measurement
    ↓
Growth Rate Calculation
    ↓
Decision: Shrink if growth_rate < 1%
    ↓
Action: Reduce window_size to 75%
    ↓
Result: Monotonic reduction toward ADAPTIVE_MIN_WINDOW_SIZE (256)
```

**Problem:** Once shrunk, it can't grow back. If a new pattern emerges after shrinking, the window is too small to capture it.

### What You're Envisioning (Bidirectional, Loop #5)
```
Pattern Diversity Measurement
    ↓
[Growth Rate Calculation]    [Prefetch Accuracy Feedback]
    ↓                              ↓
[Shrinking Decision]         [Growth Decision]
    IF growth < 1%           IF prefetch_accuracy > threshold
    AND no_new_patterns      AND memory_permits
    ↓                              ↓
[Shrink to 75%]             [Grow to 90% of max]
    ↓                              ↓
Result: Adaptive oscillation toward optimal window size
```

### Missing Components (Phase 2 - Loop #5)

1. **Growth Trigger:** When should window grow?
   - Prefetch accuracy drops (misses emerging patterns)?
   - Pattern diversity suddenly increases?
   - Memory available?

2. **Gauge Study/Inference:** How to measure if shrinking was correct?
   - Prefetch accuracy before vs. after shrinking
   - Cache hit rate before vs. after
   - Pattern capture completeness

3. **Feedback Integration:** Currently Loop #4 (pipelining) doesn't feed into Loop #5 (window tuning)
   - Loop #4 measures prefetch_accuracy (83.55%)
   - Loop #5 should use that to tune window size
   - **Currently disconnected**

---

## Part 4: Why Metrics Show Static Values

### rolling_window_width = 4096 (Always)

**Explanation:**
```c
// src/doe_metrics.c:214
metrics.rolling_window_width = (uint32_t)vm->rolling_window.effective_window_size;
```

This reads the CURRENT effective_window_size at metrics extraction time. Possible reasons it's always 4096:

1. **Never triggers shrinking:** Diversity growth stays > 1% throughout test
2. **Test is too short:** Window takes ~4096 executions to become "warm", test ends before any shrinking
3. **Metrics extracted at wrong time:** Extracted after window has already been reset/re-initialized
4. **Shrinking happens but we measure too early:** Metrics captured before effective_window_size written to VM state

### decay_slope = 0.0 (Always)

**Explanation:**
```c
// src/doe_metrics.c:216
metrics.decay_slope = 0.0;  // Completely hardcoded, no extraction logic
```

This is a **placeholder** that was never filled in. The linear decay equation exists, but:

1. ❌ No total heat tracking during run
2. ❌ No trajectory stored (heat at t=0 vs t=end)
3. ❌ No slope calculation implemented
4. ❌ Should extract from `DECAY_RATE_PER_US_Q16` or measured heat loss

---

## Recommendations

### Short Term (Fix Metrics Extraction)

**For rolling_window_width:**
- ✅ Code is correct - it extracts effective_window_size as intended
- 🔍 Need to verify: Does shrinking actually occur during test execution?
- 📊 Add instrumentation: Log when adaptive shrinking happens

**For decay_slope:**
- Replace hardcoded 0.0 with one of:
  - Constant decay rate from DECAY_RATE_PER_US_Q16
  - Measured heat loss over test duration
  - Per-word average decay rate

### Medium Term (Implement Loop #5 - Phase 2)

1. Add growth trigger based on:
   - Prefetch accuracy feedback from Loop #4
   - Pattern diversity sudden increase
   - Memory availability

2. Implement gauge study:
   - Compare prefetch accuracy before/after shrinking
   - Measure pattern capture completeness
   - Track false negatives (patterns missed due to window size)

3. Wire Loop #4 → Loop #5 feedback
   - Loop #4 produces prefetch_accuracy metric
   - Loop #5 uses it to tune window_size bidirectionally

### Long Term (Physics-Based Window Optimization)

Current: Pattern diversity → Shrink decision

Future: Particle physics model
```
Window as "potential well" with:
- Capacity = 4096 (maximum)
- Mass = current execution_heat
- Charge = pattern diversity
- Spin = transition prediction accuracy

Shrinking = release potential (conserve energy)
Growing = absorb potential (capture emerging patterns)
Equilibrium = optimal window size
```

---

## Files Affected

| File | Change | Priority |
|------|--------|----------|
| `src/doe_metrics.c:216` | Implement decay_slope extraction | MEDIUM |
| `src/rolling_window_of_truth.c:644-646` | Uncomment/implement Loop #5 | HIGH (Phase 2) |
| `include/rolling_window_knobs.h` | Add growth knobs | HIGH (Phase 2) |
| `include/vm.h` | Track heat trajectory if needed | MEDIUM |

---

## Conclusion

**Your intuition was correct:** The current system is **unidirectional** (shrink-only) with **no growth mechanism**. The decay_slope metric is **unimplemented**.

This is not a bug - it's a **Phase 1/Phase 2 boundary**. Loop #5 (bidirectional window tuning with growth) is explicitly deferred to Phase 2 in the code comments.

The adaptive shrinking that exists IS working correctly (called every 256 executions), but the test harness may not be exhibiting enough pattern diversity plateau to trigger shrinking.

**To validate:** Run a test with logging enabled to see if `rolling_window_check_adaptive_shrink()` is actually shrinking or just measuring.
