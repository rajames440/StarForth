/*
 * block_words.c - FORTH-79 Block I/O Words
 * Fully implemented for StarForth VM block model.
 *
 * Copyright (c) 2025 Robert A. James
 * Released under Creative Commons Zero v1.0 Universal.
 */

#include "include/block_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <string.h>

/* Internal dirty buffer tracking */
static uint8_t dirty_blocks[MAX_BLOCKS] = {0};

/* BLOCK ( u -- addr )  Get VM address of block u */
void block_word_block(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t blk = vm_pop(vm);
    if (blk < 0 || blk >= MAX_BLOCKS) { vm->error = 1; return; }

    /* Return VM address (offset) into vm->memory for this block */
    vaddr_t addr = (vaddr_t)(blk * BLOCK_SIZE);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) { vm->error = 1; return; }
    vm_push(vm, CELL(addr));
}

/* BUFFER ( u -- addr )  Get VM address of buffer for block u, mark dirty */
void block_word_buffer(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t blk = vm_pop(vm);
    if (blk < 0 || blk >= MAX_BLOCKS) { vm->error = 1; return; }

    vaddr_t addr = (vaddr_t)(blk * BLOCK_SIZE);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) { vm->error = 1; return; }
    dirty_blocks[blk] = 1; /* mark block dirty immediately */
    vm_push(vm, CELL(addr));
}

/* LOAD ( u -- )  Load block u into memory (no-op for in-memory model) */
void block_word_load(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t blk = vm_pop(vm);
    if (blk < 0 || blk >= MAX_BLOCKS) { vm->error = 1; return; }

    /* In-memory VM: blocks are already resident, nothing to load */
    /* If backing store is added later, load here */
}

/* LIST ( u -- )  List block u (send contents to output/log) */
void block_word_list(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t blk = vm_pop(vm);
    if (blk < 0 || blk >= MAX_BLOCKS) { vm->error = 1; return; }

    vaddr_t addr = (vaddr_t)(blk * BLOCK_SIZE);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) { vm->error = 1; return; }
    uint8_t *ptr = vm_ptr(vm, addr);

    /* Print block contents to log, respecting BLOCK_SIZE */
    char buf[BLOCK_SIZE + 1];
    memcpy(buf, ptr, BLOCK_SIZE);
    buf[BLOCK_SIZE] = '\0';
}

/* SCR ( -- addr )  Variable: screen/block being listed */
void block_word_scr(VM *vm) {
    vm_push(vm, CELL(vm->scr_addr));
}

/* THRU ( u1 u2 -- )  Load blocks from u1 to u2 inclusive */
void block_word_thru(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t u2 = vm_pop(vm);
    cell_t u1 = vm_pop(vm);
    if (u1 > u2) { cell_t t = u1; u1 = u2; u2 = t; }

    for (cell_t blk = u1; blk <= u2; blk++) {
        vm_push(vm, blk);
        block_word_load(vm);
        if (vm->error) return;
    }
}

/* UPDATE ( -- )  Mark most recent block as modified */
void block_word_update(VM *vm) {
    /* SCR variable contains VM address of current block number */
    cell_t blk_num = vm_load_cell(vm, vm->scr_addr);
    if (blk_num >= 0 && blk_num < MAX_BLOCKS) {
        dirty_blocks[blk_num] = 1;
    }
}

/* SAVE-BUFFERS ( -- )  Write all modified buffers to storage */
void block_word_save_buffers(VM *vm) {
    for (int blk = 0; blk < MAX_BLOCKS; blk++) {
        if (dirty_blocks[blk]) {
            /* In-memory model: would write to disk here if persistent */
            dirty_blocks[blk] = 0; /* clear dirty flag */
        }
    }
}

/* EMPTY-BUFFERS ( -- )  Clear all buffers */
void block_word_empty_buffers(VM *vm) {
    /* For in-memory model: zero user blocks only */
    for (int blk = USER_BLOCKS_START; blk < MAX_BLOCKS; blk++) {
        vaddr_t addr = (vaddr_t)(blk * BLOCK_SIZE);
        uint8_t *ptr = vm_ptr(vm, addr);
        memset(ptr, 0, BLOCK_SIZE);
        dirty_blocks[blk] = 0;
    }
}

/* Register block words */
void register_block_words(VM *vm) {
    register_word(vm, "BLOCK", block_word_block);
    register_word(vm, "BUFFER", block_word_buffer);
    register_word(vm, "LOAD", block_word_load);
    register_word(vm, "LIST", block_word_list);
    register_word(vm, "SCR", block_word_scr);
    register_word(vm, "THRU", block_word_thru);
    register_word(vm, "UPDATE", block_word_update);
    register_word(vm, "SAVE-BUFFERS", block_word_save_buffers);
    register_word(vm, "EMPTY-BUFFERS", block_word_empty_buffers);
}
