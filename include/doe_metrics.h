/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/**
 * @file doe_metrics.h
 * @brief Design of Experiments (DoE) metrics collection API
 *
 * Provides C-level programmatic access to VM metrics for physics-based experiments.
 * Enables efficient in-process data collection without string parsing overhead.
 *
 * Usage:
 *   struct DoeMetrics metrics = metrics_from_vm(vm);
 *   metrics_write_csv_row(stdout, &metrics, run_number);
 */

#ifndef DOE_METRICS_H
#define DOE_METRICS_H

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Complete metrics snapshot for a single DoE run
 *
 * Represents all 35 metrics collected per test iteration, mirroring the CSV schema.
 * All values extracted directly from VM structures (no string parsing).
 */
typedef struct {
    /* === Metrics === */
    uint32_t total_lookups;       /* Total dictionary lookups during run */

    /* === Cache Statistics === */
    uint64_t cache_hits;          /* Cache hits (if enabled) */
    double cache_hit_percent;     /* Hit percentage (0-100) */
    uint64_t bucket_hits;         /* Bucket hits after cache miss */
    double bucket_hit_percent;    /* Hit percentage (0-100) */

    /* === Latency Metrics === */
    int64_t cache_hit_latency_ns;       /* Average cache hit latency (ns) */
    int64_t cache_hit_stddev_ns;        /* StdDev of cache hit latency */
    int64_t bucket_search_latency_ns;   /* Average bucket search latency (ns) */
    int64_t bucket_search_stddev_ns;    /* StdDev of bucket search latency */

    /* === Pipelining Metrics (Loop #4) === */
    uint64_t context_predictions_total; /* Total prefetch predictions */
    uint64_t context_correct;           /* Successful predictions */
    double context_accuracy_percent;    /* Accuracy percentage (0-100) */
    uint64_t cache_promotions;          /* Hotwords cache promotions */
    uint64_t cache_demotions;           /* Hotwords cache demotions */

    /* === Rolling Window Metrics (Loop #2) === */
    double window_diversity_percent;    /* Window pattern diversity (0-100) */
    uint32_t window_final_size_bytes;   /* Final rolling window size */
    uint32_t rolling_window_width;      /* Width of rolling window */
    uint64_t total_executions;          /* Words recorded in window */
    uint64_t window_variance_q48;       /* Pattern variance (Q48.16) */

    /* === Heat Dynamics (Loop #1 & #3) === */
    double decay_slope;                 /* Decay rate slope */
    uint64_t total_heat;                /* Aggregate execution heat */
    uint64_t hot_word_count;            /* Words above heat threshold */
    uint64_t stale_word_count;          /* Words with decaying heat */
    double stale_word_ratio;            /* Stale words / total words */
    double avg_word_heat;               /* Average execution heat */

    /* === Heartbeat & Timing (Loop #7) === */
    uint64_t tick_count;                /* Total heartbeat ticks elapsed */
    uint64_t tick_target_ns;            /* Current tick interval (adaptive) */
    uint64_t inference_run_count;       /* Times inference engine invoked */
    uint64_t early_exit_count;          /* ANOVA early-exits (variance stable) */

    /* === Window & Decay Inference (Loop #5 & #6) === */
    double prefetch_accuracy_percent;   /* Speculative prefetch success rate (0-100) */
    uint64_t prefetch_attempts;         /* Total speculative prefetch attempts */
    uint64_t prefetch_hits;             /* Successful prefetch hits */
    uint64_t window_tuning_checks;      /* Number of times window was tuned */
    uint32_t final_effective_window_size; /* Final window size after tuning */

    /* === Performance === */
    int64_t vm_workload_duration_ns_q48;  /* VM workload duration (Q48.16, nanoseconds) */
    uint64_t total_runtime_ms;          /* Total execution time (ms) */
    uint64_t words_executed;            /* Total word executions */
    uint64_t dictionary_lookups;        /* Dictionary search operations */
    uint64_t memory_allocated_bytes;    /* Total memory allocated */
    double speedup_vs_baseline;         /* Speedup ratio vs baseline */

    /* === Statistical === */
    double ci_lower_95;                 /* 95% confidence interval lower */
    double ci_upper_95;                 /* 95% confidence interval upper */

    /* === System State Deltas === */
    int64_t cpu_temp_delta_c_q48;       /* CPU temp change during run (Q48.16, °C) */
    int64_t cpu_freq_delta_mhz_q48;     /* CPU freq change during run (Q48.16, MHz) */

    /* === Tuning Knobs === */
    uint32_t decay_rate_q16;            /* Decay rate (Q16 fixed-point) */
    uint32_t decay_min_interval_ns;     /* Min decay interval (ns) */
    uint32_t rolling_window_size;       /* Rolling window size */
    uint32_t adaptive_shrink_rate;      /* Shrink rate percentage */
    uint32_t heat_cache_demotion_threshold; /* Demotion threshold */

    /* === Loop Enable Flags (2^7 factorial) === */
    int enable_loop_1_heat_tracking;    /* Loop #1: Execution heat tracking */
    int enable_loop_2_rolling_window;   /* Loop #2: Rolling window history */
    int enable_loop_3_linear_decay;     /* Loop #3: Linear decay */
    int enable_loop_4_pipelining;       /* Loop #4: Pipelining metrics */
    int enable_loop_5_window_inference; /* Loop #5: Window width inference */
    int enable_loop_6_decay_inference;  /* Loop #6: Decay slope inference */
    int enable_loop_7_adaptive_heartrate; /* Loop #7: Adaptive heartrate */

    /* === Legacy Configuration === */
    int enable_hotwords_cache;          /* 1 if cache enabled, 0 otherwise */
    int enable_pipelining;              /* 1 if pipelining enabled, 0 otherwise */
} DoeMetrics;

