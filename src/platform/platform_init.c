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
