/*

                                 ***   StarForth   ***
  block_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* FORTH-79 Block & Buffer Words:
 * BLOCK   ( u -- addr )          Get block buffer address
 * BUFFER  ( u -- addr )          Get empty buffer for block
 * UPDATE  ( -- )                 Mark current block as modified
 * SAVE-BUFFERS ( -- )           Save all modified blocks
 * EMPTY-BUFFERS ( -- )          Mark all buffers as empty
 * FLUSH   ( -- )                Save buffers and empty them
 * LIST    ( u -- )              List block contents
 * LOAD    ( u -- )              Load and interpret block
 * SCR     ( -- addr )           Variable: screen/block being listed
 * THRU    ( u1 u2 -- )          Load blocks from u1 to u2
 * --> or  → ( -- )              Continue to next block
 */

/* block_words.c - FORTH-79 Block & Buffer System */
#include "include/block_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <string.h>
#include <stdio.h>

/* Block system global state */
static BlockBuffer buffers[BUFFER_COUNT];
static int current_buffer = -1;         /* Currently active buffer */
static int access_counter = 0;          /* LRU counter */
static cell_t scr_var = 0;             /* SCR variable - current screen */
static int block_system_initialized = 0;

/* Initialize block system */
void init_block_system(VM *vm) {
    if (block_system_initialized) return;

    /* Clear all buffers */
    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(&buffers[i], 0, sizeof(BlockBuffer));
        buffers[i].block_number = -1;  /* Empty */
        buffers[i].dirty = 0;
        buffers[i].accessed = 0;
    }

    current_buffer = -1;
    access_counter = 0;
    scr_var = 0;
    block_system_initialized = 1;

    log_message(LOG_INFO, "Block system initialized: %d buffers, %d max blocks",
                BUFFER_COUNT, MAX_BLOCKS);
}

static int validate_block_num(VM *vm, int block_num) {
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        log_message(LOG_ERROR, "BLOCK: Invalid block number %d", block_num);
        vm->error = 1;
        return 0;  // Validation failed
    }
    return 1;  // Validation passed
}

/* Find buffer containing block, or -1 if not loaded */
static int find_buffer_with_block(int block_num) {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (buffers[i].block_number == block_num) {
            return i;
        }
    }
    return -1;
}

/* Find least recently used buffer */
static int find_lru_buffer(void) {
    int lru_index = 0;
    int min_access = buffers[0].accessed;

    for (int i = 1; i < BUFFER_COUNT; i++) {
        if (buffers[i].accessed < min_access) {
            min_access = buffers[i].accessed;
            lru_index = i;
        }
    }

    return lru_index;
}

/* Save buffer to persistent storage (simulated) */
static void save_buffer(int buffer_index) {
    BlockBuffer *buf = &buffers[buffer_index];

    if (!buf->dirty || buf->block_number < 0) {
        return;  /* Nothing to save */
    }

    /* In real implementation, this would write to disk/flash */
    log_message(LOG_DEBUG, "BLOCK: Saved block %d from buffer %d",
                buf->block_number, buffer_index);

    buf->dirty = 0;  /* Mark as clean */
}

/* Load block from persistent storage (simulated) */
static void load_block_into_buffer(int buffer_index, int block_num) {
    BlockBuffer *buf = &buffers[buffer_index];

    /* Clear buffer */
    memset(buf->data, 0, BLOCK_SIZE);

    /* In real implementation, this would read from disk/flash */
    /* For now, fill with placeholder content */
    snprintf((char*)buf->data, BLOCK_SIZE,
             "BLOCK %d CONTENT\n"
             "This is block %d - 1KB of data\n"
             "Lines 16 chars each for compatibility\n"
             "\n", block_num, block_num);

    buf->block_number = block_num;
    buf->dirty = 0;
    buf->accessed = ++access_counter;

    log_message(LOG_DEBUG, "BLOCK: Loaded block %d into buffer %d",
                block_num, buffer_index);
}

