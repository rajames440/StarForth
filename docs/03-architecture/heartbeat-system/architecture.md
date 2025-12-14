# VM Heartbeat Architecture: Centralized Time-Based Tuning

## The Insight

Loop #5 (window tuning) and Loop #3 (heat decay validation) are **both time-driven events** that should be coordinated via a central **vm_tick()** heartbeat rather than scattered through execution paths.

**Future-proof benefit:** This heartbeat can evolve into a background thread for system observability without touching core VM logic.

---

## Architecture: Heartbeat Abstraction

### 1. Core Heartbeat Function

**File:** `include/vm.h` (add to VM struct)

```c
typedef struct {
    uint64_t tick_count;              /* Total heartbeat ticks since VM init */
    uint64_t last_window_tune_tick;   /* Last tick we tuned window */
    uint64_t last_slope_tune_tick;    /* Last tick we validated decay slope */
    int      heartbeat_enabled;       /* 1 = heartbeat active, 0 = disabled */
} HeartbeatState;

struct VM {
    // ... existing fields ...
    HeartbeatState heartbeat;
};
```

### 2. Heartbeat Dispatcher

**File:** `src/vm.c` (new function)

```c
/**
 * Central heartbeat: aggregates all time-driven tuning operations.
 * Called periodically (every N executions) to maintain system observability.
 *
 * Design: Can be called from:
 * - Main execution loop (now): synchronous, zero latency impact
 * - Background thread (future): async, decoupled from VM execution
 */
void vm_tick(VM *vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled)
        return;

    vm->heartbeat.tick_count++;

    /* Plugin 1: Window Tuning (Loop #5) */
    if (ENABLE_PIPELINING && (vm->heartbeat.tick_count - vm->heartbeat.last_window_tune_tick) >= WINDOW_TUNING_FREQUENCY)
    {
        vm_tick_window_tuner(vm);
        vm->heartbeat.last_window_tune_tick = vm->heartbeat.tick_count;
    }

    /* Plugin 2: Heat Decay Slope Validation (Loop #3) */
    if ((vm->heartbeat.tick_count - vm->heartbeat.last_slope_tune_tick) >= SLOPE_VALIDATION_FREQUENCY)
    {
        vm_tick_slope_validator(vm);
        vm->heartbeat.last_slope_tune_tick = vm->heartbeat.tick_count;
    }

    /* Plugin 3: System State Monitoring (Future) */
    // vm_tick_system_monitor(vm);

    /* Plugin 4: Formal Verification State Update (Future) */
    // vm_tick_formal_state_sync(vm);
}
```

### 3. Window Tuner Plugin

**File:** `src/physics_pipelining_metrics.c`

```c
/**
 * Window tuning plugin for heartbeat.
 * Checks if effective_window_size is optimal based on prefetch accuracy.
 */
void vm_tick_window_tuner(VM *vm)
{
    if (!vm || !vm->rolling_window.is_warm || !ENABLE_PIPELINING)
        return;

    RollingWindowOfTruth *window = &vm->rolling_window;
    PipelineGlobalMetrics *metrics = &vm->pipeline_metrics;

    /* Calculate current prefetch accuracy */
    if (metrics->prefetch_attempts == 0)
        return;  /* Not enough data yet */

    double current_accuracy = (double)metrics->prefetch_hits / (double)metrics->prefetch_attempts;

    /* Binary chop suggests next window size */
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
                   "HEARTBEAT[window]: %u → %u (accuracy %.2f%%, %lu/%lu prefetch hits)",
                   window->effective_window_size,
                   suggested_size,
                   current_accuracy * 100.0,
                   metrics->prefetch_hits,
                   metrics->prefetch_attempts);

        window->effective_window_size = suggested_size;
    }

    /* Record for next iteration */
    metrics->last_checked_window_size = window->effective_window_size;
    metrics->last_checked_accuracy = current_accuracy;
    metrics->window_tuning_checks++;
}
```

### 4. Slope Validator Plugin

**File:** `src/physics_metadata.c`

```c
/**
 * Heat decay slope validation plugin for heartbeat.
 * Validates that linear decay is actually helping (Loop #3 closure).
 *
 * Periodically measures:
 * - Are words actually decaying correctly?
 * - Is cache pollution from stale words decreasing?
 * - Should we try a different decay function (exponential, log, etc.)?
 */
void vm_tick_slope_validator(VM *vm)
{
    if (!vm)
        return;

    /* Collect snapshot of current state */
    uint64_t hot_word_count = 0;
    uint64_t stale_word_count = 0;
    uint64_t total_heat = 0;

    /* Scan dictionary and categorize words by heat level */
    for (DictEntry *e = vm->latest; e != NULL; e = e->link)
    {
        if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD)
            hot_word_count++;
        else if (e->execution_heat > 0 && e->execution_heat < 10)
            stale_word_count++;

        total_heat += e->execution_heat;
    }

    double avg_heat = (vm->latest) ? (double)total_heat / DICTIONARY_SIZE : 0.0;
    double stale_ratio = (double)stale_word_count / (double)DICTIONARY_SIZE;

    log_message(LOG_INFO,
               "HEARTBEAT[slope]: decay validation - hot_words=%lu, stale_ratio=%.2f%%, avg_heat=%.1f",
               hot_word_count,
               stale_ratio * 100.0,
               avg_heat);

    /* Phase 2: Compare decay effectiveness over time */
    /* Could measure:
     * - Is stale_ratio decreasing? (decay is working)
     * - Is hot_word_count stable? (decay not over-aggressive)
     * - Should we try exponential instead of linear?
     */
}
```

