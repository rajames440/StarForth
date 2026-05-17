/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/*
 * apic.c (riscv64) - APIC stub. No Local APIC on RISC-V; interrupt
 * controller is PLIC (Platform-Level Interrupt Controller).  These stubs
 * satisfy the common kernel interface; PLIC wiring is a later milestone.
 */

#include "apic.h"
#include "uefi.h"
#include <stdint.h>

static uint64_t s_timer_period_tsc = 0;

int apic_init(BootInfo *boot_info)
{
    (void)boot_info;
    return 0;
}

void apic_eoi(void) { }

int apic_timer_init(uint64_t tsc_hz, uint32_t tick_hz)
{
    if (tick_hz > 0 && tsc_hz > 0)
        s_timer_period_tsc = tsc_hz / tick_hz;
    else
        s_timer_period_tsc = 10000000;
    return 0;
}

void apic_timer_start(void) { }
void apic_timer_stop(void)  { }

uint64_t apic_timer_period_tsc(void)
{
    return s_timer_period_tsc;
}
