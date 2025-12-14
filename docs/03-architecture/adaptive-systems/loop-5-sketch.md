# Loop #5: Context-Aware Window Tuning - Design Sketch

## The Challenge

**Current state:** Window size shrinks based on pattern diversity, but we don't know if shrinking improves or hurts **pipelining prediction accuracy**.

**Goal:** Use actual prefetch hit rate to guide window size tuning via binary chop search.

---

## What Loop #5 Should Do

### High-Level Flow

1. **Measure**: Track global pipelining metrics across ALL words
   - Total speculative prefetch attempts
   - Successful prefetch hits
   - Current accuracy = hits / attempts

2. **Trigger**: Periodically (e.g., every 1000 executions when warm)
   - Calculate prediction accuracy at current effective_window_size
   - Call binary chop to suggest next size to try

3. **Adapt**: Shrink or grow effective_window_size based on accuracy trend
   - If smaller window → better accuracy: keep shrinking
   - If smaller window → worse accuracy: grow back up
   - Converge to sweet spot

4. **Measure**: Record the accuracy at each window size tried
   - Build history of (window_size → accuracy) pairs
   - Use this history to accelerate convergence

---

## Architecture Needed

### 1. VM-Level Metrics (New)

**File:** `include/vm.h` (add to VM struct)

```c
typedef struct {
    uint64_t prefetch_attempts;      /* Total speculative prefetch calls */
    uint64_t prefetch_hits;          /* Successful hits (word was looked up next) */
    uint64_t window_tuning_checks;   /* How many times we've checked window size */
    uint32_t last_checked_window_size;
    double   last_checked_accuracy;
    uint32_t suggested_next_size;    /* What binary chop recommends trying */
} PipelineGlobalMetrics;
```

**Add to VM struct:**
```c
struct VM {
    // ... existing fields ...
    PipelineGlobalMetrics pipeline_metrics;  /* NEW: aggregated metrics */
};
```

### 2. Update Prefetch Feedback (in vm.c)

**When speculative prefetch succeeds:**
```c
// In execute_colon_word(), after promotion:
if (spec_entry && vm->hotwords_cache && ENABLE_HOTWORDS_CACHE)
{
    spec_entry->execution_heat = HOTWORDS_EXECUTION_HEAT_THRESHOLD + 1;
    hotwords_cache_promote(vm->hotwords_cache, spec_entry);

    // NEW: Record the prefetch attempt globally
    vm->pipeline_metrics.prefetch_attempts++;
    prev_word->transition_metrics->prefetch_attempts++;  // Per-word too
}
```

**When word is actually looked up (check if it was speculated):**
```c
// In vm_interpret_word(), during dictionary lookup:
if (entry && ENABLE_PIPELINING && vm->hotwords_cache)
{
    // NEW: Check if this word was in speculated cache (dirty bit or timestamp)
    if (word_was_speculatively_promoted_recently(entry))
    {
        vm->pipeline_metrics.prefetch_hits++;        // Global metric
        // per_word_metrics->prefetch_hits++;          // Already done in prefetch logic
    }
}
```

### 3. Window Tuning Decision Loop

**New function:** `physics_pipelining_metrics.c`

```c
/**
 * Suggest window size via binary chop based on prefetch accuracy trend.
 * Called periodically (every 1000 executions when pipelining is enabled).
 */
uint32_t loop_5_binary_chop_suggest_window(
    const PipelineGlobalMetrics *global,
    uint32_t current_window_size,
    uint32_t min_window,
    uint32_t max_window)
{
    if (global->prefetch_attempts == 0)
        return current_window_size;  /* Not enough data yet */

    double current_accuracy = (double)global->prefetch_hits / (double)global->prefetch_attempts;

    /* Binary chop: try smaller window first (aggressive shrinking)
     * If accuracy drops, back off and try larger windows */

    if (global->window_tuning_checks == 0)
    {
        /* First check: try shrinking by 25% */
        return (current_window_size * 75) / 100;
    }

    /* Compare current accuracy to last check */
    double accuracy_delta = current_accuracy - global->last_checked_accuracy;

    if (accuracy_delta > 0.01)  /* Improvement threshold: 1% */
    {
        /* Accuracy improved! Try shrinking more */
        uint32_t smaller = (current_window_size * 75) / 100;
        return (smaller > min_window) ? smaller : min_window;
    }
    else if (accuracy_delta < -0.01)
    {
        /* Accuracy degraded. Try growing instead */
        uint32_t larger = (current_window_size * 133) / 100;  /* Grow by ~33% */
        return (larger < max_window) ? larger : max_window;
    }
    else
    {
        /* Plateau: stick with current size */
        return current_window_size;
    }
}
```

