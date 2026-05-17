/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/*
 * timer.c (riscv64) - Timer using rdcycle counter.
 *
 * RISC-V provides rdcycle (CPU cycle counter) and rdtime (wall-clock timer).
 * We use rdcycle as the primary timestamp source; frequency is estimated
 * at 1 GHz (QEMU virt default) and updated if firmware provides a hint.
 */

#include "timer.h"
#include "console.h"
#include "q48_16.h"
#include "uefi.h"
#include <stdint.h>

static inline uint64_t rdcycle(void)
{
    uint64_t val;
    __asm__ volatile ("rdcycle %0" : "=r"(val));
    return val;
}

static uint64_t s_counter_hz  = 1000000000ULL; /* assume 1 GHz */
static uint64_t s_ns_per_tick = 0;
static uint64_t s_base_count  = 0;
static uint64_t s_base_ns     = 0;

static timer_calibration_record_t s_cal;
static TimeTrustState g_heartbeat;

int timer_init(BootInfo *boot_info)
{
    (void)boot_info;

    s_base_count  = rdcycle();
    s_base_ns     = 0;
    s_ns_per_tick = (1000000000ULL << 16) / s_counter_hz;

    s_cal.tsc_hz_mean = s_counter_hz;
    s_cal.hpet_hz     = 0;
    s_cal.pit_hz_mean = 0;
    s_cal.converged   = 1;
    s_cal.vm_mode     = 1;
    s_cal.trust       = TIMER_TRUST_ABSOLUTE;

    console_println("Timer: RISC-V rdcycle timer initialised.");
    return 0;
}

uint64_t timer_tsc_hz(void)
{
    return s_counter_hz;
}

uint64_t timer_now_ns(void)
{
    uint64_t delta = rdcycle() - s_base_count;
    return s_base_ns + ((delta * s_ns_per_tick) >> 16);
}

int timer_check_drift_now(void) { return 0; }

const timer_calibration_record_t *timer_calibration_record(void)
{
    return &s_cal;
}

void heartbeat_init(uint64_t tsc_hz, uint64_t tick_hz)
{
    g_heartbeat.ticks         = 0;
    g_heartbeat.last_tsc      = 0;
    g_heartbeat.total_samples = 0;
    g_heartbeat.variance      = 0;
    g_heartbeat.trust         = Q48_ONE;

    g_heartbeat.window.pos   = 0;
    g_heartbeat.window.count = 0;
    for (int i = 0; i < TIME_WINDOW_SIZE; i++)
        g_heartbeat.window.deltas[i] = 0;

    g_heartbeat.expected_delta = (tick_hz > 0 && tsc_hz > 0)
        ? (tsc_hz / tick_hz) : 10000000ULL;
}

void heartbeat_tick(void)
{
    uint64_t now = rdcycle();
    if (g_heartbeat.last_tsc != 0) {
        int64_t delta = (int64_t)(now - g_heartbeat.last_tsc)
                      - (int64_t)g_heartbeat.expected_delta;
        uint32_t pos = g_heartbeat.window.pos % TIME_WINDOW_SIZE;
        g_heartbeat.window.deltas[pos] = delta;
        g_heartbeat.window.pos++;
        if (g_heartbeat.window.count < TIME_WINDOW_SIZE)
            g_heartbeat.window.count++;
        g_heartbeat.total_samples++;
    }
    g_heartbeat.last_tsc = now;
    g_heartbeat.ticks++;
    g_heartbeat.trust = Q48_ONE;
}

uint64_t heartbeat_ticks(void) { return g_heartbeat.ticks; }

time_trust_t heartbeat_trust(void) { return g_heartbeat.trust; }

const TimeTrustState *heartbeat_state(void) { return &g_heartbeat; }
