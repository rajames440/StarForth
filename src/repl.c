/*

                                 ***   StarForth   ***
  repl.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/repl.h"
#include "../include/log.h"
#include "../include/vm.h"
#include <stdio.h>

/**
 * @brief Starts the Forth REPL (Read-Eval-Print Loop)
 * 
 * @param vm Pointer to the VM structure
 * 
 * @details This function implements an interactive REPL for the Forth interpreter.
 * It continuously reads input from the user, interprets it, and prints the result
 * until the VM is halted or an error occurs. Each successful command is acknowledged
 * with "ok", while errors are reported and reset for the next input.
 */
void vm_repl(VM *vm) {
    log_message(LOG_INFO, "Starting Forth REPL");

    char input[256];
    while (!vm->halted && !vm->error) {
        printf("\033[36mok>\033[0m \033[92m");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            printf("\033[0m");
            fflush(stdout);
            break;
        }

        printf("\033[0m");
        fflush(stdout);

        vm_interpret(vm, input);

        if (!vm->error) {
            printf(" ok\n");
        } else {
            printf(" ERROR\n");
            vm->error = 0; /* Reset error for next input */
        }
    }
}