# Phase 2 Implementation Checklist
## Quick Reference for Code Changes

---

## 📋 Struct Definitions

### Step 1: Add HeartbeatTickSnapshot (include/vm.h)
```c
/* After line ~209 (after HeartbeatSnapshot), add: */
typedef struct {
    uint32_t tick_number;           /* Sequential tick counter */
    uint64_t elapsed_ns;            /* Total elapsed since run start */
    uint64_t tick_interval_ns;      /* Actual tick interval from prior tick */

    uint32_t cache_hits_delta;      /* Cache hits this tick */
    uint32_t bucket_hits_delta;     /* Bucket hits this tick */
    uint32_t word_executions_delta; /* Words executed this tick */

    uint64_t hot_word_count;        /* Words above heat threshold */
    double avg_word_heat;           /* Mean execution heat */
    uint32_t window_width;          /* Current rolling window size */

    uint32_t predicted_label_hits;  /* Successful context predictions */
    double estimated_jitter_ns;     /* Deviation from nominal tick */
} HeartbeatTickSnapshot;

#define HEARTBEAT_TICK_BUFFER_SIZE 100000
```

### Step 2: Extend Heartbeat struct (include/vm.h)
```c
/* In the Heartbeat typedef, after existing fields, add: */

    /* === Per-tick instrumentation (Phase 2) === */
    HeartbeatTickSnapshot* tick_buffer;     /* Circular buffer */
    uint32_t tick_buffer_size;              /* Size (100K) */
    uint64_t tick_buffer_write_index;       /* Write position */
    uint64_t tick_count_total;              /* Total ticks (monotonic) */
    uint64_t run_start_ns;                  /* Run start timestamp */
    uint32_t tick_number_offset;            /* Tick counter offset */
```

### Step 3: Add tracking counters to VM struct (include/vm.h)
```c
/* In the VM typedef, add: */

    /* === Per-tick delta tracking (Phase 2) === */
    uint64_t cache_hits_last_tick;          /* Snapshot of cache hits at tick start */
    uint64_t bucket_hits_last_tick;         /* Snapshot of bucket hits at tick start */
    uint32_t word_executions_last_tick;     /* Snapshot of executions at tick start */
```

---

## 🔧 Implementation Functions

### Step 4: Heartbeat buffer initialization (src/vm.c, in vm_init)
```c
/* In vm_init(), after heartbeat struct initialization (~line 520-540), add: */

vm->heartbeat.tick_buffer_size = HEARTBEAT_TICK_BUFFER_SIZE;
vm->heartbeat.tick_buffer = (HeartbeatTickSnapshot*)calloc(
    HEARTBEAT_TICK_BUFFER_SIZE,
    sizeof(HeartbeatTickSnapshot)
);
if (!vm->heartbeat.tick_buffer) {
    /* Handle allocation failure */
    return 0;
}
vm->heartbeat.tick_buffer_write_index = 0;
vm->heartbeat.tick_count_total = 0;
vm->heartbeat.run_start_ns = sf_monotonic_ns();
vm->heartbeat.tick_number_offset = 0;

/* Initialize tracking snapshots */
vm->cache_hits_last_tick = vm->cache_hits;
vm->bucket_hits_last_tick = vm->bucket_hit_count;  /* Requires new field */
vm->word_executions_last_tick = 0;
```

### Step 5: Heartbeat buffer cleanup (src/vm.c, in vm_cleanup)
```c
/* In vm_cleanup(), add: */

if (vm->heartbeat.tick_buffer) {
    free(vm->heartbeat.tick_buffer);
    vm->heartbeat.tick_buffer = NULL;
}
```

### Step 6: Per-tick snapshot capture function (src/vm.c)
```c
/* Add new function before vm_heartbeat_run_cycle() (~line 667): */

static void heartbeat_capture_tick_snapshot(VM *vm)
{
    if (!vm || !vm->heartbeat.tick_buffer || !vm->heartbeat.heartbeat_enabled)
        return;

    uint32_t buf_idx = vm->heartbeat.tick_count_total % vm->heartbeat.tick_buffer_size;
    HeartbeatTickSnapshot *snap = &vm->heartbeat.tick_buffer[buf_idx];

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

    /* Capture delta metrics */
    snap->cache_hits_delta = vm->cache_hits - vm->cache_hits_last_tick;
    snap->bucket_hits_delta = vm->bucket_hit_count - vm->bucket_hits_last_tick;
    snap->word_executions_delta = vm->total_words_executed - vm->word_executions_last_tick;

    /* Capture state variables */
    snap->hot_word_count = vm->heartbeat.snapshots[0].hot_word_count;
    snap->avg_word_heat = (double)vm->heartbeat.snapshots[0].total_heat / 65536.0;
    snap->window_width = vm->heartbeat.snapshots[0].window_width;

    snap->predicted_label_hits = 0;  /* TODO: track context predictions */
    snap->estimated_jitter_ns = 0;   /* TODO: compute deviation from HEARTBEAT_TICK_NS */

    vm->heartbeat.tick_count_total++;
}
```

### Step 7: Inject capture into heartbeat cycle (src/vm.c)
```c
/* Modify vm_heartbeat_run_cycle() (~line 667), add at END before closing brace: */

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

### Step 8: Delta tracking in vm_tick (src/vm.c)
```c
/* At the START of vm_tick() function, capture baseline: */

