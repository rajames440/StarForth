/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * arch.c (riscv64) - Architecture abstractions for StarKernel on RISC-V 64-bit
 */

#include "arch.h"
#include <stdint.h>

extern void riscv64_install_vectors(void);

/**
 * @brief Perform the earliest RISC-V architecture initialisation (no-op).
 *
 * RISC-V has no GDT or IDT; privilege separation is handled by the
 * machine/supervisor/user execution modes (M/S/U), and the trap vector
 * table is installed by writing @c stvec in @c arch_interrupts_init()
 * via @c riscv64_install_vectors(). This stub satisfies the common
 * @c arch_early_init() call site in @c kernel_main() without any
 * RISC-V-specific action.
 */
void arch_early_init(void)
{
    /* No GDT/IDT on RISC-V; privilege via exception levels */
}

/**
 * @brief Enable supervisor-mode interrupts (RISC-V CSRSI sstatus).
 *
 * Issues @c CSRSI @c sstatus, @c 0x2 which sets bit 1 (@c SIE —
 * Supervisor Interrupt Enable) in the @c sstatus CSR. While @c SIE = 1,
 * pending supervisor-mode interrupts are taken. The @c memory clobber
 * prevents the compiler from reordering stores across the barrier.
 *
 * Must be called after @c arch_interrupts_init() has installed the
 * @c stvec trap handler, and after the PLIC driver has been configured
 * to forward the desired interrupt sources.
 */
void arch_enable_interrupts(void)
{
    __asm__ volatile ("csrsi sstatus, 0x2" ::: "memory");
}

/**
 * @brief Disable supervisor-mode interrupts (RISC-V CSRCI sstatus).
 *
 * Issues @c CSRCI @c sstatus, @c 0x2 which clears bit 1 (@c SIE) in
 * @c sstatus, masking all supervisor-mode interrupts. Machine-mode
 * interrupts are unaffected. Used around critical sections in the
 * single-threaded kernel. The @c memory clobber acts as a compiler
 * barrier; pair with a @c FENCE instruction for a hardware barrier.
 */
void arch_disable_interrupts(void)
{
    __asm__ volatile ("csrci sstatus, 0x2" ::: "memory");
}

/**
 * @brief Halt the core until the next interrupt (RISC-V WFI).
 *
 * Issues the @c WFI (Wait For Interrupt) instruction in supervisor mode.
 * The core enters a low-power state and resumes when a pending interrupt
 * is detected (or after a platform-specific timeout). In the kernel panic
 * path this is called inside an infinite loop with @c SIE cleared to stop
 * the processor permanently. In the idle path it reduces power consumption
 * between heartbeat ticks.
 *
 * The @c memory clobber prevents store-sinking across the @c WFI.
 */
void arch_halt(void)
{
    __asm__ volatile ("wfi" ::: "memory");
}

/**
 * @brief Read the RISC-V CPU cycle counter (@c rdcycle).
 *
 * Issues the @c RDCYCLE pseudo-instruction (a @c CSRRS on @c cycle, CSR
 * 0xC00) to read the 64-bit hardware performance counter that counts
 * elapsed clock cycles since reset. On RISC-V the cycle counter is the
 * architectural equivalent of the x86-64 TSC.
 *
 * Unlike the AArch64 @c CNTPCT_EL0, the RISC-V @c cycle register is a
 * per-hart counter and is not architecturally guaranteed to be synchronised
 * across harts. For single-core LithosAnanke this is not a concern.
 * Frequency is not architecturally specified; @c timer_init() assumes
 * 1 GHz for QEMU @c virt board compatibility.
 *
 * @return Current 64-bit cycle counter value; frequency platform-dependent.
 */
uint64_t arch_read_timestamp(void)
{
    uint64_t val;
    __asm__ volatile ("rdcycle %0" : "=r"(val));
    return val;
}

/**
 * @brief Initialise the RISC-V MMU (stub — deferred to a later milestone).
 *
 * Full Sv39/Sv48 page-table setup (SATP, PMP, etc.) is deferred to a
 * later milestone. This stub satisfies the common @c arch_mmu_init()
 * call site shared across all three supported ISAs. The VMM subsystem
 * (@c vmm.c) handles page-table management independently of this hook.
 */
void arch_mmu_init(void)
{
    /* Stub: Sv39/Sv48 bring-up deferred */
}
