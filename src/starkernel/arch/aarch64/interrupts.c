/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

/**
 * interrupts.c (aarch64) - Exception vector setup and common handler
 */

#include <stdint.h>
#include "arch.h"
#include "console.h"

volatile const char *g_sk_fault_word = (void *)0;

/* Defined in isr.S */
extern void aarch64_install_vectors(void);

/**
 * @brief Print a 64-bit value as "0xNNNNNNNNNNNNNNNN" to the kernel console.
 *
 * Formats @p val as a 16-nibble lowercase hex string preceded by "0x" into
 * a 19-byte stack buffer (including null terminator) then emits it via
 * @c console_puts(). Used by @c aarch64_exception_handler() to print
 * @c ESR_EL1, @c ELR_EL1, @c FAR_EL1, and @c SPSR_EL1 in the exception
 * diagnostic dump. No heap allocation or libc dependency.
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
 * @brief AArch64 common exception handler — print diagnostic and spin.
 *
 * Called from the @c isr.S exception vector table stubs for all exception
 * types (synchronous, IRQ, FIQ, SError) that are not handled by a specific
 * driver. Reads the four key EL1 exception system registers directly:
 *
 * - @c ESR_EL1 — Exception Syndrome Register: encodes the exception class
 *   (EC, bits [31:26]) and instruction-specific syndrome (ISS, bits [24:0]).
 *   The EC identifies the cause (e.g., 0x21 = data abort from EL1, 0x25 =
 *   data abort from EL0, 0x15 = SVC64).
 * - @c ELR_EL1 — Exception Link Register: the PC of the instruction that
 *   caused the exception (or the instruction after, for IRQ/FIQ/SError).
 * - @c FAR_EL1 — Fault Address Register: the virtual address that triggered
 *   a Data Abort or Instruction Abort; undefined for other exception types.
 * - @c SPSR_EL1 — Saved Program Status Register: the PSTATE at the time of
 *   exception, including the mode field and interrupt-mask bits.
 *
 * Prints all four values to the kernel console then enters an infinite
 * @c WFE loop to halt the affected core permanently. All exceptions that
 * reach this handler are fatal — there is no recovery path.
 *
 * The global @c g_sk_fault_word is set by the VM FORTH dispatcher just
 * before calling each word's function pointer; it names the word executing
 * at the time of the fault, aiding post-mortem analysis from the serial log.
 * (Currently logged only on the amd64 #GP path; future AArch64 diagnostic
 * expansion can include it here.)
 */
void aarch64_exception_handler(void)
{
    uint64_t esr, elr, far_val, spsr;
    __asm__ volatile ("mrs %0, esr_el1"  : "=r"(esr));
    __asm__ volatile ("mrs %0, elr_el1"  : "=r"(elr));
    __asm__ volatile ("mrs %0, far_el1"  : "=r"(far_val));
    __asm__ volatile ("mrs %0, spsr_el1" : "=r"(spsr));

    console_println("*** EXCEPTION (aarch64) ***");
    console_puts("  ESR_EL1 = "); print_hex(esr);   console_puts("\n");
    console_puts("  ELR_EL1 = "); print_hex(elr);   console_puts("\n");
    console_puts("  FAR_EL1 = "); print_hex(far_val); console_puts("\n");
    console_puts("  SPSR_EL1= "); print_hex(spsr);  console_puts("\n");

    for (;;) {
        __asm__ volatile ("wfe" ::: "memory");
    }
}

/**
 * @brief Install the AArch64 exception vector table (M4 milestone).
 *
 * Calls @c aarch64_install_vectors() (defined in @c isr.S) which writes
 * the address of the vector table base into @c VBAR_EL1 (Vector Base
 * Address Register, EL1). After this instruction all EL1 exceptions
 * (synchronous, IRQ, FIQ, SError) dispatch through the 512-byte-aligned
 * vector table assembled in @c isr.S.
 *
 * On AArch64 there is no IDT and no PIC to disable; the GIC replaces
 * both. The @c arch_interrupts_init() name is kept identical across all
 * ISAs so that @c kernel_main() can call the same symbol regardless of
 * the target architecture.
 *
 * Must be called after the kernel stack is established (so that the EL1
 * SP_EL1 is valid) and before @c arch_enable_interrupts().
 */
void arch_interrupts_init(void)
{
    aarch64_install_vectors();
}
