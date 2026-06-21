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

#include "physics_hotwords_cache.h"
/* In kernel builds, suppress platform_time.h inline wrappers to avoid
 * GOTPCREL relocations in PE binaries (no GOT in PE format). shim.c
 * provides the concrete sf_monotonic_ns() symbol in that case.
 * PLATFORM_TIME_NO_INLINE is also set globally via COMMON_CFLAGS in
 * Makefile.starkernel; the #ifndef guard prevents -Wall -Werror redefinition. */
#ifdef __STARKERNEL__
#ifndef PLATFORM_TIME_NO_INLINE
#define PLATFORM_TIME_NO_INLINE
#endif
#endif /* __STARKERNEL__ */
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

/**
 * @brief Initialise a @c HotwordsCache to its empty, enabled state.
 *
 * Zeroes all fields via @c memset then explicitly sets:
 * - @c cache->enabled = @c ENABLE_HOTWORDS_CACHE (build-time compile flag;
 *   0 disables the cache globally without a code change).
 * - @c cache->cache_count = 0, @c cache->lru_index = 0 — empty round-robin
 *   eviction ring.
 * - All counter fields in @c cache->stats to 0.
 * - @c stats.min_cache_hit_ns = @c INT64_MAX, @c stats.max_cache_hit_ns =
 *   @c INT64_MIN (sentinel values; updated to real observations on the first
 *   cache hit).
 * - @c stats.min_bucket_search_ns = @c INT64_MAX, @c stats.max_bucket_search_ns
 *   = @c INT64_MIN (same sentinel convention for bucket searches).
 * - All Q48.16 variance accumulators to 0.
 *
 * Safe to call multiple times (idempotent via @c memset). Must be called
 * before the first @c hotwords_cache_lookup() invocation; failure to do so
 * leaves the sentinel min/max fields uninitialised, producing nonsense in the
 * Bayesian posterior report.
 *
 * @param cache  Cache to initialise; no-op if NULL.
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

/**
 * @brief Zero all fields in a @c HotwordsCache, releasing no heap memory.
 *
 * Calls @c memset(cache, 0, sizeof(*cache)) to scrub every field including
 * the inline @c cache[] pointer array and all statistics. The cache holds no
 * separately allocated memory (the @c DictEntry pointers inside @c cache[]
 * are owned by the dictionary, not the cache), so no @c free() calls are
 * needed.
 *
 * Intended for use at VM teardown or when the caller wants to reset the cache
 * to a known-zero state between benchmark runs. After this call the cache
 * is in an unusable state (@c enabled = 0); call @c hotwords_cache_init()
 * to reinitialise it.
 *
 * @param cache  Cache to zero; no-op if NULL.
 */