/* Get block buffer address */
unsigned char* get_block_buffer(VM *vm, int block_num) {
    if (!validate_block_num(vm, block_num)) return NULL;

    /* Check if block is already loaded */
    int buffer_index = find_buffer_with_block(block_num);

    if (buffer_index >= 0) {
        /* Block found - update access time */
        buffers[buffer_index].accessed = ++access_counter;
        current_buffer = buffer_index;
        return buffers[buffer_index].data;
    }

    /* Block not loaded - find buffer to use */
    buffer_index = find_lru_buffer();

    /* Save existing buffer if dirty */
    save_buffer(buffer_index);

    /* Load new block */
    load_block_into_buffer(buffer_index, block_num);
    current_buffer = buffer_index;

    return buffers[buffer_index].data;
}

/* Get empty buffer for block */
unsigned char* get_empty_buffer(VM *vm, int block_num) {
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        log_message(LOG_ERROR, "BUFFER: Invalid block number %d", block_num);
        vm->error = 1;
        return NULL;
    }

    /* Find LRU buffer */
    int buffer_index = find_lru_buffer();

    /* Save existing buffer if dirty */
    save_buffer(buffer_index);

    /* Clear buffer and assign block number */
    memset(buffers[buffer_index].data, 0, BLOCK_SIZE);
    buffers[buffer_index].block_number = block_num;
    buffers[buffer_index].dirty = 0;
    buffers[buffer_index].accessed = ++access_counter;
    current_buffer = buffer_index;

    log_message(LOG_DEBUG, "BUFFER: Assigned empty buffer %d to block %d",
                buffer_index, block_num);

    return buffers[buffer_index].data;
}

/* Mark current buffer as dirty */
void mark_buffer_dirty(VM *vm) {
    if (current_buffer >= 0 && current_buffer < BUFFER_COUNT) {
        buffers[current_buffer].dirty = 1;
        log_message(LOG_DEBUG, "UPDATE: Marked buffer %d (block %d) as dirty",
                    current_buffer, buffers[current_buffer].block_number);
    }
}

/* Save all dirty buffers */
void save_all_buffers(VM *vm) {
    int saved_count = 0;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (buffers[i].dirty && buffers[i].block_number >= 0) {
            save_buffer(i);
            saved_count++;
        }
    }

    log_message(LOG_DEBUG, "SAVE-BUFFERS: Saved %d dirty buffers", saved_count);
}

/* Empty all buffers */
void empty_all_buffers(VM *vm) {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        buffers[i].block_number = -1;
        buffers[i].dirty = 0;
        buffers[i].accessed = 0;
    }
    current_buffer = -1;

    log_message(LOG_DEBUG, "EMPTY-BUFFERS: All buffers emptied");
}

/* FORTH-79 Block Words Implementation */

/* BLOCK ( u -- addr )  Get block buffer address */
/* BLOCK ( u -- addr )
 * Return the VM address (byte offset) of block u.
 * Semantics: addr = u * BLOCK_SIZE; no host pointers, no caching here.
 */
void block_word_block(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }

    cell_t u = vm_pop(vm);
    if (u < 0 || (uint64_t)u >= (uint64_t)MAX_BLOCKS) {
        vm->error = 1;
        return;
    }

    vaddr_t off = (vaddr_t)((uint64_t)u * (uint64_t)BLOCK_SIZE);
    if (!vm_addr_ok(vm, off, BLOCK_SIZE)) {   /* belt & suspenders */
        vm->error = 1;
        return;
    }

    /* Push the VM offset (byte address) as a cell */
    vm_push(vm, CELL(off));
}

/* BUFFER ( u -- addr )  Get empty buffer for block */
/* BUFFER ( u -- addr )
 * Return the VM address (byte offset) of block u's buffer.
 * In our unified-memory model, this is identical to BLOCK: u * BLOCK_SIZE.
 * (No implicit I/O here; UPDATE/FLUSH will handle dirtying later.)
 */
