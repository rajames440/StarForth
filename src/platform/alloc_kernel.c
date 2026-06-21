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
 * platform/alloc_kernel.c - Kernel (bare-metal) memory allocator
 *
 * Static arena with bump allocation. Free is a no-op.
 * This is appropriate for kernel use where:
 *   - VM is long-lived (no restart)
 *   - Allocations happen at init time
 *   - Runtime allocations are rare
 *
 * Arena size: 4MB by default (configurable via SF_ARENA_SIZE)
 */

#include "../../include/platform_alloc.h"
#include <stdint.h>

#ifndef SF_ARENA_SIZE
#define SF_ARENA_SIZE (4 * 1024 * 1024)  /* 4MB default */
#endif

/* Alignment for all allocations (8 bytes for 64-bit safety) */
#define SF_ALIGN 8
#define SF_ALIGN_UP(x) (((x) + (SF_ALIGN - 1)) & ~(SF_ALIGN - 1))

/* Static arena */
static uint8_t g_arena[SF_ARENA_SIZE] __attribute__((aligned(SF_ALIGN)));
static size_t g_arena_offset = 0;
static int g_initialized = 0;

/* Statistics */
static sf_alloc_stats_t g_stats = {0};

/**
 * @brief Initialise the kernel bump allocator.
 *
 * Resets @c g_arena_offset to zero and clears all @c g_stats fields.
 * Sets @c g_initialized so that @c sf_malloc() skips the lazy-init guard.
 * Safe to call at any point; all prior allocations become invalid after a
 * reset, so this must only be called before any VM allocation takes place.
 *
 * @return 0 always (cannot fail)
 */
int sf_alloc_init(void)
{
    g_arena_offset = 0;
    g_initialized = 1;

    g_stats.total_bytes = SF_ARENA_SIZE;
    g_stats.used_bytes = 0;
    g_stats.peak_bytes = 0;
    g_stats.alloc_count = 0;
    g_stats.free_count = 0;

    return 0;
}

/**
 * @brief Allocate @c size bytes from the static kernel arena.
 *
 * Advances @c g_arena_offset by @c SF_ALIGN_UP(size). Returns a pointer
 * directly into @c g_arena; the caller must not @c free() the result.
 * Returns NULL (cast 0) when the arena is exhausted or @c size is zero.
 * Lazy-calls @c sf_alloc_init() if the arena has not yet been initialised.
 *
 * @param size Number of bytes to allocate; 0 returns NULL
 * @return Pointer into the static arena, or NULL on exhaustion or zero size
 */
void* sf_malloc(size_t size)
{
    if (!g_initialized) sf_alloc_init();
    if (size == 0) return (void*)0;

    size_t aligned_size = SF_ALIGN_UP(size);
    size_t new_offset = g_arena_offset + aligned_size;

    if (new_offset > SF_ARENA_SIZE)
    {
        /* Out of memory */
        return (void*)0;
    }

    void* ptr = &g_arena[g_arena_offset];
    g_arena_offset = new_offset;

    g_stats.used_bytes = g_arena_offset;
    g_stats.alloc_count++;
    if (g_stats.used_bytes > g_stats.peak_bytes)
    {
        g_stats.peak_bytes = g_stats.used_bytes;
    }

    return ptr;
}

/**
 * @brief Allocate a zero-initialised block of @c count * @c size bytes from the arena.
 *
 * Delegates to @c sf_malloc() then manually zeroes the result with a byte loop
 * (no @c memset dependency — avoids linking the C runtime in freestanding builds).
 * Guards against @c count*size overflow by verifying @c total/size == count.
 *
 * @param count Number of elements; 0 returns NULL
 * @param size  Size of each element in bytes; 0 returns NULL
 * @return Pointer to zero-filled arena memory, or NULL on overflow/exhaustion/zero args
 */
void* sf_calloc(size_t count, size_t size)
{
    if (count == 0 || size == 0) return (void*)0;

    size_t total = count * size;

    /* Check for overflow */
    if (size != 0 && total / size != count)
    {
        return (void*)0;
    }

    void* ptr = sf_malloc(total);
    if (ptr)
    {
        /* Zero-initialize */
        uint8_t* p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++)
        {
            p[i] = 0;
        }
    }
    return ptr;
}

/**
 * @brief Resize an allocation — bump-allocator approximation.
 *
 * Because the arena does not store block sizes, the old block cannot be
 * reclaimed. This function allocates a fresh block of @c new_size bytes and
 * returns it; the old block is orphaned in the arena. The caller is responsible
 * for copying any content from @c ptr to the returned pointer before discarding
 * @c ptr. If @c ptr is NULL, behaves identically to @c sf_malloc(new_size).
 *
 * @param ptr      Existing allocation (orphaned after this call); NULL is safe
 * @param new_size Desired size in bytes; 0 returns NULL
 * @return Pointer to a new arena block of at least @c new_size bytes, or NULL
 */
void* sf_realloc(void* ptr, size_t new_size)
{
    /*
     * Bump allocator limitation: we don't track allocation sizes.
     * For kernel use, realloc is rarely needed.
     *
     * Strategy: allocate new block, caller must copy if needed.
     * This wastes memory but works for rare realloc cases.
     *
     * If ptr is NULL, this is equivalent to malloc.
     */
    if (!ptr) return sf_malloc(new_size);
    if (new_size == 0) return (void*)0;

    /* Allocate new block - old block is orphaned (bump allocator limitation) */
    return sf_malloc(new_size);
}

/**
 * @brief Release a block — no-op for the bump allocator.
 *
 * The kernel arena has no reclaim mechanism; memory is held until the
 * next @c sf_alloc_init() call (i.e. the next power cycle). Increments
 * @c g_stats.free_count so the caller can observe that @c sf_free() was
 * invoked, but no bytes are returned to the arena.
 *
 * @param ptr Block to "free"; NULL is a safe no-op
 */
void sf_free(void* ptr)
{
    /* Bump allocator: free is a no-op.
     *
     * This is acceptable because:
     * 1. VM allocations happen at init time
     * 2. VM runs until power-off
     * 3. No fragmentation issues in practice
     *
     * If needed, could implement a simple free list here.
     */
    if (ptr)
    {
        g_stats.free_count++;
    }
    (void)ptr;
}

/**
 * @brief Copy the current arena statistics into @c stats.
 *
 * Fills @c *stats with a snapshot of @c g_stats. @c total_bytes is fixed at
 * @c SF_ARENA_SIZE; @c used_bytes equals the current arena offset (exact,
 * since no reclaim occurs); @c free_count tracks @c sf_free() call count
 * even though no memory is actually released.
 *
 * @param stats Output buffer to populate; no-op if NULL
 */
void sf_alloc_get_stats(sf_alloc_stats_t* stats)
{
    if (!stats) return;
    *stats = g_stats;
}
