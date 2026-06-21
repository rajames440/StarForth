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

/**
 * @brief Perform the earliest AArch64 architecture initialisation (no-op).
 *
 * On x86-64 this function loads the GDT; AArch64 has no equivalent because
 * privilege separation is provided by the exception-level architecture
 * (EL0/EL1/EL2/EL3) rather than segment descriptors. The exception vector
 * table is installed later in @c arch_interrupts_init() by calling
 * @c aarch64_install_vectors() which writes @c VBAR_EL1.
 *
 * This stub exists to satisfy the common @c arch_early_init() call site in
 * @c kernel_main() so that the same milestone sequence compiles across all
 * three supported ISAs (amd64 / aarch64 / riscv64).
 */
void arch_early_init(void)
{
    /* No GDT on AArch64; privilege separation via exception levels.
     * Interrupt vectors installed later in arch_interrupts_init(). */
}

/**
 * @brief Enable IRQ delivery on the current core (AArch64 DAIFCLR).
 *
 * Issues @c MSR DAIFCLR, #2 which clears the @c I bit (IRQ mask) in the
 * @c DAIF (Debug / Abort / IRQ / FIQ) flags of the current exception level.
 * After this instruction the core will take IRQ exceptions from the GIC.
 * The @c memory clobber prevents the compiler from reordering stores across
 * the barrier.
 *
 * Must be called after @c arch_interrupts_init() has installed the vector
 * table in @c VBAR_EL1 and the GIC timer driver has been configured.
 */
void arch_enable_interrupts(void)
{
    __asm__ volatile ("msr daifclr, #2" ::: "memory");
}

/**
 * @brief Disable IRQ delivery on the current core (AArch64 DAIFSET).
 *
 * Issues @c MSR DAIFSET, #2 which sets the @c I bit (IRQ mask) in the
 * @c DAIF flags, masking IRQ exceptions. FIQ, SError, and Debug exceptions
 * are unaffected. Used around critical sections in the single-threaded
 * kernel where interrupt delivery would corrupt shared state.
 *
 * The @c memory clobber acts as a compiler-level barrier; for a full
 * hardware memory barrier use @c DSB before calling this function.
 */
void arch_disable_interrupts(void)
{
    __asm__ volatile ("msr daifset, #2" ::: "memory");
}

/**
 * @brief Halt the core until the next event (AArch64 WFI).
 *
 * Issues the @c WFI (Wait For Interrupt) instruction. The core enters a
 * low-power standby state and resumes execution when an IRQ, FIQ, SError,
 * debug event, or reset is detected. In the kernel panic path this is
 * called inside an infinite loop with interrupts disabled (@c DAIFSET)
 * to stop the processor permanently. In the idle path it reduces power
 * consumption between heartbeat ticks.
 *
 * The @c memory clobber prevents the compiler from sinking stores across
 * the @c WFI.
 */
void arch_halt(void)
{
    __asm__ volatile ("wfi" ::: "memory");
}

/**
 * @brief Read the AArch64 system counter (CNTPCT_EL0).
 *
 * Issues @c MRS @c %0, @c cntpct_el0 to read the 64-bit physical system
 * counter. On AArch64 the system counter is the architectural equivalent
 * of the x86-64 TSC: a monotonically incrementing clock whose frequency
 * is reported by @c CNTFRQ_EL0 and set by firmware at boot.
 *
 * Unlike @c RDTSC on x86-64, @c CNTPCT_EL0 is fully synchronised across
 * cores (no per-core skew), invariant across power-state changes, and
 * does not require serialisation for most measurement use cases. It is
 * used as the time base for @c timer_now_ns() and @c heartbeat_tick().
 *
 * @return Current 64-bit system counter value in counter ticks
 *         (frequency given by @c CNTFRQ_EL0, typically 10–62.5 MHz).
 */
uint64_t arch_read_timestamp(void)
{
    uint64_t val;
    __asm__ volatile ("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

/**
 * @brief Initialise the AArch64 MMU (stub — deferred to a later milestone).
 *
 * Full page-table setup (TTBR0/TTBR1, MAIR, TCR, SCTLR.M) is deferred to a
 * later milestone. This stub exists to satisfy the common @c arch_mmu_init()
 * call site shared across all three ISAs. The VMM subsystem (@c vmm.c)
 * handles page-table management independently of this hook.
 */
void arch_mmu_init(void)
{
    /* Stub: MMU bring-up deferred for later milestone */
}
