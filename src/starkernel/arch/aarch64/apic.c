/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * apic.c (aarch64) - APIC stub for AArch64.
 *
 * The Local APIC is x86-specific.  On AArch64 the interrupt controller is the
 * GIC (Generic Interrupt Controller).  These stubs satisfy the common kernel
 * interface without performing any real initialisation; the GIC driver will
 * be wired up in a later milestone.
 */

#include "apic.h"
#include "uefi.h"
#include <stdint.h>

static uint64_t s_timer_period_tsc = 0;

/**
 * @brief Initialise the interrupt controller (AArch64 GIC stub).
 *
 * On x86-64 this function configures the Local APIC MMIO registers.
 * On AArch64 the equivalent peripheral is the ARM Generic Interrupt
 * Controller (GIC); GIC bring-up is deferred to a later milestone.
 * This stub satisfies the common @c apic_init() call site in
 * @c kernel_main() and always returns success.
 *
 * @param boot_info Kernel boot information (ACPI/GIC base address); unused.
 * @return 0 always.
 */
int apic_init(BootInfo *boot_info)
{
    (void)boot_info;
    return 0;
}

/**
 * @brief Signal End of Interrupt to the interrupt controller (AArch64 stub).
 *
 * On x86-64 this writes to the Local APIC EOI register. On AArch64 the
 * EOI is written to the GIC CPU Interface register (GICC_EOIR); that
 * path is not yet wired. This stub is called from the AArch64 heartbeat
 * path to satisfy the common interface; it is a no-op until the GIC
 * driver is implemented.
 */
void apic_eoi(void)
{
    /* GIC EOI goes here; stub for now */
}

/**
 * @brief Configure the periodic timer (AArch64 stub).
 *
 * On x86-64 this calibrates and programs the APIC timer. On AArch64 the
 * equivalent is the ARM Generic Timer's EL1 Physical Timer (CNTP_CTL_EL0 /
 * CNTP_TVAL_EL0); that driver is deferred. This stub computes
 * @c s_timer_period_tsc (the expected number of @c CNTPCT_EL0 ticks
 * between heartbeats) which is used by @c heartbeat_init() to seed the
 * rolling-window expected-delta value.
 *
 * If either @p tsc_hz or @p tick_hz is zero, a fallback of 10,000,000
 * ticks is stored (approximately 160 ms at 62.5 MHz, a safe non-zero
 * sentinel that prevents division-by-zero in the heartbeat path).
 *
 * @param tsc_hz  System counter frequency from @c timer_init() (Hz).
 * @param tick_hz Desired heartbeat rate (Hz); 0 → uses fallback.
 * @return 0 always.
 */
int apic_timer_init(uint64_t tsc_hz, uint32_t tick_hz)
{
    if (tick_hz > 0 && tsc_hz > 0) {
        s_timer_period_tsc = tsc_hz / tick_hz;
    } else {
        s_timer_period_tsc = 10000000;
    }
    return 0;
}

/**
 * @brief Start periodic timer delivery (AArch64 stub).
 *
 * On x86-64 this unmasks the APIC timer LVT entry and re-arms the
 * initial count register. On AArch64 the ARM Generic Timer's EL1
 * Physical Timer would be enabled here; the driver is deferred.
 * This stub is a no-op that satisfies the common call site in
 * @c kernel_main() without fault.
 */
void apic_timer_start(void)
{
    /* ARM generic timer EL1 physical timer — stub */
}

/**
 * @brief Stop periodic timer delivery (AArch64 stub).
 *
 * On x86-64 this sets the APIC timer LVT mask bit. On AArch64 the
 * ARM Generic Timer would be disabled here. Stub — no-op until the
 * GIC / Generic Timer driver is implemented.
 */
void apic_timer_stop(void)
{
    /* Stub */
}

/**
 * @brief Return the expected system-counter ticks per heartbeat period.
 *
 * Returns @c s_timer_period_tsc, set by @c apic_timer_init() as
 * @c tsc_hz / @c tick_hz. This value is consumed by @c heartbeat_init()
 * to seed the rolling-window @c expected_delta so that inter-tick
 * deviation measurements are relative to the correct nominal interval.
 *
 * @return Expected @c CNTPCT_EL0 ticks per heartbeat period; 10,000,000
 *         if frequencies were not available at init time.
 */
uint64_t apic_timer_period_tsc(void)
{
    return s_timer_period_tsc;
}
