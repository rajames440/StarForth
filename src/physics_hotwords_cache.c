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

/*
 * physics_hotwords_cache.c
 *
 * Hot-words cache implementation
 * Provides frequency-driven dictionary acceleration via LRU cache
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __STARKERNEL__
#include "starkernel/console.h"
#include "starkernel/hal/hal.h"
/* Forward declaration for canonical check */
static inline int sf_is_canonical(uint64_t addr) {
    int64_t saddr = (int64_t)addr;
    return (saddr >> 47) == 0 || (saddr >> 47) == -1;
}
/* Forward declaration for debug hex print if not already present or just use a local one */
static void sf_debug_print_hex(uint64_t value) {
    char buf[19];
    buf[0] = '0'; buf[1] = 'x'; buf[18] = '\0';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
    }
    console_puts(buf);
}
#endif

#include "physics_hotwords_cache.h"
#include "platform_time.h"
#include "math_portable.h"

/* ============================================================================
 * Global Cache Instance
 * ============================================================================
 */

/* Global hot-words cache for the VM (used by benchmark and diagnostic words) */
HotwordsCache g_hotwords_cache = {0};

/* Forward declarations for helper functions */
static inline double q48_to_ns_double(int64_t q48);

/* ============================================================================
 * Cache Initialization
 * ============================================================================
 */

void hotwords_cache_init(HotwordsCache *cache) {
    if (!cache) return;

    memset(cache, 0, sizeof(*cache));
    cache->enabled = ENABLE_HOTWORDS_CACHE;
    cache->cache_count = 0;
    cache->lru_index = 0;

    /* Initialize statistics with proper sentinel values */
    cache->stats.total_lookups = 0;
    cache->stats.cache_hits = 0;
    cache->stats.bucket_hits = 0;
    cache->stats.misses = 0;
    cache->stats.evictions = 0;
    cache->stats.promotions = 0;
    cache->stats.bucket_reorders = 0;

    /* Fixed-point statistics (Q48.16 format) */
    cache->stats.cache_hit_total_ns_q48 = 0;
    cache->stats.bucket_search_total_ns_q48 = 0;
    cache->stats.cache_hit_samples = 0;
    cache->stats.bucket_search_samples = 0;

    /* Min/max latencies */
    cache->stats.min_cache_hit_ns = INT64_MAX;
    cache->stats.max_cache_hit_ns = INT64_MIN;
    cache->stats.min_bucket_search_ns = INT64_MAX;
    cache->stats.max_bucket_search_ns = INT64_MIN;

    /* Variance accumulators (Q48.16) */
    cache->stats.cache_hit_variance_sum_q48 = 0;
    cache->stats.bucket_search_variance_sum_q48 = 0;
}

