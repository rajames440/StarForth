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

/* 64-byte build-time constant baked from LITHOS_VERSION_STR (defined in repl.c) */
extern const char lithos_version[64];

/**
 * sk_repl - Run the emergency FORTH REPL on the serial console.
 *
 * Blocks until vm->halted is set (BYE word) or the VM encounters a halt.
 * Runs with interrupts enabled; the APIC heartbeat continues to fire.
 *
 * @param vm  Mama VM instance (must be fully initialised)
 */
void sk_repl(VM *vm);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_REPL_H */
