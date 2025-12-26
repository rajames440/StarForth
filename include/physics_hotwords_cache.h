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
 * physics_hotwords_cache.h
 *
 * Hot-words cache optimization: frequency-driven dictionary acceleration
 *
 * Compile-time switch to enable/disable the hot-words cache variant.
 * Allows measuring impact before/after on different build profiles.
 *
 * Usage:
 *   make clean && make ENABLE_HOTWORDS_CACHE=1  # With cache
 *   make clean && make ENABLE_HOTWORDS_CACHE=0  # Without cache
 *
 * Measure performance:
 *   BENCH-DICT-LOOKUP       (show baseline + stats)
 *   PHYSICS-SHOW-CACHE-STATS (show hit rate, evictions, etc)
 */

#ifndef PHYSICS_HOTWORDS_CACHE_H
#define PHYSICS_HOTWORDS_CACHE_H

#include <stdint.h>
#include "vm.h"

/* ============================================================================
 * Feature Switch
 * ============================================================================
 *
 * Set via -DENABLE_HOTWORDS_CACHE=1 during compilation
 * Default: enabled (can be disabled for baseline comparison)
 */

/* ============================================================================
 * Hot Words Cache Configuration
 * ============================================================================
 */

/** Maximum number of hot words to cache (typically 16-32) */
#define HOTWORDS_CACHE_SIZE 32

/** Execution heat threshold to consider a word "hot" (execution count) */
#define HOTWORDS_EXECUTION_HEAT_THRESHOLD 50

/** Reorder bucket when execution heat delta exceeds this (avoid thrashing) */
#define HOTWORDS_EXECUTION_HEAT_DELTA_THRESHOLD 100

/* ============================================================================
 * Statistics Structure
 * ============================================================================
 *
 * Tracks cache performance for measurement and diagnostics
 */

typedef struct {
    /** Total lookups performed */
    uint64_t total_lookups;

    /** Lookups that hit the hot-words cache */
    uint64_t cache_hits;

    /** Lookups that missed cache but found in bucket */
    uint64_t bucket_hits;

    /** Lookups that failed (word not found) */
    uint64_t misses;

    /** Words evicted from cache (LRU) */
    uint64_t evictions;

    /** Times a word was promoted to hot-words */
    uint64_t promotions;

    /** Times bucket was reordered */
    uint64_t bucket_reorders;

    /** === 64-bit Fixed-Point Statistics (Q48.16 format) ===
     *  Q48.16 = 48-bit integer + 16-bit fractional part
     *  Precision: 2^-16 ns ≈ 0.0000153 ns
     *  Range: ±140 trillion ns (≈ 4.4 years)
     *  Allows: Bayesian inference, precise averaging, statistical computation
     */

    /** Sum of cache hit latencies (64-bit fixed-point, Q48.16) */
    int64_t cache_hit_total_ns_q48;

    /** Sum of bucket search latencies (64-bit fixed-point, Q48.16) */
    int64_t bucket_search_total_ns_q48;

    /** Number of cache hit samples (for averaging) */
    uint64_t cache_hit_samples;

    /** Number of bucket search samples (for averaging) */
    uint64_t bucket_search_samples;

    /** Minimum cache hit latency (nanoseconds) */
    int64_t min_cache_hit_ns;

    /** Maximum cache hit latency (nanoseconds) */
    int64_t max_cache_hit_ns;

    /** Minimum bucket search latency (nanoseconds) */
    int64_t min_bucket_search_ns;

    /** Maximum bucket search latency (nanoseconds) */
    int64_t max_bucket_search_ns;

    /** Sum of squared cache hit latencies (for variance/stddev) */
    int64_t cache_hit_variance_sum_q48;

    /** Sum of squared bucket search latencies (for variance/stddev) */
    int64_t bucket_search_variance_sum_q48;
} HotwordsStats;

/* ============================================================================
 * Hot Words Cache State
 * ============================================================================
 *
 * Embedded in VM for global state
 */

typedef struct HotwordsCache_s {
    /** Cache entries (pointers to DictEntry) */
    DictEntry *cache[HOTWORDS_CACHE_SIZE];

    /** Number of valid entries in cache */
    uint32_t cache_count;

    /** LRU: last-used index for eviction */
    uint32_t lru_index;

    /** Performance statistics */
    HotwordsStats stats;

    /** Enabled/disabled flag (runtime toggle) */
    int enabled;
} HotwordsCache;