void hotwords_cache_cleanup(HotwordsCache *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

/* ============================================================================
 * Main Lookup: Cache + Bucket Search
 * ============================================================================
 */

DictEntry *hotwords_cache_lookup(HotwordsCache *cache,
                                  DictEntry **bucket,
                                  size_t bucket_count,
                                  const char *name,
                                  size_t len) {
#ifdef __STARKERNEL__
    console_println("[HOTWORDS] BEFORE MONOTONIC");
    if (!sf_time_backend || !sf_time_backend->get_monotonic_ns) {
        console_println("PANIC: sf_time_backend NULL or missing mono func");
        sk_hal_panic("sf_time_backend NULL or missing mono func");
    }
    if (!sf_is_canonical((uintptr_t)sf_time_backend->get_monotonic_ns)) {
        console_println("PANIC: mono_func non-canonical");
        sk_hal_panic("mono_func non-canonical");
    }
#endif
    uint64_t start_ns = sf_monotonic_ns();
#ifdef __STARKERNEL__
    console_println("[HOTWORDS] AFTER MONOTONIC");
#endif

    // Disabled? Go straight to bucket
    if (!cache || !cache->enabled) {
#ifdef __STARKERNEL__
        console_println("[HOTWORDS] cache disabled or null");
#endif
        for (size_t i = bucket_count; i-- > 0;) {
            DictEntry *e = bucket[i];
#ifdef __STARKERNEL__
            if (i % 10 == 0 || i == bucket_count - 1) {
                console_puts("[HOTWORDS] bucket[");
                sf_debug_print_hex(i);
                console_puts("]=");
                sf_debug_print_hex((uintptr_t)e);
                console_println("");
            }
#endif
            if ((size_t)e->name_len != len) continue;
            if (len > 1 && (unsigned char)e->name[len - 1] != (unsigned char)name[len - 1]) continue;
            if (memcmp(e->name, name, len) == 0) return e;
        }
        return NULL;
    }

    cache->stats.total_lookups++;

    // --- STEP 1: Check hot-words cache ---
    for (uint32_t i = 0; i < cache->cache_count; i++) {
        DictEntry *e = cache->cache[i];
        if (!e) continue;

        if ((size_t)e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != (unsigned char)name[len - 1]) continue;
        if (memcmp(e->name, name, len) == 0) {
            // Cache hit! Record latency with 64-bit fixed-point precision
            cache->stats.cache_hits++;
            int64_t elapsed_ns = (int64_t)(sf_monotonic_ns() - start_ns);

            /* Store in Q48.16 fixed-point format (shift left by 16 bits for fractional precision) */
            cache->stats.cache_hit_total_ns_q48 += (elapsed_ns << 16);
            cache->stats.cache_hit_samples++;

            /* Track min/max */
            if (elapsed_ns < cache->stats.min_cache_hit_ns) {
                cache->stats.min_cache_hit_ns = elapsed_ns;
            }
            if (elapsed_ns > cache->stats.max_cache_hit_ns) {
                cache->stats.max_cache_hit_ns = elapsed_ns;
            }

            /* Accumulate squared values for variance calculation (Bayesian posteriors) */
            int64_t elapsed_sq_q48 = (elapsed_ns * elapsed_ns) << 16;
            cache->stats.cache_hit_variance_sum_q48 += elapsed_sq_q48;

            return e;
        }
    }

    // --- STEP 2: Fall back to bucket search ---
    for (size_t i = bucket_count; i-- > 0;) {
        DictEntry *e = bucket[i];
        if ((size_t)e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != (unsigned char)name[len - 1]) continue;
        if (memcmp(e->name, name, len) == 0) {
            // Bucket hit! Record latency with 64-bit fixed-point precision
            cache->stats.bucket_hits++;
            int64_t elapsed_ns = (int64_t)(sf_monotonic_ns() - start_ns);

            /* Store in Q48.16 fixed-point format (shift left by 16 bits for fractional precision) */
            cache->stats.bucket_search_total_ns_q48 += (elapsed_ns << 16);
            cache->stats.bucket_search_samples++;

            /* Track min/max */
            if (elapsed_ns < cache->stats.min_bucket_search_ns) {
                cache->stats.min_bucket_search_ns = elapsed_ns;
            }
            if (elapsed_ns > cache->stats.max_bucket_search_ns) {
                cache->stats.max_bucket_search_ns = elapsed_ns;
            }

            /* Accumulate squared values for variance calculation (Bayesian posteriors) */
            int64_t elapsed_sq_q48 = (elapsed_ns * elapsed_ns) << 16;
            cache->stats.bucket_search_variance_sum_q48 += elapsed_sq_q48;

            // If hot enough, promote to cache
            if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD) {
                hotwords_cache_promote(cache, e);
            }

            return e;
        }
    }

    // Miss
    cache->stats.misses++;
    return NULL;
}

/* ============================================================================
 * Cache Promotion (LRU eviction)
 * ============================================================================
 */

void hotwords_cache_promote(HotwordsCache *cache, DictEntry *word) {
    if (!cache || !word || cache->cache_count >= HOTWORDS_CACHE_SIZE) {
        if (cache && cache->cache_count >= HOTWORDS_CACHE_SIZE) {
            // LRU eviction: remove oldest entry (round-robin)
            cache->cache[cache->lru_index] = word;
            cache->lru_index = (cache->lru_index + 1) % HOTWORDS_CACHE_SIZE;
            cache->stats.evictions++;
        }
        return;
    }

    // Check if already in cache
    for (uint32_t i = 0; i < cache->cache_count; i++) {
        if (cache->cache[i] == word) {
            return;  // Already cached
        }
    }

    // Add to cache
    cache->cache[cache->cache_count++] = word;
    cache->stats.promotions++;
}

