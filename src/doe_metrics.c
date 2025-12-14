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
 * @file doe_metrics.c
 * @brief Design of Experiments metrics collection implementation
 */

#include "doe_metrics.h"
#include "physics_hotwords_cache.h"
#include "rolling_window_of_truth.h"
#include "rolling_window_knobs.h"
#include "inference_engine.h"
#include "platform_time.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <unistd.h>
#endif

/* Forward declarations from physics system */
extern struct {
    uint64_t total_lookups;
    uint64_t cache_hits;
    uint64_t bucket_hits;
} physics_global_stats;

/**
 * Get current CPU temperature in Celsius
 */
int32_t metrics_get_cpu_temp_c(void) {
#ifdef __unix__
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return 0;

    int temp_millidegrees = 0;
    if (fscanf(f, "%d", &temp_millidegrees) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (int32_t)(temp_millidegrees / 1000);
#else
    return 0;
#endif
}

/**
 * Get current CPU frequency in MHz
 */
int32_t metrics_get_cpu_freq_mhz(void) {
#ifdef __unix__
    /* Try scaling_cur_freq first */
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f) {
        int freq_khz = 0;
        if (fscanf(f, "%d", &freq_khz) == 1) {
            fclose(f);
            return (int32_t)(freq_khz / 1000);
        }
        fclose(f);
    }

    /* Fallback to /proc/cpuinfo */
    f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                float mhz = 0.0f;
                if (sscanf(line, "cpu MHz : %f", &mhz) == 1) {
                    fclose(f);
                    return (int32_t)mhz;
                }
            }
        }
        fclose(f);
    }
#endif
    return 0;
}

/**
 * Get current timestamp as ISO 8601 string
 */
void metrics_get_timestamp(char *buf, size_t bufsize) {
    if (bufsize < 32) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(buf, bufsize, "%Y-%m-%dT%H:%M:%S", tm_info);
}

/**
 * Extract metrics from VM hotwords cache stats
 */
static void extract_cache_metrics(const HotwordsCache *cache, DoeMetrics *metrics) {
    if (!cache) {
        metrics->cache_hits = 0;
        metrics->cache_hit_percent = 0.0;
        metrics->bucket_hits = 0;
        metrics->bucket_hit_percent = 0.0;
        metrics->cache_hit_latency_ns = 0;
        metrics->cache_hit_stddev_ns = 0;
        metrics->bucket_search_latency_ns = 0;
        metrics->bucket_search_stddev_ns = 0;
        return;
    }

    const HotwordsStats *stats = &cache->stats;

    /* Cache hits */
    metrics->cache_hits = stats->cache_hits;
    if (stats->total_lookups > 0) {
        metrics->cache_hit_percent = 100.0 * (double)stats->cache_hits / (double)stats->total_lookups;
    } else {
        metrics->cache_hit_percent = 0.0;
    }

    /* Bucket hits */
    metrics->bucket_hits = stats->bucket_hits;
    if (stats->total_lookups > 0) {
        metrics->bucket_hit_percent = 100.0 * (double)stats->bucket_hits / (double)stats->total_lookups;
    } else {
        metrics->bucket_hit_percent = 0.0;
    }

    /* Cache hit latency (convert from Q48.16 to ns) */
    if (stats->cache_hit_samples > 0) {
        int64_t avg_q48 = stats->cache_hit_total_ns_q48 / (int64_t)stats->cache_hit_samples;
        metrics->cache_hit_latency_ns = avg_q48 >> 16;  /* Convert from Q48.16 to nanoseconds */

        /* StdDev calculation from variance sum */
        if (stats->cache_hit_samples > 1) {
            /* Simplified: use variance sum for estimation */
            int64_t variance_q48 = stats->cache_hit_variance_sum_q48 / (int64_t)stats->cache_hit_samples;
            metrics->cache_hit_stddev_ns = (int64_t)(variance_q48 >> 16);
        } else {
            metrics->cache_hit_stddev_ns = 0;
        }
    } else {
        metrics->cache_hit_latency_ns = 0;
        metrics->cache_hit_stddev_ns = 0;
    }

    /* Bucket search latency (convert from Q48.16 to ns) */
    if (stats->bucket_search_samples > 0) {
        int64_t avg_q48 = stats->bucket_search_total_ns_q48 / (int64_t)stats->bucket_search_samples;
        metrics->bucket_search_latency_ns = avg_q48 >> 16;  /* Convert from Q48.16 to nanoseconds */

        /* StdDev calculation from variance sum */
        if (stats->bucket_search_samples > 1) {
            int64_t variance_q48 = stats->bucket_search_variance_sum_q48 / (int64_t)stats->bucket_search_samples;
            metrics->bucket_search_stddev_ns = (int64_t)(variance_q48 >> 16);
        } else {
            metrics->bucket_search_stddev_ns = 0;
        }
    } else {
        metrics->bucket_search_latency_ns = 0;
        metrics->bucket_search_stddev_ns = 0;
    }
}

