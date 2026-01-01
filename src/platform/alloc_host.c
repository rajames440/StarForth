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

int sf_alloc_init(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    return 0;
}

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

void sf_free(void* ptr)
{
    if (!ptr) return;

    /* Note: We can't track exact bytes freed without size tracking.
     * For accurate stats, would need to wrap allocations with headers.
     * For now, just count frees. */
    g_stats.free_count++;
    free(ptr);
}

void sf_alloc_get_stats(sf_alloc_stats_t* stats)
{
    if (!stats) return;
    *stats = g_stats;
}
