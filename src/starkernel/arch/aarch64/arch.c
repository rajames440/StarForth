/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * arch.c (aarch64) - Architecture abstractions for StarKernel on AArch64
 */

#include "arch.h"
#include <stdint.h>

/* Installed by isr.S — installs VBAR_EL1 */
extern void aarch64_install_vectors(void);

void arch_early_init(void)
{
    /* No GDT on AArch64; privilege separation via exception levels.
     * Interrupt vectors installed later in arch_interrupts_init(). */
}

void arch_enable_interrupts(void)
{
    __asm__ volatile ("msr daifclr, #2" ::: "memory");
}

void arch_disable_interrupts(void)
{
    __asm__ volatile ("msr daifset, #2" ::: "memory");
}

void arch_halt(void)
{
    __asm__ volatile ("wfi" ::: "memory");
}

uint64_t arch_read_timestamp(void)
{
    uint64_t val;
    __asm__ volatile ("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

void arch_mmu_init(void)
{
    /* Stub: MMU bring-up deferred for later milestone */
}