/**
 * Extract metrics from entire VM
 */
DoeMetrics metrics_from_vm(VM *vm, uint64_t workload_duration_ns,
                           int32_t cpu_temp_delta_c, int32_t cpu_freq_delta_mhz) {
    DoeMetrics metrics = {0};

    /* Lookups */
    metrics.total_lookups = vm->hotwords_cache ? vm->hotwords_cache->stats.total_lookups : 0;

    /* Cache metrics */
    if (ENABLE_HOTWORDS_CACHE && vm->hotwords_cache) {
        extract_cache_metrics(vm->hotwords_cache, &metrics);
        metrics.enable_hotwords_cache = vm->hotwords_cache->enabled ? 1 : 0;
    } else {
        metrics.enable_hotwords_cache = 0;
    }

    /* Pipelining metrics - extract from global pipeline metrics (Loop #4) */
    metrics.context_predictions_total = vm->pipeline_metrics.prefetch_attempts;
    metrics.context_correct = vm->pipeline_metrics.prefetch_hits;
    metrics.context_accuracy_percent = 0.0;
    if (vm->pipeline_metrics.prefetch_attempts > 0) {
        metrics.context_accuracy_percent = 100.0 * (double)vm->pipeline_metrics.prefetch_hits /
                                          (double)vm->pipeline_metrics.prefetch_attempts;
    }

    /* === Rolling Window Metrics (Loop #2) === */
    metrics.window_diversity_percent = 0.0;
    metrics.window_final_size_bytes = 4096;
    metrics.rolling_window_width = (uint32_t)vm->rolling_window.effective_window_size;
    metrics.total_executions = vm->rolling_window.total_executions;
    /* Protect access to last_inference_outputs against heartbeat thread race */
    sf_mutex_lock(&vm->tuning_lock);
    metrics.window_variance_q48 = vm->last_inference_outputs ?
                                   vm->last_inference_outputs->window_variance_q48 : 0;
    sf_mutex_unlock(&vm->tuning_lock);

    /* === Heat Dynamics (Loop #1 & #3) === */
    metrics.decay_slope = (double)vm->decay_slope_q48 / 65536.0;

    /* Collect snapshot of current dictionary state */
    {
        uint64_t hot_word_count = 0;
        uint64_t stale_word_count = 0;
        uint64_t total_heat = 0;
        uint32_t word_count = 0;

        sf_mutex_lock(&vm->dict_lock);
        for (DictEntry *e = vm->latest; e != NULL; e = e->link) {
            if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD)
                hot_word_count++;
            else if (e->execution_heat > 0 && e->execution_heat < 10)
                stale_word_count++;

            total_heat += e->execution_heat;
            word_count++;
        }
        sf_mutex_unlock(&vm->dict_lock);

        metrics.total_heat = total_heat;
        metrics.hot_word_count = hot_word_count;
        metrics.stale_word_count = stale_word_count;
        metrics.stale_word_ratio = (word_count > 0) ? (double)stale_word_count / (double)word_count : 0.0;
        metrics.avg_word_heat = (word_count > 0) ? (double)total_heat / (double)word_count : 0.0;
    }

    /* === Heartbeat & Timing (Loop #7) === */
    metrics.tick_count = vm->heartbeat.tick_count;
    metrics.tick_target_ns = vm->heartbeat.tick_target_ns;
    metrics.inference_run_count = vm->heartbeat.inference_run_count;
    metrics.early_exit_count = vm->heartbeat.early_exit_count;

    /* === Cache Promotions/Demotions (Loop #4) === */
    if (vm->hotwords_cache) {
        metrics.cache_promotions = vm->hotwords_cache->stats.promotions;
        metrics.cache_demotions = vm->hotwords_cache->stats.evictions;  /* evictions = demotions */
    } else {
        metrics.cache_promotions = 0;
        metrics.cache_demotions = 0;
    }

    /* === Window & Decay Inference (Loop #5 & #6) === */
    metrics.prefetch_accuracy_percent = 0.0;
    metrics.prefetch_attempts = vm->pipeline_metrics.prefetch_attempts;
    metrics.prefetch_hits = vm->pipeline_metrics.prefetch_hits;
    metrics.window_tuning_checks = vm->pipeline_metrics.window_tuning_checks;
    metrics.final_effective_window_size = (uint32_t)vm->rolling_window.effective_window_size;

    if (vm->pipeline_metrics.prefetch_attempts > 0) {
        metrics.prefetch_accuracy_percent = 100.0 * (double)vm->pipeline_metrics.prefetch_hits /
                                            (double)vm->pipeline_metrics.prefetch_attempts;
    }

    /* === Performance counters === */
    metrics.words_executed = vm->heartbeat.words_executed;
    metrics.dictionary_lookups = vm->heartbeat.dictionary_lookups;

    /* Performance - workload duration as Q48.16 */
    metrics.vm_workload_duration_ns_q48 = (int64_t)workload_duration_ns << 16;
    metrics.total_runtime_ms = 0;
    metrics.memory_allocated_bytes = 0;
    metrics.speedup_vs_baseline = 1.0;

    /* Statistical (defaults) */
    metrics.ci_lower_95 = 0.0;
    metrics.ci_upper_95 = 0.0;

    /* System state deltas - convert to Q48.16
     * UBSan fix: Use multiplication instead of left-shift for signed values (2025-12-09)
     * Left-shifting negative values is undefined behavior */
    metrics.cpu_temp_delta_c_q48 = (int64_t)cpu_temp_delta_c * 65536;
    metrics.cpu_freq_delta_mhz_q48 = (int64_t)cpu_freq_delta_mhz * 65536;

    /* Tuning knobs */
    metrics.decay_rate_q16 = DECAY_RATE_PER_US_Q16;
    metrics.decay_min_interval_ns = DECAY_MIN_INTERVAL;
    metrics.rolling_window_size = ROLLING_WINDOW_SIZE;
    metrics.adaptive_shrink_rate = 75;
    metrics.heat_cache_demotion_threshold = 10;

    /* === Loop Enable Flags (2^7 factorial) === */
    /* L8 FINAL INTEGRATION: All loops always-on, controlled by L8 policy */
    metrics.enable_loop_1_heat_tracking = 1;         /* Always-on (internal signal) */
    metrics.enable_loop_2_rolling_window = 1;        /* Always-on (L8-controlled) */
    metrics.enable_loop_3_linear_decay = 1;          /* Always-on */
    metrics.enable_loop_4_pipelining = 1;            /* Always-on (internal signal) */
    metrics.enable_loop_5_window_inference = 1;      /* Always-on */
    metrics.enable_loop_6_decay_inference = 1;       /* Always-on */
    metrics.enable_loop_7_adaptive_heartrate = 1;    /* Always-on */

    /* Legacy configuration */
    metrics.enable_hotwords_cache = ENABLE_HOTWORDS_CACHE;
    metrics.enable_pipelining = ENABLE_PIPELINING;

    return metrics;
}

