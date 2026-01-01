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

#ifndef STARFORTH_PLATFORM_ALLOC_H
#define STARFORTH_PLATFORM_ALLOC_H

/*
 * platform_alloc.h - Portable memory allocation abstraction
 *
 * Host builds: Uses standard malloc/free
 * Kernel builds: Uses static arena with bump allocator
 *
 * Usage:
 *   void* p = sf_malloc(size);
 *   sf_free(p);
 *   void* p2 = sf_calloc(count, size);  // zero-initialized
 */

#include <stddef.h>

/*
 * Allocate memory block of given size.
 * Returns NULL on failure.
 */
void* sf_malloc(size_t size);

/*
 * Allocate zero-initialized memory for array.
 * Returns NULL on failure.
 */
void* sf_calloc(size_t count, size_t size);

/*
 * Reallocate memory block to new size.
 * Returns NULL on failure (original block unchanged).
 */
void* sf_realloc(void* ptr, size_t new_size);

/*
 * Free previously allocated memory.
 * Passing NULL is safe (no-op).
 */
void sf_free(void* ptr);

/*
 * Initialize the allocator (required for kernel builds).
 * Host builds: no-op.
 * Kernel builds: sets up the static arena.
 *
 * Returns 0 on success, -1 on failure.
 */
int sf_alloc_init(void);

/*
 * Get allocator statistics (for diagnostics).
 */
typedef struct sf_alloc_stats {
    size_t total_bytes;      /* Total arena size */
    size_t used_bytes;       /* Currently allocated */
    size_t peak_bytes;       /* High water mark */
    size_t alloc_count;      /* Number of allocations */
    size_t free_count;       /* Number of frees */
} sf_alloc_stats_t;

void sf_alloc_get_stats(sf_alloc_stats_t* stats);

#endif /* STARFORTH_PLATFORM_ALLOC_H */
