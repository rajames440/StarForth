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

/**
 * timer.h - Timer and Heartbeat Interface
 *
 * M5 Time Model:
 *   - TIME-TICKS (Q64.0): Monotonic heartbeat counter, never decreases
 *   - TIME-TRUST (Q48.16): Continuous confidence metric [0.0, 1.0]
 *   - No discrete modes (NONE/REL/ABS are legacy, being phased out)
 *   - Trust is a measurement, never gates execution
 */

#ifndef STARKERNEL_TIMER_H
#define STARKERNEL_TIMER_H

#include <stdint.h>
#include "uefi.h"
#include "q48_16.h"

/* ============================================================================
 * M5 Time Model (New)
 * ============================================================================ */

/* TIME-TRUST: Continuous confidence metric in Q48.16 format */
typedef q48_16_t time_trust_t;

/* Rolling window size for timestamp variance computation */
#define TIME_WINDOW_SIZE  64

/* TIME-TRUST thresholds in Q48.16 (for diagnostics, NOT for gating) */
#define TIME_TRUST_HIGH   Q48_ONE              /* 1.0 = full confidence */
#define TIME_TRUST_LOW    (Q48_ONE >> 2)       /* 0.25 = low confidence */

/*
 * Rolling window of timestamp deltas for variance computation.
 * Each entry is (actual_tsc_delta - expected_tsc_delta) in TSC ticks.
 */
typedef struct time_window {
    int64_t  deltas[TIME_WINDOW_SIZE];   /* Signed: can be early or late */
    uint32_t pos;                         /* Current write position */
    uint32_t count;                       /* Number of valid samples (up to SIZE) */
} TimeWindow;

/*
 * M5 Heartbeat State: Holds all time-related metrics.
 * Updated every heartbeat tick by the ISR.
 */
typedef struct time_trust_state {
    /* Core counters */
    uint64_t     ticks;            /* TIME-TICKS: monotonic heartbeat count */
    uint64_t     last_tsc;         /* TSC at last heartbeat */
    uint64_t     expected_delta;   /* Expected TSC ticks per heartbeat */

    /* Rolling window for variance */
    TimeWindow   window;

    /* Derived metrics (Q48.16) */
    q48_16_t     variance;         /* Variance of deltas */
    q48_16_t     trust;            /* TIME-TRUST: derived from variance */

    /* Statistics */
    uint64_t     total_samples;    /* Lifetime sample count */
} TimeTrustState;

/* ============================================================================
 * Legacy M4 Interface (To Be Phased Out)
 * ============================================================================ */

/*
 * Timer trust levels (LEGACY - discrete modes violate M5 spec):
 *  - NONE: no usable time base
 *  - RELATIVE: monotonic-ish, not for claims
 *  - ABSOLUTE: invariant + calibrated
 */
typedef enum timer_trust_level {
    TIMER_TRUST_NONE     = 0,
    TIMER_TRUST_RELATIVE = 1,
    TIMER_TRUST_ABSOLUTE = 2
} timer_trust_level_t;

/*
 * Timer calibration record for logging / DoE traceability.
 * Keep it minimal and serial-friendly.
 */
typedef struct timer_calibration_record {
    uint64_t hpet_hz;        /* HPET frequency derived from period_fs (if available) */
    uint64_t tsc_hz_mean;    /* Locked TSC Hz (final) */
    uint64_t pit_hz_mean;    /* PIT-based estimate (if used) */
    uint64_t cv_hpet_ppm;    /* HPET window CV in ppm (bare metal convergence) */
    uint64_t cv_pit_ppm;     /* PIT window CV in ppm (bare metal convergence) */
    uint64_t diff_ppm;       /* HPET vs PIT mean diff in ppm (bare metal convergence) */
    uint32_t windows_used;   /* number of windows consumed to converge */
    uint8_t  converged;      /* 1 if converged/locked, 0 otherwise */
    uint8_t  vm_mode;        /* 1 if hypervisor policy path used */
    uint8_t  trust;          /* timer_trust_level_t (NONE/RELATIVE/ABSOLUTE) */
    uint8_t  reserved[1];
} timer_calibration_record_t;

/* Legacy API (still works, wraps M5 internals) */
int      timer_init(BootInfo *boot_info);
uint64_t timer_tsc_hz(void);
uint64_t timer_now_ns(void);
int      timer_check_drift_now(void);
const timer_calibration_record_t *timer_calibration_record(void);

/* ============================================================================
 * M5 Heartbeat API (New)
 * ============================================================================ */

/**
 * Initialize the heartbeat subsystem.
 * Called after timer_init(), before enabling APIC timer.
 */
void heartbeat_init(uint64_t tsc_hz, uint64_t tick_hz);

/**
 * Called by heartbeat ISR on each tick.
 * Updates TIME-TICKS, samples TSC, updates rolling window and TIME-TRUST.
 */
void heartbeat_tick(void);

/**
 * Get current TIME-TICKS (monotonic heartbeat count).
 */
uint64_t heartbeat_ticks(void);

/**
 * Get current TIME-TRUST (Q48.16 confidence metric).
 */
time_trust_t heartbeat_trust(void);

/**
 * Get pointer to full heartbeat state (for diagnostics).
 */
const TimeTrustState *heartbeat_state(void);

#endif /* STARKERNEL_TIMER_H */