/**
 * Write CSV header
 */
void metrics_write_csv_header(FILE *out) {
    fprintf(out,
        /* Loop enable flags (2^7 factorial) - FIRST for easy filtering */
        "L1_heat,L2_window,L3_decay,L4_pipeline,L5_win_inf,L6_decay_inf,L7_heartrate,"
        /* Cache stats */
        "total_lookups,cache_hits,cache_hit_pct,bucket_hits,bucket_hit_pct,"
        "cache_lat_ns,cache_lat_std,bucket_lat_ns,bucket_lat_std,"
        /* Pipelining (Loop #4) */
        "ctx_pred_total,ctx_correct,ctx_acc_pct,cache_promos,cache_demos,"
        /* Rolling window (Loop #2) */
        "win_diversity_pct,win_final_bytes,win_width,win_total_exec,win_var_q48,"
        /* Heat dynamics (Loop #1 & #3) */
        "decay_slope,total_heat,hot_words,stale_words,stale_ratio,avg_heat,"
        /* Heartbeat & timing (Loop #7) */
        "tick_count,tick_target_ns,infer_runs,early_exits,"
        /* Window & decay inference (Loop #5 & #6) */
        "prefetch_acc_pct,prefetch_attempts,prefetch_hits,win_tune_checks,final_win_size,"
        /* Performance */
        "workload_ns_q48,runtime_ms,words_exec,dict_lookups,mem_bytes,speedup,"
        /* Statistical */
        "ci_lower_95,ci_upper_95,"
        /* System deltas */
        "cpu_temp_delta_q48,cpu_freq_delta_q48,"
        /* Tuning knobs */
        "decay_rate_q16,decay_min_ns,roll_win_size,shrink_rate,demo_thresh,"
        /* Legacy */
        "hotwords_cache,pipelining\n");
}

