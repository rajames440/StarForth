/*
                                  ***   StarForth   ***

  platform_init.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.459-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/platform/platform_init.c
 */

/*
 * StarForth Platform Initialization
 *
 * Selects and initializes the appropriate platform backend based on build configuration.
 *
 * License: CC0-1.0 / Public Domain
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
