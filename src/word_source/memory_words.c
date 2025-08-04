/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 */

#include "include/memory_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <string.h>

/* Safe memory boundaries - using VM's existing memory layout */
#define MEMORY_TEST_START 8192    /* Start tests at 8KB offset - safe from stacks */
#define MEMORY_TEST_SIZE  4096    /* Use 4KB for our memory testing playground */

/* ! ( n addr -- ) Store n at addr */
static void forth_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "!: Data stack underflow");
        return;
    }
    
    cell_t addr = vm_pop(vm);
    cell_t value = vm_pop(vm);
    
    /* Store directly in VM memory at safe offset */
    *(cell_t*)(vm->memory + addr) = value;
    log_message(LOG_DEBUG, "!: Stored %ld at address %ld", (long)value, (long)addr);
}

/* @ ( addr -- n ) Fetch n from addr */
static void forth_fetch(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "@: Data stack underflow");
        return;
    }
    
    cell_t addr = vm_pop(vm);
    
    cell_t value = *(cell_t*)(vm->memory + addr);
    vm_push(vm, value);
    log_message(LOG_DEBUG, "@: Fetched %ld from address %ld", (long)value, (long)addr);
}

/* C! ( c addr -- ) Store character at addr */
static void forth_c_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "C!: Data stack underflow");
        return;
    }
    
    cell_t addr = vm_pop(vm);
    cell_t ch = vm_pop(vm);
    
    vm->memory[addr] = (unsigned char)(ch & 0xFF);
    log_message(LOG_DEBUG, "C!: Stored char %d at address %ld", (int)(ch & 0xFF), (long)addr);
}

/* C@ ( addr -- c ) Fetch character from addr */
static void forth_c_fetch(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "C@: Data stack underflow");
        return;
    }
    
    cell_t addr = vm_pop(vm);
    
    cell_t ch = vm->memory[addr];
    vm_push(vm, ch);
    log_message(LOG_DEBUG, "C@: Fetched char %ld from address %ld", (long)ch, (long)addr);
}

/* +! ( n addr -- ) Add n to value at addr */
static void forth_plus_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "+!: Data stack underflow");
        return;
    }
    
    cell_t addr = vm_pop(vm);
    cell_t n = vm_pop(vm);
    
    cell_t *ptr = (cell_t*)(vm->memory + addr);
    cell_t old_val = *ptr;
    *ptr += n;
    log_message(LOG_DEBUG, "+!: Added %ld to addr %ld (%ld -> %ld)", 
                (long)n, (long)addr, (long)old_val, (long)*ptr);
}

/* FILL ( addr u c -- ) Fill u bytes at addr with c */
static void forth_fill(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "FILL: Data stack underflow");
        return;
    }
    
    cell_t ch = vm_pop(vm);
    cell_t count = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    
    if (count < 0) {
        log_message(LOG_ERROR, "FILL: Invalid count %ld", (long)count);
        return;
    }
    
    memset(vm->memory + addr, ch & 0xFF, count);
    log_message(LOG_DEBUG, "FILL: Filled %ld bytes at %ld with %d", 
                (long)count, (long)addr, (int)(ch & 0xFF));
}

/* ERASE ( addr u -- ) Fill u bytes at addr with zeros */
static void forth_erase(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "ERASE: Data stack underflow");
        return;
    }
    
    cell_t count = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    
    if (count < 0) {
        log_message(LOG_ERROR, "ERASE: Invalid count %ld", (long)count);
        return;
    }
    
    memset(vm->memory + addr, 0, count);
    log_message(LOG_DEBUG, "ERASE: Erased %ld bytes at %ld", (long)count, (long)addr);
}

void register_memory_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 memory words...");
    
    /* Register basic memory words */
    register_word(vm, "!", forth_store);
    register_word(vm, "@", forth_fetch);
    register_word(vm, "C!", forth_c_store);
    register_word(vm, "C@", forth_c_fetch);
    register_word(vm, "+!", forth_plus_store);
    register_word(vm, "FILL", forth_fill);
    register_word(vm, "ERASE", forth_erase);

    log_message(LOG_INFO, "Memory words registered successfully");
}
