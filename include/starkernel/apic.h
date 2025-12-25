/**
 * apic.h - Local APIC interface
 */

#ifndef STARKERNEL_APIC_H
#define STARKERNEL_APIC_H

#include "uefi.h"
#include <stdint.h>

/* Heartbeat timer vector (user-defined IRQ space starts at 0x20) */
#define APIC_TIMER_VECTOR  0x20

/* Initialize Local APIC (enables APIC, sets spurious vector) */
int apic_init(BootInfo *boot_info);

/* Send End-of-Interrupt signal */
void apic_eoi(void);

/**
 * Initialize APIC timer for periodic heartbeat.
 * @param tsc_hz     TSC frequency in Hz (for calibration)
 * @param tick_hz    Desired tick frequency (e.g., 100 = 100 Hz = 10ms period)
 * @return 0 on success, -1 on failure
 */
int apic_timer_init(uint64_t tsc_hz, uint32_t tick_hz);

/**
 * Start the APIC timer (enables periodic interrupts).
 * Call this after IDT and heartbeat handler are set up.
 */
void apic_timer_start(void);

/**
 * Stop the APIC timer (disables periodic interrupts).
 */
void apic_timer_stop(void);

/**
 * Get the configured timer period in TSC ticks.
 */
uint64_t apic_timer_period_tsc(void);

#endif /* STARKERNEL_APIC_H */
