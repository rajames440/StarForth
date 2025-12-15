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
 * StarForth Platform Initialization
 *
 * Selects and initializes the appropriate platform backend based on build configuration.
 *
 * License: See LICENSE file
 */

#include "../../include/platform_time.h"

/* Global backend pointer (starts as NULL) */
const sf_time_backend_t *sf_time_backend = NULL;

/* L4Re-specific init (defined in time_l4re.c) */
#ifdef __l4__
extern void sf_time_init_l4re(void);
#endif

/**
 * @brief Initialize platform time subsystem
 *
 * Selects appropriate backend based on compile-time platform:
 * - L4Re/StarshipOS: Uses RTC server if available, falls back to KIP clock
 * - POSIX: Uses clock_gettime()
 *
 * Call this once at program startup before using any time functions.
 */
void sf_time_init(void) {
    if (sf_time_backend != NULL)
        return; /* Already initialized */

#ifdef __l4__
    /* L4Re/StarshipOS build */
    sf_time_init_l4re();
    sf_time_backend = &sf_time_backend_l4re;
#else
    /* POSIX build */
    sf_time_backend = &sf_time_backend_posix;
#endif
}
