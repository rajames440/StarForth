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
 * StarForth Platform Time - L4Re/StarshipOS Backend
 *
 * Implements platform time abstraction using L4Re RTC server and KIP clock.
 *
 * License: CC0-1.0 / Public Domain
 */

#include "../../../include/platform_time.h"

#ifdef __l4__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* L4Re headers */
#include <l4/sys/kip.h>
#include <l4/re/env.h>
#include <l4/rtc/rtc>

/* L4Re RTC capability (obtained at init) */
static L4::Cap<L4rtc::Rtc> rtc_cap = L4::Cap<L4rtc::Rtc>::Invalid;
static l4_uint64_t rtc_offset = 0; /* nanoseconds from 1970 to boot */
static int rtc_available = 0;

/* L4Re implementation */

static sf_time_ns_t l4re_get_monotonic_ns(void) {
    /* Use L4Re KIP clock: monotonic time since boot */
    l4_kernel_info_t *kip = l4re_kip();
    if (!kip) {
        return 0; /* Fallback if KIP unavailable */
    }
    return l4_kip_clock_ns(kip);
}

static sf_time_ns_t l4re_get_realtime_ns(void) {
    /* Use RTC server offset + KIP clock for wall-clock time */
    if (!rtc_available) {
        /* No RTC: return monotonic time (better than nothing) */
        return l4re_get_monotonic_ns();
    }

    /* Real-time = RTC offset (ns since epoch to boot) + KIP clock (ns since boot) */
    l4_kernel_info_t *kip = l4re_kip();
    if (!kip) {
        return rtc_offset; /* Fallback to RTC offset only */
    }

    return rtc_offset + l4_kip_clock_ns(kip);
}

static int l4re_set_realtime_ns(sf_time_ns_t ns_since_epoch) {
    if (!rtc_available || !rtc_cap.is_valid()) {
        return -1; /* No RTC capability */
    }

    /* Calculate new offset: desired epoch time - current KIP clock */
    l4_kernel_info_t *kip = l4re_kip();
    if (!kip) {
        return -1;
    }

    l4_uint64_t kip_ns = l4_kip_clock_ns(kip);
    l4_uint64_t new_offset = ns_since_epoch - kip_ns;

    /* Set RTC offset via IPC */
    long ret = rtc_cap->set_timer_offset(new_offset);
    if (ret < 0) {
        return -1;
    }

    /* Update cached offset */
    rtc_offset = new_offset;
    return 0;
}

static int l4re_format_timestamp(sf_time_ns_t ns_since_epoch, char *buf, int format_24h) {
    if (!buf) {
        return -1;
    }

    /* Convert nanoseconds to seconds for libc time functions */
    time_t seconds = (time_t)(ns_since_epoch / 1000000000ULL);

    /* Use L4Re libc localtime + strftime */
    struct tm *tm_info = localtime(&seconds);
    if (!tm_info) {
        /* Fallback: show raw seconds */
        snprintf(buf, SF_TIME_STAMP_SIZE, "%llu", (unsigned long long) seconds);
        return -1;
    }

    /* Format timestamp based on 24h preference */
    const char *format = format_24h ? "%Y-%m-%d %H:%M:%S" : "%Y-%m-%d %I:%M:%S %p";
    size_t ret = strftime(buf, SF_TIME_STAMP_SIZE, format, tm_info);

    return (ret > 0) ? 0 : -1;
}

static int l4re_has_rtc(void) {
    return rtc_available;
}

/* L4Re backend vtable */
const sf_time_backend_t sf_time_backend_l4re = {
    .get_monotonic_ns = l4re_get_monotonic_ns,
    .get_realtime_ns = l4re_get_realtime_ns,
    .set_realtime_ns = l4re_set_realtime_ns,
    .format_timestamp = l4re_format_timestamp,
    .has_rtc = l4re_has_rtc,
};

/* L4Re-specific initialization (called by sf_time_init) */
void sf_time_init_l4re(void) {
    fprintf(stderr, "[L4Re Time] Initializing L4Re time backend...\n");

    /* Try to get RTC capability from L4Re environment */
    rtc_cap = L4Re::Env::env()->get_cap<L4rtc::Rtc>("rtc");

    if (!rtc_cap.is_valid()) {
        fprintf(stderr, "[L4Re Time] WARNING: RTC capability not available\n");
        fprintf(stderr, "[L4Re Time] Falling back to monotonic-only mode\n");
        fprintf(stderr, "[L4Re Time] Real-time will use KIP clock (time since boot)\n");
        rtc_available = 0;
        rtc_offset = 0;
        return;
    }

    /* RTC capability acquired successfully */
    fprintf(stderr, "[L4Re Time] RTC capability acquired\n");

    /* Query the RTC timer offset (nanoseconds from epoch to boot) */
    long ret = rtc_cap->get_timer_offset(&rtc_offset);

    if (ret < 0) {
        fprintf(stderr, "[L4Re Time] ERROR: Failed to get RTC timer offset (ret=%ld)\n", ret);
        fprintf(stderr, "[L4Re Time] RTC may not be initialized or running\n");
        fprintf(stderr, "[L4Re Time] Setting offset to 0 (will show time since boot as epoch)\n");
        rtc_available = 0;
        rtc_offset = 0;
        return;
    }

    /* Success! */
    rtc_available = 1;
    fprintf(stderr, "[L4Re Time] RTC offset retrieved: %llu ns since epoch\n",
            (unsigned long long) rtc_offset);

    /* Convert offset to a readable date for verification */
    time_t boot_time_sec = (time_t)(rtc_offset / 1000000000ULL);
    struct tm *boot_tm = gmtime(&boot_time_sec);
    if (boot_tm) {
        char boot_str[64];
        strftime(boot_str, sizeof(boot_str), "%Y-%m-%d %H:%M:%S UTC", boot_tm);
        fprintf(stderr, "[L4Re Time] System boot time: %s\n", boot_str);
    }

    /* Display current time */
    l4_kernel_info_t *kip = l4re_kip();
    if (kip) {
        l4_uint64_t now_ns = rtc_offset + l4_kip_clock_ns(kip);
        time_t now_sec = (time_t)(now_ns / 1000000000ULL);
        struct tm *now_tm = gmtime(&now_sec);
        if (now_tm) {
            char now_str[64];
            strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S UTC", now_tm);
            fprintf(stderr, "[L4Re Time] Current time: %s\n", now_str);
        }
    }

    fprintf(stderr, "[L4Re Time] Initialization complete\n");
}

#else
/* Dummy implementation when not building for L4Re */
const sf_time_backend_t sf_time_backend_l4re = {0};

void sf_time_init_l4re(void) {
}
#endif /* __l4__ */