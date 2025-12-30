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

static void print_hex64(uint64_t v) {
    char buf[19];
    buf[0] = '0'; buf[1] = 'x'; buf[18] = '\0';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((v >> ((15 - i) * 4)) & 0xF);
        buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
    }
    console_puts(buf);
}

static void* kernel_alloc(size_t size, size_t align) {
    if (size == 0) {
        return (void*)0;
    }

    /* VM arena requests get routed through the PMM-backed allocator */
    size_t arena_size = sk_vm_arena_size();
    console_puts("[kernel_alloc] size=");
    print_hex64((uint64_t)size);
    console_puts(" arena_size=");
    print_hex64((uint64_t)arena_size);
    console_puts(" match=");
    console_puts((size == arena_size) ? "YES" : "NO");
    console_println("");
    
    if (size == arena_size) {
        if (!sk_vm_arena_is_initialized()) {
            if (sk_vm_arena_alloc() == 0) {
                return NULL;
            }
        }
        void *ptr = sk_vm_arena_ptr();
        console_puts("[kernel_alloc] returning arena ptr=");
        print_hex64((uint64_t)(uintptr_t)ptr);
        console_println("");
        return ptr;
    }

    void *ptr;
    if (align <= sizeof(void*)) {
        ptr = kmalloc(size);
    } else {
        ptr = kmalloc_aligned(size, align);
    }
    console_puts("[kernel_alloc] returning kmalloc ptr=");
    print_hex64((uint64_t)(uintptr_t)ptr);
    console_println("");
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

static void host_services_print_hex(const void *ptr) {
    if (!ptr) {
        console_puts("NULL");
        return;
    }
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    buf[18] = '\0';
    uint64_t value = (uint64_t)(uintptr_t)ptr;
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (value >> ((15 - i) * 4)) & 0xF;
        buf[i + 2] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
    }
    console_puts(buf);
}

static void host_services_log_ptr(const char *label, const void *ptr) {
    console_puts("    ");
    console_puts(label);
    console_puts(" = ");
    host_services_print_hex(ptr);
    console_println("");
}


static void host_services_dump_table(void) {
    if (host_services_logged) {
        return;
    }

    console_println("[HAL][host] VMHostServices table:");
    host_services_log_ptr("alloc", kernel_services.alloc);
    host_services_log_ptr("free", kernel_services.free);
    host_services_log_ptr("monotonic_ns", kernel_services.monotonic_ns);
    host_services_log_ptr("mutex_init", kernel_services.mutex_init);
    host_services_log_ptr("mutex_lock", kernel_services.mutex_lock);
    host_services_log_ptr("mutex_unlock", kernel_services.mutex_unlock);
    host_services_log_ptr("mutex_destroy", kernel_services.mutex_destroy);
    host_services_log_ptr("puts", kernel_services.puts);
    host_services_log_ptr("putc", kernel_services.putc);
    host_services_log_ptr("is_executable_ptr", kernel_services.is_executable_ptr);
    host_services_log_ptr("owns_xt_entry", kernel_services.owns_xt_entry);
    host_services_log_ptr("panic", kernel_services.panic);
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
