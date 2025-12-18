/**
 * arch.c (aarch64) - Architecture abstractions for StarKernel on ARM64
 */

#include "arch.h"

void arch_early_init(void)
{
    /* Placeholder for exception level transitions and vector setup */
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
    __asm__ volatile ("wfi");
}

uint64_t arch_read_timestamp(void)
{
    uint64_t cnt;
    __asm__ volatile ("mrs %0, cntpct_el0" : "=r"(cnt));
    return cnt;
}

void arch_mmu_init(void)
{
    /* Stub: page table setup will land in later milestones */
}