void hotwords_cache_cleanup(HotwordsCache *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

/* ============================================================================
 * Main Lookup: Cache + Bucket Search
 * ============================================================================
 */

/**
 * @brief Look up a FORTH word using the hot-words cache, falling back to the bucket.
 *
 * The primary dictionary fast-path. Records a start timestamp via
 * @c sf_monotonic_ns() and then executes a two-stage search:
 *
 * **Stage 1 — Cache check (O(HOTWORDS_CACHE_SIZE)):**
 * Scans the @c cache->cache[] ring for an entry whose @c name_len and name
 * match @p name/@p len, using the cheap-first filter (@c name_len →
 * last-character → @c memcmp). On a hit the elapsed nanoseconds are
 * converted to Q48.16 and accumulated into @c stats.cache_hit_total_ns_q48;
 * the squared value is accumulated into @c stats.cache_hit_variance_sum_q48
 * for Bayesian posterior computation by @c calculate_posterior(). Min/max
 * latency sentinels are also updated.
 *
 * **Stage 2 — Bucket fallback (O(bucket_count)):**
 * Scans @p bucket[] in reverse-insertion order (newest definition wins).
 * On a hit the elapsed time is recorded in the @c bucket_search_* accumulators
 * with the same Q48.16 and variance treatment as Stage 1. If the found
 * word's @c execution_heat exceeds @c HOTWORDS_EXECUTION_HEAT_THRESHOLD it
 * is promoted into the cache via @c hotwords_cache_promote().
 *
 * **Disabled path:** If @c cache is NULL or @c cache->enabled is 0, the
 * function skips Stage 1 and performs a bare reverse-linear scan of @p bucket[]
 * returning the result immediately (no statistics are recorded).
 *
 * A miss increments @c stats.misses and returns NULL.
 *
 * @param cache         Hot-words cache; may be NULL or disabled (falls back
 *                      to bucket scan without statistics).
 * @param bucket        First-character hash bucket — array of @c DictEntry*
 *                      pointers, searched in reverse order.
 * @param bucket_count  Number of valid pointers in @p bucket.
 * @param name          Word name to find (not NUL-terminated).
 * @param len           Length of @p name in bytes.
 * @return              Matching @c DictEntry*, or NULL if not found.
 */
DictEntry *hotwords_cache_lookup(HotwordsCache *cache,
                                  DictEntry **bucket,
                                  size_t bucket_count,
                                  const char *name,
                                  size_t len) {
    uint64_t start_ns = sf_monotonic_ns();

    // Disabled? Go straight to bucket
    if (!cache || !cache->enabled) {
        for (size_t i = bucket_count; i-- > 0;) {
            DictEntry *e = bucket[i];
            if (!e) continue;
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
        if (!e) continue;
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

/**
 * @brief Promote a @c DictEntry into the hot-words cache, evicting the oldest entry if full.
 *
 * Two paths depending on whether the cache has a free slot:
 *
 * - **Slot available** (@c cache->cache_count < @c HOTWORDS_CACHE_SIZE):
 *   Appends @p word at @c cache->cache[cache_count++] after verifying it is
 *   not already present (linear scan). Increments @c stats.promotions.
 *
 * - **Cache full** (@c cache->cache_count >= @c HOTWORDS_CACHE_SIZE):
 *   Overwrites @c cache->cache[cache->lru_index] with @p word (round-robin
 *   least-recently-used eviction) and advances @c lru_index modulo
 *   @c HOTWORDS_CACHE_SIZE. Increments @c stats.evictions. The evicted entry
 *   is not freed — it remains alive in the dictionary; only its slot in the
 *   cache ring is surrendered.
 *
 * Called from @c hotwords_cache_lookup() whenever a bucket search finds a
 * word hot enough to warrant caching (heat > @c HOTWORDS_EXECUTION_HEAT_THRESHOLD).
 *
 * @param cache  The cache to promote into; no-op if NULL.
 * @param word   The @c DictEntry to promote; no-op if NULL. Ownership is not
 *               transferred — the cache holds a borrowed pointer into the
 *               dictionary, which owns the entry.
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

/**
 * @brief Sort a hash bucket in-place by descending execution heat (bubble sort).
 *
 * Performs a classic in-place bubble sort over the @p bucket array so that
 * words with higher @c execution_heat appear at lower array indices. The
 * hot-words cache and heat-stratified lookup both benefit from having the
 * hottest words at the front of each bucket: the first-character hash buckets
 * are linear arrays, and placing hot words up front reduces the scan depth on
 * Stage 1 cache misses that fall through to Stage 2 bucket search.
 *
 * Bubble sort is chosen deliberately: buckets are small (typically single
 * digits to low dozens of words per first-character class), already nearly
 * sorted after the previous reorganization pass, and the cost of a O(n²)
 * inner loop on n ≤ 30 is negligible compared to the outer @c qsort on
 * all 256 buckets in @c dict_reorganize_buckets_by_heat(). For the individual
 * per-bucket call path (e.g. after a single promotion), bubble sort avoids
 * the @c malloc overhead that @c qsort's temporary storage would require.
 *
 * No-op for @p count < 2.
 *
 * @param bucket  Array of @c DictEntry* pointers to sort in-place.
 *                NULL-check is the caller's responsibility.
 * @param count   Number of valid entries in @p bucket.
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

/**
 * @brief Zero all fields in a @c HotwordsStats structure.
 *
 * Resets every counter (@c total_lookups, @c cache_hits, @c bucket_hits,
 * @c misses, @c evictions, @c promotions, @c bucket_reorders) to zero,
 * resets all Q48.16 accumulator fields (@c cache_hit_total_ns_q48,
 * @c bucket_search_total_ns_q48, @c cache_hit_variance_sum_q48,
 * @c bucket_search_variance_sum_q48) to zero, resets sample counts to zero,
 * and restores the min/max latency sentinels:
 * - @c min_*_ns = @c INT64_MAX
 * - @c max_*_ns = @c INT64_MIN
 *
 * Intended for DoE benchmark runs that need a clean statistical baseline
 * between treatment cells. The parent @c HotwordsCache also holds the
 * @c cache[] pointer array and LRU state; those are not touched — only the
 * @c HotwordsStats sub-struct is cleared.
 *
 * @param stats  Statistics structure to reset; no-op if NULL.
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

/**
 * @brief Print a formatted hot-words cache statistics report to @c stdout.
 *
 * Emits a Unicode box-drawing table covering three sections:
 *
 * **LOOKUPS** — total, cache hits, bucket hits, misses with percentages.
 *
 * **LATENCY STATISTICS** — for each of cache-hits and bucket-searches
 * (when their sample counts are non-zero):
 * - Min / avg / max in nanoseconds, derived from the Q48.16 accumulators
 *   by right-shifting 16 bits: @c avg_ns = @c total_ns_q48 / samples >> 16.
 * - Standard deviation computed from the Welford-style variance accumulator:
 *   @c var = E[X²] − E[X]², then @c stddev = sqrt_q48(var), converted to
 *   nanoseconds via @c q48_to_ns_double() for display only.
 * - Speedup ratio (bucket avg / cache avg) when both sample sets are present.
 *
 * **CACHE MANAGEMENT** — promotions, evictions, and bucket reorder counts.
 *
 * If @p stats is NULL or the total lookup count is zero, prints a brief
 * "No lookups performed." notice instead of the full table. All floating-point
 * usage in this function is display-only (via @c q48_to_ns_double()); the
 * underlying accumulators remain in Q48.16 integer format.
 *
 * @param stats  Statistics to report; no-op if NULL.
 */
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

/**
 * @brief Enable or disable the hot-words cache at runtime.
 *
 * Sets @c cache->enabled to @p enabled (non-zero = active, 0 = bypassed).
 * When disabled, @c hotwords_cache_lookup() skips Stage 1 entirely and
 * falls through directly to a bare bucket scan, recording no statistics.
 * This allows benchmark harnesses to compare cache-on vs. cache-off behaviour
 * without rebuilding the binary.
 *
 * @param cache    Cache to configure; no-op if NULL.
 * @param enabled  Non-zero to enable the cache; 0 to disable.
 */
void hotwords_cache_set_enabled(HotwordsCache *cache, int enabled) {
    if (!cache) return;
    cache->enabled = enabled;
}

/* ============================================================================
 * Bayesian Inference Implementation
 * ============================================================================
 */

/**
 * @brief Convert a Q48.16 fixed-point value to a @c double nanosecond count.
 *
 * Divides @p q48 by 65536.0 to recover the real-valued nanosecond count.
 * This is the only floating-point operation in the module and is intentionally
 * confined to display paths — @c hotwords_stats_print() and
 * @c hotwords_bayesian_report() — so that all internal accumulation and
 * statistical computation remains in Q48.16 integer arithmetic.
 *
 * @param q48  Value in Q48.16 format (1.0 ns = 65536).
 * @return     The same quantity as a @c double in nanoseconds.
 */
static inline double q48_to_ns_double(int64_t q48) {
    return (double)q48 / 65536.0;
}

/**
 * @brief Compute a Bayesian latency posterior for one measurement distribution.
 *
 * Given the accumulated Q48.16 statistics for a set of @p sample_count
 * timing observations (either cache-hit or bucket-search latencies), computes
 * the following posterior quantities entirely in Q48.16 integer arithmetic:
 *
 * - **Mean** (@c mean_ns_q48): @c total_ns_q48 / sample_count.
 * - **Variance** (@c stddev_ns_q48): @c E[X²] − @c E[X]² where @c E[X²] is
 *   the per-sample average of @p variance_sum_q48 and @c E[X]² =
 *   @c mean² scaled to Q48.16 via @c >> 16. Variance is clamped to 0 to
 *   guard numerical rounding.
 * - **Standard deviation** (@c stddev_ns_q48): @c sqrt_q48(variance).
 * - **Median** (@c median_ns_q48): @c (min_ns + max_ns) / 2 — a quick
 *   midpoint estimate rather than a true median, consistent across all
 *   call sites.
 * - **Standard error** (@c se_q48): @c stddev / √N, computed by converting
 *   @p sample_count to Q48.16 (@c sample_count << 16) and calling
 *   @c sqrt_q48(), then dividing @c stddev << 16 by the result.
 * - **95% credible interval**: @c mean ± Z₉₅ × SE where
 *   @c Z₉₅ = 128431 (1.96 × 65536).
 * - **99% credible interval**: @c mean ± Z₉₉ × SE where
 *   @c Z₉₉ = 168888 (2.576 × 65536).
 *   Lower credible bounds are clamped at 0 (latency is non-negative).
 *
 * Returns a zeroed @c BayesianLatencyPosterior if @p sample_count is 0.
 *
 * @param total_ns_q48      Sum of all latency observations in Q48.16.
 * @param variance_sum_q48  Sum of squared latency observations in Q48.16
 *                          (i.e., @c Σ(xᵢ² << 16)).
 * @param min_ns            Minimum observed latency in integer nanoseconds.
 * @param max_ns            Maximum observed latency in integer nanoseconds.
 * @param sample_count      Number of observations; 0 → returns zeroed struct.
 * @return                  @c BayesianLatencyPosterior with all fields in Q48.16.
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
 * @brief Return the Bayesian latency posterior for cache-hit observations.
 *
 * Forwards the cache-hit accumulators from @p stats to @c calculate_posterior():
 * @c cache_hit_total_ns_q48, @c cache_hit_variance_sum_q48, @c min_cache_hit_ns,
 * @c max_cache_hit_ns, and @c cache_hit_samples. Returns a zeroed
 * @c BayesianLatencyPosterior if @p stats is NULL or no cache-hit samples have
 * been recorded yet.
 *
 * The returned posterior's @c mean_ns_q48, @c stddev_ns_q48, and 95%/99%
 * credible-interval fields are consumed by @c hotwords_speedup_estimate() and
 * @c hotwords_bayesian_report() to compare cache-hit latency against bucket-search
 * latency.
 *
 * @param stats  @c HotwordsStats containing cache-hit timing accumulators.
 * @return       Posterior in Q48.16; all fields zero if no samples exist.
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
 * @brief Return the Bayesian latency posterior for bucket-search observations.
 *
 * Forwards the bucket-search accumulators from @p stats to @c calculate_posterior():
 * @c bucket_search_total_ns_q48, @c bucket_search_variance_sum_q48,
 * @c min_bucket_search_ns, @c max_bucket_search_ns, and @c bucket_search_samples.
 * Returns a zeroed @c BayesianLatencyPosterior if @p stats is NULL or no
 * bucket-search samples have been recorded.
 *
 * The returned posterior's mean and credible-interval fields feed
 * @c hotwords_speedup_estimate() (as the slower distribution) and the
 * baseline section of @c hotwords_bayesian_report().
 *
 * @param stats  @c HotwordsStats containing bucket-search timing accumulators.
 * @return       Posterior in Q48.16; all fields zero if no samples exist.
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
 * @brief Compute a speedup estimate with credible intervals using the delta method.
 *
 * Derives a dimensionless speedup ratio @c S = bucket_mean / cache_mean in
 * Q48.16 fixed-point and propagates uncertainty via the delta method for ratios:
 * @code
 *   SE(S) ≈ S × sqrt( (SE_bucket / bucket_mean)² + (SE_cache / cache_mean)² )
 * @endcode
 * All arithmetic is Q48.16 integer throughout; the only deviation is the
 * intermediate @c sqrt() call which also runs in Q48.16 via @c sqrt_q48().
 *
 * Computed fields in the returned @c SpeedupEstimate (all Q48.16):
 * - @c speedup_factor_q48 — point estimate of @c S.
 * - @c credible_lower_95_q48 / @c credible_upper_95_q48 — 95% credible
 *   interval: @c S ± 1.96 × SE(S) (Z₉₅ = 128431 in Q48.16). Lower bound
 *   clamped at 0.
 * - @c probability_gt_10pct_q48 — P(speedup > 1.1×), computed as
 *   @c 0.5 × (1 + erf(z / √2)) where @c z = (1.1 − S) / SE(S) is negated
 *   to give the right-tail probability and @c √2 = 92681 in Q48.16.
 * - @c probability_gt_double_q48 — P(speedup > 2.0×), same formula with
 *   target = 2.0 × 65536 = 131072.
 *
 * Returns a default @c SpeedupEstimate with @c speedup_factor_q48 = 65536
 * (1.0×) if either distribution has zero samples or if @p stats is NULL.
 *
 * @param stats  @c HotwordsStats with both cache-hit and bucket-search samples.
 * @return       @c SpeedupEstimate with all fields in Q48.16 fixed-point.
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
 * @brief Print a full Bayesian cache performance report comparing two experimental conditions.
 *
 * Emits a structured Unicode report to @c stdout comparing the latency
 * distributions of two measurement sets — one collected with the hot-words
 * cache enabled, the other collected without (baseline). The report is divided
 * into five sections:
 *
 * 1. **WITH CACHE ENABLED** — posterior mean, stddev, and 95%/99% credible
 *    intervals for cache-hits and bucket-searches separately, via
 *    @c hotwords_posterior_cache_hits() and @c hotwords_posterior_bucket_searches().
 *
 * 2. **WITHOUT CACHE (Baseline)** — posterior mean, stddev, and 95% credible
 *    interval for all lookups (treated as bucket searches). If no bucket data
 *    is available in @p stats_without_cache, the bucket-search mean from the
 *    with-cache run is used as an approximation.
 *
 * 3. **SPEEDUP ESTIMATE (Bucket vs Cache)** — @c hotwords_speedup_estimate()
 *    point estimate, 95% credible interval, P(>1.1×), and P(>2.0×).
 *
 * 4. **CACHE EFFECTIVENESS** — total lookups, cache-hit rate, bucket-hit rate,
 *    and miss rate as percentages of total.
 *
 * 5. **OVERALL PERFORMANCE** — weighted-average latency with cache vs. baseline
 *    all-bucket latency, overall speedup, and absolute time saved per lookup.
 *    The weighted average is: @c (N_cache×avg_cache + N_bucket×avg_bucket) /
 *    @c (N_cache + N_bucket).
 *
 * All latency values are displayed in nanoseconds (floating-point via
 * @c q48_to_ns_double()) for readability; underlying accumulators are Q48.16
 * integer throughout.
 *
 * No-op if either @p stats_with_cache or @p stats_without_cache is NULL.
 *
 * @param stats_with_cache     Statistics from a run with cache enabled.
 * @param stats_without_cache  Statistics from a baseline run without cache.
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