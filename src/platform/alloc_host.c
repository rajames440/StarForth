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
 */

/*
 * platform/alloc_host.c - Host (Linux/POSIX) memory allocator
 *
 * Simple wrapper around standard malloc/free with statistics tracking.
 */

#include "../../include/platform_alloc.h"
#include <stdlib.h>
#include <string.h>

/* Statistics tracking */
static sf_alloc_stats_t g_stats = {0};

/**
 * @brief Initialise the host allocator statistics.
 *
 * Resets the @c g_stats struct to zero. Safe to call multiple times;
 * subsequent calls simply reset the counters. No heap memory is allocated
 * by this function — the underlying allocator is the system @c malloc.
 *
 * @return 0 always (cannot fail on the hosted path)
 */
int sf_alloc_init(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    return 0;
}

/**
 * @brief Allocate @c size bytes from the system heap.
 *
 * Delegates to @c malloc(). On success, increments @c g_stats.used_bytes
 * and @c g_stats.alloc_count, and updates @c g_stats.peak_bytes if the
 * new used total exceeds the recorded peak.
 *
 * @param size Number of bytes to allocate; 0 returns NULL
 * @return Pointer to allocated memory, or NULL on allocation failure or zero size
 */
void* sf_malloc(size_t size)
{
    if (size == 0) return NULL;

    void* ptr = malloc(size);
    if (ptr)
    {
        g_stats.used_bytes += size;
        g_stats.alloc_count++;
        if (g_stats.used_bytes > g_stats.peak_bytes)
        {
            g_stats.peak_bytes = g_stats.used_bytes;
        }
    }
    return ptr;
}

/**
 * @brief Allocate a zero-initialised array of @c count elements of @c size bytes each.
 *
 * Delegates to @c calloc(). Updates statistics the same way as @c sf_malloc():
 * adds @c count*size to @c used_bytes, increments @c alloc_count, and updates
 * @c peak_bytes.
 *
 * @param count Number of elements; 0 returns NULL
 * @param size  Size of each element in bytes; 0 returns NULL
 * @return Pointer to zero-filled memory, or NULL on allocation failure or zero arguments
 */
void* sf_calloc(size_t count, size_t size)
{
    if (count == 0 || size == 0) return NULL;

    void* ptr = calloc(count, size);
    if (ptr)
    {
        size_t total = count * size;
        g_stats.used_bytes += total;
        g_stats.alloc_count++;
        if (g_stats.used_bytes > g_stats.peak_bytes)
        {
            g_stats.peak_bytes = g_stats.used_bytes;
        }
    }
    return ptr;
}

/**
 * @brief Resize a previously allocated block to @c new_size bytes.
 *
 * Delegates to @c realloc(). On success, increments @c g_stats.alloc_count.
 * The exact byte delta is not tracked because the original allocation size is
 * not stored alongside the pointer — @c used_bytes is therefore approximate
 * after any @c sf_realloc() call. For accurate accounting, wrap allocations
 * with size-tracking headers.
 *
 * @param ptr      Pointer to an existing allocation, or NULL (behaves like @c sf_malloc)
 * @param new_size Desired new size in bytes; 0 may free the block (platform-defined)
 * @return Pointer to resized memory, or NULL on failure (original block unchanged)
 */
void* sf_realloc(void* ptr, size_t new_size)
{
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr)
    {
        /* Note: Can't track exact delta without size tracking */
        g_stats.alloc_count++;
    }
    return new_ptr;
}

/**
 * @brief Release a previously allocated block back to the system heap.
 *
 * Delegates to @c free(). Increments @c g_stats.free_count on non-NULL @c ptr.
 * The freed byte count is not subtracted from @c used_bytes because the
 * original size is not tracked — see @c sf_realloc() note.
 *
 * @param ptr Pointer to free; NULL is a safe no-op
 */
void sf_free(void* ptr)
{
    if (!ptr) return;

    /* Note: We can't track exact bytes freed without size tracking.
     * For accurate stats, would need to wrap allocations with headers.
     * For now, just count frees. */
    g_stats.free_count++;
    free(ptr);
}

/**
 * @brief Copy the current allocator statistics into @c stats.
 *
 * Fills @c *stats with a snapshot of @c g_stats. Note that @c used_bytes
 * overstates the live total when blocks have been freed or reallocated,
 * because freed sizes are not tracked. @c free_count is accurate.
 *
 * @param stats Output buffer to populate; no-op if NULL
 */
void sf_alloc_get_stats(sf_alloc_stats_t* stats)
{
    if (!stats) return;
    *stats = g_stats;
}
