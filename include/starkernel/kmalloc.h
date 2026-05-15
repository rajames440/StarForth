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
 * kmalloc.h - Kernel heap allocator interface
 */

#ifndef STARKERNEL_KMALLOC_H
#define STARKERNEL_KMALLOC_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t peak_bytes;
    uint64_t free_bytes;
} kmalloc_stats_t;

int kmalloc_init(uint64_t heap_size_bytes);
int kmalloc_is_initialized(void);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t align);
void kfree(void *ptr);
kmalloc_stats_t kmalloc_get_stats(void);
uintptr_t kmalloc_heap_base_addr(void);
uintptr_t kmalloc_heap_end_addr(void);

#endif /* STARKERNEL_KMALLOC_H */
