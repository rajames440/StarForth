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

#include "include/memory_words.h"
#include "../../include/word_registry.h"
#include "vm.h"
#include "../../include/log.h"
#include <string.h>


/**
 * @brief Implements FORTH word '@' - Fetch cell from VM memory
 * 
 * Stack effect: ( addr -- n )
 * Fetches a cell value from the given memory address in VM memory.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_fetch(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        vm->error = 1;
        return;
    }
    cell_t value = vm_load_cell(vm, addr);
    vm_push(vm, value);
}

/**
 * @brief Implements FORTH word '!' - Store cell into VM memory
 *
 * Stack effect: ( n addr -- )
 * Stores a cell value at the given memory address in VM memory.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_store(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t value = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        vm->error = 1;
        return;
    }
    vm_store_cell(vm, addr, value);
}

/**
 * @brief Implements FORTH word 'C@' - Fetch byte from VM memory
 *
 * Stack effect: ( addr -- c )
 * Fetches a byte value from the given memory address in VM memory.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_cfetch(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, 1)) {
        vm->error = 1;
        return;
    }
    uint8_t value = vm_load_u8(vm, addr);
    vm_push(vm, (cell_t) value);
}

/**
 * @brief Implements FORTH word 'C!' - Store byte into VM memory
 *
 * Stack effect: ( c addr -- )
 * Stores a byte value at the given memory address in VM memory.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_cstore(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t value = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, 1)) {
        vm->error = 1;
        return;
    }
    vm_store_u8(vm, addr, (uint8_t)(value & 0xFF));
}

/**
 * @brief Implements FORTH word '+!' - Add to contents of cell
 *
 * Stack effect: ( n addr -- )
 * Adds n to the contents of the cell at the given address.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_plus_store(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t n = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        vm->error = 1;
        return;
    }
    cell_t current = vm_load_cell(vm, addr);
    vm_store_cell(vm, addr, current + n);
}

/**
 * @brief Implements FORTH word '-!' - Subtract from contents of cell
 *
 * Stack effect: ( n addr -- )
 * Subtracts n from the contents of the cell at the given address.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_minus_store(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t n = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        vm->error = 1;
        return;
    }
    cell_t current = vm_load_cell(vm, addr);
    vm_store_cell(vm, addr, current - n);
}

/**
 * @brief Implements FORTH word 'FILL' - Fill memory with byte value
 *
 * Stack effect: ( addr len c -- )
 * Fills len bytes of memory starting at addr with the value c.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_fill(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    cell_t c_val = vm_pop(vm);
    size_t len = (size_t) vm_pop(vm);
    cell_t addr_cell = vm_pop(vm);

    /* Try VM address first */
    vaddr_t addr = VM_ADDR(addr_cell);
    if (vm_addr_ok(vm, addr, len)) {
        /* It's a valid VM address, use vm_ptr */
        uint8_t *ptr = vm_ptr(vm, addr);
        memset(ptr, (int) (c_val & 0xFF), len);
    } else {
        /* It's an external pointer (from BLOCK/BUFFER), use it directly */
        uint8_t *ptr = (uint8_t *) (uintptr_t) addr_cell;
        memset(ptr, (int) (c_val & 0xFF), len);
    }
}

/**
 * @brief Implements FORTH word 'MOVE' - Copy memory region
 *
 * Stack effect: ( addr1 addr2 len -- )
 * Copies len bytes from addr1 to addr2, handling overlapping regions correctly.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_move(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    size_t len = (size_t) vm_pop(vm);
    vaddr_t addr2 = VM_ADDR(vm_pop(vm));
    vaddr_t addr1 = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr1, len) || !vm_addr_ok(vm, addr2, len)) {
        vm->error = 1;
        return;
    }
    uint8_t *src = vm_ptr(vm, addr1);
    uint8_t *dst = vm_ptr(vm, addr2);
    memmove(dst, src, len);
}

/**
 * @brief Implements FORTH word 'ERASE' - Zero memory region
 *
 * Stack effect: ( addr len -- )
 * Sets len bytes to zero starting at addr.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_erase(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    size_t len = (size_t) vm_pop(vm);
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, len)) {
        vm->error = 1;
        return;
    }
    uint8_t *ptr = vm_ptr(vm, addr);
    memset(ptr, 0, len);
}

/**
 * @brief Implements FORTH word '2@' - Fetch double cell
 *
 * Stack effect: ( addr -- x_low x_high )
 * Fetches two consecutive cells from memory, pushing low cell then high cell.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_2fetch(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    vaddr_t addr = VM_ADDR(vm_pop(vm));

    const size_t sz = sizeof(cell_t);
    /* need room for two cells starting at addr */
    if (!vm_addr_ok(vm, addr, sz) || !vm_addr_ok(vm, addr + (vaddr_t) sz, sz)) {
        vm->error = 1;
        return;
    }

    cell_t low = vm_load_cell(vm, addr);
    cell_t high = vm_load_cell(vm, addr + (vaddr_t) sz);

    /* Push order: low first, high second (so high ends up on top) */
    vm_push(vm, low);
    vm_push(vm, high);
}

/**
 * @brief Implements FORTH word '2!' - Store double cell
 *
 * Stack effect: ( x_low x_high addr -- )
 * Stores two consecutive cells to memory, low cell at addr, high cell at addr+cell.
 *
 * @param vm Pointer to the VM structure
 */
void memory_word_2store(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    vaddr_t addr = VM_ADDR(vm_pop(vm)); /* top of stack: addr */
    cell_t high = vm_pop(vm); /* next: high part */
    cell_t low = vm_pop(vm); /* next: low part */

    const size_t sz = sizeof(cell_t);
    if (!vm_addr_ok(vm, addr, sz) || !vm_addr_ok(vm, addr + (vaddr_t) sz, sz)) {
        vm->error = 1;
        return;
    }

    vm_store_cell(vm, addr, low);
    vm_store_cell(vm, addr + (vaddr_t) sz, high);
}

/* CELLS ( n -- n' )  Multiply by cell size (bytes per cell). */
/**
 * @brief Implements FORTH word 'CELLS' - Scale by cell size
 *
 * Stack effect: ( n -- n' )
 * Multiplies top of stack by cell size in bytes.
 *
 * @param vm Pointer to the VM structure
 */
static void memory_word_cells(VM *vm) {
    if (!vm) return;
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "CELLS: stack underflow");
        return;
    }
    cell_t n = vm_pop(vm);
    vm_push(vm, n * (cell_t) sizeof(cell_t));
}


/**
 * @brief Register all memory manipulation words with the VM
 *
 * Registers all standard memory words (@, !, C@, C!, etc.) with the VM's word dictionary.
 *
 * @param vm Pointer to the VM structure
 */
void register_memory_words(VM *vm) {
    register_word(vm, "@", memory_word_fetch);
    register_word(vm, "!", memory_word_store);
    register_word(vm, "C@", memory_word_cfetch);
    register_word(vm, "C!", memory_word_cstore);
    register_word(vm, "+!", memory_word_plus_store);
    register_word(vm, "-!", memory_word_minus_store);
    register_word(vm, "2@", memory_word_2fetch);
    register_word(vm, "2!", memory_word_2store);
    register_word(vm, "FILL", memory_word_fill);
    register_word(vm, "MOVE", memory_word_move);
    register_word(vm, "ERASE", memory_word_erase);
    register_word(vm, "CELLS", memory_word_cells);
}