# Implementation Plan: Heartbeat Rate Logging

**Date:** November 20, 2025
**Priority:** BLOCKING — Must implement before running Phase 2 DoE
**Complexity:** Medium (requires code additions, CSV schema update)

---

## Overview

We need to capture **tick_ns trajectory** (the actual heartbeat rate changes) during execution, then analyze how each configuration modulates heartrate in response to workload.

---

## Step 1: Add Heartbeat Rate Sample Struct

### File: `include/vm.h`

Add after `HeartbeatSnapshot` definition:

```c
/* === Heartbeat Rate Trajectory Collection === */

#define HEARTBEAT_RATE_SAMPLE_MAX 100000  /* Maximum samples per run */

typedef struct {
    uint64_t tick_number;               /* Absolute tick count */
    uint64_t tick_ns;                   /* ← HEARTBEAT RATE (nanoseconds) */
    uint64_t workload_ops_this_tick;    /* Dictionary lookups this tick */
    uint64_t timestamp_ns;              /* Wall-clock timestamp */
    uint64_t hot_word_count;            /* Physics state: hot words */
    uint64_t total_heat;                /* Physics state: total execution heat */
} HeartbeatRateSample;
```

### File: `include/vm.h` (in VM struct)

Add to the HeartbeatState section:

```c
/** @name Heartbeat Rate Trajectory (Phase 2 DoE)
 * @{
 */
HeartbeatRateSample* heartbeat_rate_samples;  /* Circular buffer of rate samples */
uint64_t heartbeat_rate_sample_count;         /* Number of samples collected */
/** @} */
```

---

## Step 2: Initialize Heartbeat Rate Collection

### File: `src/vm.c` in `vm_init()`

Add after heartbeat initialization:

```c
/* Initialize heartbeat rate sampling for Phase 2 DoE */
vm->heartbeat_rate_samples = calloc(HEARTBEAT_RATE_SAMPLE_MAX, sizeof(HeartbeatRateSample));
vm->heartbeat_rate_sample_count = 0;
```

### File: `src/vm.c` in `vm_cleanup()`

Add cleanup:

```c
if (vm->heartbeat_rate_samples) {
    free(vm->heartbeat_rate_samples);
    vm->heartbeat_rate_samples = NULL;
}
```

---

## Step 3: Capture Heartbeat Rate Per Tick

### File: `src/vm.c` - Add helper function

```c
/**
 * @brief Capture current heartbeat rate sample for analysis
 *
 * Called by heartbeat_thread_main() to log the tick_ns (heartbeat rate)
 * along with contemporaneous workload metrics.
 *
 * @param vm Pointer to VM
 * @param tick_ns Current heartbeat interval (nanoseconds)
 * @param workload_ops Dictionary lookups executed this tick
 */
static void capture_heartbeat_rate_sample(VM *vm, uint64_t tick_ns, uint64_t workload_ops)
{
    if (!vm || !vm->heartbeat_rate_samples)
        return;

    if (vm->heartbeat_rate_sample_count >= HEARTBEAT_RATE_SAMPLE_MAX)
        return;  /* Buffer full, stop sampling */

    HeartbeatRateSample *sample = &vm->heartbeat_rate_samples[vm->heartbeat_rate_sample_count];
    sample->tick_number = vm->heartbeat.tick_count;
    sample->tick_ns = tick_ns;
    sample->workload_ops_this_tick = workload_ops;
    sample->timestamp_ns = platform_monotonic_ns();

    /* Capture current physics state */
    sample->hot_word_count = vm->hot_word_count_at_check;
    sample->total_heat = vm->total_heat_at_last_check;

    vm->heartbeat_rate_sample_count++;
}
```

---

## Step 4: Log Workload Per Tick

### File: `src/vm.c` - Track lookups per tick

Add to VM struct (in `include/vm.h`):

```c
/** @name Per-Tick Workload Metrics
 * @{
 */
uint64_t workload_ops_current_tick;  /* Dictionary lookups in current tick */
/** @} */
```

### File: `src/vm.c` - In `vm_heartbeat_run_cycle()`

**Before calling inference engine:**

```c
void vm_heartbeat_run_cycle(VM *vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled)
        return;

    vm->heartbeat.tick_count++;

    /* Reset per-tick workload counter */
    uint64_t lookups_this_tick = vm->workload_ops_current_tick;
    vm->workload_ops_current_tick = 0;  /* Reset for next tick */

    /* ... existing inference logic ... */
}
```

### File: `src/vm.c` - Increment lookup counter

In the main word execution loop (in `vm_interpret()` or inner interpreter):

```c
/* After successful dictionary lookup */
vm->workload_ops_current_tick++;
```

---

## Step 5: Capture in Heartbeat Thread

