/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * repl.h - Emergency FORTH REPL for LithosAnanke kernel
 */

#ifndef STARKERNEL_REPL_H
#define STARKERNEL_REPL_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * sk_repl - Run the emergency FORTH REPL on the serial console.
 *
 * Blocks until vm->halted is set (BYE word) or the VM encounters a halt.
 * Runs with interrupts enabled; the APIC heartbeat continues to fire.
 *
 * @param vm  Mama VM instance (must be fully initialised)
 */
void sk_repl(VM *vm);

/**
 * sk_repl_run - Bare REPL loop (no banner).
 *
 * Same as sk_repl but skips the version/welcome banner.  Used by START
 * to enter a child VM's interpreter loop without reprinting the header.
 *
 * @param vm  Fully initialised VM instance
 */
void sk_repl_run(VM *vm);

/**
 * sk_repl_step - Execute one REPL turn on a VM and return.
 *
 * Prints the VM's prompt, reads one line, interprets it, prints ok/ERROR,
 * then returns.  Used by the Compudynamics VM-STEP primitive so Hera can
 * give a single REPL quantum to a child VM without surrendering control
 * for the full sk_repl_run() loop.
 *
 * @param vm  Fully initialised VM instance
 * @return 1 if the VM is still running, 0 if it halted during this turn
 */
int sk_repl_step(VM *vm);

/**
 * sk_repl_set_active_vm - Redirect REPL input to a different VM (USE word).
 *
 * Pass NULL to restore default dispatch (Mama's VM).
 * The change takes effect on the next REPL iteration.
 *
 * @param vm  Target VM, or NULL for default
 */
void sk_repl_set_active_vm(VM *vm);

/**
 * sk_repl_get_active_vm - Return the current USE-redirected VM, or NULL.
 */
VM *sk_repl_get_active_vm(void);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_REPL_H */
