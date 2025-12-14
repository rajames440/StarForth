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

int sf_mutex_init(sf_mutex_t *mutex)
{
    if (!mutex) return -1;
    mutex->state = 0;
    return 0;
}

void sf_mutex_destroy(sf_mutex_t *mutex)
{
    if (!mutex) return;
    mutex->state = 0;
}

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

void sf_mutex_unlock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    __sync_lock_release(&mutex->state);
}

#else /* POSIX */

int sf_mutex_init(sf_mutex_t *mutex)
{
    if (!mutex) return -1;
    return pthread_mutex_init(&mutex->handle, NULL);
}

void sf_mutex_destroy(sf_mutex_t *mutex)
{
    if (!mutex) return;
    pthread_mutex_destroy(&mutex->handle);
}

void sf_mutex_lock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    pthread_mutex_lock(&mutex->handle);
}

void sf_mutex_unlock(sf_mutex_t *mutex)
{
    if (!mutex) return;
    pthread_mutex_unlock(&mutex->handle);
}

#endif
