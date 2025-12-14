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
                vm->error = 0; /* Reset error for next input */
            }
        } else {
            /* In script mode, still reset error flag but don't print */
            if (vm->error) {
                vm->error = 0;
            }
        }
    }
}