/**
 * Write CSV row - MUST match header column order exactly
 */
void metrics_write_csv_row(FILE *out, const DoeMetrics *metrics) {
    fprintf(out,
        /* Loop enable flags (2^7 factorial) - FIRST for easy filtering */
        "%d,%d,%d,%d,%d,%d,%d,"
        /* Cache stats */
        "%u,%lu,%.2f,%lu,%.2f,"
        "%ld,%ld,%ld,%ld,"
        /* Pipelining (Loop #4) */
        "%lu,%lu,%.2f,%lu,%lu,"
        /* Rolling window (Loop #2) */
        "%.2f,%u,%u,%lu,%lu,"
        /* Heat dynamics (Loop #1 & #3) */
        "%.6f,%lu,%lu,%lu,%.6f,%.6f,"
        /* Heartbeat & timing (Loop #7) */
        "%lu,%lu,%lu,%lu,"
        /* Window & decay inference (Loop #5 & #6) */
        "%.2f,%lu,%lu,%lu,%u,"
        /* Performance */
        "%ld,%lu,%lu,%lu,%lu,%.4f,"
        /* Statistical */
        "%.6f,%.6f,"
        /* System deltas */
        "%ld,%ld,"
        /* Tuning knobs */
        "%u,%u,%u,%u,%u,"
        /* Legacy */
        "%d,%d\n",
        /* Loop enable flags */
        metrics->enable_loop_1_heat_tracking,
        metrics->enable_loop_2_rolling_window,
        metrics->enable_loop_3_linear_decay,
        metrics->enable_loop_4_pipelining,
        metrics->enable_loop_5_window_inference,
        metrics->enable_loop_6_decay_inference,
        metrics->enable_loop_7_adaptive_heartrate,
        /* Cache stats */
        metrics->total_lookups,
        metrics->cache_hits,
        metrics->cache_hit_percent,
        metrics->bucket_hits,
        metrics->bucket_hit_percent,
        metrics->cache_hit_latency_ns,
        metrics->cache_hit_stddev_ns,
        metrics->bucket_search_latency_ns,
        metrics->bucket_search_stddev_ns,
        /* Pipelining (Loop #4) */
        metrics->context_predictions_total,
        metrics->context_correct,
        metrics->context_accuracy_percent,
        metrics->cache_promotions,
        metrics->cache_demotions,
        /* Rolling window (Loop #2) */
        metrics->window_diversity_percent,
        metrics->window_final_size_bytes,
        metrics->rolling_window_width,
        metrics->total_executions,
        metrics->window_variance_q48,
        /* Heat dynamics (Loop #1 & #3) */
        metrics->decay_slope,
        metrics->total_heat,
        metrics->hot_word_count,
        metrics->stale_word_count,
        metrics->stale_word_ratio,
        metrics->avg_word_heat,
        /* Heartbeat & timing (Loop #7) */
        metrics->tick_count,
        metrics->tick_target_ns,
        metrics->inference_run_count,
        metrics->early_exit_count,
        /* Window & decay inference (Loop #5 & #6) */
        metrics->prefetch_accuracy_percent,
        metrics->prefetch_attempts,
        metrics->prefetch_hits,
        metrics->window_tuning_checks,
        metrics->final_effective_window_size,
        /* Performance */
        metrics->vm_workload_duration_ns_q48,
        metrics->total_runtime_ms,
        metrics->words_executed,
        metrics->dictionary_lookups,
        metrics->memory_allocated_bytes,
        metrics->speedup_vs_baseline,
        /* Statistical */
        metrics->ci_lower_95,
        metrics->ci_upper_95,
        /* System deltas */
        metrics->cpu_temp_delta_c_q48,
        metrics->cpu_freq_delta_mhz_q48,
        /* Tuning knobs */
        metrics->decay_rate_q16,
        metrics->decay_min_interval_ns,
        metrics->rolling_window_size,
        metrics->adaptive_shrink_rate,
        metrics->heat_cache_demotion_threshold,
        /* Legacy */
        metrics->enable_hotwords_cache,
        metrics->enable_pipelining);
}

