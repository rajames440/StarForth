/**
 * arch.h - Architecture abstraction layer for StarKernel
 *
 * Provides a minimal interface used across early boot, interrupt control,
 * low-power halting, timestamp reads, and MMU bring-up.
 */

#ifndef STARKERNEL_ARCH_H
#define STARKERNEL_ARCH_H

#include <stdint.h>

/* Early CPU setup prior to enabling higher-level subsystems */
void arch_early_init(void);

/* Interrupt control */
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);
void arch_interrupts_init(void);

/* Halt/idle CPU until the next interrupt */
void arch_halt(void);

/* Low-overhead timestamp counter (architecture-specific source) */
uint64_t arch_read_timestamp(void);

/* MMU initialization hook (platform-specific implementation) */
void arch_mmu_init(void);

/* Architecture-friendly pause/yield hint inside busy loops */
static inline void arch_relax(void)
{
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("pause");
#elif defined(__aarch64__)
    __asm__ volatile ("yield");
#elif defined(__riscv)
    __asm__ volatile ("nop");
#else
    __asm__ volatile ("" ::: "memory");
#endif
}

#endif /* STARKERNEL_ARCH_H */
