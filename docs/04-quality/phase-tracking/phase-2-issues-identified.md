# Phase 2 Issues: Shrink/Grow, Decay Slope, Fixed-Point Violations

**Status:** Root cause analysis in progress
**Severity:** CRITICAL - Physics calculations violating hard rules
**Date:** 2025-11-08

---

## Critical Issue #1: Float/Double Violations (Hard Rule)

**Rule:** Only Q48.16 fixed-point math in StarForth physics. No doubles/floats. **ZERO exceptions.**

### Violations Found

#### File: src/doe_metrics.c (Metrics Extraction)
| Line | Code | Violation | Should Be |
|------|------|-----------|-----------|
| 88 | `float mhz = 0.0f;` | CPU freq to float | Q48.16 fixed-point |
| 134 | `100.0 * (double)cache_hits / (double)total_lookups` | % calculation as double | Q48.16 fixed-point ratio |
| 142 | `100.0 * (double)bucket_hits / (double)total_lookups` | % calculation as double | Q48.16 fixed-point ratio |
| 206-207 | `100.0 * (double)prefetch_hits / (double)prefetch_attempts` | % calculation as double | Q48.16 fixed-point ratio |
| 237 | `(double)stale_word_count / (double)word_count` | Ratio as double | Q48.16 fixed-point ratio |
| 238 | `(double)total_heat / (double)word_count` | Ratio as double | Q48.16 fixed-point ratio |
| 250-251 | `100.0 * (double)prefetch_hits / (double)prefetch_attempts` | % calculation as double | Q48.16 fixed-point ratio |

#### File: src/rolling_window_of_truth.c (Adaptive Shrinking)
| Line | Code | Violation | Should Be |
|------|------|-----------|-----------|
| 525 | `double capture_rate = ...` | Ratio as double | Q48.16 fixed-point ratio |
| 614-616 | `double growth_rate = (double)diversity_delta / (double)last_diversity` | Growth calculation as double | Q48.16 fixed-point ratio |
| 619 | `double threshold = (double)ADAPTIVE_GROWTH_THRESHOLD / 100.0` | Threshold as double | Q48.16 fixed-point |

#### File: include/doe_metrics.h (Struct Definition)
| Line | Field | Type | Should Be |
|------|-------|------|-----------|
| 56 | `cache_hit_percent` | `double` | `uint64_t (Q48.16)` |
| 58 | `bucket_hit_percent` | `double` | `uint64_t (Q48.16)` |
| 69 | `context_accuracy_percent` | `double` | `uint64_t (Q48.16)` |
| 72 | `window_diversity_percent` | `double` | `uint64_t (Q48.16)` |
| 75 | `decay_slope` | `double` | `uint64_t (Q48.16)` - **UNINITIALIZED** |
| 79 | `stale_word_ratio` | `double` | `uint64_t (Q48.16)` |
| 80 | `avg_word_heat` | `double` | `uint64_t (Q48.16)` |

