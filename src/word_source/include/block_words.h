/*

                                 ***   StarForth   ***
  block_words.h - FORTH-79 Standard and ANSI C99 ONLY
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

#ifndef BLOCK_WORDS_H
#define BLOCK_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Block & Buffer System Configuration */
#define BLOCK_SIZE 1024                                 /* 1KB per block */
#define MAX_BLOCKS (VM_MEMORY_SIZE / BLOCK_SIZE)        /* 1024 blocks from 1MB */
#define BUFFER_COUNT 4                                  /* Number of block buffers */


/* Block buffer types for future hardware support */
typedef enum {
    BUFFER_TYPE_MEMORY, /* Direct memory access (current) */
    BUFFER_TYPE_HARDWARE, /* Future: Hardware register blocks */
    BUFFER_TYPE_STORAGE, /* Future: Persistent storage blocks */
    BUFFER_TYPE_NETWORK /* Future: Network packet buffers */
} buffer_type_t;

/* Enhanced block system state */
typedef struct {
    unsigned char data[BLOCK_SIZE]; /* Block content */
    int block_number; /* Which block is loaded (-1 = empty) */
    int dirty; /* Modified flag */
    int accessed; /* LRU counter */
    buffer_type_t type; /* Buffer type for future hardware support */
    uintptr_t hw_address; /* Future: Hardware backing address */
} BlockBuffer;

void register_block_words(VM * vm);

/* Block system initialization */
void init_block_system(VM * vm);

/* Block management functions */
unsigned char *get_block_buffer(VM *vm, int block_num);

unsigned char *get_empty_buffer(VM *vm, int block_num);

void mark_buffer_dirty(VM * vm);
void save_all_buffers(VM * vm);
void empty_all_buffers(VM * vm);

#endif /* BLOCK_WORDS_H */