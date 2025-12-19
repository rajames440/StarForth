/**
* timer.h - Timer calibration interface
 */

#ifndef STARKERNEL_TIMER_H
#define STARKERNEL_TIMER_H

#include <stdint.h>
#include "uefi.h"

/*
 * Timer trust levels:
 *  - NONE: no usable time base (should not happen after successful init)
 *  - RELATIVE: monotonic-ish time suitable for bring-up / scheduling, NOT for claims
 *  - ABSOLUTE: invariant + calibrated time suitable for drift bounds / claims
 */
typedef enum timer_trust_level {
    TIMER_TRUST_NONE     = 0,
    TIMER_TRUST_RELATIVE = 1,
    TIMER_TRUST_ABSOLUTE = 2
} timer_trust_t;

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
    uint8_t  trust;          /* timer_trust_t (NONE/RELATIVE/ABSOLUTE) */
    uint8_t  reserved[1];
} timer_calibration_record_t;

int      timer_init(BootInfo *boot_info);
uint64_t timer_tsc_hz(void);
uint64_t timer_now_ns(void);
int      timer_check_drift_now(void);
const timer_calibration_record_t *timer_calibration_record(void);

#endif /* STARKERNEL_TIMER_H */
