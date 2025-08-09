#include "repl.h"
#include "log.h"
#include <stdio.h>

void vm_repl(VM *vm) {
    log_message(LOG_INFO, "Starting Forth REPL");

    char input[256];
    while (!vm->halted && !vm->error) {
        printf("ok> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        vm_interpret(vm, input);

        if (!vm->error) {
            printf(" ok\n");
        } else {
            printf(" ERROR\n");
            vm->error = 0; /* Reset error for next input */
        }
    }
}