**Impact:** Physics calculations contaminated with IEEE 754 floating-point. This affects:
- Shrinking decision accuracy
- Metrics comparability across runs
- Formal verification (Isabelle model doesn't include floating-point)

---

## Critical Issue #2: Decay Slope = 0.0 (Uninitialized)

**Location:** src/doe_metrics.c:216

```c
metrics.decay_slope = 0.0;  // ❌ Hardcoded placeholder
```

### What Should It Be?

**Your statement:** "I kind of settled on `starting` at 2:1 for the slope and I don't know why that going ignored."

**Interpretation needed:**
- Does 2:1 mean `heat_at_start : heat_at_end = 2:1` (heat halved)?
- Should it be stored as Q48.16 ratio? (2:1 = 131072 in Q48.16 = 2 << 16)
- Or is it `decay_slope = (heat_start - heat_end) / time_elapsed` (rate)?

### Current Logic (Non-existent)

No tracking of:
1. Total dictionary heat at test START
2. Total dictionary heat at test END
3. Time elapsed during test
4. Calculation of slope as (heat_start - heat_end) / elapsed_time

### What Exists

```c
// In physics_metadata.c:164-203
void physics_metadata_apply_linear_decay(DictEntry *entry, uint64_t elapsed_ns)
{
    // decay_amount = (elapsed_us * DECAY_RATE_PER_US_Q16) >> 16
    // Individual words decay correctly, but aggregate isn't tracked
}
```

**Problem:** Individual word decay works, but we never capture:
- Dictionary heat snapshot at test start
- Dictionary heat snapshot at test end
- Calculate the actual observed slope

---

## Critical Issue #3: Adaptive Window Not Changing (Root Cause Analysis)

### Hypothesis 1: Pattern Diversity Never Plateaus ❌ (You don't buy this)

**Current Logic:**
```c
growth_rate = (current_diversity - last_diversity) / last_diversity
// Threshold = 1% (ADAPTIVE_GROWTH_THRESHOLD = 1)
// If growth_rate < 1% → shrink
```

**Your Challenge:** "Pattern diversity never plateaus - I don't buy it. Something isn't instrumented correctly."

**Likely Root Cause:** The diversity measurement itself may be wrong.

Looking at `rolling_window_measure_diversity()`:
```c
uint64_t unique_transitions = 0;
for each word in window:
    if (current != prev_word):
        unique_transitions++
return unique_transitions
```

This counts:
```
Execution: A B A C A D A E A F
Transitions: A→B, B→A, A→C, C→A, A→D, D→A, A→E, E→A, A→F = 9 unique transitions
```

**But:** If execution is `A B C D E F ... Z AA AB AC ...` (new patterns keep arriving), diversity grows forever.

**Question:** Is the test harness producing bounded or unbounded word sequences?

### Missing Instrumentation

Need to log EVERY 256 executions:
```
[Check #1]
  total_executions = 512
  current_diversity = 847
  last_diversity = 0 (baseline)
  growth_rate = undefined (first check)

[Check #2]
  total_executions = 768
  current_diversity = 852
  last_diversity = 847
  growth_rate = (852-847)/847 = 0.59% < 1% threshold
  → SHRINK: 4096 → 3072

[Check #3]
  total_executions = 1024
  current_diversity = 856
  last_diversity = 852
  growth_rate = (856-852)/852 = 0.47% < 1% threshold
  → SHRINK: 3072 → 2304
```

**Current State:** We have NO logging to see this sequence. `effective_window_size` always reads as 4096 at metrics extraction time.

---

## Critical Issue #4: Bidirectional Feedback (Shrink AND Grow)

**Architecture:** Currently UNIDIRECTIONAL
```
Pattern Diversity Measurement
    ↓
Growth Rate < 1%?
    ↓ YES
  SHRINK
    ↓ NO
  Do Nothing (never grow)
```

**Your Question:** "Does it shrink AND grow? Is there a driving metric and input into maybe a gauge study and inference?"

**Answer:** Currently NO. To make it bidirectional:

```
Loop #4 (Prefetch Accuracy) → Feedback Signal
    ↓
Loop #5 (Window Tuner)
    ↓
Measure: prefetch_accuracy at current window_size
    ↓
Binary Chop Logic:
  IF accuracy improved:
    → Try shrinking more
  IF accuracy degraded:
    → Grow back up
  IF stable:
    → Hold current size
```

**Missing:** Loop #4 → Loop #5 wiring. Prefetch accuracy isn't feeding into window size decisions.

---

## Summary of Required Fixes

### IMMEDIATE (Blocking Phase 2 Validation)

1. **Add Instrumentation** to `rolling_window_check_adaptive_shrink()`
   - Log diversity measurements every check
   - Log growth_rate calculations
   - Log shrink decisions and results
   - **Capture:** When/why effective_window_size changes

2. **Clarify Decay Slope**
   - What is "2:1 starting slope"? (ratio? rate? direction?)
   - How should it be calculated? (total heat delta? per-word average?)
   - How should it be stored? (Q48.16?)

3. **Fix Float/Double Violations**
   - Replace all `double` calculations with Q48.16
   - Update doe_metrics.h struct to use `uint64_t` instead of `double`
   - Update doe_metrics.c to use fixed-point arithmetic

4. **Implement Decay Slope Extraction**
   - Capture dictionary heat at test start
   - Capture dictionary heat at test end
   - Calculate slope: (heat_start - heat_end) / elapsed_time (in Q48.16)
   - Store in metrics

### FOLLOW-UP (Phase 2 Architecture)

5. **Wire Loop #4 → Loop #5 Feedback**
   - Prefetch accuracy as input to window tuning
   - Binary chop to suggest grow/shrink/hold
   - Bidirectional adaptation

6. **Implement Gauge Study**
   - Track (window_size → accuracy) pairs
   - Measure if shrinking actually improves prediction
   - Adjust knobs based on observed trade-offs

---

## Questions for User

1. **Decay Slope:** What does "2:1 starting slope" mean?
   - Is it `initial_heat : final_heat` ratio?
   - Is it a decay rate (heat units per second)?
   - Should it be stored as Q48.16?

2. **Adaptive Shrinking:** Should we add logging to see if shrinking is actually happening?
   - Would help diagnose why metrics show 4096 always
   - Can pinpoint if problem is measurement, calculation, or no shrinking

3. **Bidirectional Growth:** Should Loop #5 be wired now or after Phase 2 design validation?
   - Need binary chop + prefetch feedback loop
   - Or focus on fixing shrinking first?

---

## Validation Checklist (Once Fixed)

- [ ] All physics math uses Q48.16 only (no doubles/floats)
- [ ] doe_metrics.h struct updated to uint64_t
- [ ] Instrumentation logging shows shrinking happening
- [ ] decay_slope extracted and calculated correctly
- [ ] Test-03+ runs show varying rolling_window_width in metrics
- [ ] test-03+ runs show non-zero decay_slope
- [ ] Bidirectional tuning (grow + shrink) working
- [ ] Gauge study metrics captured
