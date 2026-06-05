/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * timer.c (aarch64) - Timer subsystem using ARM Generic Timer (CNTPCT_EL0)
 *
 * Uses the 64-bit system counter available as CNTPCT_EL0.  The counter
 * frequency is read from CNTFRQ_EL0 (set by firmware).
 */

#include "timer.h"
#include "console.h"
#include "q48_16.h"
#include "uefi.h"
#include <stdint.h>

/* ─── ARM generic-timer helpers ─────────────────────────────────────── */

static inline uint64_t cntpct_read(void)
{
    uint64_t val;
    __asm__ volatile ("isb; mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline uint64_t cntfrq_read(void)
{
    uint64_t val;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

/* ─── Module state ───────────────────────────────────────────────────── */

static uint64_t s_counter_hz   = 0;   /* CNTFRQ_EL0 (ticks/second) */
static uint64_t s_ns_per_tick  = 0;   /* nanoseconds per counter tick (scaled) */
static uint64_t s_base_count   = 0;   /* counter value at timer_init() */
static uint64_t s_base_ns      = 0;   /* ns offset at timer_init() */

static timer_calibration_record_t s_cal;

/* ─── Heartbeat state ────────────────────────────────────────────────── */

static TimeTrustState g_heartbeat;

/* ─── timer_init ─────────────────────────────────────────────────────── */

int timer_init(BootInfo *boot_info)
{
    (void)boot_info;

    uint64_t freq = cntfrq_read();
    if (freq == 0) {
        /* Firmware did not set CNTFRQ; assume 62.5 MHz (Cortex-A57 default) */
        freq = 62500000ULL;
    }

    s_counter_hz  = freq;
    s_base_count  = cntpct_read();
    s_base_ns     = 0;

    /* ns_per_tick = 1e9 / freq — store as (1e9 << 16) / freq for fixed-point */
    s_ns_per_tick = (1000000000ULL << 16) / freq;

    s_cal.tsc_hz_mean  = freq;
    s_cal.hpet_hz      = 0;
    s_cal.pit_hz_mean  = 0;
    s_cal.converged    = 1;
    s_cal.vm_mode      = 1;
    s_cal.trust        = TIMER_TRUST_ABSOLUTE;

    console_println("Timer: AArch64 generic timer initialised.");
    return 0;
}

/* ─── timer_tsc_hz ───────────────────────────────────────────────────── */

uint64_t timer_tsc_hz(void)
{
    return s_counter_hz ? s_counter_hz : 62500000ULL;
}

/* ─── timer_now_ns ───────────────────────────────────────────────────── */

uint64_t timer_now_ns(void)
{
    uint64_t delta = cntpct_read() - s_base_count;
    /* delta * (1e9 / freq) = (delta * ns_per_tick_q16) >> 16 */
    return s_base_ns + ((delta * s_ns_per_tick) >> 16);
}

/* ─── timer_check_drift_now ──────────────────────────────────────────── */

int timer_check_drift_now(void)
{
    return 0;  /* Stub: ARM counter has no drift against itself */
}

/* ─── timer_calibration_record ───────────────────────────────────────── */

const timer_calibration_record_t *timer_calibration_record(void)
{
    return &s_cal;
}

/* ─── Heartbeat ──────────────────────────────────────────────────────── */

void heartbeat_init(uint64_t tsc_hz, uint64_t tick_hz)
{
    g_heartbeat.ticks          = 0;
    g_heartbeat.last_tsc       = 0;
    g_heartbeat.total_samples  = 0;
    g_heartbeat.variance       = 0;
    g_heartbeat.trust          = Q48_ONE;

    g_heartbeat.window.pos   = 0;
    g_heartbeat.window.count = 0;
    for (int i = 0; i < TIME_WINDOW_SIZE; i++) {
        g_heartbeat.window.deltas[i] = 0;
    }

    g_heartbeat.expected_delta = (tick_hz > 0 && tsc_hz > 0)
        ? (tsc_hz / tick_hz)
        : 10000000ULL;
}

void heartbeat_tick(void)
{
    uint64_t now = cntpct_read();
    if (g_heartbeat.last_tsc != 0) {
        int64_t delta = (int64_t)(now - g_heartbeat.last_tsc)
                      - (int64_t)g_heartbeat.expected_delta;
        uint32_t pos = g_heartbeat.window.pos % TIME_WINDOW_SIZE;
        g_heartbeat.window.deltas[pos] = delta;
        g_heartbeat.window.pos++;
        if (g_heartbeat.window.count < TIME_WINDOW_SIZE) {
            g_heartbeat.window.count++;
        }
        g_heartbeat.total_samples++;
    }
    g_heartbeat.last_tsc = now;
    g_heartbeat.ticks++;
    g_heartbeat.trust = Q48_ONE;  /* Simplified: full trust on ARM */
}

uint64_t heartbeat_ticks(void)
{
    return g_heartbeat.ticks;
}

time_trust_t heartbeat_trust(void)
{
    return g_heartbeat.trust;
}

const TimeTrustState *heartbeat_state(void)
{
    return &g_heartbeat;
}
