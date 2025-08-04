/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 *
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 * To the extent possible under law, the author(s) have dedicated all copyright and related
 * and neighboring rights to this software to the public domain worldwide.
 * This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 */

#include "vm.h"
#include "log.h"
#include "word_registry.h"
#include "../test/include/test.h"

int main() {
    VM vm;
    
    /* Enable debug logging to see what's happening */
    log_set_level(LOG_DEBUG);
    
    vm_init(&vm);
    
    if (log_get_level() >= LOG_DEBUG) {
        run_word_tests(&vm);
    }

    vm_repl(&vm);
    
    return 0;
}