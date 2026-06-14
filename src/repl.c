/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

#include "../include/repl.h"
#include "../include/log.h"
#include "../include/vm.h"
#include <stdio.h>

/*
 * vm_fault_handler — extension point for non-recoverable VM errors.
 *
 * Called when EMERGENCY_CONSOLE_ENABLED=0 and the REPL encounters an error.
 * The default implementation logs the fault and returns; the REPL loop then
 * exits because vm->error is not reset.
 *
 * Override by providing a non-weak definition in a platform-specific file
 * (e.g., starkernel/hal/fault.c) for reset, watchdog, or debug-probe hooks.
 */
__attribute__((weak)) void vm_fault_handler(VM *vm) {
    log_message(LOG_ERROR, "VM fault — emergency console disabled; halting");
    vm->halted = 1;
}

/**
 * @brief Starts the Forth REPL (Read-Eval-Print Loop)
 *
 * @param vm Pointer to the VM structure
 * @param script_mode If 1, suppress prompts and "ok" output (for piped input)
 *
 * @details This function implements an interactive REPL for the Forth interpreter.
 * It continuously reads input from the user, interprets it, and prints the result
 * until the VM is halted or an error occurs.
 *
 * In interactive mode (script_mode=0):
 *   - Prints colored prompt "ok> "
 *   - Acknowledges successful commands with " ok"
 *   - Reports errors with " ERROR"
 *
 * In script mode (script_mode=1):
 *   - Suppresses all prompts and status messages
 *   - Suitable for piped input (heredoc, pipes, redirects)
 */
void vm_repl(VM *vm, int script_mode) {
    if (!script_mode) {
        log_message(LOG_INFO, "Starting Forth REPL");
    }

    char input[256];
    while (!vm->halted && !vm->error) {
        /* Print prompt only in interactive mode */
        if (!script_mode) {
            if (vm->zuse_session)
                printf("\033[36mzuse)ok>\033[0m \033[92m");
            else
                printf("\033[36mok>\033[0m \033[92m");
            fflush(stdout);
        }

        if (!fgets(input, sizeof(input), stdin)) {
            if (!script_mode) {
                printf("\033[0m");
                fflush(stdout);
            }
            break;
        }

        if (!script_mode) {
            printf("\033[0m");
            fflush(stdout);
        }

        vm_interpret(vm, input);

        if (!script_mode) {
            /* Print status only in interactive mode */
            if (!vm->error) {
                printf(" ok\n");
            } else {
                printf(" ERROR\n");
#if EMERGENCY_CONSOLE_ENABLED
                vm->error = 0; /* recover: allow next input */
#else
                vm_fault_handler(vm); /* non-recoverable: extension point */
#endif
            }
        } else {
            if (vm->error) {
#if EMERGENCY_CONSOLE_ENABLED
                vm->error = 0;
#else
                vm_fault_handler(vm);
#endif
            }
        }
    }
}