# Heartbeat Per-Tick Instrumentation Plan
## Minimal-Overhead Time-Series Capture

**Objective**: Capture 7 key metrics per heartbeat tick (~1ms interval) without impacting timing accuracy

**Strategy**: Lightweight circular buffer, minimal copying, decoupled from main heartbeat logic

---

## Architecture

### 1. HeartbeatTickSnapshot struct (NEW)
```c
// In include/vm.h, add after HeartbeatSnapshot:

typedef struct {
    uint32_t tick_number;           /* Sequential tick counter */
    uint64_t elapsed_ns;            /* Total elapsed since run start */
    uint64_t tick_interval_ns;      /* Actual tick interval from prior tick */

    /* === Core metrics === */
    uint32_t cache_hits_delta;      /* Cache hits this tick */
    uint32_t bucket_hits_delta;     /* Bucket hits this tick */
    uint32_t word_executions_delta; /* Words executed this tick */

    /* === Feedback signals === */
    uint64_t hot_word_count;        /* Words above heat threshold */
    double avg_word_heat;           /* Mean execution heat (Q48.16 ÷ 65536) */
    uint32_t window_width;          /* Current rolling window size */

    /* === Derived metrics === */
    uint32_t predicted_label_hits;  /* Successful context predictions */
    double estimated_jitter_ns;     /* Deviation from nominal tick */
} HeartbeatTickSnapshot;

#define HEARTBEAT_TICK_BUFFER_SIZE 100000  /* ~100K ticks = ~100 seconds at 1kHz */
```

### 2. Heartbeat context extension (IN vm.h)
```c
// In the Heartbeat struct (around line 230-250), add:

typedef struct {
    /* ... existing fields ... */

    /* === Per-tick instrumentation (Phase 2) === */
    HeartbeatTickSnapshot* tick_buffer;     /* Circular buffer of tick snapshots */
    uint32_t tick_buffer_size;              /* Allocated size */
    uint64_t tick_buffer_write_index;       /* Current write position (wraps) */
    uint64_t tick_count_total;              /* Total ticks since run start (monotonic) */

    /* === Reference points === */
    uint64_t run_start_ns;                  /* Monotonic time at run start */
    uint32_t tick_number_offset;            /* Tick counter for this run */
} Heartbeat;
```

### 3. Initialization (IN vm_init)
```c
// In vm_init(), after heartbeat struct init:

vm->heartbeat.tick_buffer_size = HEARTBEAT_TICK_BUFFER_SIZE;
vm->heartbeat.tick_buffer = (HeartbeatTickSnapshot*)calloc(
    HEARTBEAT_TICK_BUFFER_SIZE,
    sizeof(HeartbeatTickSnapshot)
);
vm->heartbeat.tick_buffer_write_index = 0;
vm->heartbeat.tick_count_total = 0;
vm->heartbeat.run_start_ns = sf_monotonic_ns();
vm->heartbeat.tick_number_offset = 0;
```

### 4. Cleanup (IN vm_cleanup)
```c
// In vm_cleanup():

if (vm->heartbeat.tick_buffer) {
    free(vm->heartbeat.tick_buffer);
    vm->heartbeat.tick_buffer = NULL;
}
```

### 5. Capture function (NEW, in vm.c)
```c
static void heartbeat_capture_tick_snapshot(VM *vm)
{
    if (!vm || !vm->heartbeat.tick_buffer || !vm->heartbeat.heartbeat_enabled)
        return;

    /* Circular buffer write position */
    uint32_t buf_idx = vm->heartbeat.tick_count_total % vm->heartbeat.tick_buffer_size;
    HeartbeatTickSnapshot *snap = &vm->heartbeat.tick_buffer[buf_idx];

    /* === Capture timestamp === */
    uint64_t now_ns = sf_monotonic_ns();
    snap->elapsed_ns = now_ns - vm->heartbeat.run_start_ns;
    snap->tick_number = vm->heartbeat.tick_count_total;

    /* Compute tick interval from prior tick */
    if (vm->heartbeat.tick_count_total > 0) {
        HeartbeatTickSnapshot *prior_snap =
            &vm->heartbeat.tick_buffer[(buf_idx - 1 + vm->heartbeat.tick_buffer_size)
                                       % vm->heartbeat.tick_buffer_size];
        snap->tick_interval_ns = snap->elapsed_ns - prior_snap->elapsed_ns;
    } else {
        snap->tick_interval_ns = 0;
    }

    /* === Capture delta metrics === */
    /* These need to be tracked by main VM loop (instrumentation step 2) */
    /* For now, extract from accessible VM state */
    snap->cache_hits_delta = 0;      /* TODO: track in vm_tick() */
    snap->bucket_hits_delta = 0;     /* TODO: track in vm_tick() */
    snap->word_executions_delta = 0; /* TODO: track in vm_tick() */

    /* === Capture state variables === */
    snap->hot_word_count = vm->heartbeat.snapshots[0].hot_word_count;

    /* Convert decay slope from Q48.16 to double */
    snap->avg_word_heat = (double)vm->heartbeat.snapshots[0].total_heat / 65536.0;
    snap->window_width = vm->heartbeat.snapshots[0].window_width;

    /* === Derived metrics === */
    snap->predicted_label_hits = 0;  /* TODO: track context prediction success */
    snap->estimated_jitter_ns = 0;   /* TODO: compute as deviation from HEARTBEAT_TICK_NS */

    /* Increment tick counter */
    vm->heartbeat.tick_count_total++;
}
```