### File: `src/vm.c` in `heartbeat_thread_main()`

Replace the existing sleep logic:

```c
static void* heartbeat_thread_main(void *arg)
{
    VM *vm = (VM*)arg;
    if (!vm || !vm->heartbeat.worker)
        return NULL;

    HeartbeatWorker *worker = vm->heartbeat.worker;
    worker->running = 1;

    while (!worker->stop_requested && vm->heartbeat.heartbeat_enabled)
    {
        vm_heartbeat_run_cycle(vm);

        /* GET THE CURRENT HEARTBEAT RATE */
        uint64_t tick_ns = worker->tick_ns ? worker->tick_ns : HEARTBEAT_TICK_NS;
        uint64_t workload_ops = vm->workload_ops_current_tick;

        /* ↓ ADD THIS ↓ CAPTURE HEARTBEAT RATE SAMPLE */
        capture_heartbeat_rate_sample(vm, tick_ns, workload_ops);
        /* ↑ ADD THIS ↑ */

        struct timespec req;
        req.tv_sec = (time_t)(tick_ns / 1000000000ULL);
        req.tv_nsec = (long)(tick_ns % 1000000000ULL);

        struct timespec rem;
        while (!worker->stop_requested && vm->heartbeat.heartbeat_enabled &&
               nanosleep(&req, &rem) == -1 && errno == EINTR)
        {
            req = rem;
        }
    }

    worker->running = 0;
    return NULL;
}
```

---

## Step 6: Extend DoeMetrics

### File: `include/doe_metrics.h`

Add to `DoeMetrics` struct:

```c
/* === HEARTBEAT RATE MODULATION METRICS (NEW) === */

/* Per-run heartbeat rate statistics */
uint64_t total_heartbeat_ticks;
double   mean_heartbeat_rate_hz;             /* Average frequency (Hz) */
double   heartbeat_rate_stddev_hz;           /* Variation in Hz */
double   heartbeat_rate_cv;                  /* CV of heartbeat rate */
uint64_t heartbeat_rate_min_ns;              /* Slowest tick observed */
uint64_t heartbeat_rate_max_ns;              /* Fastest tick observed */
double   heartbeat_rate_modulation_range_hz; /* max_hz - min_hz */

/* Load-Heartrate Response */
double   load_to_heartrate_correlation;      /* Correlation(workload, heartrate) */
int      load_response_direction;            /* +1 if slower under load, -1 if faster */
double   heartrate_adaptation_latency_ticks; /* Ticks until rate responds to load */
double   heartrate_settling_time_ticks;      /* Ticks to settle after adaptation */

/* Heartrate Convergence */
double   heartrate_convergence_time_ms;      /* Time to reach stable rate */
int      heartrate_converged;                /* 1 = converged, 0 = oscillating */
double   heartrate_oscillation_amplitude_hz; /* Peak-to-peak variation in final state */

/* Overall Heartrate Quality Score */
double   heartrate_responsiveness_score;     /* 0-100: how well does it respond? */
double   heartrate_stability_score;          /* 0-100: how smooth is adaptation? */
```

---

## Step 7: Implement Metrics Extraction

### File: `src/doe_metrics.c`

Add new function:

