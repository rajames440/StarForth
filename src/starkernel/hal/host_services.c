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
 * vm_host.c - Kernel host services implementation
 *
 * Provides kernel implementations of host service hooks.
 * All VM allocations/time/mutex go through these functions.
 */

#ifdef __STARKERNEL__

#include "vm_host.h"
#include "starkernel/hal/hal.h"
#include "kmalloc.h"
#include "console.h"
#include "timer.h"
#include "arch.h"
#include "starkernel/vm/arena.h"
#include <stdbool.h>

/**
 * Parity mode: deterministic fake time
 * Increments by 1000ns (1us) per call for reproducibility
 */
#ifndef PARITY_MODE
#define PARITY_MODE 0
#endif

static VMHostServices kernel_services;
static uint64_t parity_fake_ns = 0;
static bool host_services_initialized = false;
static bool host_services_logged = false;

/* ============================================================================
 * Memory Allocation
 * ============================================================================ */

static void* kernel_alloc(size_t size, size_t align) {
    if (size == 0) {
        return (void*)0;
    }

    size_t arena_size = sk_vm_arena_size();
    if (size == arena_size) {
        if (!sk_vm_arena_is_initialized()) {
            /* Hera (first call): use the PMM-backed arena singleton */
            if (sk_vm_arena_alloc() == 0) {
                return NULL;
            }
            return sk_vm_arena_ptr();
        }
        /* Baby VM: each needs its own isolated region; returning the singleton
         * here causes all VMs to share Hera's memory, overwriting her compiled
         * word bodies when the baby registers its dictionary. */
        void *p = (align <= sizeof(void*)) ? kmalloc(size)
                                           : kmalloc_aligned(size, align);
        if (p) {
            uint8_t *b = (uint8_t *)p;
            size_t   i;
            for (i = 0; i < size; i++) b[i] = 0;
        }
        return p;
    }

    void *ptr;
    if (align <= sizeof(void*)) {
        ptr = kmalloc(size);
    } else {
        ptr = kmalloc_aligned(size, align);
    }
    return ptr;
}

static void kernel_free(void *ptr) {
    if (!ptr) {
        return;
    }
    /* VM arena lifetime is managed separately; ignore free requests */
    if (ptr == sk_vm_arena_ptr()) {
        return;
    }
    kfree(ptr);
}

/* ============================================================================
 * Monotonic Time
 * ============================================================================ */

static uint64_t kernel_monotonic_ns(void) {
#if PARITY_MODE
    /* Deterministic: 1us per call */
    parity_fake_ns += 1000;
    return parity_fake_ns;
#else
    /* Real time from TSC */
    uint64_t tsc = arch_read_timestamp();
    uint64_t hz = timer_tsc_hz();
    if (hz == 0) {
        return 0;  /* Timer not calibrated */
    }
    /* Convert TSC ticks to nanoseconds */
    /* ns = tsc * 1_000_000_000 / hz */
    /* Avoid overflow: ns = tsc / (hz / 1_000_000_000) */
    /* But hz >> 1e9, so: ns = (tsc * 1000) / (hz / 1_000_000) */
    uint64_t mhz = hz / 1000000ULL;
    if (mhz == 0) mhz = 1;
    return (tsc * 1000ULL) / mhz;
#endif
}

/* ============================================================================
 * Mutex Operations (No-op in single-threaded kernel)
 * ============================================================================ */

static int kernel_mutex_init(void *mutex) {
    (void)mutex;
    return 0;  /* Success */
}

static int kernel_mutex_lock(void *mutex) {
    (void)mutex;
    return 0;  /* Success */
}

static int kernel_mutex_unlock(void *mutex) {
    (void)mutex;
    return 0;  /* Success */
}

static void kernel_mutex_destroy(void *mutex) {
    (void)mutex;
    /* Nothing to do */
}

/* ============================================================================
 * Console Output
 * ============================================================================ */

static int kernel_puts(const char *str) {
    if (!str) return -1;
    console_puts(str);
    /* Count characters (console_puts doesn't return count) */
    int count = 0;
    while (str[count]) count++;
    return count;
}

static int kernel_putc(int c) {
    console_putc((char)c);
    return c;
}

static bool kernel_xt_entry_owned(const void *ptr, size_t bytes) {
    if (!ptr || bytes == 0) {
        return false;
    }
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t entry_end = addr + bytes;
    if (entry_end < addr) {
        return false;  /* Overflow check */
    }

    /* Check if in VM arena (dictionary entries are allocated here) */
    if (sk_vm_arena_is_initialized()) {
        uint8_t *arena_ptr = sk_vm_arena_ptr();
        size_t arena_size = sk_vm_arena_size();
        uintptr_t arena_base = (uintptr_t)arena_ptr;
        uintptr_t arena_end = arena_base + arena_size;
        if (addr >= arena_base && entry_end <= arena_end) {
            return true;
        }
    }

    /* Check if in kmalloc heap (for other allocations) */
    uintptr_t heap_base = kmalloc_heap_base_addr();
    uintptr_t heap_end = kmalloc_heap_end_addr();
    if (heap_base != 0 && heap_end != 0) {
        if (addr >= heap_base && entry_end <= heap_end) {
            return true;
        }
    }

    return false;
}

/* Static wrappers for HAL functions - avoids GOT references in PE.
 * With -fPIC, cross-TU function pointer assignments use GOT-relative loads,
 * but PE files don't have a working GOT. These wrappers provide local
 * addresses that use RIP-relative LEA instead of GOT loads. */
static bool kernel_is_executable_ptr(const void *ptr) {
    return sk_hal_is_executable_ptr(ptr);
}

static void kernel_panic(const char *message) {
    sk_hal_panic(message);
}

static void host_services_dump_table(void) {
    host_services_logged = true;
}

const VMHostServices* sk_host_services(void) {
    if (!host_services_initialized) {
        sk_host_init();
    }
    return &kernel_services;
}

void sk_host_init(void) {
    if (host_services_initialized) {
        host_services_dump_table();
        return;
    }

    /* Reset parity time if needed */
    parity_fake_ns = 0;

    kernel_services.alloc = kernel_alloc;
    kernel_services.free = kernel_free;
    kernel_services.monotonic_ns = kernel_monotonic_ns;
    kernel_services.mutex_init = kernel_mutex_init;
    kernel_services.mutex_lock = kernel_mutex_lock;
    kernel_services.mutex_unlock = kernel_mutex_unlock;
    kernel_services.mutex_destroy = kernel_mutex_destroy;
    kernel_services.puts = kernel_puts;
    kernel_services.putc = kernel_putc;
    kernel_services.is_executable_ptr = kernel_is_executable_ptr;
    kernel_services.owns_xt_entry = kernel_xt_entry_owned;
    kernel_services.panic = kernel_panic;
    kernel_services.parity_mode = PARITY_MODE;
    kernel_services.verbose = 0;

    host_services_initialized = true;
    host_services_dump_table();
}

#endif /* __STARKERNEL__ */
