# Pipelining Feedback Loop: WIRED BUT NOT UTILIZED (Critical Issue)

## The Problem

The pipelining infrastructure has **asymmetric wiring**: data collection hooks exist, but the decision functions are never called. This creates a broken promise.

### Loop #4: Pipelining Transition Metrics

**Data Collection (WIRED):**
```c
// src/vm.c:545-548 (EXECUTES when ENABLE_PIPELINING=1)
if (prev_word && prev_word->transition_metrics && ENABLE_PIPELINING)
{
    transition_metrics_record(prev_word->transition_metrics, word_id, DICTIONARY_SIZE);
}
```

**Decision Logic (NOT UTILIZED):**
```c
// src/physics_pipelining_metrics.c:164 (DEFINED but NEVER CALLED)
int transition_metrics_should_speculate(const WordTransitionMetrics *metrics, uint32_t target_word_id) {
    if (!metrics || !metrics->transition_heat) return 0;
    if (metrics->total_transitions < MIN_SAMPLES_FOR_SPECULATION) return 0;

    int64_t prob_q48 = transition_metrics_get_probability_q48(metrics, target_word_id);
    if (prob_q48 < SPECULATION_THRESHOLD_Q48) return 0;

    return 1;  /* Should speculate */
}
```

**Consequence:**
- When `ENABLE_PIPELINING=1`: Metrics are collected but **never examined**
- The decision function exists in the header (looks active to users)
- Setting the flag gives false impression of optimization (it's not)

---

## The Root Cause

Phase 2 development was **partially started but never completed**:

1. **Infrastructure wired (Phase 1 work):**
   - Data collection hooks added to vm.c
   - Metrics structures allocated per word
   - Transition counting functions work

2. **Decision logic stubbed (Phase 2 design):**
   - `transition_metrics_should_speculate()` exists
   - Comments note "TODO: Phase 4 - Add ROI check here"
   - Function is exportable but never invoked

3. **Integration missing (Phase 2 work not done):**
   - No caller for `transition_metrics_should_speculate()`
   - No prefetch/speculation action taken when it returns 1
   - No word reordering or predictive loading in execution path

---

## Loop #5: Context Window Tuning (Also Broken)

**The Binary Chop Stub:**
```c
// src/physics_pipelining_metrics.c:340 (DEFINED but NEVER CALLED)
uint32_t transition_metrics_binary_chop_suggest_window(...) {
    /* Phase 1: Stub - just return doubled window size per user's request */
    /* Phase 2 will implement actual binary chop search ... */

    if (current_window >= 8) return 1;
    if (current_window == 1) return 2;
    if (current_window == 2) return 4;
    if (current_window == 4) return 8;
    return current_window;
}
```

**Problem:**
- Comment admits it's a stub
- Never called from anywhere
- Not integrated with effective_window_size tuning

---

## What to Do RIGHT NOW

### OPTION A: Disable Until Phase 2 Ready
Remove the half-wired data collection until Phase 2 implementation is ready:

**File:** `src/vm.c:545-548`

```c
/* DISABLED: Phase 2 pipelining not yet utilized (see PIPELINING_WIRED_NOT_UTILIZED.md) */
// if (prev_word && prev_word->transition_metrics && ENABLE_PIPELINING)
// {
//     transition_metrics_record(prev_word->transition_metrics, word_id, DICTIONARY_SIZE);
// }
```

**And:** `src/vm.c:578-581`

```c
/* DISABLED: Phase 2 pipelining not yet utilized */
// if (ENABLE_PIPELINING && w)
// {
//     prev_word = w;
// }
```

**Effect:** When users set `ENABLE_PIPELINING=1`, nothing happens (honest)

**Rationale:**
- Stops creating false impression that pipelining works
- Prevents wasted memory allocating transition_metrics per word
- Removes confusing code in hot execution path

---

### OPTION B: Wire In Decision Logic (Phase 2 Work)
Actually implement the optimization (bigger effort):

1. After collecting metrics, **call the decision function** when should_speculate() returns true
2. **Implement prefetch action**: If speculation is favorable, load word into instruction cache/prefetch buffer
3. **Wire binary chop**: Call suggest_window() and actually update effective_window_size
4. **Measure impact**: Add DoE knob for ENABLE_PIPELINING=1 to test speedup vs baseline

**This is Phase 2 work** - significant implementation

---

### OPTION C: Delete It
If pipelining is not planned for StarForth, remove the entire infrastructure:

1. Delete `src/physics_pipelining_metrics.c`
2. Remove declarations from headers
3. Remove data collection hooks from vm.c
4. Remove `ENABLE_PIPELINING` flag

**Rationale:** Clean up unneeded infrastructure

---

## Current Status in Code

| Item | State | Note |
|------|-------|------|
| Data collection hooks | Wired (vm.c:545, 578) | Only active if ENABLE_PIPELINING=1 |
| Metrics structures | Allocated per word | Wastes memory when disabled |
| Decision function | Defined (line 164) | Never called |
| Exported in header | Yes | Users think it's available |
| Build flag | ENABLE_PIPELINING ?= 0 | Disabled by default |
| DoE configuration | No knob | Phase 1 DoE doesn't test it |
| Documentation | Admits it's stub | "Phase 2 will implement..." |

---

## Recommendation

**OPTION A (Disable)** is the right move:

1. **Honest**: Stop pretending pipelining works
2. **Low-risk**: Removes dead code from hot path
3. **Future-proof**: When Phase 2 work begins, uncomment and properly wire
4. **Unblocks**: Clears confusion about what's actually optimizing

**Then document:**
- Why it's disabled
- What Phase 2 work looks like
- How to re-enable when ready

---

**User request:** These loops must be attended to RIGHT NOW
**Diagnosis:** Both are Phase 2 stubs left partially wired
**Recommendation:** Disable until Phase 2 work is ready
**Files to change:** src/vm.c (lines 545-548, 578-581)