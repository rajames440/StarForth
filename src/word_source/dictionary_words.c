/*

                                 ***   StarForth   ***
  dictionary_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/11/25, 10:35 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "include/dictionary_words.h"
#include "../../include/word_registry.h"
#include "vm.h"
#include "../../include/log.h"

/* === SAFE DICTIONARY WORDS IMPLEMENTATION (addresses are VM offsets) === */

void dictionary_word_here(VM *vm) {
    vm_align(vm);
    vm_push(vm, vm->here);
}

/* ALLOT ( n -- )  Allocate n bytes in dictionary (can be negative) */
void dictionary_word_allot(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t n = vm_pop(vm);
    cell_t new_here = vm->here + n;
    if (new_here < 0 || new_here > (cell_t) VM_MEMORY_SIZE) {
        vm->error = 1;
        return;
    }
    vm->here = new_here;
}

/* , ( n -- )  Compile cell into dictionary */
void dictionary_word_comma(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t n = vm_pop(vm);
    vm_align(vm);
    if (vm->here + (cell_t) sizeof(cell_t) > VM_MEMORY_SIZE) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm->here);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        vm->error = 1;
        return;
    }
    vm_store_cell(vm, addr, n);
    vm->here += sizeof(cell_t);
}

/* C, ( c -- )  Compile byte into dictionary */
void dictionary_word_c_comma(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t c = vm_pop(vm);
    if (vm->here + 1 > VM_MEMORY_SIZE) {
        vm->error = 1;
        return;
    }
    vaddr_t a = VM_ADDR(vm->here);
    if (!vm_addr_ok(vm, a, 1)) {
        vm->error = 1;
        return;
    }
    vm_store_u8(vm, a, (uint8_t)(c & 0xFF));
    vm->here += 1;
}

/* 2, ( d -- )  Compile double-cell into dictionary: low then high */
void dictionary_word_2comma(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t high = vm_pop(vm);
    cell_t low = vm_pop(vm);
    vm_align(vm);
    if (vm->here + (cell_t)(2 * sizeof(cell_t)) > VM_MEMORY_SIZE) {
        vm->error = 1;
        return;
    }
    vaddr_t a = VM_ADDR(vm->here);
    if (!vm_addr_ok(vm, a, 2 * sizeof(cell_t))) {
        vm->error = 1;
        return;
    }
    vm_store_cell(vm, a, low);
    vm_store_cell(vm, a + sizeof(cell_t), high);
    vm->here += 2 * sizeof(cell_t);
}

/* PAD ( -- addr )  VM address of 512-byte temporary text buffer at top of memory */
void dictionary_word_pad(VM *vm) {
    size_t pad_offset = VM_MEMORY_SIZE - 512;
    vm_push(vm, (cell_t) pad_offset);
}

/* SP@ ( -- sp )  Return the current stack-pointer index (top is 0) */
void dictionary_word_sp_fetch(VM *vm) {
    /* No stack args required */
    vm_push(vm, (cell_t) vm->dsp);
}

/* SP! ( sp -- )  Restore the stack-pointer, but never grow the stack */
void dictionary_word_sp_store(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    } /* need one arg */
    cell_t new_sp = vm_pop(vm);

    /* Valid range: -1 (empty stack) up to current dsp (can shrink, not grow) */
    if (new_sp < -1 || new_sp > vm->dsp) {
        vm->error = 1;
        return;
    }

    vm->dsp = new_sp;
    /* We don’t need to scrub values above dsp; they’re considered garbage/unused. */
}

/* LATEST ( -- addr )  Return VM address near most recent compiled definition (end of dictionary) */
void dictionary_word_latest(VM *vm) {
    vm_push(vm, vm->here);
}

/* Registration */
void register_dictionary_words(VM *vm) {
    log_message(LOG_INFO, "Registering dictionary & compilation words...");
    register_word(vm, "HERE", dictionary_word_here);
    register_word(vm, "ALLOT", dictionary_word_allot);
    register_word(vm, ",", dictionary_word_comma);
    register_word(vm, "C,", dictionary_word_c_comma);
    register_word(vm, "2,", dictionary_word_2comma);
    register_word(vm, "PAD", dictionary_word_pad);
    register_word(vm, "SP!", dictionary_word_sp_store);
    register_word(vm, "SP@", dictionary_word_sp_fetch);
    register_word(vm, "LATEST", dictionary_word_latest);
    log_message(LOG_INFO, "Dictionary & compilation words registered and tested");
}