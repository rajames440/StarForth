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
 * arena.c - StarKernel VM arena initialization
 *
 * Provides kernel-specific VM initialization with PMM-backed arena.
 * This replaces the hosted vm_init() for kernel builds.
 *
 * M7 Non-Negotiable: VM arena is PMM-backed contiguous pages.
 */

#ifdef __STARKERNEL__

#include "starkernel/vm/arena.h"
#include "vm_host.h"
#include "pmm.h"
#include "vmm.h"
#include "console.h"
#include "starkernel/hal/hal.h"
#include "dictionary_management.h"
#include <stddef.h>
#include <stdint.h>

/* Forward declaration - we'll need the VM struct */
/* For now, define minimal interface; full integration comes later */

#define SK_VM_ARENA_VADDR        0xFFFF900000000000ULL
#define SK_VM_GUARD_SIZE         PMM_PAGE_SIZE
#define SK_VM_GUARD_PATTERN_HEAD 0x5ac0ffeed0c0ffeeULL
#define SK_VM_GUARD_PATTERN_TAIL 0x0bad0badf00df00dULL

/* VM memory size from hosted VM (5 MB) */
#define SK_VM_MEMORY_SIZE   (5ULL * 1024ULL * 1024ULL)
#define SK_VM_TOTAL_SIZE    (SK_VM_MEMORY_SIZE + (2ULL * SK_VM_GUARD_SIZE))
#define SK_VM_ARENA_PAGES   ((SK_VM_TOTAL_SIZE + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE)

/* Arena state */
static uint64_t vm_arena_paddr = 0;
static uint64_t vm_arena_guard_vaddr = 0;
static uint64_t vm_arena_client_vaddr = 0;
static int vm_arena_initialized = 0;

static void guard_fill(uint64_t base)
{
    uint64_t *ptr = (uint64_t *)(uintptr_t)base;
    ptr[0] = SK_VM_GUARD_PATTERN_HEAD;
    ptr[(SK_VM_GUARD_SIZE / sizeof(uint64_t)) - 1] = SK_VM_GUARD_PATTERN_TAIL;
}

static int guard_check(uint64_t base)
{
    const uint64_t *ptr = (const uint64_t *)(uintptr_t)base;
    if (ptr[0] != SK_VM_GUARD_PATTERN_HEAD) {
        return 0;
    }
    if (ptr[(SK_VM_GUARD_SIZE / sizeof(uint64_t)) - 1] != SK_VM_GUARD_PATTERN_TAIL) {
        return 0;
    }
    return 1;
}

/**
 * sk_vm_arena_alloc - Allocate PMM-backed VM arena
 *
 * Allocates contiguous physical pages and maps them RW+NX.
 * Returns virtual address of arena, or 0 on failure.
 */
uint64_t sk_vm_arena_alloc(void) {
    if (vm_arena_initialized) {
        return vm_arena_client_vaddr;
    }

    /* Allocate contiguous physical pages */
    vm_arena_paddr = pmm_alloc_contiguous(SK_VM_ARENA_PAGES);
    if (vm_arena_paddr == 0) {
        console_println("sk_vm_arena_alloc: PMM allocation failed");
        return 0;
    }

    vm_arena_guard_vaddr = SK_VM_ARENA_VADDR - SK_VM_GUARD_SIZE;
    vm_arena_client_vaddr = vm_arena_guard_vaddr + SK_VM_GUARD_SIZE;
    uint64_t flags = VMM_FLAG_WRITABLE | VMM_FLAG_NX;

    if (vmm_map_range(vm_arena_guard_vaddr, vm_arena_paddr,
                      SK_VM_ARENA_PAGES * PMM_PAGE_SIZE, flags) != 0) {
        console_println("sk_vm_arena_alloc: VMM mapping failed");
        pmm_free_contiguous(vm_arena_paddr, SK_VM_ARENA_PAGES);
        vm_arena_paddr = 0;
        return 0;
    }

    /* Zero the arena */
    uint8_t *arena = (uint8_t *)(uintptr_t)vm_arena_client_vaddr;
    for (size_t i = 0; i < SK_VM_MEMORY_SIZE; i++) {
        arena[i] = 0;
    }

    guard_fill(vm_arena_guard_vaddr);
    guard_fill(vm_arena_guard_vaddr + SK_VM_GUARD_SIZE + SK_VM_MEMORY_SIZE);

    vm_arena_initialized = 1;

    console_puts("VM arena allocated: ");
    /* Print address (simple hex) */
    {
        char buf[20];
        uint64_t v = vm_arena_client_vaddr;
        int i = 18;
        buf[19] = '\0';
        buf[18] = '\n';
        while (i > 2) {
            int d = v & 0xF;
            buf[--i] = (d < 10) ? ('0' + d) : ('a' + d - 10);
            v >>= 4;
        }
        buf[1] = 'x';
        buf[0] = '0';
        console_puts(buf);
    }
    console_puts(" (");
    /* Print size in MB */
    {
        char buf[16];
        uint64_t mb = SK_VM_MEMORY_SIZE / (1024 * 1024);
        int i = 0;
        if (mb == 0) {
            buf[i++] = '0';
        } else {
            char tmp[16];
            int j = 0;
            while (mb > 0) {
                tmp[j++] = '0' + (mb % 10);
                mb /= 10;
            }
            while (j > 0) {
                buf[i++] = tmp[--j];
            }
        }
        buf[i] = '\0';
        console_puts(buf);
    }
    console_println(" MB)");

    return vm_arena_client_vaddr;
}

/**
 * sk_vm_arena_free - Free the VM arena
 *
 * Unmaps and frees the PMM-backed arena.
 */
void sk_vm_arena_free(void) {
    if (!vm_arena_initialized) {
        return;
    }

    /* Unmap each page */
    for (uint64_t offset = 0; offset < SK_VM_TOTAL_SIZE; offset += PMM_PAGE_SIZE) {
        vmm_unmap_page(vm_arena_guard_vaddr + offset);
    }

    /* Free physical memory */
    pmm_free_contiguous(vm_arena_paddr, SK_VM_ARENA_PAGES);

    vm_arena_paddr = 0;
    vm_arena_guard_vaddr = 0;
    vm_arena_client_vaddr = 0;
    vm_arena_initialized = 0;
}

/**
 * sk_vm_arena_ptr - Get pointer to VM arena
 *
 * Returns NULL if arena not allocated.
 */
uint8_t* sk_vm_arena_ptr(void) {
    if (!vm_arena_initialized) {
        return (uint8_t*)0;
    }
    return (uint8_t *)(uintptr_t)vm_arena_client_vaddr;
}

/**
 * sk_vm_arena_size - Get VM arena size
 */
size_t sk_vm_arena_size(void) {
    return SK_VM_MEMORY_SIZE;
}

/**
 * sk_vm_arena_is_initialized - Check if arena is ready
 */
int sk_vm_arena_is_initialized(void) {
    return vm_arena_initialized;
}

void sk_vm_arena_assert_guards(const char *tag) {
    if (!vm_arena_initialized) {
        return;
    }
    if (!guard_check(vm_arena_guard_vaddr) ||
        !guard_check(vm_arena_guard_vaddr + SK_VM_GUARD_SIZE + SK_VM_MEMORY_SIZE)) {
        console_puts("[VM][arena] guard corruption at ");
        console_println(tag ? tag : "unknown");
        vm_dictionary_log_last_word(NULL, tag);
        sk_hal_panic("VM arena guard corrupted");
    }
}

#endif /* __STARKERNEL__ */