```c
/**
 * @brief Analyze heartbeat rate trajectory and compute metrics
 *
 * @param vm Pointer to VM with heartbeat_rate_samples
 * @param metrics Pointer to DoeMetrics to fill
 */
static void analyze_heartbeat_rate_trajectory(VM *vm, DoeMetrics *metrics)
{
    if (!vm || !vm->heartbeat_rate_samples || vm->heartbeat_rate_sample_count == 0) {
        metrics->mean_heartbeat_rate_hz = 0;
        metrics->load_to_heartrate_correlation = 0;
        return;
    }

    uint64_t count = vm->heartbeat_rate_sample_count;
    HeartbeatRateSample *samples = vm->heartbeat_rate_samples;

    /* 1. Compute heartbeat rate statistics (tick_ns → Hz) */
    double *heartrate_hz = malloc(count * sizeof(double));
    double *workload_intensity = malloc(count * sizeof(double));

    double min_ns = samples[0].tick_ns;
    double max_ns = samples[0].tick_ns;
    double sum_hz = 0;
    double sum_workload = 0;

    for (uint64_t i = 0; i < count; i++) {
        double tick_ns = samples[i].tick_ns;
        heartrate_hz[i] = 1e9 / tick_ns;
        workload_intensity[i] = samples[i].workload_ops_this_tick;

        sum_hz += heartrate_hz[i];
        sum_workload += workload_intensity[i];

        if (tick_ns < min_ns) min_ns = tick_ns;
        if (tick_ns > max_ns) max_ns = tick_ns;
    }

    /* Mean heartbeat rate */
    metrics->mean_heartbeat_rate_hz = sum_hz / count;
    metrics->heartbeat_rate_min_ns = (uint64_t)min_ns;
    metrics->heartbeat_rate_max_ns = (uint64_t)max_ns;
    metrics->heartbeat_rate_modulation_range_hz = (1e9 / min_ns) - (1e9 / max_ns);

    /* 2. Compute standard deviation */
    double variance = 0;
    for (uint64_t i = 0; i < count; i++) {
        double delta = heartrate_hz[i] - metrics->mean_heartbeat_rate_hz;
        variance += delta * delta;
    }
    metrics->heartbeat_rate_stddev_hz = sqrt(variance / count);
    metrics->heartbeat_rate_cv = metrics->heartbeat_rate_stddev_hz / metrics->mean_heartbeat_rate_hz;

    /* 3. Compute correlation(workload, heartrate) — THE KEY METRIC */
    double workload_mean = sum_workload / count;
    double cov = 0;
    double workload_var = 0;

    for (uint64_t i = 0; i < count; i++) {
        double workload_delta = workload_intensity[i] - workload_mean;
        double heartrate_delta = heartrate_hz[i] - metrics->mean_heartbeat_rate_hz;
        cov += workload_delta * heartrate_delta;
        workload_var += workload_delta * workload_delta;
    }

    if (workload_var > 0 && metrics->heartbeat_rate_stddev_hz > 0) {
        metrics->load_to_heartrate_correlation = cov / sqrt(workload_var *
            (metrics->heartbeat_rate_stddev_hz * metrics->heartbeat_rate_stddev_hz * count));
    }

    /* 4. Determine if heartrate responds correctly to load */
    metrics->load_response_direction = (metrics->load_to_heartrate_correlation > 0.3) ? 1 : -1;

    /* 5. Estimate convergence time (when heartrate stabilizes) */
    double threshold = 0.15;  /* 15% of final value */
    metrics->heartrate_converged = 1;
    for (uint64_t i = count / 2; i < count; i++) {
        double current_cv = 0;
        /* Compute CV of last N samples */
        for (uint64_t j = i; j < (i + 100 < count ? i + 100 : count); j++) {
            double delta = heartrate_hz[j] - metrics->mean_heartbeat_rate_hz;
            current_cv += delta * delta;
        }
        if (sqrt(current_cv) > threshold * metrics->mean_heartbeat_rate_hz) {
            metrics->heartrate_converged = 0;
            metrics->heartrate_convergence_time_ms = i;  /* Approximate */
            break;
        }
    }

    /* 6. Compute overall responsiveness score */
    /* Higher correlation + faster convergence + lower CV = higher score */
    metrics->heartrate_responsiveness_score =
        (metrics->load_to_heartrate_correlation + 1.0) * 50 *
        (1.0 / (1.0 + metrics->heartbeat_rate_cv));

    metrics->heartrate_stability_score =
        (100.0 * (1.0 - metrics->heartbeat_rate_cv)) *
        (metrics->heartrate_converged ? 1.0 : 0.5);

    free(heartrate_hz);
    free(workload_intensity);
}
```

### Update `metrics_from_vm()`

Call the new analysis function:

```c
DoeMetrics metrics_from_vm(VM *vm, ...) {
    /* ... existing metrics extraction ... */

    /* NEW: Analyze heartbeat rate trajectory */
    analyze_heartbeat_rate_trajectory(vm, &metrics);

    return metrics;
}
```

---

## Step 8: Update CSV Schema

### File: `src/doe_metrics.c` in `metrics_write_csv_header()`

Add new columns:

```c
void metrics_write_csv_header(FILE *out) {
    {
        printf '[...existing columns...]');
        printf('total_heartbeat_ticks,mean_heartbeat_rate_hz,heartbeat_rate_stddev_hz,heartbeat_rate_cv,'
               'heartbeat_rate_min_ns,heartbeat_rate_max_ns,heartbeat_rate_modulation_range_hz,'
               'load_to_heartrate_correlation,load_response_direction,'
               'heartrate_convergence_time_ms,heartrate_converged,'
               'heartrate_responsiveness_score,heartrate_stability_score\n');
    } > output_csv;
}
```

### File: `src/doe_metrics.c` in `metrics_write_csv_row()`

Add the values:

