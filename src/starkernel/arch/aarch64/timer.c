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

/**
 * @brief Read the AArch64 Physical System Counter with ISB serialisation.
 *
 * Issues an @c ISB (Instruction Synchronisation Barrier) before reading
 * @c CNTPCT_EL0 to ensure all preceding instructions have completed before
 * the counter value is sampled. This prevents the counter read from being
 * speculated ahead of a preceding store or flag update, which would produce
 * an erroneously early timestamp.
 *
 * The ISB is heavier than strictly necessary for most timing uses but
 * provides the same ordering guarantee as @c RDTSCP on x86-64. For
 * interval measurements where start and end reads are in the same path,
 * the overhead (1–3 cycles) is negligible.
 *
 * @return Current 64-bit @c CNTPCT_EL0 counter value in system-counter ticks.
 */
static inline uint64_t cntpct_read(void)
{
    uint64_t val;
    __asm__ volatile ("isb; mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

/**
 * @brief Read the AArch64 Counter Frequency Register (@c CNTFRQ_EL0).
 *
 * @c CNTFRQ_EL0 is a read-only EL0-accessible register whose value is
 * written by EL3/EL2 firmware at boot to advertise the system counter
 * frequency in Hz. It is the architectural way to discover @c CNTPCT_EL0's
 * tick rate without out-of-band knowledge of the board.
 *
 * QEMU's SBSA/virt machine models typically set this to 62,500,000 Hz
 * (62.5 MHz) for Cortex-A57 compatibility. Real hardware varies widely
 * (10 MHz on Raspberry Pi 4, 24 MHz on Cortex-A53 dev boards, etc.).
 * @c timer_init() falls back to 62,500,000 if the register reads zero.
 *
 * @return Counter frequency in Hz as set by firmware; 0 if firmware did
 *         not initialise the register (uncommon but possible in bare-metal
 *         bring-up without a proper firmware stack).
 */
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

/**
 * @brief Initialise the AArch64 timer subsystem from @c CNTFRQ_EL0 (M5).
 *
 * Reads the system counter frequency from @c CNTFRQ_EL0. If firmware left
 * this register zero (a non-conforming but possible situation in bare-metal
 * bring-up) the function falls back to 62,500,000 Hz — the canonical
 * Cortex-A57 / QEMU SBSA frequency.
 *
 * Derived values stored in module state:
 * - @c s_counter_hz — raw frequency in Hz.
 * - @c s_base_count — @c CNTPCT_EL0 sample at init time; all subsequent
 *   @c timer_now_ns() calls compute elapsed ticks as
 *   @c (cntpct_read() - s_base_count).
 * - @c s_base_ns — 0 at init; the ns origin for @c timer_now_ns().
 * - @c s_ns_per_tick — Q16.16 fixed-point nanoseconds per counter tick,
 *   computed as @c (1e9 << 16) / @c freq to avoid floating-point in the
 *   freestanding build. @c timer_now_ns() right-shifts by 16 after the
 *   multiply to recover the integer nanosecond count.
 *
 * The calibration record @c s_cal is populated with the firmware-reported
 * frequency, @c converged = 1, and @c trust = @c TIMER_TRUST_ABSOLUTE
 * because the ARM Generic Timer is invariant and synchronised by
 * architecture — no drift-detection loop is needed.
 *
 * @param boot_info Kernel @c BootInfo (ACPI/memory map); unused on AArch64
 *                  for timer init (GIC base address will be used in a later
 *                  milestone for the interrupt controller).
 * @return 0 always.
 */
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

/**
 * @brief Return the system counter frequency in Hz.
 *
 * Returns @c s_counter_hz as set from @c CNTFRQ_EL0 by @c timer_init().
 * If @c timer_init() has not been called yet (or firmware reported 0 and
 * the fallback was not applied), returns 62,500,000 Hz as a safe default.
 *
 * Used by @c apic_timer_init() to compute @c s_timer_period_tsc and by
 * the shim time backend to convert counter ticks to nanoseconds before
 * the full timer subsystem is available.
 *
 * @return System counter frequency in Hz; at least 62,500,000.
 */
uint64_t timer_tsc_hz(void)
{
    return s_counter_hz ? s_counter_hz : 62500000ULL;
}

/* ─── timer_now_ns ───────────────────────────────────────────────────── */

/**
 * @brief Return a monotonic nanosecond timestamp.
 *
 * Reads @c CNTPCT_EL0, computes the delta from @c s_base_count (captured
 * at @c timer_init()), and converts to nanoseconds using the Q16.16
 * fixed-point @c s_ns_per_tick:
 *
 * @code
 *   ns = s_base_ns + ((delta * s_ns_per_tick) >> 16)
 * @endcode
 *
 * The multiplication uses 64-bit arithmetic; at typical counter frequencies
 * (≤ 100 MHz) and reasonable uptime intervals (days to weeks) there is no
 * overflow risk in the delta-to-ns conversion.
 *
 * @return Monotonic nanosecond count since @c timer_init(), starting at 0.
 */
uint64_t timer_now_ns(void)
{
    uint64_t delta = cntpct_read() - s_base_count;
    /* delta * (1e9 / freq) = (delta * ns_per_tick_q16) >> 16 */
    return s_base_ns + ((delta * s_ns_per_tick) >> 16);
}

/* ─── timer_check_drift_now ──────────────────────────────────────────── */

/**
 * @brief Check for timer drift (AArch64 stub — always returns 0).
 *
 * On x86-64 this function cross-checks the TSC against the HPET or PM
 * Timer to detect frequency drift. The AArch64 Generic Timer is
 * architecturally invariant and synchronised across all cores in the
 * system; it cannot drift against itself. This stub always returns 0
 * (no drift detected) to satisfy the common @c timer_check_drift_now()
 * call site.
 *
 * @return 0 always.
 */
int timer_check_drift_now(void)
{
    return 0;  /* Stub: ARM counter has no drift against itself */
}

/* ─── timer_calibration_record ───────────────────────────────────────── */

/**
 * @brief Return a pointer to the timer calibration record.
 *
 * Returns @c &s_cal which is populated by @c timer_init() with:
 * - @c tsc_hz_mean = @c CNTFRQ_EL0 (or 62.5 MHz fallback)
 * - @c hpet_hz = 0 (no HPET on AArch64)
 * - @c pit_hz_mean = 0 (no PIT on AArch64)
 * - @c converged = 1 (Generic Timer is already calibrated by firmware)
 * - @c vm_mode = 1 (QEMU SBSA always uses virtualised Generic Timer)
 * - @c trust = @c TIMER_TRUST_ABSOLUTE
 *
 * The record is exposed to @c kernel_main() and @c timer.h consumers for
 * boot-log reporting and for the heartbeat subsystem's trust initialisation.
 *
 * @return Pointer to the module-static calibration record; valid for the
 *         lifetime of the kernel.
 */
const timer_calibration_record_t *timer_calibration_record(void)
{
    return &s_cal;
}

/* ─── Heartbeat ──────────────────────────────────────────────────────── */

/**
 * @brief Initialise the heartbeat rolling-window state.
 *
 * Zeroes the @c TimeTrustState @c g_heartbeat and sets up the expected
 * inter-tick interval as @c tsc_hz / @c tick_hz system-counter ticks. On
 * AArch64 "TSC" refers to @c CNTPCT_EL0; the variable name is preserved
 * for cross-ISA consistency.
 *
 * If either @p tsc_hz or @p tick_hz is zero, @c expected_delta falls back
 * to 10,000,000 ticks (≈ 160 ms at 62.5 MHz) which is a safe non-zero
 * sentinel preventing division-by-zero in the rolling window.
 *
 * Initial @c trust is set to @c Q48_ONE (full trust) because the ARM
 * Generic Timer is invariant by architecture and requires no initial
 * warm-up period unlike the x86 TSC.
 *
 * @param tsc_hz  System counter frequency (Hz); from @c timer_tsc_hz().
 * @param tick_hz Heartbeat interrupt rate (Hz); from @c apic_timer_init().
 */
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

/**
 * @brief Record one heartbeat tick and update the inter-tick deviation window.
 *
 * Called from the timer ISR (or its AArch64 stub equivalent) at each
 * periodic heartbeat. Reads @c CNTPCT_EL0 and, if @c last_tsc is
 * non-zero, computes the signed deviation:
 *
 * @code
 *   delta = (now - last_tsc) - expected_delta
 * @endcode
 *
 * Stores @p delta into the circular @c window.deltas[] buffer at position
 * @c (window.pos % TIME_WINDOW_SIZE) and advances the pointer. On AArch64,
 * @c trust is unconditionally set to @c Q48_ONE (full confidence) because
 * the Generic Timer is invariant and needs no statistical quality estimate.
 * The x86-64 path uses @c variance_to_trust() instead.
 *
 * Increments both @c total_samples (lifetime count) and @c ticks (monotonic
 * heartbeat counter used by @c heartbeat_ticks()).
 */
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

/**
 * @brief Return the total number of heartbeat ticks since @c heartbeat_init().
 *
 * @c ticks is incremented unconditionally on every @c heartbeat_tick() call,
 * including the first tick where no delta is recorded (because @c last_tsc
 * is still zero). It therefore counts timer interrupts from boot, not valid
 * delta samples.
 *
 * @return Monotonic tick counter; starts at 0, incremented at each heartbeat.
 */
uint64_t heartbeat_ticks(void)
{
    return g_heartbeat.ticks;
}

/**
 * @brief Return the current TIME-TRUST quality metric in Q48.16 format.
 *
 * On AArch64 this always returns @c Q48_ONE (the value 1.0 in Q48.16,
 * i.e., @c 0x0000000000010000) because the ARM Generic Timer is invariant
 * by architecture and no statistical quality degradation is expected.
 *
 * The x86-64 implementation derives trust from the rolling-window variance
 * of inter-tick TSC deviations using @c variance_to_trust().
 *
 * @return TIME-TRUST as Q48.16; always @c Q48_ONE on AArch64.
 */
time_trust_t heartbeat_trust(void)
{
    return g_heartbeat.trust;
}

/**
 * @brief Return a pointer to the heartbeat @c TimeTrustState.
 *
 * Provides read access to the full @c g_heartbeat structure for callers
 * that need to inspect the rolling window contents, variance estimate, or
 * expected delta — for example, the VM bootstrap parity collector
 * (@c sk_parity_collect()) and the @c sk_hal_time_trust() HAL accessor.
 *
 * The pointer is valid for the lifetime of the kernel (module-static storage).
 *
 * @return Pointer to @c g_heartbeat; never NULL.
 */
const TimeTrustState *heartbeat_state(void)
{
    return &g_heartbeat;
}