void vm_tick(VM *vm)
{
    /* Capture deltas at function start */
    uint64_t cache_hits_start = vm->cache_hits;
    uint64_t bucket_hits_start = vm->bucket_hit_count;  /* REQUIRES new field */
    uint32_t word_exec_start = 0;  /* REQUIRES new field to track total */

    /* ... rest of vm_tick() logic ... */

    /* At END of vm_tick(), before return: */

    /* Update snapshots for next tick */
    vm->cache_hits_last_tick = vm->cache_hits;
    vm->bucket_hits_last_tick = vm->bucket_hit_count;
    vm->word_executions_last_tick = 0;  /* Update as needed */
}
```

### Step 9: CSV export function (src/vm.c or new file)
```c
/* Add new function: */

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
    uint32_t buf_start = (total_ticks >= buf_size) ? (total_ticks % buf_size) : 0;

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

### Step 10: Integrate into DoE output (src/main.c or src/vm.c)
```c
/* In run_doe_experiment() after metrics collected, add: */

/* Export heartbeat time-series */
char heartbeat_csv_path[512];
snprintf(heartbeat_csv_path, sizeof(heartbeat_csv_path),
         "/tmp/heartbeat_timeseries_run_%u.csv", run_number);
FILE *hb_out = fopen(heartbeat_csv_path, "w");
if (hb_out) {
    heartbeat_export_csv(vm, hb_out, config_name, run_number);
    fclose(hb_out);
}
```

---

## ✅ Testing Checklist

### Unit Tests
- [ ] HeartbeatTickSnapshot struct compiles without errors
- [ ] Heartbeat struct extends without breaking existing code
- [ ] vm_init() allocates buffer successfully
- [ ] vm_cleanup() frees buffer without leak
- [ ] heartbeat_capture_tick_snapshot() completes in <1 µs
- [ ] Circular buffer wrapping works (test with tick 99,999 → 100,000)

### Integration Tests
- [ ] Abbreviated DoE run (1 config, 10 runs, 1000 ticks each)
- [ ] CSV export produces valid per-tick data
- [ ] Tick numbers are sequential
- [ ] elapsed_ns is monotonically increasing
- [ ] tick_interval_ns matches expected ~1ms (±jitter)
- [ ] hot_word_count, avg_word_heat populated correctly

### Regression Tests
- [ ] No performance regression in heartbeat timing
- [ ] No memory leaks (valgrind --leak-check=full)
- [ ] Existing tests still pass (make test)

---

## 📋 Header/Footer Declarations

### Add to include/vm.h
```c
/* Existing forward declaration (already there): */
struct HeartbeatWorker;

/* ADD AFTER HeartbeatSnapshot: */
typedef struct { ... } HeartbeatTickSnapshot;

/* ADD TO FUNCTION DECLARATIONS: */
void heartbeat_export_csv(VM *vm, FILE *out, const char *config_name, uint32_t run_number);
```

---

## 🔍 Verification Steps

After implementing all 10 steps:

1. **Compile Check**
   ```bash
   make clean && make fastest 2>&1 | grep -i error
   # Should have zero errors
   ```

2. **Quick Test**
   ```bash
   ./build/amd64/fastest/starforth --doe-experiment 2>&1 | tail -5
   # Should output CSV row as before
   ```

3. **Abbreviated Run**
   ```bash
   # Create test script that runs 1 config × 3 runs
   # Check that heartbeat_timeseries_run_*.csv files are created
   # Verify CSV has proper header + data rows
   ```

4. **Circular Buffer Test**
   ```bash
   # Modify code to print tick_count_total after 100K+ ticks
   # Verify wrapping logic (tick 100,001 % 100,000 = 1)
   # Verify buffer reconstruction in correct order
   ```

---

## 💾 File Locations Summary

| File | Location | Changes |
|------|----------|---------|
| HeartbeatTickSnapshot | include/vm.h | NEW struct (30 lines) |
| Heartbeat extension | include/vm.h | 5 new fields (~10 lines) |
| VM tracking counters | include/vm.h | 3 new fields (~5 lines) |
| vm_init() | src/vm.c | ~12 lines initialization |
| vm_cleanup() | src/vm.c | ~4 lines cleanup |
| heartbeat_capture_tick_snapshot() | src/vm.c | ~50 lines new function |
| vm_heartbeat_run_cycle() | src/vm.c | 2 lines injection |
| vm_tick() | src/vm.c | 6 lines delta tracking |
| heartbeat_export_csv() | src/vm.c | ~30 lines new function |
| run_doe_experiment() | src/vm.c | ~8 lines integration |

**Total Changes**: ~155 lines of code across 1 header + 1 source file

---

## 🚀 Execution Order

1. **Phase 1**: Add structs (steps 1-3) → Compile test
2. **Phase 2**: Implement functions (steps 4-6) → Unit test
3. **Phase 3**: Integrate into heartbeat (step 7) → Integration test
4. **Phase 4**: Add delta tracking (step 8) → Functional test
5. **Phase 5**: Implement export (step 9) → CSV output test
6. **Phase 6**: Wire into DoE (step 10) → Full system test

---

**Ready to implement. Each step takes ~10-15 minutes. Total: ~90 minutes.**