void block_word_buffer(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }

    cell_t u = vm_pop(vm);
    if (u < 0 || (uint64_t)u >= (uint64_t)MAX_BLOCKS) {
        vm->error = 1;
        return;
    }

    vaddr_t off = (vaddr_t)((uint64_t)u * (uint64_t)BLOCK_SIZE);
    if (!vm_addr_ok(vm, off, BLOCK_SIZE)) {
        vm->error = 1;
        return;
    }

    vm_push(vm, CELL(off));
}

/* UPDATE ( -- )  Mark current block as modified */
void block_word_update(VM *vm) {
    init_block_system(vm);
    mark_buffer_dirty(vm);
}

/* SAVE-BUFFERS ( -- )  Save all modified blocks */
void block_word_save_buffers(VM *vm) {
    init_block_system(vm);
    save_all_buffers(vm);
}

/* EMPTY-BUFFERS ( -- )  Mark all buffers as empty */
void block_word_empty_buffers(VM *vm) {
    init_block_system(vm);
    empty_all_buffers(vm);
}

/* FLUSH ( -- )  Save buffers and empty them */
void block_word_flush(VM *vm) {
    init_block_system(vm);
    save_all_buffers(vm);
    empty_all_buffers(vm);
}

/* LIST ( u -- )  List block contents */
/* LIST ( u -- )
 * Print block u (16 lines × 64 chars) without using host pointers.
 * Side effect: SCR := u (matches LOAD behavior).
 */
void block_word_list(VM *vm) {
    /* Need a block number on the stack */
    if (vm->dsp < 0) { vm->error = 1; return; }

    cell_t u = vm_pop(vm);
    if (u < 0 || (uint64_t)u >= (uint64_t)MAX_BLOCKS) {
        vm->error = 1;
        return;
    }

    /* SCR := u */
    vm_store_cell(vm, vm->scr_addr, u);

    /* Start of block as VM byte offset */
    vaddr_t off = (vaddr_t)((uint64_t)u * (uint64_t)BLOCK_SIZE);
    if (!vm_addr_ok(vm, off, BLOCK_SIZE)) { vm->error = 1; return; }

    /* Header */
    printf("\n-- BLOCK %lld --\n", (long long)u);

    /* Print 16 lines, right-trim spaces/NULs/CR/LF */
    for (int i = 0; i < 16; i++) {
        vaddr_t line_off = off + (vaddr_t)(i * 64);
        if (!vm_addr_ok(vm, line_off, 64)) { vm->error = 1; return; }

        char line[64 + 1];
        for (int j = 0; j < 64; j++) {
            line[j] = (char)vm_load_u8(vm, line_off + (vaddr_t)j);
        }
        line[64] = '\0';

        /* trim */
        int len = 64;
        while (len > 0) {
            char c = line[len - 1];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0') len--;
            else break;
        }
        line[len] = '\0';

        /* Render control chars as spaces (defensive) */
        for (int j = 0; j < len; j++) {
            unsigned char ch = (unsigned char)line[j];
            if (ch < 32) line[j] = ' ';
        }

        printf("%02d: %.*s\n", i, len, line);
    }
    fflush(stdout);
}

/* LOAD ( u -- )  Load and interpret block */
/* LOAD ( u -- )
 * Set SCR to u, then interpret the 16×64-byte lines from block u.
 * Uses VM offsets only (no host pointers).
 */
