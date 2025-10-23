/*
                                  ***   StarForth   ***

  time.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.453-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/platform/linux/time.c
 */

/*
 * StarForth Platform Time - Linux/POSIX Backend
 *
 * Implements platform time abstraction using POSIX clock_gettime().
 *
 * License: CC0-1.0 / Public Domain
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