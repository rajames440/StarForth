/**
 * arch.c (amd64) - Architecture abstractions for StarKernel on x86_64
 */

#include "arch.h"

void arch_early_init(void)
{
    /* Placeholder for GDT/IDT setup once implemented */
}

void arch_enable_interrupts(void)
{
    __asm__ volatile ("sti");
}

void arch_disable_interrupts(void)
{
    __asm__ volatile ("cli");
}

void arch_halt(void)
{
    __asm__ volatile ("hlt");
}

uint64_t arch_read_timestamp(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void arch_mmu_init(void)
{
    /* Stub: paging will be wired up in later milestones */
}
