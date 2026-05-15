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
 * pmm.h - Physical Memory Manager interface for StarKernel
 *
 * Bitmap-based allocator for 4KB physical pages. Initialization parses the
 * UEFI memory map provided in BootInfo and exposes simple allocation and
 * statistics helpers for early kernel bring-up.
 */

#ifndef STARKERNEL_PMM_H
#define STARKERNEL_PMM_H

#include <stdint.h>
#include <stddef.h>
#include "uefi.h"

#define PMM_PAGE_SIZE 4096u

typedef struct {
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t free_pages;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
} pmm_stats_t;

int pmm_init(BootInfo *boot_info);
int pmm_is_initialized(void);

uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_contiguous(uint64_t num_pages);
void pmm_free_page(uint64_t paddr);
void pmm_free_contiguous(uint64_t paddr, uint64_t num_pages);

pmm_stats_t pmm_get_stats(void);

#endif /* STARKERNEL_PMM_H */