### 4. Integration Point

**In rolling_window_of_truth.c: `rolling_window_check_adaptive_shrink()`**

```c
void rolling_window_check_adaptive_shrink(RollingWindowOfTruth* window)
{
    // ... existing adaptive shrinking code ...

    /* Loop #5: Binary chop window tuning (Phase 2 IMPLEMENTATION) */
    if (ENABLE_PIPELINING && window->is_warm)
    {
        window->adaptive_check_count++;

        /* Check every 1000 executions for tuning opportunity */
        if (window->adaptive_check_count % 1000 == 0)
        {
            PipelineGlobalMetrics *metrics = &vm->pipeline_metrics;

            /* Calculate current accuracy */
            double current_accuracy = (metrics->prefetch_attempts > 0)
                ? (double)metrics->prefetch_hits / (double)metrics->prefetch_attempts
                : 0.0;

            /* Ask binary chop for next window size to try */
            uint32_t suggested_size = loop_5_binary_chop_suggest_window(
                metrics,
                window->effective_window_size,
                ADAPTIVE_MIN_WINDOW_SIZE,
                ROLLING_WINDOW_SIZE
            );

            /* Apply if different */
            if (suggested_size != window->effective_window_size)
            {
                log_message(LOG_INFO,
                           "Loop #5: Binary chop tuning: %u → %u (accuracy %.2f%%, %lu/%lu hits)",
                           window->effective_window_size,
                           suggested_size,
                           current_accuracy * 100.0,
                           metrics->prefetch_hits,
                           metrics->prefetch_attempts);

                window->effective_window_size = suggested_size;
            }

            /* Record this check for next iteration */
            metrics->last_checked_window_size = window->effective_window_size;
            metrics->last_checked_accuracy = current_accuracy;
            metrics->window_tuning_checks++;
        }
    }
}
```

---

## Key Design Decisions

### A. Accuracy Measurement
- **Count**: Successful speculative promotions that were actually used
- **Challenge**: How to know if lookup was "predicted" vs "natural"?
  - Option 1: Mark speculatively-promoted entries with a timestamp/flag
  - Option 2: Track word IDs predicted and check if they match next lookup
  - Option 3: Use per-word prefetch metrics already collected

### B. Tuning Frequency
- **Every 1000 executions** (when window is warm)
- Could be tunable: `make WINDOW_TUNING_FREQUENCY=1000`

### C. Search Strategy
- **Greedy shrinking first**: Try smaller windows aggressively
- **Backoff on degradation**: If accuracy drops, grow back up
- **Convergence**: Stop when oscillating around sweet spot

### D. Knobs Needed (Makefile)
```bash
make ENABLE_PIPELINING=1              # Must be enabled for Loop #5
make WINDOW_TUNING_FREQUENCY=1000     # How often to check
make WINDOW_TUNING_ACCURACY_THRESHOLD=1  # % improvement to continue shrinking
```

---

## Validation Approach

Once Loop #5 is wired, DoE would test:
- **A_BASELINE**: No optimizations
- **A_B_CACHE**: Cache only
- **A_C_FULL**: Window tuning with pipelining only
- **A_B_C_FULL**: Cache + window tuning + pipelining

**Metrics to collect:**
- Final effective_window_size (what did it converge to?)
- Final prefetch accuracy (%)
- Throughput improvement vs baseline

---

## Risks & Unknowns

1. **Overhead**: Checking accuracy every 1000 executions adds latency
2. **Noise**: If accuracy is noisy, binary chop might oscillate
3. **Workload-dependent**: Optimal window size might vary by workload
4. **False positives**: Speculative lookup might not correlate with prediction

---

## Sketch Summary

**Loop #5 would:**
1. Track global prefetch accuracy (hits/attempts)
2. Every 1000 executions, ask binary chop: "Is current window size optimal?"
3. Binary chop suggests shrinking, holding, or growing based on accuracy trend
4. Apply suggestion and record for next iteration
5. Over time, converge to window size that maximizes prediction accuracy

**Code locations if implemented:**
- VM struct: Add PipelineGlobalMetrics field
- vm.c: Update global metrics on prefetch attempts/hits
- physics_pipelining_metrics.c: `loop_5_binary_chop_suggest_window()`
- rolling_window_of_truth.c: Call binary chop from adaptive shrink check

**Effort**: ~150 LOC across 3-4 files

---

**Question for user:** Does this architecture align with what you had in mind for Loop #5?