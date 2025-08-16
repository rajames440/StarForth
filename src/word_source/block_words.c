/*

                                 ***   StarForth   ***
  block_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/11/25, 10:06 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "include/block_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <string.h>

/* ----------------------------------------------------------------------
 * Model notes:
 * - A "block" is a contiguous BLOCK_SIZE region inside vm->memory.
 * - VM addresses are byte offsets (vaddr_t) into vm->memory.
 * - SCR is a VM cell that holds the "current screen" (block number).
 * - We track a simple dirty flag per block; persistence is a no-op for now.
 * ---------------------------------------------------------------------- */

static uint8_t dirty_blocks[MAX_BLOCKS] = {0};

/* --- small helpers ---------------------------------------------------- */

static int block_in_range(cell_t blk) {
    /* block 0 is invalid in tests; highest valid is MAX_BLOCKS-1 */
    if (blk < 1) return 0;
    if (blk >= (cell_t) MAX_BLOCKS) return 0;
    return 1;
}

static vaddr_t block_to_addr(cell_t blk) {
    /* blk is assumed valid (>=1) */
    return (vaddr_t)((uint64_t) blk * (uint64_t) BLOCK_SIZE);
}

static void set_scr(VM *vm, cell_t blk) {
    if (!vm) return;
    vm_store_cell(vm, vm->scr_addr, (cell_t) blk);
}

/* Optional utility surface (matches your header) */
void init_block_system(VM *vm) {
    (void) vm;
    memset(dirty_blocks, 0, sizeof(dirty_blocks));
}

unsigned char *get_block_buffer(VM *vm, int block_num) {
    if (!vm) return NULL;
    cell_t blk = (cell_t) block_num;
    if (!block_in_range(blk)) return NULL;

    vaddr_t addr = block_to_addr(blk);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) return NULL;

    set_scr(vm, blk);
    return (unsigned char *) vm_ptr(vm, addr);
}

unsigned char *get_empty_buffer(VM *vm, int block_num) {
    if (!vm) return NULL;
    cell_t blk = (cell_t) block_num;
    if (!block_in_range(blk)) return NULL;

    vaddr_t addr = block_to_addr(blk);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) return NULL;

    unsigned char *p = (unsigned char *) vm_ptr(vm, addr);
    memset(p, 0, BLOCK_SIZE);
    dirty_blocks[blk] = 1;
    set_scr(vm, blk);
    return p;
}

void mark_buffer_dirty(VM *vm) {
    if (!vm) return;
    cell_t blk = vm_load_cell(vm, vm->scr_addr);
    if (block_in_range(blk)) dirty_blocks[blk] = 1;
}

void save_all_buffers(VM *vm) {
    (void) vm;
    for (int i = 1; i < (int) MAX_BLOCKS; ++i) {
        if (dirty_blocks[i]) {
            /* Persist here in the future; in-memory model clears the flag. */
            dirty_blocks[i] = 0;
        }
    }
}

void empty_all_buffers(VM *vm) {
    if (!vm) return;
    for (int blk = USER_BLOCKS_START; blk < (int) MAX_BLOCKS; ++blk) {
        vaddr_t addr = block_to_addr((cell_t) blk);
        unsigned char *p = (unsigned char *) vm_ptr(vm, addr);
        memset(p, 0, BLOCK_SIZE);
        dirty_blocks[blk] = 0;
    }
}

/* --- Words ------------------------------------------------------------ */

/* BLOCK ( u -- addr ) : address of block u (no dirty mark) */
void block_word_block(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (!block_in_range(blk)) {
        vm->error = 1;
        return;
    }

    vaddr_t addr = block_to_addr(blk);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);
    vm_push(vm, CELL(addr));
}

/* BUFFER ( u -- addr ) : address of block u and mark as dirty */
void block_word_buffer(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (!block_in_range(blk)) {
        vm->error = 1;
        return;
    }

    vaddr_t addr = block_to_addr(blk);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);
    dirty_blocks[blk] = 1;
    vm_push(vm, CELL(addr));
}

/* UPDATE ( -- ) : mark current SCR block as dirty */
void block_word_update(VM *vm) {
    cell_t blk = vm_load_cell(vm, vm->scr_addr);
    if (!block_in_range(blk)) {
        vm->error = 1;
        return;
    }
    dirty_blocks[blk] = 1;
}

/* SAVE-BUFFERS ( -- ) : write dirty buffers (no-op -> clear flags) */
void block_word_save_buffers(VM *vm) {
    save_all_buffers(vm);
}

/* EMPTY-BUFFERS ( -- ) : zero user blocks and clear dirty flags */
void block_word_empty_buffers(VM *vm) {
    empty_all_buffers(vm);
}

/* FLUSH ( -- ) : save all buffers and invalidate (no caching, just clear flags) */
void block_word_flush(VM *vm) {
    (void) vm;
    save_all_buffers(vm);
}

/* LOAD ( u -- ) : set SCR and (future: interpret block) */
void block_word_load(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (!block_in_range(blk)) {
        vm->error = 1;
        return;
    }

    vaddr_t addr = block_to_addr(blk);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);

    /* Future: interpret the text in this block. TIB integration comes later.
       For now, success with no output keeps tests happy. */
}

/* LIST ( u -- ) : set SCR and (optionally) print the block */
void block_word_list(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (!block_in_range(blk)) {
        vm->error = 1;
        return;
    }

    vaddr_t addr = block_to_addr(blk);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);

    /* Print a short preview to logs. Tests currently don’t diff output. */
    const uint8_t *p = vm_ptr(vm, addr);
    size_t preview = 0;
    char line[81];
    while (preview < BLOCK_SIZE && preview < 80 && p[preview] && p[preview] != '\n' && p[preview] != '\r') {
        line[preview] = (char) p[preview];
        preview++;
    }
    line[preview] = '\0';
    log_message(LOG_DEBUG, "LIST %ld: \"%s\"%s", (long) blk, line, (preview == 80 ? "..." : ""));
}

/* THRU ( u1 u2 -- ) : LOAD each block from u1..u2 inclusive */
void block_word_thru(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t u2 = vm_pop(vm);
    cell_t u1 = vm_pop(vm);

    if (u1 > u2) {
        cell_t t = u1;
        u1 = u2;
        u2 = t;
    }
    if (!block_in_range(u1) || !block_in_range(u2)) {
        vm->error = 1;
        return;
    }

    for (cell_t blk = u1; blk <= u2; ++blk) {
        vm_push(vm, blk);
        block_word_load(vm);
        if (vm->error) return;
    }
}

/* SCR ( -- addr ) : push VM address of SCR variable */
void block_word_scr(VM *vm) {
    vm_push(vm, CELL(vm->scr_addr));
}

/* --- Registration ----------------------------------------------------- */

void register_block_words(VM *vm) {
    register_word(vm, "BLOCK", block_word_block);
    register_word(vm, "BUFFER", block_word_buffer);
    register_word(vm, "UPDATE", block_word_update);
    register_word(vm, "SAVE-BUFFERS", block_word_save_buffers);
    register_word(vm, "EMPTY-BUFFERS", block_word_empty_buffers);
    register_word(vm, "FLUSH", block_word_flush);
    register_word(vm, "LOAD", block_word_load);
    register_word(vm, "LIST", block_word_list);
    register_word(vm, "THRU", block_word_thru);
    register_word(vm, "SCR", block_word_scr);
}