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

/*
                                 ***   StarForth   ***
  block_words.c - FORTH-79 Block Words (Layer 3: Forth Interface)
 Last modified - 10/02/25, 03:55 PM ET
  Author: Robert A. James (rajames) - StarshipOS Forth Project.

  License: Creative Commons Zero v1.0 Universal
  <http://creativecommons.org/publicdomain/zero/1.0/>
*/

#include "include/block_words.h"
#include "../../include/word_registry.h"
#include "../../include/vm.h"
#include "../../include/block_subsystem.h"
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------------
 * Architecture:
 * - Layer 1: blkio (vtable abstraction)
 * - Layer 2: block_subsystem (RAM 0-1023, disk 1024+, 4KB packing)
 * - Layer 3: block_words (THIS FILE - Forth interface)
 *
 * All block I/O now goes through block_subsystem.h API.
 * Block 0 is RESERVED for volume metadata.
 * ---------------------------------------------------------------------- */

static void set_scr(VM *vm, cell_t blk) {
    if (!vm) return;
    vm_store_cell(vm, vm->scr_addr, (cell_t) blk);
}

/* Optional utility surface (matches your header) */
/**
 * @brief Initializes the block system
 * @param vm Pointer to the VM instance
 */
void init_block_system(VM *vm) {
    (void) vm;
    /* Subsystem initialization happens in main.c via blk_subsys_init() */
}

/**
 * @brief Gets a buffer for the specified block
 * @param vm Pointer to the VM instance
 * @param block_num Block number to retrieve
 * @return Pointer to block buffer or NULL if invalid
 */
unsigned char *get_block_buffer(VM *vm, int block_num) {
    if (!vm) return NULL;
    if (block_num == 0) return NULL; /* Block 0 reserved */

    uint8_t *buf = blk_get_buffer((uint32_t) block_num, 0); /* read-only */
    if (buf) {
        set_scr(vm, (cell_t) block_num);
    }
    return buf;
}

/**
 * @brief Gets an empty buffer for the specified block
 * @param vm Pointer to the VM instance
 * @param block_num Block number to create
 * @return Pointer to empty block buffer or NULL if invalid
 */
unsigned char *get_empty_buffer(VM *vm, int block_num) {
    if (!vm) return NULL;
    if (block_num == 0) return NULL; /* Block 0 reserved */

    uint8_t *buf = blk_get_empty_buffer((uint32_t) block_num);
    if (buf) {
        set_scr(vm, (cell_t) block_num);
    }
    return buf;
}

/**
 * @brief Marks the current block buffer as dirty
 * @param vm Pointer to the VM instance
 */
void mark_buffer_dirty(VM *vm) {
    if (!vm) return;
    cell_t blk = vm_load_cell(vm, vm->scr_addr);
    if (blk > 0) {
        blk_update((uint32_t) blk);
    }
}

/**
 * @brief Saves all dirty buffers
 * @param vm Pointer to the VM instance
 */
void save_all_buffers(VM *vm) {
    (void) vm;
    blk_flush(0); /* flush all */
}

/**
 * @brief Empties all user block buffers
 * @param vm Pointer to the VM instance
 */
void empty_all_buffers(VM *vm) {
    if (!vm) return;
    /* Zero blocks USER_BLOCKS_START through max available */
    uint32_t total = blk_get_total_blocks();
    for (uint32_t blk = USER_BLOCKS_START; blk < total; ++blk) {
        uint8_t *buf = blk_get_buffer(blk, 1); /* writable */
        if (buf) {
            memset(buf, 0, BLOCK_SIZE);
        }
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
    if (blk == 0 || !blk_is_valid((uint32_t) blk)) {
        vm->error = 1;
        return;
    }

    uint8_t *buf = blk_get_buffer((uint32_t) blk, 0);
    if (!buf) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);
    /* Return pointer as cell_t (blocks are external to VM memory now) */
    vm_push(vm, (cell_t)(uintptr_t)buf);
}

