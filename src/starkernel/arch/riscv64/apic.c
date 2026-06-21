/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * apic.c (riscv64) - APIC stub. No Local APIC on RISC-V; interrupt
 * controller is PLIC (Platform-Level Interrupt Controller).  These stubs
 * satisfy the common kernel interface; PLIC wiring is a later milestone.
 */

#include "apic.h"
#include "uefi.h"
#include <stdint.h>

static uint64_t s_timer_period_tsc = 0;

/**
 * @brief Initialise the interrupt controller (RISC-V PLIC stub).
 *
 * On x86-64 this function configures the Local APIC. On RISC-V the
 * Platform-Level Interrupt Controller (PLIC) handles external interrupt
 * routing; PLIC bring-up is deferred to a later milestone. This stub
 * satisfies the common @c apic_init() call site in @c kernel_main()
 * and always returns success.
 *
 * @param boot_info Kernel boot information (ACPI/PLIC base address); unused.
 * @return 0 always.
 */
int apic_init(BootInfo *boot_info)
{
    (void)boot_info;
    return 0;
}

/**
 * @brief Signal End of Interrupt to the interrupt controller (RISC-V stub).
 *
 * On x86-64 this writes to the Local APIC EOI register. On RISC-V EOI
 * is handled by reading the PLIC claim/complete register; that path is
 * not yet wired. This stub is a no-op that satisfies the common interface.
 */
void apic_eoi(void) { }

/**
 * @brief Configure the periodic timer (RISC-V stub).
 *
 * On x86-64 this calibrates and programs the APIC timer. On RISC-V the
 * SBI (Supervisor Binary Interface) @c sbi_set_timer() call or the CLINT
 * (Core Local Interrupt) would be used to schedule timer interrupts; that
 * driver is deferred. This stub computes @c s_timer_period_tsc — the
 * expected number of @c rdcycle ticks between heartbeats — which seeds
 * @c heartbeat_init()'s @c expected_delta.
 *
 * Falls back to 10,000,000 cycles if either frequency is zero (a safe
 * non-zero sentinel preventing division-by-zero in the heartbeat path).
 *
 * @param tsc_hz  Cycle-counter frequency from @c timer_init() (Hz).
 * @param tick_hz Desired heartbeat rate (Hz); 0 → uses fallback.
 * @return 0 always.
 */
int apic_timer_init(uint64_t tsc_hz, uint32_t tick_hz)
{
    if (tick_hz > 0 && tsc_hz > 0)
        s_timer_period_tsc = tsc_hz / tick_hz;
    else
        s_timer_period_tsc = 10000000;
    return 0;
}

/**
 * @brief Start periodic timer delivery (RISC-V stub).
 *
 * On x86-64 this unmasks the APIC timer. On RISC-V a periodic timer would
 * be armed via CLINT or SBI here; the driver is deferred. No-op stub.
 */
void apic_timer_start(void) { }

/**
 * @brief Stop periodic timer delivery (RISC-V stub).
 *
 * On x86-64 this masks the APIC timer. On RISC-V a periodic timer would
 * be disarmed via CLINT or SBI here; the driver is deferred. No-op stub.
 */
void apic_timer_stop(void)  { }

/**
 * @brief Return the expected cycle-counter ticks per heartbeat period.
 *
 * Returns @c s_timer_period_tsc, set by @c apic_timer_init() as
 * @c tsc_hz / @c tick_hz. This value is consumed by @c heartbeat_init()
 * to seed the rolling-window @c expected_delta so that inter-tick
 * deviation measurements are relative to the correct nominal interval.
 *
 * @return Expected @c rdcycle ticks per heartbeat period; 10,000,000 if
 *         frequencies were not available at init time.
 */
uint64_t apic_timer_period_tsc(void)
{
    return s_timer_period_tsc;
}
