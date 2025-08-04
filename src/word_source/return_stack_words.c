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

/* return_stack_words.c - FORTH-79 Return Stack Operation Words */
#include "include/return_stack_words.h"
#include "../../include/log.h"
#include "../../include/word_registry.h"
#include <string.h>

/* >R - Move item from data to return stack ( n -- ) ( R: -- n ) */
static void word_to_r(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, ">R: Data stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t value = vm_pop(vm);
    vm_rpush(vm, value);
    
    log_message(LOG_DEBUG, ">R: Moved %ld from data to return stack", (long)value);
}

/* R> - Move item from return to data stack ( -- n ) ( R: n -- ) */
static void word_r_from(VM *vm) {
    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "R>: Return stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t value = vm_rpop(vm);
    vm_push(vm, value);
    
    log_message(LOG_DEBUG, "R>: Moved %ld from return to data stack", (long)value);
}

/* R@ - Copy top of return stack to data stack ( -- n ) ( R: n -- n ) */
static void word_r_fetch(VM *vm) {
    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "R@: Return stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t value = vm->return_stack[vm->rsp];
    vm_push(vm, value);
    
    log_message(LOG_DEBUG, "R@: Copied %ld from return stack to data stack", (long)value);
}

/* RP! - Set return stack pointer ( addr -- ) */
static void word_rp_store(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "RP!: Data stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t addr = vm_pop(vm);
    
    /* Validate the address range for safety */
    if (addr < -1 || addr > STACK_SIZE - 1) {
        log_message(LOG_ERROR, "RP!: Invalid return stack pointer %ld", (long)addr);
        vm->error = 1;
        return;
    }
    
    vm->rsp = (int)addr;
    
    log_message(LOG_DEBUG, "RP!: Set return stack pointer to %ld", (long)addr);
}

/* RP@ - Get return stack pointer ( -- addr ) */
static void word_rp_fetch(VM *vm) {
    vm_push(vm, (cell_t)vm->rsp);
    
    log_message(LOG_DEBUG, "RP@: Return stack pointer %d", vm->rsp);
}

/* Register all return stack operation words */
void register_return_stack_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 return stack words...");
    
    /* Register words - vm_create_word returns DictEntry* or NULL */
    if (!vm_create_word(vm, ">R", 2, word_to_r)) {
        log_message(LOG_ERROR, "Failed to register >R");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: >R");
    
    if (!vm_create_word(vm, "R>", 2, word_r_from)) {
        log_message(LOG_ERROR, "Failed to register R>");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: R>");
    
    if (!vm_create_word(vm, "R@", 2, word_r_fetch)) {
        log_message(LOG_ERROR, "Failed to register R@");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: R@");
    
    if (!vm_create_word(vm, "RP!", 3, word_rp_store)) {
        log_message(LOG_ERROR, "Failed to register RP!");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: RP!");
    
    if (!vm_create_word(vm, "RP@", 3, word_rp_fetch)) {
        log_message(LOG_ERROR, "Failed to register RP@");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: RP@");
    
    log_message(LOG_INFO, "Return stack words registered successfully");
}