/* ============================================================================
 * Bucket Reordering (move hot words forward)
 * ============================================================================
 */

void hotwords_bucket_reorder(DictEntry **bucket, size_t count) {
    if (!bucket || count < 2) return;

    // Simple bubble sort: move high-execution-heat words forward
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            if (bucket[j]->execution_heat < bucket[j + 1]->execution_heat) {
                // Swap
                DictEntry *tmp = bucket[j];
                bucket[j] = bucket[j + 1];
                bucket[j + 1] = tmp;
            }
        }
    }
}

/* ============================================================================
 * Statistics
 * ============================================================================
 */

void hotwords_stats_reset(HotwordsStats *stats) {
    if (!stats) return;

    /* Reset counters */
    stats->total_lookups = 0;
    stats->cache_hits = 0;
    stats->bucket_hits = 0;
    stats->misses = 0;
    stats->evictions = 0;
    stats->promotions = 0;
    stats->bucket_reorders = 0;

    /* Reset fixed-point accumulators */
    stats->cache_hit_total_ns_q48 = 0;
    stats->bucket_search_total_ns_q48 = 0;
    stats->cache_hit_samples = 0;
    stats->bucket_search_samples = 0;

    /* Reset min/max latencies */
    stats->min_cache_hit_ns = INT64_MAX;
    stats->max_cache_hit_ns = INT64_MIN;
    stats->min_bucket_search_ns = INT64_MAX;
    stats->max_bucket_search_ns = INT64_MIN;

    /* Reset variance accumulators */
    stats->cache_hit_variance_sum_q48 = 0;
    stats->bucket_search_variance_sum_q48 = 0;
}

void hotwords_stats_print(const HotwordsStats *stats) {
    if (!stats) return;

    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║      Hot-Words Cache Statistics (64-bit Fixed-Point Q48.16)    ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    uint64_t total = stats->cache_hits + stats->bucket_hits + stats->misses;
    if (total == 0) {
        printf("No lookups performed.\n");
        return;
    }

    double cache_hit_pct = 100.0 * stats->cache_hits / total;
    double bucket_hit_pct = 100.0 * stats->bucket_hits / total;
    double miss_pct = 100.0 * stats->misses / total;

    printf("LOOKUPS (Statistically Valid Sample Size):\n");
    printf("  Total:         %lu\n", total);
    printf("  Cache hits:    %lu (%.2f%%)\n", stats->cache_hits, cache_hit_pct);
    printf("  Bucket hits:   %lu (%.2f%%)\n", stats->bucket_hits, bucket_hit_pct);
    printf("  Misses:        %lu (%.2f%%)\n", stats->misses, miss_pct);

    printf("\nLATENCY STATISTICS (64-bit Fixed-Point Precision):\n");

    /* Cache hit latency (Q48.16 format: shift right by 16 bits to get nanoseconds) */
    if (stats->cache_hit_samples > 0) {
        int64_t avg_cache_ns_q48 = stats->cache_hit_total_ns_q48 / (int64_t)stats->cache_hit_samples;
        double avg_cache_ns = (double)avg_cache_ns_q48 / 65536.0;  /* Q48.16 to double */
        double min_cache_ns = (double)stats->min_cache_hit_ns;
        double max_cache_ns = (double)stats->max_cache_hit_ns;

        printf("  Cache Hits (%lu samples):\n", stats->cache_hit_samples);
        printf("    Min:  %.3f ns\n", min_cache_ns);
        printf("    Avg:  %.3f ns\n", avg_cache_ns);
        printf("    Max:  %.3f ns\n", max_cache_ns);

        /* Calculate variance and standard deviation */
        int64_t variance_sum_q48 = stats->cache_hit_variance_sum_q48 / (int64_t)stats->cache_hit_samples;
        int64_t mean_sq_q48 = (avg_cache_ns_q48 * avg_cache_ns_q48) / (1LL << 16);
        int64_t var_q48 = variance_sum_q48 - mean_sq_q48;
        int64_t stddev_q48 = (var_q48 >= 0) ? sqrt_q48(var_q48) : 0;
        double stddev = q48_to_ns_double(stddev_q48);
        printf("    StdDev: %.3f ns\n", stddev);
    }

    if (stats->bucket_search_samples > 0) {
        int64_t avg_bucket_ns_q48 = stats->bucket_search_total_ns_q48 / (int64_t)stats->bucket_search_samples;
        double avg_bucket_ns = (double)avg_bucket_ns_q48 / 65536.0;  /* Q48.16 to double */
        double min_bucket_ns = (double)stats->min_bucket_search_ns;
        double max_bucket_ns = (double)stats->max_bucket_search_ns;

        printf("  Bucket Searches (%lu samples):\n", stats->bucket_search_samples);
        printf("    Min:  %.3f ns\n", min_bucket_ns);
        printf("    Avg:  %.3f ns\n", avg_bucket_ns);
        printf("    Max:  %.3f ns\n", max_bucket_ns);

        /* Calculate variance and standard deviation */
        int64_t variance_sum_q48 = stats->bucket_search_variance_sum_q48 / (int64_t)stats->bucket_search_samples;
        int64_t mean_sq_q48 = (avg_bucket_ns_q48 * avg_bucket_ns_q48) / (1LL << 16);
        int64_t var_q48 = variance_sum_q48 - mean_sq_q48;
        int64_t stddev_q48 = (var_q48 >= 0) ? sqrt_q48(var_q48) : 0;
        double stddev = q48_to_ns_double(stddev_q48);
        printf("    StdDev: %.3f ns\n", stddev);

        /* Speedup calculation */
        if (stats->cache_hit_samples > 0) {
            int64_t avg_cache_ns_q48 = stats->cache_hit_total_ns_q48 / (int64_t)stats->cache_hit_samples;
            double avg_bucket_ns_dbl = (double)avg_bucket_ns_q48 / 65536.0;
            double speedup = avg_bucket_ns_dbl / ((double)avg_cache_ns_q48 / 65536.0);
            printf("  Speedup:        %.2f× (bucket vs cache)\n", speedup);
        }
    }

    printf("\nCACHE MANAGEMENT:\n");
    printf("  Promotions:    %lu\n", stats->promotions);
    printf("  Evictions:     %lu\n", stats->evictions);
    printf("  Reorders:      %lu\n", stats->bucket_reorders);

    printf("\n════════════════════════════════════════════════════════════════\n\n");
}

