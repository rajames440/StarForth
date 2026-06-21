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

#include "platform_alloc.h"
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
 * @brief Allocate memory from the static kernel arena.
 *
 * Bump-allocates @p size bytes from @c g_arena, rounding up to @c SF_ALIGN
 * (8 bytes) to preserve alignment for 64-bit values. Lazily calls
 * @c sf_alloc_init() on the first invocation if the arena has not been
 * explicitly initialised. Returns @c NULL for zero-size requests and when
 * the arena is exhausted.
 *
 * @note Because this is a bump allocator there is no reclaim path; once the
 *       arena is full it stays full until the kernel is reset.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block on success, @c NULL on failure.
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
 * @brief Allocate and zero-initialise a contiguous array from the kernel arena.
 *
 * Computes @p count × @p size, checks for integer overflow via
 * @c total/size != count, then delegates to @c sf_malloc(). The returned
 * block is zeroed with a manual byte loop rather than @c memset so that the
 * kernel build remains freestanding with no libc dependency.
 * Returns @c NULL when either argument is zero, on overflow, or on arena
 * exhaustion.
 *
 * @param count Number of elements to allocate.
 * @param size  Size of each element in bytes.
 * @return Pointer to the zeroed block on success, @c NULL on failure.
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
 * @brief Resize an allocation in the kernel arena.
 *
 * Because this is a bump allocator with no size metadata, the original block
 * cannot be grown in place. Instead a fresh block of @p new_size bytes is
 * bump-allocated and returned; the original block at @p ptr is orphaned
 * (leaked in place) for the lifetime of the kernel.
 *
 * Special cases match the C standard:
 * - @p ptr == @c NULL → equivalent to @c sf_malloc(@p new_size).
 * - @p new_size == 0 → returns @c NULL (caller treats old block as freed).
 *
 * The caller is responsible for copying content from the old block before
 * discarding the old pointer; this function does not perform the copy.
 *
 * @param ptr      Pointer to the existing allocation (may be @c NULL).
 * @param new_size Desired size of the new block in bytes.
 * @return Pointer to the new block on success, @c NULL on failure or when
 *         @p new_size is zero.
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
 * @brief Release a kernel arena allocation (no-op).
 *
 * The kernel bump allocator has no reclaim mechanism — once bytes are
 * allocated from @c g_arena they remain consumed until the kernel resets.
 * This function exists solely to satisfy the @c sf_free() contract expected
 * by shared VM code, and to keep @c g_stats.free_count accurate for
 * diagnostic purposes.
 *
 * Callers must not assume that freed memory is reclaimed or reusable.
 *
 * @param ptr Pointer previously returned by @c sf_malloc() / @c sf_calloc()
 *            (may be @c NULL; silently ignored).
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
 * @brief Retrieve a snapshot of kernel allocator statistics.
 *
 * Copies the internal @c g_stats structure to the caller-supplied buffer.
 * Because the bump allocator never reclaims memory, @c used_bytes is the
 * exact number of arena bytes consumed to date and @c peak_bytes equals
 * @c used_bytes. @c free_count counts @c sf_free() calls for accounting
 * purposes only — it does not reflect any actual reclamation.
 *
 * @param stats Output buffer to receive the statistics; silently returns
 *              without writing if @p stats is @c NULL.
 */
void sf_alloc_get_stats(sf_alloc_stats_t* stats)
{
    if (!stats) return;
    *stats = g_stats;
}
