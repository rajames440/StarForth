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
 * platform_lock.h - Portable mutex abstraction for StarForth
 */

#ifndef STARFORTH_PLATFORM_LOCK_H
#define STARFORTH_PLATFORM_LOCK_H

#if defined(L4RE_TARGET) || defined(STARFORTH_MINIMAL)
typedef struct sf_mutex
{
    volatile unsigned int state;
} sf_mutex_t;
#else
#include <pthread.h>
typedef struct sf_mutex
{
    pthread_mutex_t handle;
} sf_mutex_t;
#endif

int  sf_mutex_init(sf_mutex_t *mutex);
void sf_mutex_destroy(sf_mutex_t *mutex);
void sf_mutex_lock(sf_mutex_t *mutex);
void sf_mutex_unlock(sf_mutex_t *mutex);

#endif /* STARFORTH_PLATFORM_LOCK_H */