void hotwords_cache_set_enabled(HotwordsCache *cache, int enabled) {
    if (!cache) return;
    cache->enabled = enabled;
}

/* ============================================================================
 * Bayesian Inference Implementation
 * ============================================================================
 */

/**
 * Helper: Convert Q48.16 fixed-point value to nanoseconds (double) for display only
 * NOT used in calculations - all calculations stay in fixed-point
 */
static inline double q48_to_ns_double(int64_t q48) {
    return (double)q48 / 65536.0;
}

/**
 * Calculate statistics for a single distribution (cache hits or bucket searches)
 * Returns posterior with all values in Q48.16 fixed-point format
 * Pure fixed-point arithmetic - no floating-point operations
 */
static BayesianLatencyPosterior calculate_posterior(
    int64_t total_ns_q48,
    int64_t variance_sum_q48,
    int64_t min_ns,
    int64_t max_ns,
    uint64_t sample_count) {

    const int64_t Z95 = 128431LL;  /* 1.96 * 65536 */
    const int64_t Z99 = 168888LL;  /* 2.576 * 65536 */

    BayesianLatencyPosterior posterior = {0};
    posterior.sample_count = sample_count;

    if (sample_count == 0) {
        return posterior;
    }

    /* Calculate mean from accumulated Q48.16 values (pure integer division) */
    int64_t mean_q48 = total_ns_q48 / (int64_t)sample_count;
    posterior.mean_ns_q48 = mean_q48;

    /* Calculate variance: E[X^2] - E[X]^2, all in Q48.16 */
    int64_t mean_sq_q48 = (mean_q48 * mean_q48) >> 16;
    int64_t variance_sum_corrected = variance_sum_q48 / (int64_t)sample_count;
    int64_t variance_q48 = variance_sum_corrected - mean_sq_q48;

    if (variance_q48 < 0) variance_q48 = 0;  /* Guard numerical errors */

    /* Calculate standard deviation using Q48.16 sqrt */
    int64_t stddev_q48 = sqrt_q48(variance_q48);
    posterior.stddev_ns_q48 = stddev_q48;

    /* Median: average of min/max */
    posterior.median_ns_q48 = (min_ns + max_ns) / 2;

    /* Standard error: stddev / sqrt(N) in Q48.16 */
    /* sqrt(sample_count) approximation for Q48.16 arithmetic */
    int64_t sqrt_n_q48 = sqrt_q48((int64_t)sample_count << 16);  /* Convert N to Q48.16 */
    int64_t se_q48 = (stddev_q48 << 16) / (sqrt_n_q48 >> 16);  /* Careful division */

    /* 95% credible interval: mean ± Z95 * SE (Z95 = 1.96 << 16) */
    int64_t margin_95 = (Z95 * se_q48) >> 16;
    posterior.credible_lower_95 = mean_q48 - margin_95;
    posterior.credible_upper_95 = mean_q48 + margin_95;

    /* 99% credible interval: mean ± Z99 * SE (Z99 = 2.576 << 16) */
    int64_t margin_99 = (Z99 * se_q48) >> 16;
    posterior.credible_lower_99 = mean_q48 - margin_99;
    posterior.credible_upper_99 = mean_q48 + margin_99;

    /* Clamp lower bounds to >= 0 */
    if (posterior.credible_lower_95 < 0) posterior.credible_lower_95 = 0;
    if (posterior.credible_lower_99 < 0) posterior.credible_lower_99 = 0;

    return posterior;
}