/* BUFFER ( u -- addr ) : address of block u and mark as dirty */
void block_word_buffer(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (blk == 0 || !blk_is_valid((uint32_t) blk)) {
        vm->error = 1;
        return;
    }

    uint8_t *buf = blk_get_buffer((uint32_t) blk, 1); /* writable */
    if (!buf) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);
    vm_push(vm, (cell_t)(uintptr_t)buf);
}

/* UPDATE ( -- ) : mark current SCR block as dirty */
void block_word_update(VM *vm) {
    cell_t blk = vm_load_cell(vm, vm->scr_addr);
    if (blk == 0 || !blk_is_valid((uint32_t) blk)) {
        vm->error = 1;
        return;
    }
    blk_update((uint32_t) blk);
}

/* SAVE-BUFFERS ( -- ) : write dirty buffers */
void block_word_save_buffers(VM *vm) {
    save_all_buffers(vm);
}

/* EMPTY-BUFFERS ( -- ) : zero user blocks and clear dirty flags */
void block_word_empty_buffers(VM *vm) {
    empty_all_buffers(vm);
}

/* FLUSH ( -- ) : save all buffers and invalidate */
void block_word_flush(VM *vm) {
    (void) vm;
    blk_flush(0);
}

/* LOAD ( u -- ) : set SCR and (future: interpret block) */
void block_word_load(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (blk == 0 || !blk_is_valid((uint32_t) blk)) {
        vm->error = 1;
        return;
    }

    uint8_t *buf = blk_get_buffer((uint32_t) blk, 0);
    if (!buf) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);

    /* Interpret the block content as Forth source */
    /* Block is 1024 bytes, null-terminate it for interpretation */
    char block_text[1025];
    memcpy(block_text, buf, 1024);
    block_text[1024] = '\0';

    /* Interpret the block content */
    vm_interpret(vm, block_text);
}

/* LIST ( u -- ) : set SCR and (optionally) print the block */
void block_word_list(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t blk = vm_pop(vm);
    if (blk == 0 || !blk_is_valid((uint32_t) blk)) {
        vm->error = 1;
        return;
    }

    uint8_t *buf = blk_get_buffer((uint32_t) blk, 0);
    if (!buf) {
        vm->error = 1;
        return;
    }

    set_scr(vm, blk);

    /* Print formatted block output with line numbers */
    printf("\nBlock %ld\n", (long) blk);

    /* FORTH blocks are typically 16 lines of 64 characters */
    for (int line = 0; line < 16; line++) {
        printf("%02d: ", line);
        for (int col = 0; col < 64; col++) {
            int idx = line * 64 + col;
            char ch = (char) buf[idx];
            /* Print printable characters, show spaces as-is */
            if (ch >= 32 && ch < 127) {
                putchar(ch);
            } else if (ch == 0) {
                /* Null bytes shown as spaces for readability */
                putchar(' ');
            } else {
                /* Non-printable shown as '.' */
                putchar('.');
            }
        }
        printf("\n");
    }
    printf("\n");
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
    if (u1 == 0 || !blk_is_valid((uint32_t) u1) || !blk_is_valid((uint32_t) u2)) {
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

/* --> ( -- ) : continue interpretation on next sequential block */
void block_word_next_block(VM *vm) {
    cell_t current_scr = vm_load_cell(vm, vm->scr_addr);
    cell_t next_blk = current_scr + 1;

    if (next_blk == 0 || !blk_is_valid((uint32_t) next_blk)) {
        vm->error = 1;
        return;
    }

    /* Get the next block's content */
    uint8_t *buf = blk_get_buffer((uint32_t) next_blk, 0);
    if (!buf) {
        vm->error = 1;
        return;
    }

    /* Update SCR to next block */
    set_scr(vm, next_blk);

    /* Interpret the next block's content inline */
    char block_text[1025];
    memcpy(block_text, buf, 1024);
    block_text[1024] = '\0';

    /* Recursively interpret - this allows definitions to span blocks */
    vm_interpret(vm, block_text);
}

/* --- Registration ----------------------------------------------------- */

/**
 * @brief Registers all block-related FORTH words
 * @param vm Pointer to the VM instance
 */
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
    register_word(vm, "-->", block_word_next_block);
}