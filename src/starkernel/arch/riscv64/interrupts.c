/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * interrupts.c (riscv64) - Trap vector setup and common exception handler
 */

#include <stdint.h>
#include "arch.h"
#include "console.h"

volatile const char *g_sk_fault_word = (void *)0;

extern void riscv64_install_vectors(void);

/**
 * @brief Print a 64-bit value as "0xNNNNNNNNNNNNNNNN" to the kernel console.
 *
 * Formats @p val as a 16-nibble lowercase hex string preceded by "0x" into
 * a 19-byte stack buffer (including null terminator) and emits it via
 * @c console_puts(). Used by @c riscv64_exception_handler() to print
 * @c scause, @c sepc, and @c stval in the exception diagnostic dump.
 * No heap allocation or libc dependency.
 *
 * @param val 64-bit value to format.
 */
static void print_hex(uint64_t val)
{
    char buf[19];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        int nibble = (int)(val & 0xF);
        buf[2 + i] = (char)(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
        val >>= 4;
    }
    buf[18] = '\0';
    console_puts(buf);
}

/**
 * @brief RISC-V common trap handler — print diagnostic and spin.
 *
 * Called from the @c isr.S trap vector for all synchronous exceptions
 * and asynchronous interrupts that are not handled by a specific driver.
 * Reads three supervisor-mode CSRs directly:
 *
 * - @c scause — Supervisor Cause Register. Bit 63 distinguishes
 *   interrupts (1) from exceptions (0); bits [62:0] encode the cause
 *   code (e.g., 0 = instruction address misaligned, 2 = illegal
 *   instruction, 5 = load access fault, 13 = store page fault,
 *   8 = ecall from U-mode). See RISC-V Privileged Spec §4.1.9.
 * - @c sepc — Supervisor Exception Program Counter. The PC of the
 *   instruction that caused the trap (or the return address for
 *   interrupt handlers); execution resumes from @c sepc on SRET.
 * - @c stval — Supervisor Trap Value. For load/store page faults this
 *   holds the faulting virtual address; for illegal instruction faults
 *   it holds the faulting instruction encoding; zero for other causes.
 *
 * Prints all three values to the kernel console then enters an infinite
 * @c WFI loop to halt the core permanently. All exceptions that reach
 * this handler are fatal in the current single-hart kernel.
 *
 * @c g_sk_fault_word is set by the VM FORTH dispatcher and names the
 * word executing at the time of the trap, aiding post-mortem serial
 * log analysis.
 */
void riscv64_exception_handler(void)
{
    uint64_t scause, sepc, stval;
    __asm__ volatile ("csrr %0, scause" : "=r"(scause));
    __asm__ volatile ("csrr %0, sepc"   : "=r"(sepc));
    __asm__ volatile ("csrr %0, stval"  : "=r"(stval));

    console_println("*** EXCEPTION (riscv64) ***");
    console_puts("  scause = "); print_hex(scause); console_puts("\n");
    console_puts("  sepc   = "); print_hex(sepc);   console_puts("\n");
    console_puts("  stval  = "); print_hex(stval);  console_puts("\n");

    for (;;) {
        __asm__ volatile ("wfi" ::: "memory");
    }
}

/**
 * @brief Install the RISC-V supervisor trap vector table (M4 milestone).
 *
 * Calls @c riscv64_install_vectors() (defined in @c isr.S) which writes
 * the trap vector base address into @c stvec (Supervisor Trap Vector Base
 * Address Register, CSR 0x105). The vector mode field (bits [1:0]) is
 * set to @c 0 (Direct mode) so all traps dispatch to the single base
 * address, or @c 1 (Vectored mode) so async interrupts dispatch to
 * @c stvec + 4*cause.
 *
 * On RISC-V there is no IDT and no legacy PIC; the PLIC replaces both.
 * The @c arch_interrupts_init() name is shared across all three ISAs so
 * that @c kernel_main() calls the same symbol regardless of target.
 *
 * Must be called after the supervisor stack (@c sp) is established and
 * before @c arch_enable_interrupts().
 */
void arch_interrupts_init(void)
{
    riscv64_install_vectors();
}