/**
 * Calculate posterior for cache hits
 */
BayesianLatencyPosterior hotwords_posterior_cache_hits(const HotwordsStats *stats) {
    if (!stats || stats->cache_hit_samples == 0) {
        BayesianLatencyPosterior empty = {0};
        return empty;
    }

    return calculate_posterior(
        stats->cache_hit_total_ns_q48,
        stats->cache_hit_variance_sum_q48,
        stats->min_cache_hit_ns,
        stats->max_cache_hit_ns,
        stats->cache_hit_samples
    );
}

/**
 * Calculate posterior for bucket searches
 */
BayesianLatencyPosterior hotwords_posterior_bucket_searches(const HotwordsStats *stats) {
    if (!stats || stats->bucket_search_samples == 0) {
        BayesianLatencyPosterior empty = {0};
        return empty;
    }

    return calculate_posterior(
        stats->bucket_search_total_ns_q48,
        stats->bucket_search_variance_sum_q48,
        stats->min_bucket_search_ns,
        stats->max_bucket_search_ns,
        stats->bucket_search_samples
    );
}

/**
 * Calculate speedup estimate with credible intervals
 * Pure 64-bit fixed-point Q48.16 arithmetic throughout
 * All results_run_01_2025_12_08 FORTH-compatible: can be pushed to stack and manipulated by FORTH
 */
struct SpeedupEstimate hotwords_speedup_estimate(const HotwordsStats *stats) {
    const int64_t Q48 = 65536LL;
    const int64_t SQRT2 = 92681LL;  /* sqrt(2) * 65536 ≈ 1.414 * 65536 */

    struct SpeedupEstimate est = {0};

    if (!stats || stats->cache_hit_samples == 0 || stats->bucket_search_samples == 0) {
        est.speedup_factor_q48 = Q48;  /* Default: 1.0x speedup */
        return est;
    }

    BayesianLatencyPosterior cache_post = hotwords_posterior_cache_hits(stats);
    BayesianLatencyPosterior bucket_post = hotwords_posterior_bucket_searches(stats);

    /* Guard divide-by-zero */
    if (cache_post.mean_ns_q48 < 1) cache_post.mean_ns_q48 = 1;

    /* Point estimate: speedup = bucket_time / cache_time (dimensionless, in Q48.16) */
    int64_t speedup_q48 = (bucket_post.mean_ns_q48 << 16) / cache_post.mean_ns_q48;
    est.speedup_factor_q48 = speedup_q48;

    /* Delta-method SE for ratio: SE(speedup) ≈ speedup * sqrt((SE_bucket/bucket)^2 + (SE_cache/cache)^2) */
    int64_t se_cache_q48 = (cache_post.stddev_ns_q48 << 16) / (int64_t)stats->cache_hit_samples;
    int64_t se_bucket_q48 = (bucket_post.stddev_ns_q48 << 16) / (int64_t)stats->bucket_search_samples;

