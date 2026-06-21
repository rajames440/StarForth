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
 * StarForth Platform Time - Linux/POSIX Backend
 *
 * Implements platform time abstraction using POSIX clock_gettime().
 *
 * License: See LICENSE file
 */

#define _POSIX_C_SOURCE 199309L
#include "../../../include/platform_time.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

/* POSIX implementation */

/**
 * @brief Return the current monotonic time in nanoseconds.
 *
 * Reads @c CLOCK_MONOTONIC via @c clock_gettime(). The clock advances
 * continuously and is unaffected by wall-clock adjustments — suitable for
 * measuring elapsed durations such as execution latency and decay intervals.
 * Returns 0 on @c clock_gettime() failure (should not occur on any modern
 * POSIX kernel).
 *
 * @return Nanoseconds since an arbitrary but fixed epoch, or 0 on error
 */
static sf_time_ns_t posix_get_monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (sf_time_ns_t) ts.tv_sec * 1000000000ULL + (sf_time_ns_t) ts.tv_nsec;
}

/**
 * @brief Return the current wall-clock time in nanoseconds since the Unix epoch.
 *
 * Reads @c CLOCK_REALTIME via @c clock_gettime(). Subject to NTP or @c settimeofday()
 * adjustments — use @c posix_get_monotonic_ns() for interval timing.
 * Returns 0 on @c clock_gettime() failure.
 *
 * @return Nanoseconds since 1970-01-01 00:00:00 UTC, or 0 on error
 */
static sf_time_ns_t posix_get_realtime_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (sf_time_ns_t) ts.tv_sec * 1000000000ULL + (sf_time_ns_t) ts.tv_nsec;
}

/**
 * @brief Set the system real-time clock to @c ns_since_epoch.
 *
 * Converts @c ns_since_epoch to a @c struct timespec and calls
 * @c clock_settime(CLOCK_REALTIME). Requires @c CAP_SYS_TIME or equivalent
 * privilege; fails with @c EPERM if the caller lacks permission.
 *
 * @param ns_since_epoch Desired new wall-clock time in nanoseconds since Unix epoch
 * @return 0 on success, non-zero errno-encoded error on failure
 */
static int posix_set_realtime_ns(sf_time_ns_t ns_since_epoch) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns_since_epoch / 1000000000ULL);
    ts.tv_nsec = (long) (ns_since_epoch % 1000000000ULL);
    return clock_settime(CLOCK_REALTIME, &ts);
}

/**
 * @brief Format a nanosecond-precision wall-clock time as a human-readable string.
 *
 * Converts @c ns_since_epoch to local time, formats hours/minutes/seconds via
 * @c strftime(), then appends a three-digit millisecond suffix. The result is
 * written into @c buf (must be at least @c SF_TIME_STAMP_SIZE bytes). On
 * @c localtime() or @c strftime() failure, writes @c "??:??:??.???" into @c buf
 * and returns -1.
 *
 * @param ns_since_epoch Wall-clock time in nanoseconds since Unix epoch
 * @param buf            Output buffer (at least @c SF_TIME_STAMP_SIZE bytes); must not be NULL
 * @param format_24h     Non-zero for 24-hour format (@c %H:%M:%S), zero for 12-hour (@c %I:%M:%S %p)
 * @return 0 on success, -1 if @c buf is NULL or time formatting fails
 */
static int posix_format_timestamp(sf_time_ns_t ns_since_epoch, char *buf, int format_24h) {
    if (!buf)
        return -1;

    time_t seconds = (time_t)(ns_since_epoch / 1000000000ULL);
    unsigned int ms = (unsigned int)((ns_since_epoch % 1000000000ULL) / 1000000ULL);
    struct tm *tm_info = localtime(&seconds);
    if (!tm_info) {
        strncpy(buf, "??:??:??.???", SF_TIME_STAMP_SIZE - 1);
        buf[SF_TIME_STAMP_SIZE - 1] = '\0';
        return -1;
    }

    char hms[16];
    const char *format = format_24h ? "%H:%M:%S" : "%I:%M:%S %p";
    if (strftime(hms, sizeof(hms), format, tm_info) == 0) {
        strncpy(buf, "??:??:??.???", SF_TIME_STAMP_SIZE - 1);
        buf[SF_TIME_STAMP_SIZE - 1] = '\0';
        return -1;
    }

    snprintf(buf, SF_TIME_STAMP_SIZE, "%s.%03u", hms, ms);
    return 0;
}

/**
 * @brief Probe whether a real-time clock is available.
 *
 * Attempts a @c clock_gettime(CLOCK_REALTIME) probe. POSIX systems invariably
 * support @c CLOCK_REALTIME so this will return 1 in practice; the check
 * mirrors the bare-metal backend's interface where RTC presence is genuinely
 * uncertain.
 *
 * @return 1 if @c CLOCK_REALTIME is readable, 0 otherwise
 */
static int posix_has_rtc(void) {
    /* POSIX systems typically have working CLOCK_REALTIME */
    struct timespec ts;
    return (clock_gettime(CLOCK_REALTIME, &ts) == 0) ? 1 : 0;
}

/* POSIX backend vtable */
const sf_time_backend_t sf_time_backend_posix = {
    .get_monotonic_ns = posix_get_monotonic_ns,
    .get_realtime_ns = posix_get_realtime_ns,
    .set_realtime_ns = posix_set_realtime_ns,
    .format_timestamp = posix_format_timestamp,
    .has_rtc = posix_has_rtc,
};