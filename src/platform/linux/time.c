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
#include <time.h>
#include <string.h>

/* POSIX implementation */

static sf_time_ns_t posix_get_monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (sf_time_ns_t) ts.tv_sec * 1000000000ULL + (sf_time_ns_t) ts.tv_nsec;
}

static sf_time_ns_t posix_get_realtime_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (sf_time_ns_t) ts.tv_sec * 1000000000ULL + (sf_time_ns_t) ts.tv_nsec;
}

static int posix_set_realtime_ns(sf_time_ns_t ns_since_epoch) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns_since_epoch / 1000000000ULL);
    ts.tv_nsec = (long) (ns_since_epoch % 1000000000ULL);
    return clock_settime(CLOCK_REALTIME, &ts);
}

static int posix_format_timestamp(sf_time_ns_t ns_since_epoch, char *buf, int format_24h) {
    if (!buf)
        return -1;

    time_t seconds = (time_t)(ns_since_epoch / 1000000000ULL);
    struct tm *tm_info = localtime(&seconds);
    if (!tm_info) {
        strncpy(buf, "??:??:??", SF_TIME_STAMP_SIZE);
        return -1;
    }

    const char *format = format_24h ? "%H:%M:%S" : "%I:%M:%S %p";
    if (strftime(buf, SF_TIME_STAMP_SIZE, format, tm_info) == 0) {
        strncpy(buf, "??:??:??", SF_TIME_STAMP_SIZE);
        return -1;
    }

    return 0;
}

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