    /* Relative standard errors: SE/mean */
    int64_t rel_se_cache_q48 = (se_cache_q48 << 16) / cache_post.mean_ns_q48;
    int64_t rel_se_bucket_q48 = (se_bucket_q48 << 16) / bucket_post.mean_ns_q48;

    /* Combine: sqrt((rel_se_cache)^2 + (rel_se_bucket)^2) */
    int64_t sum_sq = ((rel_se_cache_q48 * rel_se_cache_q48) >> 16) +
                     ((rel_se_bucket_q48 * rel_se_bucket_q48) >> 16);
    int64_t rel_se_ratio_q48 = sqrt_q48(sum_sq);

    /* SE(speedup) = speedup * rel_se_ratio */
    int64_t se_speedup_q48 = (speedup_q48 * rel_se_ratio_q48) >> 16;

    /* 95% credible interval for speedup (Z1.96 = 128431 in Q48.16) */
    int64_t margin_95_q48 = (128431LL * se_speedup_q48) >> 16;
    est.credible_lower_95_q48 = speedup_q48 - margin_95_q48;
    est.credible_upper_95_q48 = speedup_q48 + margin_95_q48;

    if (est.credible_lower_95_q48 < 0) est.credible_lower_95_q48 = 0;

    /* Probability of speedup > 1.1× using erf_q48 (pure fixed-point) */
    /* Z-score: (1.1 - speedup) / SE_speedup, computed in Q48.16 */
    /* Result: erf returns Q48.16 in [-65536, 65536], convert to probability [0, 65536] */
    int64_t target_110_q48 = 72090LL;  /* 1.1 * 65536 */
    int64_t z_10pct_q48 = ((target_110_q48 - speedup_q48) << 16) / se_speedup_q48;
    int64_t erf_val_10pct = erf_q48((-z_10pct_q48) / SQRT2);
    /* Convert erf result to probability: P = 0.5 * (1 + erf) in Q48.16 */
    est.probability_gt_10pct_q48 = (Q48 + erf_val_10pct) >> 1;

    /* Probability of speedup > 2.0× */
    int64_t target_200_q48 = 131072LL;  /* 2.0 * 65536 */
    int64_t z_2x_q48 = ((target_200_q48 - speedup_q48) << 16) / se_speedup_q48;
    int64_t erf_val_2x = erf_q48((-z_2x_q48) / SQRT2);
    est.probability_gt_double_q48 = (Q48 + erf_val_2x) >> 1;

    return est;
}

/**
 * Generate detailed Bayesian inference report comparing with/without cache
 */
