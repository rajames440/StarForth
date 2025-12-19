/**
 * arch.c (riscv64) - Architecture abstractions for StarKernel on RISC-V
 */

#include "arch.h"

void arch_early_init(void)
{
    /* Placeholder for trap vector and CSR initialization */
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
    __asm__ volatile ("wfi");
}

uint64_t arch_read_timestamp(void)
{
    uint64_t time;
    __asm__ volatile ("rdtime %0" : "=r"(time));
    return time;
}

void arch_mmu_init(void)
{
    /* Stub: SV39/48 page table setup will be added later */
}
