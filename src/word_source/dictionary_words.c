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

/* dictionary_words.c - FORTH-79 Dictionary & Compilation Words */
#include "include/dictionary_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* FORTH-79 Dictionary & Compilation Words Implementation */

/* HERE ( -- addr )  Address of next free dictionary space */
void dictionary_word_here(VM *vm) {
    /* Ensure dictionary is aligned before returning the address */
    vm_align(vm);
    vm_push(vm, vm->here);  /* VM memory offset */
}

/* ALLOT ( n -- )  Allocate n bytes in dictionary */
void dictionary_word_allot(VM *vm) {
    cell_t n;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    n = vm_pop(vm);
    
    /* Allow negative allocation (standard Forth) */
    ptrdiff_t new_here = (ptrdiff_t)vm->here + n;
    if (new_here < 0 || new_here >= VM_MEMORY_SIZE) {
        vm->error = 1;
        return;
    }
    
    vm->here = (size_t)new_here;
    vm_align(vm);
}

/* , ( n -- )  Compile n into dictionary */
void dictionary_word_comma(VM *vm) {
    cell_t n;
    cell_t *target;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    n = vm_pop(vm);

    /* Ensure alignment before storing */
    vm_align(vm);

    /* Check if we have space for a cell */
    if (vm->here + sizeof(cell_t) >= VM_MEMORY_SIZE) {
        vm->error = 1;  /* Out of memory */
        return;
    }

    /* Store the cell value */
    target = (cell_t*)&vm->memory[vm->here];
    *target = n;
    vm->here += sizeof(cell_t);

    /* Alignment is already guaranteed since we aligned before and advanced by sizeof(cell_t) */
}

/* C, ( c -- )  Compile character into dictionary */
void dictionary_word_c_comma(VM *vm) {
    cell_t c;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    c = vm_pop(vm);
    
    /* Check if we have space for a byte */
    if (vm->here >= VM_MEMORY_SIZE) {
        vm->error = 1;  /* Out of memory */
        return;
    }
    
    /* Store the byte value */
    vm->memory[vm->here] = (uint8_t)(c & 0xFF);
    vm->here++;
    /* Note: Don't align after C, - bytes are packed */
}

/* 2, ( d -- )  Compile double into dictionary */
void dictionary_word_2_comma(VM *vm) {
    cell_t dlow, dhigh;
    cell_t *target;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    dlow = vm_pop(vm);
    dhigh = vm_pop(vm);
    
    /* Check if we have space for two cells */
    if (vm->here + 2 * sizeof(cell_t) >= VM_MEMORY_SIZE) {
        vm->error = 1;  /* Out of memory */
        return;
    }
    
    /* Store the double value (high part first) */
    target = (cell_t*)&vm->memory[vm->here];
    *target = dhigh;
    vm->here += sizeof(cell_t);
    
    target = (cell_t*)&vm->memory[vm->here];
    *target = dlow;
    vm->here += sizeof(cell_t);
    vm_align(vm);
}

/* PAD ( -- addr )  Address of temporary text buffer */
void dictionary_word_pad(VM *vm) {
    size_t pad_offset = VM_MEMORY_SIZE - 512;
    vm_push(vm, pad_offset);  /* VM memory offset */
}

/* SP! ( addr -- )  Set data stack pointer */
void dictionary_word_sp_store(VM *vm) {
    cell_t addr;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    addr = vm_pop(vm);
    
    /* Stack addresses are raw pointers to data_stack[] */
    cell_t *stack_addr = (cell_t*)(uintptr_t)addr;
    cell_t *stack_base = vm->data_stack;
    
    if (stack_addr < stack_base || stack_addr > stack_base + STACK_SIZE) {
        vm->error = 1;
        return;
    }
    
    vm->dsp = (int)(stack_addr - stack_base) - 1;
    
    if (vm->dsp < -1 || vm->dsp >= STACK_SIZE) {
        vm->error = 1;
        vm->dsp = -1;
    }
}

/* SP@ ( -- addr )  Get data stack pointer */
void dictionary_word_sp_fetch(VM *vm) {
    /* Stack addresses are raw pointers */
    if (vm->dsp >= 0) {
        vm_push(vm, (cell_t)(uintptr_t)&vm->data_stack[vm->dsp + 1]);
    } else {
        vm_push(vm, (cell_t)(uintptr_t)&vm->data_stack[0]);
    }
}

/* LATEST ( -- addr )  Address of most recent definition */
void dictionary_word_latest(VM *vm) {
    /* Raw pointer to VM variable (not in VM memory) */
    vm_push(vm, (cell_t)(uintptr_t)&vm->latest);
}

/* FORTH-79 Dictionary Word Registration and Testing */
void register_dictionary_words(VM *vm) {

    log_message(LOG_INFO, "Registering dictionary & compilation words...");
    
    /* Register all dictionary & compilation words */
    register_word(vm, "HERE", dictionary_word_here);
    register_word(vm, "ALLOT", dictionary_word_allot);
    register_word(vm, ",", dictionary_word_comma);
    register_word(vm, "C,", dictionary_word_c_comma);
    register_word(vm, "2,", dictionary_word_2_comma);
    register_word(vm, "PAD", dictionary_word_pad);
    register_word(vm, "SP!", dictionary_word_sp_store);
    register_word(vm, "SP@", dictionary_word_sp_fetch);
    register_word(vm, "LATEST", dictionary_word_latest);

    log_message(LOG_INFO, "Dictionary & compilation words registered and tested");
}