void hotwords_bayesian_report(const HotwordsStats *stats_with_cache,
                              const HotwordsStats *stats_without_cache) {
    if (!stats_with_cache || !stats_without_cache) return;

    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║         Bayesian Performance Analysis: Cache Impact            ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    /* WITH CACHE statistics */
    BayesianLatencyPosterior cache_with = hotwords_posterior_cache_hits(stats_with_cache);
    BayesianLatencyPosterior bucket_with = hotwords_posterior_bucket_searches(stats_with_cache);

    printf("WITH CACHE ENABLED:\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("Cache Hits (N=%lu):\n", cache_with.sample_count);
    printf("  Mean:           %.2f ns\n", q48_to_ns_double(cache_with.mean_ns_q48));
    printf("  Std Dev:        %.2f ns\n", q48_to_ns_double(cache_with.stddev_ns_q48));
    printf("  95%% Credible:   [%.2f, %.2f] ns\n", q48_to_ns_double(cache_with.credible_lower_95), q48_to_ns_double(cache_with.credible_upper_95));
    printf("  99%% Credible:   [%.2f, %.2f] ns\n\n", q48_to_ns_double(cache_with.credible_lower_99), q48_to_ns_double(cache_with.credible_upper_99));

    printf("Bucket Searches (N=%lu):\n", bucket_with.sample_count);
    printf("  Mean:           %.2f ns\n", q48_to_ns_double(bucket_with.mean_ns_q48));
    printf("  Std Dev:        %.2f ns\n", q48_to_ns_double(bucket_with.stddev_ns_q48));
    printf("  95%% Credible:   [%.2f, %.2f] ns\n", q48_to_ns_double(bucket_with.credible_lower_95), q48_to_ns_double(bucket_with.credible_upper_95));
    printf("  99%% Credible:   [%.2f, %.2f] ns\n\n", q48_to_ns_double(bucket_with.credible_lower_99), q48_to_ns_double(bucket_with.credible_upper_99));

    /* WITHOUT CACHE statistics */
    printf("WITHOUT CACHE (Baseline):\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("All Lookups (N=%lu):\n", stats_without_cache->cache_hits + stats_without_cache->bucket_hits);

    /* Estimate average latency without cache (all go to bucket) */
    BayesianLatencyPosterior all_lookups_baseline = hotwords_posterior_bucket_searches(stats_without_cache);
    if (all_lookups_baseline.sample_count == 0) {
        /* Fallback if bucket data not available */
        all_lookups_baseline.mean_ns_q48 = bucket_with.mean_ns_q48;
    }

    printf("  Mean:           %.2f ns\n", q48_to_ns_double(all_lookups_baseline.mean_ns_q48));
    printf("  Std Dev:        %.2f ns\n", q48_to_ns_double(all_lookups_baseline.stddev_ns_q48));
    printf("  95%% Credible:   [%.2f, %.2f] ns\n\n", q48_to_ns_double(all_lookups_baseline.credible_lower_95), q48_to_ns_double(all_lookups_baseline.credible_upper_95));

    /* Speedup Analysis */
    struct SpeedupEstimate speedup = hotwords_speedup_estimate(stats_with_cache);

    printf("SPEEDUP ESTIMATE (Bucket vs Cache):\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  Point Estimate:         %.2f×\n", q48_to_ns_double(speedup.speedup_factor_q48));
    printf("  95%% Credible Interval: [%.2f×, %.2f×]\n", q48_to_ns_double(speedup.credible_lower_95_q48), q48_to_ns_double(speedup.credible_upper_95_q48));
    printf("  P(Speedup > 1.1×):      %.1f%%  (credible improvement exists)\n", q48_to_ns_double(speedup.probability_gt_10pct_q48) * 100.0);
    printf("  P(Speedup > 2.0×):      %.1f%%  (significant speedup)\n\n", q48_to_ns_double(speedup.probability_gt_double_q48) * 100.0);

    /* Hit Rate Analysis */
    uint64_t total_lookups_with = stats_with_cache->cache_hits + stats_with_cache->bucket_hits + stats_with_cache->misses;
    double cache_hit_rate = (total_lookups_with > 0) ? (100.0 * stats_with_cache->cache_hits / total_lookups_with) : 0.0;

    printf("CACHE EFFECTIVENESS:\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  Total Lookups:          %lu\n", total_lookups_with);
    printf("  Cache Hits:             %lu (%.1f%%)\n", stats_with_cache->cache_hits, cache_hit_rate);
    printf("  Bucket Hits:            %lu (%.1f%%)\n", stats_with_cache->bucket_hits,
           100.0 * stats_with_cache->bucket_hits / total_lookups_with);
    printf("  Misses:                 %lu (%.1f%%)\n\n", stats_with_cache->misses,
           100.0 * stats_with_cache->misses / total_lookups_with);

    /* Overall Performance */
    double mixed_avg_latency = ((double)stats_with_cache->cache_hits * q48_to_ns_double(cache_with.mean_ns_q48) +
                                (double)stats_with_cache->bucket_hits * q48_to_ns_double(bucket_with.mean_ns_q48)) /
                               (stats_with_cache->cache_hits + stats_with_cache->bucket_hits);
    double baseline_avg_latency = q48_to_ns_double(all_lookups_baseline.mean_ns_q48);
    double overall_speedup = baseline_avg_latency / mixed_avg_latency;

    printf("OVERALL PERFORMANCE (WITH vs WITHOUT CACHE):\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  Baseline (all bucket):  %.2f ns/lookup\n", baseline_avg_latency);
    printf("  With cache:             %.2f ns/lookup\n", mixed_avg_latency);
    printf("  Overall Speedup:        %.2f×\n", overall_speedup);
    printf("  Time Saved per Lookup:  %.2f ns (%.1f%%)\n\n", baseline_avg_latency - mixed_avg_latency,
           100.0 * (baseline_avg_latency - mixed_avg_latency) / baseline_avg_latency);

    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
}