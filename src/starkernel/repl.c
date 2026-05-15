/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * repl.c - Emergency FORTH REPL for LithosAnanke kernel
 *
 * Direct adaptation of src/repl.c for the freestanding kernel context.
 * Replaces libc stdio (fgets/printf/fflush) with HAL serial I/O:
 *   - Input:  console_getc() busy-wait with local echo and backspace support
 *   - Output: console_puts() / console_putc()
 *
 * Runs with interrupts enabled so the APIC heartbeat fires normally.
 * Designed as the last thing kernel_main does before the idle loop.
 */

#include "starkernel/repl.h"
#include "console.h"
#include "vm.h"
#include "version.h"

#define SK_KERNEL_VERSION "1.0.0"

/*===========================================================================
 * sk_readline - blocking line read from serial console with echo
 *
 * Spins on console_getc() until a full line arrives.
 * Supports backspace (0x7F and \b) and ignores other control characters.
 * Returns the number of characters placed in buf (not counting '\0').
 *===========================================================================*/

static int sk_readline(char *buf, int size)
{
    int n = 0;

    for (;;) {
        int c;
        do { c = console_getc(); } while (c < 0);   /* spin until byte arrives */

        if (c == '\r' || c == '\n') {
            console_putc('\n');
            break;
        }

        /* backspace: DEL (0x7F) or BS (0x08) */
        if ((c == 0x7F || c == '\b') && n > 0) {
            n--;
            /* VT100 erase: move back, overwrite with space, move back again */
            console_putc('\b');
            console_putc(' ');
            console_putc('\b');
            continue;
        }

        if (c < 0x20) continue;       /* ignore other control characters */
        if (n >= size - 1) continue;  /* buffer full — drop character */

        buf[n++] = (char)c;
        console_putc((char)c);        /* echo */
    }

    buf[n] = '\0';
    return n;
}

/*===========================================================================
 * sk_repl - Emergency FORTH REPL
 *
 * Mirrors vm_repl() from src/repl.c:
 *   - Prints "ok> " prompt
 *   - Reads a line via sk_readline
 *   - Calls vm_interpret
 *   - Prints " ok" or " ERROR" depending on vm->error
 *   - Resets vm->error and loops
 *===========================================================================*/

void sk_repl(VM *vm)
{
    char input[256];

    console_puts("LithosAnanke Version "); console_println(SK_KERNEL_VERSION);
    console_puts("StarForth Version ");    console_println(STARFORTH_VERSION);
    console_println("");
    console_println("StarForth Emergency CLI");
    console_println("FORTH-79 interpreter — type BYE or power off to exit");
    console_println("");

    while (!vm->halted) {
        console_puts("ok> ");

        sk_readline(input, sizeof(input));

        if (input[0] == '\0') {
            console_puts(" ok\n");
            continue;
        }

        vm_interpret(vm, input);

        if (vm->error) {
            console_puts(" ERROR\n");
            vm->error = 0;
        } else {
            console_puts(" ok\n");
        }
    }
}
