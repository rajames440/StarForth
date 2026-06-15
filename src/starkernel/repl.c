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
 *   - Input:  console_getc() non-blocking poll with local echo and backspace
 *   - Output: console_puts() / console_putc()
 *
 * Idle spin:  polls console_getc() and services the adaptive heartbeat.
 *             The APIC timer ISR increments heartbeat_ticks() at 100 Hz;
 *             the idle spin calls sk_repl_idle() once per SK_IDLE_BEAT_INTERVAL
 *             ticks.  On QEMU TCG the ISR must fire for ticks to advance —
 *             check "Heartbeat: N ticks" in the serial log to confirm.
 *
 * Runs with interrupts enabled so the APIC heartbeat fires normally.
 * Designed as the last thing kernel_main does before the idle loop.
 */

#include "starkernel/repl.h"
#include "console.h"
#include "vm.h"
#include "version.h"
#include "starkernel/timer.h"
#include "starkernel/arch.h"
#include <stdint.h>

const char lithos_version[64] = LITHOS_VERSION_STR;

/*===========================================================================
 * USE-word dispatch: which VM receives REPL input.
 *
 * NULL means "use the REPL's own vm parameter" (default — Mama).
 * Set via sk_repl_set_active_vm(); read by sk_repl_run() each iteration.
 *===========================================================================*/

static VM *g_repl_active_vm = (void *)0;

void sk_repl_set_active_vm(VM *vm) { g_repl_active_vm = vm; }
VM  *sk_repl_get_active_vm(void)   { return g_repl_active_vm; }

/*===========================================================================
 * Idle heartbeat service
 *
 * Called from sk_readline when heartbeat_ticks() has advanced by at least
 * SK_IDLE_BEAT_INTERVAL since the last service call.  Extend this function
 * as higher-level subsystems (msg_fabric, capsule scheduler) come online.
 *
 * TODO: cadence policy and subsystem dispatch belong in Compudynamics once
 * that layer governs cooperative VM execution.
 *===========================================================================*/

#define SK_IDLE_BEAT_INTERVAL  100u   /* ticks between idle service calls (1 s at 100 Hz) */

static uint64_t g_last_beat_tick;    /* zero-initialized (BSS) */

static void sk_repl_idle(void)
{
    /* Placeholder — extended by higher-level subsystems as they come online */
    (void)0;
}

/*===========================================================================
 * sk_readline - line read from serial console with echo
 *
 * Non-blocking poll of console_getc().  While no character is ready the idle
 * spin services the adaptive heartbeat at SK_IDLE_BEAT_INTERVAL tick cadence.
 * Supports backspace (0x7F and \b) and ignores other control characters.
 * Returns the number of characters placed in buf (not counting '\0').
 *===========================================================================*/

static int sk_readline(char *buf, int size)
{
    int n = 0;

    for (;;) {
        int c = console_getc();   /* non-blocking poll */

        if (c < 0) {
            /* Idle — service the adaptive heartbeat if a beat has elapsed */
            uint64_t now = heartbeat_ticks();
            if (now - g_last_beat_tick >= SK_IDLE_BEAT_INTERVAL) {
                g_last_beat_tick = now;
                sk_repl_idle();
            }
            /*
             * Do NOT use hlt here: QEMU single-threaded TCG can't process
             * its APIC timer callbacks while the guest CPU is halted (the
             * event loop and the TCG thread share the same OS thread).
             * Interrupts are delivered at TB boundaries in a tight loop.
             * On real hardware a wfi/hlt would be appropriate; add it here
             * under an #ifdef REAL_HARDWARE guard when that path is needed.
             */
            arch_relax();   /* PAUSE — reduce power, maintain tight poll */
            continue;
        }

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
 *   - Sets vm->emergency_console = 1 for the duration (this IS the emergency
 *     console; bypasses ACL so zuse authentication is not required to recover)
 *   - Prints "zuse)ok> " when zuse_session=1, else "ok> "
 *   - Reads a line via sk_readline (non-blocking, heartbeat-serviced)
 *   - Calls vm_interpret
 *   - Prints " ok" or " ERROR"
 *   - When EMERGENCY_CONSOLE_ENABLED=1: resets vm->error and loops (recovery)
 *   - When EMERGENCY_CONSOLE_ENABLED=0: halts VM on error (no fallthrough surface)
 *===========================================================================*/

#if !EMERGENCY_CONSOLE_ENABLED
static void sk_fault_handler(VM *vm) {
    console_println("VM fault — emergency console disabled; halting");
    vm->halted = 1;
}
#endif

void sk_repl_run(VM *vm)
{
    char input[256];
    VM  *active;

    vm->halted = 0;

    while (!vm->halted) {
        /* USE may redirect input to a different VM each iteration */
        active = g_repl_active_vm ? g_repl_active_vm : vm;

        /* emergency_console bypasses ACL — only active at the bare ok> prompt.
         * zuse)ok> is authenticated, not emergency: ACL applies normally. */
        active->emergency_console = active->zuse_session ? 0 : 1;

        if (active->zuse_session)
            console_puts("zuse)ok> ");
        else
            console_puts("ok> ");

        sk_readline(input, sizeof(input));

        if (input[0] == '\0') {
            console_puts(" ok\n");
            continue;
        }

        vm_interpret(active, input);

        if (active->error) {
            console_puts(" ERROR\n");
            active->error = 0;
        } else {
            console_puts(" ok\n");
        }
    }
}

void sk_repl(VM *vm)
{
    console_println(lithos_version);
    console_puts("StarForth Version ");    console_println(STARFORTH_VERSION);
    console_println("");
    console_println("StarForth Emergency CLI");
    console_println("FORTH-79 interpreter — type BYE or power off to exit");
    console_println("");

    sk_repl_run(vm);
}
