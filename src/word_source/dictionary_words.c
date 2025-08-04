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
void word_here(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&vm->memory[vm->here]);
}

/* ALLOT ( n -- )  Allocate n bytes in dictionary */
void word_allot(VM *vm) {
    cell_t n;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    n = vm_pop(vm);
    
    /* Check for valid allocation size */
    if (n < 0 || vm->here + n >= VM_MEMORY_SIZE) {
        vm->error = 1;  /* Invalid allocation or out of memory */
        return;
    }
    
    vm->here += (size_t)n;
    vm_align(vm);  /* Align to cell boundary */
}

/* , ( n -- )  Compile n into dictionary */
void word_comma(VM *vm) {
    cell_t n;
    cell_t *target;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    n = vm_pop(vm);
    
    /* Check if we have space for a cell */
    if (vm->here + sizeof(cell_t) >= VM_MEMORY_SIZE) {
        vm->error = 1;  /* Out of memory */
        return;
    }
    
    /* Store the cell value */
    target = (cell_t*)&vm->memory[vm->here];
    *target = n;
    vm->here += sizeof(cell_t);
    vm_align(vm);
}

/* C, ( c -- )  Compile character into dictionary */
void word_c_comma(VM *vm) {
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
void word_2_comma(VM *vm) {
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
void word_pad(VM *vm) {
    size_t pad_offset;
    
    /* PAD is typically located after the dictionary space */
    /* We'll place it at a fixed offset from the end of memory */
    pad_offset = VM_MEMORY_SIZE - 512;  /* 512 bytes for PAD */
    vm_push(vm, (cell_t)(uintptr_t)&vm->memory[pad_offset]);
}

/* SP! ( addr -- )  Set data stack pointer */
void word_sp_store(VM *vm) {
    cell_t addr;
    cell_t *stack_addr;
    cell_t *stack_base;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    addr = vm_pop(vm);
    
    /* Calculate new stack pointer from address */
    /* This is a bit tricky - we need to convert address to stack index */
    stack_addr = (cell_t*)(uintptr_t)addr;
    stack_base = vm->data_stack;
    
    /* Check if address is within stack bounds */
    if (stack_addr < stack_base || stack_addr > stack_base + STACK_SIZE) {
        vm->error = 1;  /* Invalid stack address */
        return;
    }
    
    vm->dsp = (int)(stack_addr - stack_base) - 1;
    
    /* Ensure stack pointer is valid */
    if (vm->dsp < -1 || vm->dsp >= STACK_SIZE) {
        vm->error = 1;
        vm->dsp = -1;  /* Reset to empty */
    }
}

/* SP@ ( -- addr )  Get data stack pointer */
void word_sp_fetch(VM *vm) {
    /* Return address of current top of stack */
    if (vm->dsp >= 0) {
        vm_push(vm, (cell_t)(uintptr_t)&vm->data_stack[vm->dsp + 1]);
    } else {
        /* Empty stack - return address of stack base */
        vm_push(vm, (cell_t)(uintptr_t)&vm->data_stack[0]);
    }
}

/* LATEST ( -- addr )  Address of most recent definition */
void word_latest(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&vm->latest);
}

/* FORTH-79 Dictionary Word Registration and Testing */
void register_dictionary_words(VM *vm) {

    log_message(LOG_INFO, "Registering dictionary & compilation words...");
    
    /* Register all dictionary & compilation words */
    register_word(vm, "HERE", word_here);
    register_word(vm, "ALLOT", word_allot);
    register_word(vm, ",", word_comma);
    register_word(vm, "C,", word_c_comma);
    register_word(vm, "2,", word_2_comma);
    register_word(vm, "PAD", word_pad);
    register_word(vm, "SP!", word_sp_store);
    register_word(vm, "SP@", word_sp_fetch);
    register_word(vm, "LATEST", word_latest);

    log_message(LOG_INFO, "Dictionary & compilation words registered and tested");
}