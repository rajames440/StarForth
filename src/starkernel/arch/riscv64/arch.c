/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

#include "arch.h"
#include <stdint.h>

extern void riscv64_install_vectors(void);

void arch_early_init(void)
{
    /* No GDT/IDT on RISC-V; privilege via exception levels */
}

void arch_enable_interrupts(void)
{
    __asm__ volatile ("csrsi sstatus, 0x2" ::: "memory");
}

void arch_disable_interrupts(void)
{
    __asm__ volatile ("csrci sstatus, 0x2" ::: "memory");
}

void arch_halt(void)
{
    __asm__ volatile ("wfi" ::: "memory");
}

uint64_t arch_read_timestamp(void)
{
    uint64_t val;
    __asm__ volatile ("rdcycle %0" : "=r"(val));
    return val;
}

void arch_mmu_init(void)
{
    /* Stub: Sv39/Sv48 bring-up deferred */
}
