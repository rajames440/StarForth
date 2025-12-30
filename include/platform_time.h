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

#ifndef STARFORTH_PLATFORM_TIME_H
#define STARFORTH_PLATFORM_TIME_H

/*
 * StarForth Platform Time Abstraction
 *
 * Provides portable timing interface for both POSIX and L4Re/StarshipOS builds.
 * Uses vtable pattern similar to blkio subsystem.
 *
 * License: See LICENSE file
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Time representation (nanoseconds since epoch) */
typedef uint64_t sf_time_ns_t;

/* Timestamp string buffer size */
#define SF_TIME_STAMP_SIZE 32

/* Platform time backend vtable */
typedef struct {
    /**
     * @brief Get monotonic time (for performance measurement)
     * @return Nanoseconds since system boot (never decreases)
     */
    sf_time_ns_t (*get_monotonic_ns)(void);

    /**
     * @brief Get wall-clock time
     * @return Nanoseconds since Unix epoch (1970-01-01 00:00:00 UTC)
     *         Returns 0 if RTC unavailable
     */
    sf_time_ns_t (*get_realtime_ns)(void);

    /**
     * @brief Set wall-clock time (may require privileges)
     * @param ns_since_epoch Nanoseconds since Unix epoch
     * @return 0 on success, -1 on error
     */
    int (*set_realtime_ns)(sf_time_ns_t ns_since_epoch);

    /**
     * @brief Format timestamp as human-readable string
     * @param ns_since_epoch Nanoseconds since Unix epoch
     * @param buf Output buffer (SF_TIME_STAMP_SIZE bytes)
     * @param format_24h Use 24-hour format (1) or 12-hour (0)
     * @return 0 on success, -1 on error
     */
    int (*format_timestamp)(sf_time_ns_t ns_since_epoch, char *buf, int format_24h);

    /**
     * @brief Check if real-time clock is available
     * @return 1 if RTC available, 0 if not
     */
    int (*has_rtc)(void);
} sf_time_backend_t;

/* Global backend pointer (set at initialization) */
extern const sf_time_backend_t *sf_time_backend;

/* Platform-specific backend registration */
extern const sf_time_backend_t sf_time_backend_posix; /* POSIX implementation */
extern const sf_time_backend_t sf_time_backend_l4re; /* L4Re/StarshipOS implementation */

/* Convenience wrappers (inline for zero overhead)
 * For kernel builds (__STARKERNEL__), these are provided by shim.c as
 * non-inline functions to avoid GOT indirection issues with -fPIC. */

#ifndef __STARKERNEL__
static inline sf_time_ns_t sf_monotonic_ns(void) {
    return sf_time_backend->get_monotonic_ns();
}

static inline sf_time_ns_t sf_realtime_ns(void) {
    return sf_time_backend->get_realtime_ns();
}

static inline int sf_set_realtime_ns(sf_time_ns_t ns) {
    return sf_time_backend->set_realtime_ns(ns);
}

static inline int sf_format_timestamp(sf_time_ns_t ns, char *buf, int format_24h) {
    return sf_time_backend->format_timestamp(ns, buf, format_24h);
}

static inline int sf_has_rtc(void) {
    return sf_time_backend->has_rtc();
}
#else
/* Kernel: declarations only - implementations in shim.c */
sf_time_ns_t sf_monotonic_ns(void);
sf_time_ns_t sf_realtime_ns(void);
int sf_set_realtime_ns(sf_time_ns_t ns);
int sf_format_timestamp(sf_time_ns_t ns, char *buf, int format_24h);
int sf_has_rtc(void);
#endif

/* Time conversion helpers */

static inline sf_time_ns_t sf_seconds_to_ns(uint64_t seconds) {
    return seconds * 1000000000ULL;
}

static inline uint64_t sf_ns_to_seconds(sf_time_ns_t ns) {
    return ns / 1000000000ULL;
}

static inline uint64_t sf_ns_to_ms(sf_time_ns_t ns) {
    return ns / 1000000ULL;
}

static inline uint64_t sf_ns_to_us(sf_time_ns_t ns) {
    return ns / 1000ULL;
}

/* Initialize platform time backend (call once at startup) */
void sf_time_init(void);

#ifdef __cplusplus
}
#endif

#endif /* STARFORTH_PLATFORM_TIME_H */