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
/* ! ( n addr -- ) Store n at addr */
static void memory_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "!: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t addr = vm_pop(vm);
    cell_t value = vm_pop(vm);

    if (addr < 0 || addr >= (VM_MEMORY_SIZE - sizeof(cell_t))) {
        log_message(LOG_ERROR, "!: Address %ld out of bounds", (long)addr);
        vm->error = 1;
        return;
    }

    if (addr % sizeof(cell_t) != 0) {
        log_message(LOG_ERROR, "!: Misaligned address %ld (sizeof(cell_t)=%zu, addr %% %zu = %ld)",
                    (long)addr, sizeof(cell_t), sizeof(cell_t), (long)(addr % sizeof(cell_t)));
        vm->error = 1;
        return;
    }

    *(cell_t*)(&vm->memory[addr]) = value;
    log_message(LOG_DEBUG, "!: Stored %ld at address %ld", (long)value, (long)addr);
}


/* @ ( addr -- n ) Fetch n from addr */
static void memory_fetch(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "@: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t addr = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || addr >= (VM_MEMORY_SIZE - sizeof(cell_t))) {
        log_message(LOG_ERROR, "@: Address %ld out of bounds", (long)addr);
        vm->error = 1;
        return;
    }

    // Enhanced alignment check with debugging
    if (addr % sizeof(cell_t) != 0) {
        log_message(LOG_ERROR, "@: Misaligned address %ld (sizeof(cell_t)=%zu, addr %% %zu = %ld)",
                    (long)addr, sizeof(cell_t), sizeof(cell_t), (long)(addr % sizeof(cell_t)));
        vm->error = 1;
        return;
    }

    cell_t value = *(cell_t*)(&vm->memory[addr]);
    vm_push(vm, value);
    log_message(LOG_DEBUG, "@: Fetched %ld from address %ld", (long)value, (long)addr);
}

/* C! ( c addr -- ) Store character at addr */
static void memory_c_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "C!: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t addr = vm_pop(vm);
    cell_t ch = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || addr >= VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "C!: Address %ld out of bounds", (long)addr);
        vm->error = 1;
        return;
    }

    vm->memory[addr] = (unsigned char)(ch & 0xFF);
    log_message(LOG_DEBUG, "C!: Stored char %d at address %ld", (int)(ch & 0xFF), (long)addr);
}

/* C@ ( addr -- c ) Fetch character from addr */
static void memory_c_fetch(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "C@: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t addr = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || addr >= VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "C@: Address %ld out of bounds", (long)addr);
        vm->error = 1;
        return;
    }

    cell_t ch = vm->memory[addr];
    vm_push(vm, ch);
    log_message(LOG_DEBUG, "C@: Fetched char %ld from address %ld", (long)ch, (long)addr);
}

/* +! ( n addr -- ) Add n to value at addr */
static void memory_plus_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "+!: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t addr = vm_pop(vm);
    cell_t n = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || addr >= (VM_MEMORY_SIZE - sizeof(cell_t))) {
        log_message(LOG_ERROR, "+!: Address %ld out of bounds", (long)addr);
        vm->error = 1;
        return;
    }

    // Alignment check for cell_t access
    if (addr % sizeof(cell_t) != 0) {
        log_message(LOG_ERROR, "+!: Misaligned address %ld", (long)addr);
        vm->error = 1;
        return;
    }

    cell_t *ptr = (cell_t*)(&vm->memory[addr]);
    cell_t old_val = *ptr;
    *ptr += n;
    log_message(LOG_DEBUG, "+!: Added %ld to addr %ld (%ld -> %ld)",
                (long)n, (long)addr, (long)old_val, (long)*ptr);
}

/* FILL ( addr u c -- ) Fill u bytes at addr with c */
static void memory_fill(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "FILL: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t ch = vm_pop(vm);
    cell_t count = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || count < 0 || (addr + count) > VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "FILL: Invalid range [%ld, %ld)", (long)addr, (long)(addr + count));
        vm->error = 1;
        return;
    }

    memset(&vm->memory[addr], ch & 0xFF, count);
    log_message(LOG_DEBUG, "FILL: Filled %ld bytes at %ld with %d",
                (long)count, (long)addr, (int)(ch & 0xFF));
}

/* ERASE ( addr u -- ) Fill u bytes at addr with zeros */
static void memory_erase(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "ERASE: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t count = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || count < 0 || (addr + count) > VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "ERASE: Invalid range [%ld, %ld)", (long)addr, (long)(addr + count));
        vm->error = 1;
        return;
    }

    memset(&vm->memory[addr], 0, count);
    log_message(LOG_DEBUG, "ERASE: Erased %ld bytes at %ld", (long)count, (long)addr);
}

void register_memory_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 memory words...");

    /* Register basic memory words */
    register_word(vm, "!", memory_store);
    register_word(vm, "@", memory_fetch);
    register_word(vm, "C!", memory_c_store);
    register_word(vm, "C@", memory_c_fetch);
    register_word(vm, "+!", memory_plus_store);
    register_word(vm, "FILL", memory_fill);
    register_word(vm, "ERASE", memory_erase);

    log_message(LOG_INFO, "Memory words registered successfully");
}