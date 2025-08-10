/*
 * memory_words.c - FORTH-79 Memory Access and Manipulation Words
 * Fully implemented for StarForth VM model.
 *
 * Copyright (c) 2025 Robert A. James
 * Released under Creative Commons Zero v1.0 Universal.
 */

#include "include/memory_words.h"
#include "../../include/word_registry.h"
#include "vm.h"
#include <string.h>   /* memset, memmove */

/* @ ( addr -- n )  Fetch cell from VM memory */
void memory_word_fetch(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) { vm->error = 1; return; }
    cell_t value = vm_load_cell(vm, addr);
    vm_push(vm, value);
}

/* ! ( n addr -- )  Store cell into VM memory */
void memory_word_store(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t value = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) { vm->error = 1; return; }
    vm_store_cell(vm, addr, value);
}

/* C@ ( addr -- c )  Fetch byte from VM memory */
void memory_word_cfetch(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, 1)) { vm->error = 1; return; }
    uint8_t value = vm_load_u8(vm, addr);
    vm_push(vm, (cell_t)value);
}

/* C! ( c addr -- )  Store byte into VM memory */
void memory_word_cstore(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t value = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, 1)) { vm->error = 1; return; }
    vm_store_u8(vm, addr, (uint8_t)(value & 0xFF));
}

/* +! ( n addr -- )  Add n to contents of cell at addr */
void memory_word_plus_store(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t n = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) { vm->error = 1; return; }
    cell_t current = vm_load_cell(vm, addr);
    vm_store_cell(vm, addr, current + n);
}

/* -! ( n addr -- )  Subtract n from contents of cell at addr */
void memory_word_minus_store(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    cell_t n = vm_pop(vm);
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) { vm->error = 1; return; }
    cell_t current = vm_load_cell(vm, addr);
    vm_store_cell(vm, addr, current - n);
}

/* FILL ( addr len c -- )  Fill len bytes at addr with c */
void memory_word_fill(VM *vm) {
    if (vm->dsp < 2) { vm->error = 1; return; }
    cell_t c_val = vm_pop(vm);
    size_t len = (size_t)vm_pop(vm);
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, len)) { vm->error = 1; return; }
    uint8_t *ptr = vm_ptr(vm, addr);
    for (size_t i = 0; i < len; i++) {
        ptr[i] = (uint8_t)(c_val & 0xFF);
    }
}

/* MOVE ( addr1 addr2 len -- )  Copy len bytes from addr1 to addr2 */
void memory_word_move(VM *vm) {
    if (vm->dsp < 2) { vm->error = 1; return; }
    size_t len = (size_t)vm_pop(vm);
    vaddr_t addr2 = VM_ADDR(vm_pop(vm));
    vaddr_t addr1 = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr1, len) || !vm_addr_ok(vm, addr2, len)) { vm->error = 1; return; }
    uint8_t *src = vm_ptr(vm, addr1);
    uint8_t *dst = vm_ptr(vm, addr2);
    memmove(dst, src, len);
}

/* ERASE ( addr len -- )  Zero len bytes starting at addr */
void memory_word_erase(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    size_t len = (size_t)vm_pop(vm);
    vaddr_t addr = VM_ADDR(vm_pop(vm));
    if (!vm_addr_ok(vm, addr, len)) { vm->error = 1; return; }
    uint8_t *ptr = vm_ptr(vm, addr);
    memset(ptr, 0, len);
}

/* Register memory words */
void register_memory_words(VM *vm) {
    register_word(vm, "@",    memory_word_fetch);
    register_word(vm, "!",    memory_word_store);
    register_word(vm, "C@",   memory_word_cfetch);
    register_word(vm, "C!",   memory_word_cstore);
    register_word(vm, "+!",   memory_word_plus_store);
    register_word(vm, "-!",   memory_word_minus_store);
    register_word(vm, "FILL", memory_word_fill);
    register_word(vm, "MOVE", memory_word_move);
    register_word(vm, "ERASE", memory_word_erase);
}
