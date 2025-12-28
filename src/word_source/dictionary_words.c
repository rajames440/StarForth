/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

#include "include/dictionary_words.h"
#include "../../include/word_registry.h"
#include "vm.h"
#include "../../include/log.h"

/* === SAFE DICTIONARY WORDS IMPLEMENTATION (addresses are VM offsets) === */

/**
 * @brief HERE ( -- addr ) Returns the dictionary pointer
 * @param vm Pointer to the VM structure
 */
void dictionary_word_here(VM *vm) {
    vm_align(vm);
    /* Return absolute pointer to HERE in arena */
    vm_push(vm, (cell_t)(uintptr_t)(vm->memory + vm->here));
}

/**
 * @brief ALLOT ( n -- ) Allocate n bytes in dictionary
 * @param vm Pointer to the VM structure
 * @details Allocates n bytes in the dictionary. n can be negative to deallocate space
 */
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

/**
 * @brief , ( n -- ) Compile cell into dictionary
 * @param vm Pointer to the VM structure
 * @details Compiles a single cell value into the dictionary at HERE
 */
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

/**
 * @brief C, ( c -- ) Compile byte into dictionary
 * @param vm Pointer to the VM structure
 * @details Compiles a single byte value into the dictionary at HERE
 */
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

/**
 * @brief 2, ( d -- ) Compile double-cell into dictionary
 * @param vm Pointer to the VM structure
 * @details Compiles a double cell value into the dictionary at HERE, low cell first
 */
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

/**
 * @brief PAD ( -- addr ) Get scratch pad buffer address
 * @param vm Pointer to the VM structure
 * @details Returns VM address of 512-byte temporary text buffer at top of memory
 */
void dictionary_word_pad(VM *vm) {
    size_t pad_offset = VM_MEMORY_SIZE - 512;
    vm_push(vm, (cell_t) pad_offset);
}

/**
 * @brief SP@ ( -- sp ) Get current stack pointer
 * @param vm Pointer to the VM structure
 * @details Returns the current stack-pointer index (top is 0)
 */
void dictionary_word_sp_fetch(VM *vm) {
    /* No stack args required */
    vm_push(vm, (cell_t) vm->dsp);
}

/**
 * @brief SP! ( sp -- ) Set stack pointer
 * @param vm Pointer to the VM structure
 * @details Restores the stack-pointer, but never allows growing the stack
 */
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

/**
 * @brief LATEST ( -- addr ) Get latest definition address
 * @param vm Pointer to the VM structure
 * @details Returns VM address near most recent compiled definition (end of dictionary)
 */
void dictionary_word_latest(VM *vm) {
    /* M7 Rule: LATEST must return absolute pointer if we are in M7 absolute mode,
       but usually Forth LATEST returns the address of the latest entry header.
       Here vm->latest IS the absolute pointer to the latest header. */
    vm_push(vm, (cell_t)(uintptr_t)vm->latest);
}

/* Registration */
void register_dictionary_words(VM *vm) {
    register_word(vm, "HERE", dictionary_word_here);
    register_word(vm, "ALLOT", dictionary_word_allot);
    register_word(vm, ",", dictionary_word_comma);
    register_word(vm, "C,", dictionary_word_c_comma);
    register_word(vm, "2,", dictionary_word_2comma);
    register_word(vm, "PAD", dictionary_word_pad);
    register_word(vm, "SP!", dictionary_word_sp_store);
    register_word(vm, "SP@", dictionary_word_sp_fetch);
    register_word(vm, "LATEST", dictionary_word_latest);
}