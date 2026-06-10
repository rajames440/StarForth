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

/* Common exception handler — called from isr.S for all exception types */
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

void arch_interrupts_init(void)
{
    aarch64_install_vectors();
}