void block_word_load(VM *vm) {
    /* Need a block number on the stack */
    if (vm->dsp < 0) { vm->error = 1; return; }

    cell_t u = vm_pop(vm);
    if (u < 0 || (uint64_t)u >= (uint64_t)MAX_BLOCKS) {
        vm->error = 1;
        return;
    }

    /* SCR := u */
    vm_store_cell(vm, vm->scr_addr, u);

    /* Start of block as VM byte offset */
    vaddr_t off = (vaddr_t)((uint64_t)u * (uint64_t)BLOCK_SIZE);
    if (!vm_addr_ok(vm, off, BLOCK_SIZE)) { vm->error = 1; return; }

    /* Forth-79 blocks are 16 lines × 64 chars, no guaranteed newlines.
       We read each 64-char field, trim right spaces/NULs, and interpret. */
    for (int i = 0; i < 16; i++) {
        vaddr_t line_off = off + (vaddr_t)(i * 64);
        if (!vm_addr_ok(vm, line_off, 64)) { vm->error = 1; return; }

        /* Copy 64 bytes to a local buffer (avoid new includes) */
        char line[64 + 1];
        for (int j = 0; j < 64; j++) {
            line[j] = (char)vm_load_u8(vm, line_off + (vaddr_t)j);
        }
        line[64] = '\0';

        /* Trim trailing spaces/NULs/CR/LF */
        int len = 64;
        while (len > 0) {
            char c = line[len - 1];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0') {
                len--;
            } else {
                break;
            }
        }
        line[len] = '\0';

        /* Skip blank lines and line-comments starting with '\' */
        if (len == 0) continue;
        if (line[0] == '\\') continue;

        /* Interpret this line */
        vm_interpret(vm, line);
        if (vm->halted || vm->error) break;
    }
}

/* SCR ( -- addr )  Variable: screen/block being listed */
void block_word_scr(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&scr_var);
}

/* THRU ( u1 u2 -- )  Load blocks from u1 to u2 */
/* THRU ( u1 u2 -- )
 * LOAD each block from u1 through u2 inclusive (order-agnostic).
 * Uses VM offsets only; relies on block_word_load to do the work.
 */
void block_word_thru(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }

    cell_t u2 = vm_pop(vm);
    cell_t u1 = vm_pop(vm);

    /* normalize order */
    if (u1 > u2) { cell_t t = u1; u1 = u2; u2 = t; }

    if (u1 < 0 || (uint64_t)u2 >= (uint64_t)MAX_BLOCKS) {
        vm->error = 1;
        return;
    }

    for (cell_t u = u1; u <= u2; ++u) {
        /* push and LOAD */
        vm_push(vm, u);
        block_word_load(vm);
        if (vm->halted || vm->error) break;
    }
}

/* --> ( -- )  Continue to next block (arrow) */
/* --> ( -- )   Advance to next screen and load it.
 * Uses the VM-backed SCR variable (vm->scr_addr).
 */
void block_word_arrow(VM *vm) {
    /* SCR := SCR + 1 */
    cell_t cur = vm_load_cell(vm, vm->scr_addr);
    cur += 1;
    vm_store_cell(vm, vm->scr_addr, cur);

    /* Leave the new block number on the stack (common practice) */
    vm_push(vm, cur);

    /* Load/list the new block if your implementation expects it */
    block_word_load(vm);
}

/* FORTH-79 Block & Buffer Word Registration */
void register_block_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 Block & Buffer words...");

    /* Register all block & buffer words */
    register_word(vm, "BLOCK", block_word_block);
    register_word(vm, "BUFFER", block_word_buffer);
    register_word(vm, "UPDATE", block_word_update);
    register_word(vm, "SAVE-BUFFERS", block_word_save_buffers);
    register_word(vm, "EMPTY-BUFFERS", block_word_empty_buffers);
    register_word(vm, "FLUSH", block_word_flush);
    register_word(vm, "LIST", block_word_list);
    register_word(vm, "LOAD", block_word_load);
    register_word(vm, "SCR", block_word_scr);
    register_word(vm, "THRU", block_word_thru);
    register_word(vm, "-->", block_word_arrow);

    /* Initialize block system */
    init_block_system(vm);

    log_message(LOG_INFO, "Block & Buffer system registered (%d buffers, %d max blocks)",
                BUFFER_COUNT, MAX_BLOCKS);
}
