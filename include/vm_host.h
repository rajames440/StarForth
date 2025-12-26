/**
 * vm_host.h - Host services abstraction for StarForth VM
 *
 * Provides pluggable allocator, time, and mutex services.
 * Hosted builds use libc; kernel builds use PMM/kmalloc/TSC.
 *
 * M7 Non-Negotiable: All allocation goes through these hooks.
 * No direct malloc/free in VM code.
 */

#ifndef STARKERNEL_VM_HOST_H
#define STARKERNEL_VM_HOST_H

#include <stdint.h>
#include <stddef.h>

/**
 * Host allocator function signature
 * Returns NULL on failure
 */
typedef void* (*sk_alloc_fn)(size_t size, size_t align);

/**
 * Host free function signature
 */
typedef void (*sk_free_fn)(void *ptr);

/**
 * Host monotonic time function signature
 * Returns nanoseconds since arbitrary epoch
 * In PARITY_MODE, returns deterministic fake time
 */
typedef uint64_t (*sk_time_fn)(void);

/**
 * Host mutex operations (no-op in kernel single-threaded mode)
 */
typedef int (*sk_mutex_init_fn)(void *mutex);
typedef int (*sk_mutex_lock_fn)(void *mutex);
typedef int (*sk_mutex_unlock_fn)(void *mutex);
typedef void (*sk_mutex_destroy_fn)(void *mutex);

/**
 * Host console output (for words that print)
 * Returns number of bytes written, or -1 on error
 */
typedef int (*sk_puts_fn)(const char *str);
typedef int (*sk_putc_fn)(int c);

/**
 * VMHostServices - pluggable host operations
 *
 * Hosted builds point these at libc functions.
 * Kernel builds point these at kernel implementations.
 */
typedef struct VMHostServices {
    /* Memory allocation */
    sk_alloc_fn         alloc;
    sk_free_fn          free;

    /* Monotonic time source */
    sk_time_fn          monotonic_ns;

    /* Mutex operations (can be no-ops) */
    sk_mutex_init_fn    mutex_init;
    sk_mutex_lock_fn    mutex_lock;
    sk_mutex_unlock_fn  mutex_unlock;
    sk_mutex_destroy_fn mutex_destroy;

    /* Console output */
    sk_puts_fn          puts;
    sk_putc_fn          putc;

    /* Flags */
    int                 parity_mode;    /* 1 = deterministic time */
    int                 verbose;        /* 1 = enable logging */
} VMHostServices;

/**
 * Get the current host services instance.
 * Returns hosted or kernel services depending on build.
 */
const VMHostServices* sk_host_services(void);

/**
 * Initialize host services for kernel build.
 * Must be called before any VM operations.
 */
#ifdef __STARKERNEL__
void sk_host_init(void);
#endif

/**
 * Convenience macros for common operations
 * Use these instead of direct malloc/free/etc.
 */
#define SK_ALLOC(size)          (sk_host_services()->alloc((size), sizeof(void*)))
#define SK_ALLOC_ALIGNED(s, a)  (sk_host_services()->alloc((s), (a)))
#define SK_FREE(ptr)            (sk_host_services()->free(ptr))
#define SK_TIME_NS()            (sk_host_services()->monotonic_ns())
#define SK_PUTS(str)            (sk_host_services()->puts(str))
#define SK_PUTC(c)              (sk_host_services()->putc(c))

/**
 * Mutex wrapper macros
 */
#define SK_MUTEX_INIT(m)    (sk_host_services()->mutex_init(m))
#define SK_MUTEX_LOCK(m)    (sk_host_services()->mutex_lock(m))
#define SK_MUTEX_UNLOCK(m)  (sk_host_services()->mutex_unlock(m))
#define SK_MUTEX_DESTROY(m) (sk_host_services()->mutex_destroy(m))

#endif /* STARKERNEL_VM_HOST_H */
