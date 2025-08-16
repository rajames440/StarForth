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

#include "repl.h"
#include "log.h"
#include "platform/starforth_platform.h"

void vm_repl(VM *vm) {
    log_message(LOG_INFO, "Starting Forth REPL");

    char input[256];
    while (!vm->halted && !vm->error) {
        sf_printf("ok> ");
        sf_fflush(sf_stdout);

        if (!sf_fgets(input, sizeof(input), sf_stdin)) {
            break;
        }

        vm_interpret(vm, input);

        if (!vm->error) {
            sf_printf(" ok\n");
        } else {
            sf_printf(" ERROR\n");
            vm->error = 0; /* Reset error for next input */
        }
    }
}