/* ============================================================================
 * Public API
 * ============================================================================
 */

/**
 * Initialize hot-words cache
 * Should be called during VM initialization
 */
void hotwords_cache_init(HotwordsCache *cache);

/**
 * Cleanup hot-words cache
 */
void hotwords_cache_cleanup(HotwordsCache *cache);

/**
 * Lookup word in cache first, then fall back to bucket search
 * Returns the found word, or NULL if not found
 *
 * This is the main function that replaces vm_find_word() logic
 */
DictEntry *hotwords_cache_lookup(HotwordsCache *cache,
                                  DictEntry **bucket,
                                  size_t bucket_count,
                                  const char *name,
                                  size_t len);

/**
 * Promote a word to the hot-words cache
 * Called when a word's execution heat exceeds threshold
 */
void hotwords_cache_promote(HotwordsCache *cache, DictEntry *word);

/**
 * Reorder a bucket by execution heat (move hot words forward)
 * Called when execution heat delta exceeds threshold
 */
void hotwords_bucket_reorder(DictEntry **bucket, size_t count);

/**
 * Reset statistics (for before/after comparison)
 */
void hotwords_stats_reset(HotwordsStats *stats);

/**
 * Print statistics to stdout
 */
void hotwords_stats_print(const HotwordsStats *stats);

/**
 * Enable/disable cache at runtime (for A/B testing)
 */
void hotwords_cache_set_enabled(HotwordsCache *cache, int enabled);

/* ============================================================================
 * Bayesian Inference Statistics
 * ============================================================================
 */

/**
 * Bayesian statistics structure for inference and credible intervals
 * All values stored in Q48.16 fixed-point (64-bit signed integer)
 * representing nanoseconds with 2^-16 ≈ 0.0000153 ns precision
 */
typedef struct {
    int64_t mean_ns_q48;         /* Mean latency (Q48.16) */
    int64_t stddev_ns_q48;       /* Standard deviation (Q48.16) */
    int64_t median_ns_q48;       /* Median (p50) (Q48.16) */
    int64_t credible_lower_95;   /* Lower bound of 95% credible interval (Q48.16) */
    int64_t credible_upper_95;   /* Upper bound of 95% credible interval (Q48.16) */
    int64_t credible_lower_99;   /* Lower bound of 99% credible interval (Q48.16) */
    int64_t credible_upper_99;   /* Upper bound of 99% credible interval (Q48.16) */
    uint64_t sample_count;       /* Number of samples in posterior */
} BayesianLatencyPosterior;

/**
 * Calculate posterior distribution for cache hits
 * Uses accumulated samples to compute credible intervals
 */
BayesianLatencyPosterior hotwords_posterior_cache_hits(const HotwordsStats *stats);

/**
 * Calculate posterior distribution for bucket searches
 * Uses accumulated samples to compute credible intervals
 */
BayesianLatencyPosterior hotwords_posterior_bucket_searches(const HotwordsStats *stats);

/**
 * Calculate speedup estimate with credible interval
 * All values in Q48.16 fixed-point format (64-bit signed integer)
 * Probabilities are in range [0, Q48_SCALE] representing [0%, 100%]
 */
struct SpeedupEstimate {
    int64_t speedup_factor_q48;      /* Bucket latency / Cache latency (Q48.16) */
    int64_t credible_lower_95_q48;   /* Lower bound (95% credible) (Q48.16) */
    int64_t credible_upper_95_q48;   /* Upper bound (95% credible) (Q48.16) */
    int64_t probability_gt_10pct_q48; /* P(speedup > 1.1) in Q48.16, [0, 65536] = [0%, 100%] */
    int64_t probability_gt_double_q48;/* P(speedup > 2.0) in Q48.16, [0, 65536] = [0%, 100%] */
};

/**
 * Calculate speedup estimate with uncertainty
 */
struct SpeedupEstimate hotwords_speedup_estimate(const HotwordsStats *stats);

/**
 * Generate detailed Bayesian inference report
 * Compares cache vs bucket performance with confidence measures
 */
void hotwords_bayesian_report(const HotwordsStats *stats_with_cache,
                              const HotwordsStats *stats_without_cache);

#endif /* PHYSICS_HOTWORDS_CACHE_H */
