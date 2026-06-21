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

/*
 * platform/threading.c - Minimal mutex abstraction bridging POSIX and L4Re
 */

#include "../../include/platform_lock.h"

#if !defined(STARFORTH_MINIMAL) && !defined(L4RE_TARGET)
#include <pthread.h>
#endif

#if defined(STARFORTH_MINIMAL) || defined(L4RE_TARGET)
/* Spinlock-based mutex for minimal/embedded builds */

/**
 * @brief Initialise a spinlock mutex (minimal/L4Re build).
 *
 * Sets @c mutex->state to 0 (unlocked). Safe to call on a mutex that has
 * already been destroyed. On embedded targets where @c pthread is unavailable,
 * this replaces @c pthread_mutex_init().
 *
 * @param mutex Mutex to initialise; returns -1 if NULL
 * @return 0 on success, -1 if @c mutex is NULL
 */
int sf_mutex_init(sf_mutex_t *mutex)
{
    if (!mutex) return -1;
    mutex->state = 0;
    return 0;
}

/**
 * @brief Destroy a spinlock mutex (minimal/L4Re build).
 *
 * Resets @c mutex->state to 0. No resources are released because the
 * spinlock uses only the single @c state field. Safe to call on an
 * already-destroyed mutex.
 *
 * @param mutex Mutex to destroy; no-op if NULL
 */
void sf_mutex_destroy(sf_mutex_t *mutex)
{
    if (!mutex) return;
    mutex->state = 0;
}

/**
 * @brief Acquire a spinlock mutex (minimal/L4Re build).
 *
 * Busy-waits using @c __sync_lock_test_and_set() until the lock is obtained.
 * Issues an architecture-appropriate yield hint (@c PAUSE on x86,
 * @c YIELD on ARM/AArch64, @c NOP on RISC-V) inside the spin loop to
 * reduce power consumption and SMT contention.
 *
 * @param mutex Mutex to lock; no-op if NULL
 */
void sf_mutex_lock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    while (__sync_lock_test_and_set(&mutex->state, 1))
    {
        /* CPU yield to reduce power consumption and improve performance */
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
        /* x86/x86_64: PAUSE instruction reduces pipeline flush penalty */
        __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
        /* ARM/AArch64: YIELD hint for SMT threads */
        __asm__ __volatile__("yield" ::: "memory");
#elif defined(__riscv)
        /* RISC-V: NOP as yield hint */
        __asm__ __volatile__("nop" ::: "memory");
#else
        /* Fallback: compiler barrier prevents optimization */
        __asm__ __volatile__("" ::: "memory");
#endif
    }
}

/**
 * @brief Release a spinlock mutex (minimal/L4Re build).
 *
 * Clears the lock state atomically using @c __sync_lock_release(), which
 * implies a full memory barrier on all supported architectures. Threads
 * blocked in @c sf_mutex_lock() will observe the release on their next
 * test-and-set iteration.
 *
 * @param mutex Mutex to unlock; no-op if NULL
 */
void sf_mutex_unlock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    __sync_lock_release(&mutex->state);
}

#else /* POSIX */

/**
 * @brief Initialise a POSIX mutex (hosted build).
 *
 * Delegates to @c pthread_mutex_init() with default attributes.
 *
 * @param mutex Mutex to initialise; returns -1 if NULL
 * @return 0 on success, -1 if @c mutex is NULL, or a positive @c errno value
 *         propagated from @c pthread_mutex_init()
 */
int sf_mutex_init(sf_mutex_t *mutex)
{
    if (!mutex) return -1;
    return pthread_mutex_init(&mutex->handle, NULL);
}

/**
 * @brief Destroy a POSIX mutex (hosted build).
 *
 * Delegates to @c pthread_mutex_destroy(). The mutex must not be locked
 * when destroyed. Return value is discarded; callers that need the error
 * code should call @c pthread_mutex_destroy() directly.
 *
 * @param mutex Mutex to destroy; no-op if NULL
 */
void sf_mutex_destroy(sf_mutex_t *mutex)
{
    if (!mutex) return;
    pthread_mutex_destroy(&mutex->handle);
}

/**
 * @brief Acquire a POSIX mutex (hosted build).
 *
 * Delegates to @c pthread_mutex_lock(). Blocks until the mutex is available.
 * Return value is discarded; a locking error will manifest as a deadlock or
 * data race, not a silent failure.
 *
 * @param mutex Mutex to lock; no-op if NULL
 */
void sf_mutex_lock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    pthread_mutex_lock(&mutex->handle);
}

/**
 * @brief Release a POSIX mutex (hosted build).
 *
 * Delegates to @c pthread_mutex_unlock(). Must be called by the same thread
 * that acquired the lock. Return value is discarded.
 *
 * @param mutex Mutex to unlock; no-op if NULL
 */
void sf_mutex_unlock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    pthread_mutex_unlock(&mutex->handle);
}

#endif