/**
 * Print metrics as human-readable text
 */
void metrics_print_text(FILE *out, const DoeMetrics *metrics) {
    fprintf(out, "\n=== DoE Metrics ===\n");
    fprintf(out, "Lookups:       %u\n", metrics->total_lookups);
    fprintf(out, "Cache Hits:    %lu (%.2f%%)\n", metrics->cache_hits, metrics->cache_hit_percent);
    fprintf(out, "Bucket Hits:   %lu (%.2f%%)\n", metrics->bucket_hits, metrics->bucket_hit_percent);
    fprintf(out, "Hit Latency:   %ld ns (±%ld)\n", metrics->cache_hit_latency_ns, metrics->cache_hit_stddev_ns);
    fprintf(out, "Search Latency: %ld ns (±%ld)\n", metrics->bucket_search_latency_ns, metrics->bucket_search_stddev_ns);
    fprintf(out, "Predictions:   %lu / %lu (%.2f%% accurate)\n",
            metrics->context_correct, metrics->context_predictions_total, metrics->context_accuracy_percent);
    fprintf(out, "Window Width:  %u bytes\n", metrics->rolling_window_width);
    fprintf(out, "Decay Slope:   %.2f\n", metrics->decay_slope);
    fprintf(out, "Workload Time: %ld ns (Q48.16)\n", metrics->vm_workload_duration_ns_q48);
    fprintf(out, "CPU Temp Delta: %ld°C (Q48.16)\n", metrics->cpu_temp_delta_c_q48);
    fprintf(out, "CPU Freq Delta: %ld MHz (Q48.16)\n", metrics->cpu_freq_delta_mhz_q48);
}

/**
 * Integer square root using Newton's method (pure C99, no math.h)
 */
static uint64_t isqrt(uint64_t n) {
    if (n == 0) return 0;
    if (n < 4) return 1;

    uint64_t x = n;
    uint64_t y = (x + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }

    return x;
}

/**
 * Write reduced James Law CSV row (20 columns for window scaling experiment)
 *
 * This outputs only the subset of metrics needed for the James Law validation experiment.
 * Format matches the header in run_window_sweep.sh (minus the 5 script-prepended columns).
 *
 * Expected columns (20):
 *   workload_duration_ns, l8_mode_final, lambda_effective, K_statistic, K_deviation,
 *   entropy, cv, temporal_decay, stability_score, ns_per_word, total_runtime_ms,
 *   window_final, hot_word_count, avg_heat, bucket_mean_K, bucket_cv_K,
 *   bucket_window_var, bucket_heat_var, bucket_mode_transitions, bucket_collapse_flag
 */