### 6. Injection point (IN vm_heartbeat_run_cycle)
```c
// Modify vm_heartbeat_run_cycle() (around line 667):

static void vm_heartbeat_run_cycle(VM *vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled)
        return;

    vm_tick(vm);
    vm_tick_apply_background_decay(vm, sf_monotonic_ns());
    sf_mutex_lock(&vm->tuning_lock);
    rolling_window_service(&vm->rolling_window);
    sf_mutex_unlock(&vm->tuning_lock);
    heartbeat_publish_snapshot(vm);

    /* === NEW: Capture per-tick instrumentation === */
    heartbeat_capture_tick_snapshot(vm);
}
```

---

## Phase 2: Track Delta Metrics

To get accurate cache/bucket/execution deltas, we need to track counts within `vm_tick()`:

```c
// In vm_tick() (start of function):
uint64_t cache_hits_start = vm->cache_hit_count;    /* Existing field */
uint64_t bucket_hits_start = vm->bucket_hit_count;  /* New field to add */
uint32_t word_exec_start = vm->total_words_executed; /* New field to add */

// At end of vm_tick():
snap->cache_hits_delta = vm->cache_hit_count - cache_hits_start;
snap->bucket_hits_delta = vm->bucket_hit_count - bucket_hits_start;
snap->word_executions_delta = vm->total_words_executed - word_exec_start;
```

**NOTE**: This requires adding 2-3 new fields to the VM struct, but they're just counters (minimal memory overhead).

---

## Phase 3: Extract Time-Series to CSV

After run completes (in run_doe_experiment or equivalent):

```c
void heartbeat_export_csv(VM *vm, FILE *out, const char *config_name, uint32_t run_number)
{
    if (!vm || !vm->heartbeat.tick_buffer || !out)
        return;

    /* Write header */
    fprintf(out, "run_id,config,tick_number,elapsed_ns,tick_interval_ns,"
                 "cache_hits_delta,bucket_hits_delta,word_executions_delta,"
                 "hot_word_count,avg_word_heat,window_width,predicted_hits,jitter_ns\n");

    /* Iterate circular buffer in order */
    uint64_t total_ticks = vm->heartbeat.tick_count_total;
    uint32_t buf_size = vm->heartbeat.tick_buffer_size;
    uint32_t buf_start = (total_ticks >= buf_size)
                         ? (total_ticks % buf_size)
                         : 0;

    for (uint64_t i = 0; i < total_ticks && i < buf_size; i++) {
        uint32_t buf_idx = (buf_start + i) % buf_size;
        HeartbeatTickSnapshot *snap = &vm->heartbeat.tick_buffer[buf_idx];

        fprintf(out, "%u,%s,%u,%lu,%lu,%u,%u,%u,%lu,%.2f,%u,%u,%lu\n",
                run_number, config_name, snap->tick_number, snap->elapsed_ns,
                snap->tick_interval_ns, snap->cache_hits_delta, snap->bucket_hits_delta,
                snap->word_executions_delta, snap->hot_word_count, snap->avg_word_heat,
                snap->window_width, snap->predicted_label_hits,
                (uint64_t)snap->estimated_jitter_ns);
    }
}
```

---

## Data Collection Strategy

### Per-Run Output
Two CSV files per run:
1. **experiment_results.csv** - Existing aggregate metrics (1 row per run)
2. **heartbeat_timeseries_{run_id}.csv** - New per-tick metrics (100K rows per run)

### Storage Consideration
```
750 runs × 100K ticks/run = 75 million CSV rows
~1KB per row = ~75 GB total

MITIGATIONS:
- Option A: Write only first/last 10K ticks + random sample
  Result: ~10M rows, ~10 GB (compressed: ~500 MB)
- Option B: Every 10th tick (10K rows per run)
  Result: ~7.5M rows, ~7.5 GB
- Option C: Every tick, but use binary format instead of CSV
  Result: ~2-3 GB uncompressed
```

**Recommendation**: Use Option B (every 10th tick) for Phase 2 prototyping, can increase density later.

---

## Implementation Checklist

- [ ] Add `HeartbeatTickSnapshot` struct to `include/vm.h`
- [ ] Extend `Heartbeat` struct with tick buffer fields
- [ ] Add buffer allocation in `vm_init()`
- [ ] Add buffer cleanup in `vm_cleanup()`
- [ ] Implement `heartbeat_capture_tick_snapshot()` in `src/vm.c`
- [ ] Inject call into `vm_heartbeat_run_cycle()`
- [ ] Add tracking fields to VM struct (cache_hits, bucket_hits, word_executions)
- [ ] Update delta calculation in `vm_tick()` (mark start/end counters)
- [ ] Implement `heartbeat_export_csv()` for extraction
- [ ] Modify `run_doe_experiment()` to call `heartbeat_export_csv()` per run
- [ ] Test with abbreviated run (10 ticks, verify CSV output)
- [ ] Verify circular buffer wrapping with 100+ ticks
- [ ] Verify no timing regression from instrumentation

---

## Overhead Analysis

Instrumentation cost per tick:
- **7 field assignments**: ~10 ns
- **1 modulo operation** (circular buffer): ~5 ns
- **1 monotonic clock read**: ~100 ns
- **Total**: ~115 ns per tick

vs. HEARTBEAT_TICK_NS = 1,000,000 ns (1 ms)

**Overhead**: 115 / 1,000,000 = 0.0115% (negligible)

---

## Next Steps

1. Implement per-tick snapshot capture infrastructure
2. Test with abbreviated experiment (1 config, 10 runs, write every 10th tick)
3. Verify CSV output and circular buffer logic
4. Implement RunStabilityFingerprint computation on exported CSV
5. Aggregate across 150+ runs per config
6. Compute ConfigurationFingerprint and golden config

---

**Captain, ready to build. This keeps the heartbeat timing clean while capturing full dynamics.**