/**
 * @brief Extract complete metrics snapshot from VM
 *
 * Collects all metrics from current VM state after a test harness run.
 * Workload duration and CPU deltas must be supplied by caller.
 *
 * @param vm Pointer to Forth VM instance
 * @param workload_duration_ns Actual VM workload execution time (nanoseconds)
 * @param cpu_temp_delta_c CPU temperature change during run (°C)
 * @param cpu_freq_delta_mhz CPU frequency change during run (MHz)
 * @return Populated metrics structure (timestamp filled in)
 */
DoeMetrics metrics_from_vm(VM *vm, uint64_t workload_duration_ns,
                           int32_t cpu_temp_delta_c, int32_t cpu_freq_delta_mhz);

/**
 * @brief Write CSV header row
 *
 * Writes the 35-column CSV header to output stream.
 *
 * @param out Output file stream
 */
void metrics_write_csv_header(FILE *out);

/**
 * @brief Write metrics as CSV row
 *
 * Writes a single run's metrics as comma-separated values.
 *
 * @param out Output file stream
 * @param metrics Metrics structure to write
 */
void metrics_write_csv_row(FILE *out, const DoeMetrics *metrics);

/**
 * @brief Write reduced James Law CSV row (20 columns)
 *
 * Outputs only the subset of metrics needed for the James Law validation experiment.
 * This matches the header format in run_window_sweep.sh (minus the 5 script-prepended columns).
 *
 * @param out Output file stream
 * @param metrics Metrics structure to write
 * @param vm VM instance (for heartbeat bucket aggregates and L8 mode)
 */
void metrics_write_james_law_csv_row(FILE *out, const DoeMetrics *metrics, VM *vm);

/**
 * @brief Print metrics as human-readable text
 *
 * Outputs metrics in formatted text for debugging/verification.
 *
 * @param out Output file stream
 * @param metrics Metrics structure to print
 */
void metrics_print_text(FILE *out, const DoeMetrics *metrics);

/**
 * @brief Get current CPU temperature (Celsius)
 *
 * Reads /sys/class/thermal/thermal_zone0/temp on Linux,
 * returns 0 if unavailable.
 *
 * @return Temperature in °C, or 0 if unable to read
 */
int32_t metrics_get_cpu_temp_c(void);

/**
 * @brief Get current CPU frequency (MHz)
 *
 * Tries multiple sources:
 * 1. /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
 * 2. /proc/cpuinfo
 * Returns 0 if unavailable.
 *
 * @return Frequency in MHz, or 0 if unable to read
 */
int32_t metrics_get_cpu_freq_mhz(void);

/**
 * @brief Get current timestamp as ISO 8601 string
 *
 * Format: "YYYY-MM-DDTHH:MM:SS"
 *
 * @param buf Buffer to write timestamp (minimum 32 bytes)
 * @param bufsize Size of buffer
 */
void metrics_get_timestamp(char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* DOE_METRICS_H */