void metrics_write_james_law_csv_row(FILE *out, const DoeMetrics *metrics, VM *vm) {
    /* Extract rolling window size for calculations */
    uint32_t rolling_window_size = metrics->rolling_window_size;
    if (rolling_window_size == 0) rolling_window_size = 1; /* Avoid division by zero */

    /* Calculate James Law metrics */
    uint32_t window_width = metrics->rolling_window_width;
    double lambda_effective = (double)window_width;
    double K_statistic = (window_width > 0) ? ((double)window_width / (double)rolling_window_size) : 1.0;
    double K_deviation = K_statistic - 1.0;

    /* Calculate derived metrics */
    double entropy = metrics->window_diversity_percent / 100.0;  /* Normalize to 0-1 */

    /* Coefficient of variation: use cache hit latency stddev / mean as proxy */
    double cv_metric = (metrics->cache_hit_latency_ns > 0)
        ? ((double)metrics->cache_hit_stddev_ns / (double)metrics->cache_hit_latency_ns)
        : 1.0;

    double temporal_decay = metrics->decay_slope;

    /* Stability score: inverse of stale word ratio (higher = more stable) */
    double stability_score = (metrics->stale_word_ratio < 1.0)
        ? (1.0 - metrics->stale_word_ratio) * 10000.0  /* Scale to match expected range */
        : 0.0;

    /* Performance: nanoseconds per word executed */
    double ns_per_word = (metrics->words_executed > 0)
        ? ((double)metrics->vm_workload_duration_ns_q48 / (double)metrics->words_executed)
        : 0.0;

    /* Get L8 mode from VM heartbeat state */
    uint8_t l8_mode_final = 1;  /* Default to mode 1 if unavailable */
    if (vm && vm->heartbeat.tick_buffer_write_index > 0) {
        /* Get last tick's L8 mode */
        uint32_t last_idx = (vm->heartbeat.tick_buffer_write_index - 1) % HEARTBEAT_TICK_BUFFER_SIZE;
        l8_mode_final = vm->heartbeat.tick_buffer[last_idx].l8_mode;
    }

    /* Calculate bucket aggregates from heartbeat state */
    double bucket_mean_K = 0.0;
    double bucket_cv_K = 0.0;
    double bucket_window_var = 0.0;
    double bucket_heat_var = 0.0;
    uint32_t bucket_mode_transitions = 0;
    int bucket_collapse_flag = 0;

    if (vm && vm->heartbeat.bucket_tick_count > 0) {
        uint32_t n = vm->heartbeat.bucket_tick_count;

        /* Mean K */
        bucket_mean_K = vm->heartbeat.bucket_sum_K / (double)n;

        /* CV of K: sqrt(variance) / mean using integer approximation */
        double mean_K_squared = bucket_mean_K * bucket_mean_K;
        double variance_K = (vm->heartbeat.bucket_sum_K_squared / (double)n) - mean_K_squared;
        if (variance_K > 0.0 && bucket_mean_K > 0.0) {
            /* Scale variance to avoid precision loss, compute integer sqrt, then scale back */
            uint64_t variance_scaled = (uint64_t)(variance_K * 1000000.0);
            uint64_t stddev_scaled = isqrt(variance_scaled);
            double stddev_K = (double)stddev_scaled / 1000.0;
            bucket_cv_K = (stddev_K / bucket_mean_K) * 100.0;  /* As percentage */
        }

        /* Window and heat variance (using accumulated sums as proxy) */
        bucket_window_var = vm->heartbeat.bucket_sum_window_variance;
        bucket_heat_var = vm->heartbeat.bucket_sum_heat_variance;

        bucket_mode_transitions = vm->heartbeat.bucket_mode_transitions;
        bucket_collapse_flag = vm->heartbeat.bucket_collapse_flag;
    }

    /* Write CSV row: 20 columns */
    fprintf(out,
        "%ld,%u,%.6f,%.6f,%.6f,"              /* workload_duration_ns, l8_mode_final, lambda_effective, K_statistic, K_deviation */
        "%.6f,%.6f,%.6f,%.6f,%.6f,"           /* entropy, cv, temporal_decay, stability_score, ns_per_word */
        "%lu,%u,%lu,%.6f,"                    /* total_runtime_ms, window_final, hot_word_count, avg_heat */
        "%.6f,%.6f,%.6f,%.6f,%u,%d\n",        /* bucket_mean_K, bucket_cv_K, bucket_window_var, bucket_heat_var, bucket_mode_transitions, bucket_collapse_flag */

        /* Row 1-5 */
        metrics->vm_workload_duration_ns_q48,
        l8_mode_final,
        lambda_effective,
        K_statistic,
        K_deviation,

        /* Row 6-10 */
        entropy,
        cv_metric,
        temporal_decay,
        stability_score,
        ns_per_word,

        /* Row 11-14 */
        metrics->total_runtime_ms,
        metrics->final_effective_window_size,
        metrics->hot_word_count,
        metrics->avg_word_heat,

        /* Row 15-20 */
        bucket_mean_K,
        bucket_cv_K,
        bucket_window_var,
        bucket_heat_var,
        bucket_mode_transitions,
        bucket_collapse_flag
    );
}