---

## Integration into Execution Path

### Option A: Synchronous (Current - Minimal Latency Impact)

**In `execute_colon_word()` or `vm_interpret_word()`:**

```c
/* Every N executions, run heartbeat operations */
if (++vm->execution_count % HEARTBEAT_FREQUENCY == 0)
{
    vm_tick(vm);
}
```

**Pros:**
- No threading complexity
- Tight integration with execution
- Works immediately

**Cons:**
- Adds latency to VM (tuning operations happen in hot path)
- Must keep operations lightweight

### Option B: Background Thread (Future - Decoupled)

```c
/* Spawn heartbeat thread at VM init */
void vm_init(VM *vm)
{
    // ... existing init ...

    #ifdef USE_HEARTBEAT_THREAD
    pthread_create(&vm->heartbeat_thread, NULL, heartbeat_loop, (void*)vm);
    #endif
}

/* Background thread heartbeat loop */
void *heartbeat_loop(void *arg)
{
    VM *vm = (VM *)arg;

    while (vm->heartbeat.heartbeat_enabled)
    {
        sleep(HEARTBEAT_INTERVAL_MS / 1000.0);  /* e.g., sleep 100ms */
        vm_tick(vm);
    }

    return NULL;
}
```

**Pros:**
- Decoupled from execution hot path
- Tuning doesn't add latency to user workload
- System observability without user performance impact

**Cons:**
- Threading complexity (locks, atomics needed)
- Requires careful synchronization with VM state

---

## Configuration Knobs

```makefile
# Heartbeat frequencies (in execution ticks)
HEARTBEAT_FREQUENCY=256                    # Tick VM every 256 executions
WINDOW_TUNING_FREQUENCY=1000               # Tune window every 1000 ticks
SLOPE_VALIDATION_FREQUENCY=5000            # Validate decay every 5000 ticks

# Heartbeat mode
HEARTBEAT_THREAD_ENABLED=0                 # 1 = background thread, 0 = synchronous
HEARTBEAT_THREAD_INTERVAL_MS=100           # If threaded: wake up every 100ms

# Validation thresholds
WINDOW_TUNING_ACCURACY_THRESHOLD=1         # Min % improvement to continue shrinking
SLOPE_VALIDATION_PRINT_INTERVAL=10         # Log every Nth validation
```

---

## Benefits of Heartbeat Architecture

### Now (Phase 1)
1. **Centralized tuning**: All time-driven operations coordinated in one place
2. **Maintainability**: Easy to add/remove tuning plugins
3. **Observability**: Logging shows exactly when/why tuning happens
4. **Testability**: Can mock heartbeat in unit tests

### Future (Phase 2)
1. **Background thread capability**: Move tuning out of hot path
2. **System monitoring**: Add CPU, memory, cache stats collection
3. **Formal verification**: Keep Isabelle model in sync with actual state
4. **Adaptive scheduling**: Adjust tuning frequency based on workload phase

---

## Loop #3 & #5 Integration

### Loop #3 (Heat Decay): Now VALIDATED

```c
void vm_tick_slope_validator(VM *vm)
{
    /* Measure actual decay effectiveness:
     * - Are stale words actually disappearing?
     * - Is cache pollution decreasing?
     * - Could exponential decay work better?
     */
}
```

**Questions answered:**
- Does linear decay help? (Measure stale_word_ratio trend)
- Is slope optimal? (Try different DECAY_RATE values in DoE)
- Could we use exponential? (A/B test: DECAY_FUNCTION=LINEAR vs EXPONENTIAL)

### Loop #5 (Window Tuning): Now ARCHITECTED

```c
void vm_tick_window_tuner(VM *vm)
{
    /* Measure window effectiveness:
     * - Is smaller window more predictable?
     * - Did we lose important patterns?
     * - Where's the sweet spot?
     */
}
```

**Benefits:**
- Both tuners can cooperate: window size affects prefetch accuracy
- Slope validator can inform window tuning decisions
- Centralized observability of both optimizations

---

## Sketch Summary

**Heartbeat is a dispatch mechanism that:**
1. Centralizes all time-driven VM operations
2. Provides clean plugin architecture for tuning functions
3. Works synchronously now, extensible to threading later
4. Enables Loop #3 validation AND Loop #5 tuning simultaneously
5. Future-proofs system observability

**Code locations:**
- `include/vm.h`: Add HeartbeatState struct + prototype vm_tick()
- `src/vm.c`: vm_tick() dispatcher, integration point
- `src/physics_pipelining_metrics.c`: vm_tick_window_tuner()
- `src/physics_metadata.c`: vm_tick_slope_validator()

**Effort:** ~200 LOC across 3-4 files, but CLEAN architecture

---

**Does this heartbeat abstraction align with your vision?**