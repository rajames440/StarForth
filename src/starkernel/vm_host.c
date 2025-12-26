/**
 * vm_host.c - Kernel host services implementation
 *
 * Provides kernel implementations of host service hooks.
 * All VM allocations/time/mutex go through these functions.
 */

#ifdef __STARKERNEL__

#include "../../include/starkernel/vm_host.h"
#include "kmalloc.h"
#include "console.h"
#include "timer.h"
#include "arch.h"

/**
 * Parity mode: deterministic fake time
 * Increments by 1000ns (1us) per call for reproducibility
 */
#ifndef PARITY_MODE
#define PARITY_MODE 0
#endif

static uint64_t parity_fake_ns = 0;

/* ============================================================================
 * Memory Allocation
 * ============================================================================ */

static void* kernel_alloc(size_t size, size_t align) {
    if (size == 0) {
        return (void*)0;
    }
    if (align <= sizeof(void*)) {
        return kmalloc(size);
    }
    return kmalloc_aligned(size, align);
}

static void kernel_free(void *ptr) {
    if (ptr) {
        kfree(ptr);
    }
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

/* ============================================================================
 * Host Services Instance
 * ============================================================================ */

static VMHostServices kernel_services = {
    .alloc          = kernel_alloc,
    .free           = kernel_free,
    .monotonic_ns   = kernel_monotonic_ns,
    .mutex_init     = kernel_mutex_init,
    .mutex_lock     = kernel_mutex_lock,
    .mutex_unlock   = kernel_mutex_unlock,
    .mutex_destroy  = kernel_mutex_destroy,
    .puts           = kernel_puts,
    .putc           = kernel_putc,
    .parity_mode    = PARITY_MODE,
    .verbose        = 0
};

const VMHostServices* sk_host_services(void) {
    return &kernel_services;
}

void sk_host_init(void) {
    /* Reset parity time if needed */
    parity_fake_ns = 0;

    /* Could initialize other host state here */
}

#endif /* __STARKERNEL__ */
