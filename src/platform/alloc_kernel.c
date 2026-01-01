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

void sf_alloc_get_stats(sf_alloc_stats_t* stats)
{
    if (!stats) return;
    *stats = g_stats;
}