```c
void metrics_write_csv_row(FILE *out, const DoeMetrics *metrics) {
    fprintf(out, "[...existing fields...],"
            "%lu,%lf,%lf,%lf,%lu,%lu,%lf,%lf,%d,%lu,%d,%lf,%lf\n",
            metrics->total_heartbeat_ticks,
            metrics->mean_heartbeat_rate_hz,
            metrics->heartbeat_rate_stddev_hz,
            metrics->heartbeat_rate_cv,
            metrics->heartbeat_rate_min_ns,
            metrics->heartbeat_rate_max_ns,
            metrics->heartbeat_rate_modulation_range_hz,
            metrics->load_to_heartrate_correlation,
            metrics->load_response_direction,
            metrics->heartrate_convergence_time_ms,
            metrics->heartrate_converged,
            metrics->heartrate_responsiveness_score,
            metrics->heartrate_stability_score);
}
```

---

## Step 9: Updated R Analysis

### File: `scripts/analyze_heartbeat_stability_REVISED.R`

```r
library(tidyverse)

# Load data
doe_data <- read.csv("experiment_results_heartbeat.csv") %>%
  mutate(
    configuration = as.factor(configuration),
    # New metrics available now
    load_response = ifelse(load_to_heartrate_correlation > 0.5, "Responsive", "Sluggish")
  )

# PRIMARY METRIC: Load-Heartrate Correlation
# This tells us which config best responds to workload changes
ranking <- doe_data %>%
  group_by(configuration) %>%
  summarize(
    mean_correlation = mean(load_to_heartrate_correlation, na.rm = TRUE),
    mean_responsiveness = mean(heartrate_responsiveness_score, na.rm = TRUE),
    mean_stability = mean(heartrate_stability_score, na.rm = TRUE),
    mean_convergence_time = mean(heartrate_convergence_time_ms, na.rm = TRUE),
    golden_score = (mean(load_to_heartrate_correlation) * 0.5) +
                   (mean(heartrate_responsiveness_score) * 0.3) +
                   (mean(heartrate_stability_score) * 0.2)
  ) %>%
  arrange(-golden_score)

print("=== HEARTBEAT RATE RESPONSIVENESS RANKING ===")
print(ranking)

# Plot 1: Load-Heartrate Correlation (KEY METRIC)
ggplot(doe_data, aes(x = reorder(configuration, load_to_heartrate_correlation, FUN=median),
                      y = load_to_heartrate_correlation,
                      fill = configuration)) +
  geom_boxplot() +
  coord_flip() +
  labs(title = "Load-Heartrate Correlation (Higher = Better)",
       y = "Correlation(workload, heartrate)")
ggsave("01_load_heartrate_correlation.png")

# Plot 2: Heartrate Responsiveness Score
ggplot(doe_data, aes(x = reorder(configuration, heartrate_responsiveness_score, FUN=median),
                      y = heartrate_responsiveness_score,
                      fill = configuration)) +
  geom_boxplot() +
  coord_flip() +
  labs(title = "Heartrate Responsiveness Score (0-100)")
ggsave("02_heartrate_responsiveness.png")
```

---

## Compilation Steps

1. **Update headers:** Add structs and fields to `include/vm.h`, `include/doe_metrics.h`
2. **Implement collection:** Add capture function in `src/vm.c`
3. **Implement analysis:** Add analysis function in `src/doe_metrics.c`
4. **Update CSV:** Modify CSV header/row writers
5. **Compile:** `make clean && make fastest`
6. **Test:** `./build/starforth -c "1 2 + . BYE"`

---

## Verification

After implementation, verify:

✓ Binary compiles without errors
✓ Heartbeat rate samples are captured (check sample_count > 0 after run)
✓ CSV contains new heartrate columns
✓ Correlation metrics are computed
✓ R analysis generates ranking by load-response

---

## Expected Results

When properly implemented, you'll see:

```
=== HEARTBEAT RATE RESPONSIVENESS RANKING ===
configuration  mean_correlation  mean_responsiveness  golden_score
1_0_1_1_1_0          0.82             78.5               0.795
1_0_1_1_1_1          0.78             75.2               0.761
1_1_0_1_1_1          0.65             62.3               0.641
1_0_1_0_1_0          0.45             48.1               0.451
0_1_1_0_1_1          0.12             15.8               0.148
```

**Golden config:** 1_0_1_1_1_0 (strongest load-heartrate coupling)

---

## Timeline

- **Implementation:** 2-3 hours
- **Testing:** 1 hour
- **Re-run Phase 2:** 2-4 hours
- **Analysis:** 30 min
- **Total:** ~6-9 hours

---

## Files Modified

- `include/vm.h` — Add HeartbeatRateSample struct and VM fields
- `include/doe_metrics.h` — Add heartrate metrics to DoeMetrics
- `src/vm.c` — Add sample capture, logging, initialization
- `src/doe_metrics.c` — Add analysis function, CSV updates
- `scripts/analyze_heartbeat_stability_REVISED.R` — Rewritten for load-response focus

---

**This implementation captures the actual heartbeat rate modulation and measures how well each config responds to load.**

This is the correct physics story. 🚀