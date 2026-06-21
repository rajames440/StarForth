/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
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

/**
 * @brief Read the RISC-V CPU cycle counter (@c rdcycle).
 *
 * Issues the @c RDCYCLE pseudo-instruction (a @c CSRRS on @c cycle,
 * CSR 0xC00) to read the 64-bit hardware cycle counter. On RISC-V the
 * cycle counter is a per-hart monotonically incrementing register whose
 * frequency equals the hart's clock rate — the architectural equivalent
 * of the x86-64 TSC.
 *
 * Unlike @c CNTPCT_EL0 on AArch64, @c cycle is not architecturally
 * synchronised across harts; this is acceptable for the single-hart
 * LithosAnanke build. The frequency is not provided by hardware CSR
 * (unlike AArch64's @c CNTFRQ_EL0); @c timer_init() assumes 1 GHz
 * for QEMU @c virt-machine compatibility.
 *
 * @return Current 64-bit cycle count; wraps at UINT64_MAX (about 584 years
 *         at 1 GHz — not a practical concern).
 */
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

/**
 * @brief Initialise the RISC-V timer subsystem (M5 milestone).
 *
 * On RISC-V there is no architectural CSR that directly reports the
 * @c rdcycle frequency (unlike AArch64's @c CNTFRQ_EL0). The timer
 * subsystem therefore assumes 1,000,000,000 Hz (1 GHz), which matches
 * QEMU's @c virt machine default clock. Real hardware board support would
 * need to read the frequency from a device tree or firmware table and
 * update @c s_counter_hz before computing @c s_ns_per_tick.
 *
 * Steps performed:
 * 1. Snapshots @c rdcycle() into @c s_base_count as the ns origin.
 * 2. Computes @c s_ns_per_tick as @c (1e9 << 16) / @c s_counter_hz in
 *    Q16.16 fixed-point to avoid floating-point in the freestanding build.
 * 3. Fills @c s_cal with the assumed frequency and sets
 *    @c TIMER_TRUST_ABSOLUTE (the @c rdcycle counter is invariant by
 *    specification once enabled, though its frequency is merely assumed
 *    rather than measured).
 *
 * @param boot_info Kernel @c BootInfo (device-tree / ACPI pointer); unused
 *                  at this milestone — clock frequency is hard-coded.
 * @return 0 always.
 */
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

/**
 * @brief Return the assumed cycle-counter frequency in Hz.
 *
 * Returns @c s_counter_hz (initialised to 1,000,000,000 by the module).
 * Used by @c apic_timer_init() to compute the expected inter-tick period
 * and by the shim time backend to convert cycle counts to nanoseconds.
 *
 * @return Cycle-counter frequency in Hz; 1,000,000,000 (assumed 1 GHz).
 */
uint64_t timer_tsc_hz(void)
{
    return s_counter_hz;
}

/**
 * @brief Return a monotonic nanosecond timestamp.
 *
 * Reads @c rdcycle(), computes the elapsed delta from @c s_base_count
 * (captured at @c timer_init()), and converts to nanoseconds using the
 * Q16.16 fixed-point @c s_ns_per_tick:
 *
 * @code
 *   ns = s_base_ns + ((delta * s_ns_per_tick) >> 16)
 * @endcode
 *
 * At 1 GHz the multiplication can represent up to ~18 seconds before
 * the 64-bit product of @c delta × @c s_ns_per_tick saturates. For
 * longer intervals the counter wraps and @c timer_now_ns() will produce
 * incorrect values — acceptable for the current single-hart POST-only
 * kernel which runs for at most a few minutes.
 *
 * @return Monotonic nanosecond count since @c timer_init(), starting at 0.
 */
uint64_t timer_now_ns(void)
{
    uint64_t delta = rdcycle() - s_base_count;
    return s_base_ns + ((delta * s_ns_per_tick) >> 16);
}

/**
 * @brief Check for cycle-counter drift (RISC-V stub — always returns 0).
 *
 * On x86-64 this cross-checks the TSC against the HPET. The RISC-V
 * @c rdcycle counter has no independent reference to check against at
 * this milestone. Always returns 0 (no drift) to satisfy the common
 * call site.
 *
 * @return 0 always.
 */
int timer_check_drift_now(void) { return 0; }

/**
 * @brief Return a pointer to the timer calibration record.
 *
 * Returns @c &s_cal populated by @c timer_init() with the assumed 1 GHz
 * frequency and @c TIMER_TRUST_ABSOLUTE. Consumed by @c kernel_main()
 * for boot-log reporting and by the heartbeat subsystem's trust init.
 *
 * @return Pointer to the module-static calibration record; valid for the
 *         lifetime of the kernel.
 */
const timer_calibration_record_t *timer_calibration_record(void)
{
    return &s_cal;
}

/**
 * @brief Initialise the heartbeat rolling-window state.
 *
 * Zeroes @c g_heartbeat and sets the expected inter-tick interval as
 * @c tsc_hz / @c tick_hz cycle-counter ticks. Falls back to 10,000,000
 * cycles if either argument is zero. Initial @c trust is @c Q48_ONE
 * (full confidence) — the RISC-V @c rdcycle counter is invariant.
 *
 * @param tsc_hz  Cycle-counter frequency (Hz); from @c timer_tsc_hz().
 * @param tick_hz Heartbeat rate (Hz); from @c apic_timer_init().
 */
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

/**
 * @brief Record one heartbeat tick and update the inter-tick deviation window.
 *
 * Reads @c rdcycle() and, if @c last_tsc is non-zero, records the signed
 * deviation @c ((now - last_tsc) - expected_delta) into the circular
 * @c window.deltas[] buffer. Increments @c ticks and @c total_samples.
 * Sets @c trust = @c Q48_ONE unconditionally — the RISC-V cycle counter
 * is invariant and needs no statistical quality estimate.
 *
 * Called from the RISC-V timer ISR stub (or its no-op placeholder) at
 * each periodic heartbeat period.
 */
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

/**
 * @brief Return the total number of heartbeat ticks since @c heartbeat_init().
 *
 * @return Monotonic tick counter; incremented once per @c heartbeat_tick() call.
 */
uint64_t heartbeat_ticks(void) { return g_heartbeat.ticks; }

/**
 * @brief Return the current TIME-TRUST quality metric in Q48.16 format.
 *
 * Always returns @c Q48_ONE on RISC-V because the @c rdcycle counter is
 * invariant by specification and no statistical quality degradation is
 * expected. The x86-64 implementation derives trust from rolling-window
 * variance.
 *
 * @return TIME-TRUST as Q48.16; always @c Q48_ONE on RISC-V.
 */
time_trust_t heartbeat_trust(void) { return g_heartbeat.trust; }

/**
 * @brief Return a pointer to the heartbeat @c TimeTrustState.
 *
 * Provides read access to the full @c g_heartbeat structure for
 * @c sk_parity_collect(), @c sk_hal_time_trust(), and other consumers.
 * The pointer is valid for the lifetime of the kernel.
 *
 * @return Pointer to @c g_heartbeat; never NULL.
 */
const TimeTrustState *heartbeat_state(void) { return &g_heartbeat; }
