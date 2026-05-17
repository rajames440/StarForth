/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James. All rights reserved.
  Licensed under the StarForth License, Version 1.0.
 */

#include <stdint.h>
#include "arch.h"
#include "console.h"

extern void riscv64_install_vectors(void);

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

void arch_interrupts_init(void)
{
    riscv64_install_vectors();
}
