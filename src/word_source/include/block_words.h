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

/**
 * @file block_words.h
 * @brief FORTH-79 Block and Buffer System Implementation
 *
 * This file implements the standard FORTH-79 block and buffer words:
 * @li BLOCK   ( u -- addr )          Get block buffer address
 * @li BUFFER  ( u -- addr )          Get empty buffer for block
 * @li UPDATE  ( -- )                 Mark current block as modified
 * @li SAVE-BUFFERS ( -- )           Save all modified blocks
 * @li EMPTY-BUFFERS ( -- )          Mark all buffers as empty
 * @li FLUSH   ( -- )                Save buffers and empty them
 * @li LIST    ( u -- )              List block contents
 * @li LOAD    ( u -- )              Load and interpret block
 * @li SCR     ( -- addr )           Variable: screen/block being listed
 * @li THRU    ( u1 u2 -- )          Load blocks from u1 to u2
 * @li --> or  → ( -- )              Continue to next block
 */

#ifndef BLOCK_WORDS_H
#define BLOCK_WORDS_H

#include "vm.h"

/** @name Block System Configuration
 * @{
 */
/** @brief Size of each block in bytes (1KB) */
#define BLOCK_SIZE 1024
/** @brief Maximum number of blocks (1024 blocks from 1MB memory) */
#define MAX_BLOCKS (VM_MEMORY_SIZE / BLOCK_SIZE)
/** @brief Number of block buffers in memory */
#define BUFFER_COUNT 4
/** @} */


/** 
 * @brief Block buffer types for different storage mechanisms
 */
typedef enum {
    BUFFER_TYPE_MEMORY, /**< Direct memory access (current implementation) */
    BUFFER_TYPE_HARDWARE, /**< Future: Hardware register blocks */
    BUFFER_TYPE_STORAGE, /**< Future: Persistent storage blocks */
    BUFFER_TYPE_NETWORK /**< Future: Network packet buffers */
} buffer_type_t;

/**
 * @brief Block buffer structure maintaining buffer state
 */
typedef struct {
    unsigned char data[BLOCK_SIZE]; /**< Block content buffer */
    int block_number; /**< Currently loaded block number (-1 = empty) */
    int dirty; /**< Modified flag for write-back */
    int accessed; /**< LRU counter for buffer management */
    buffer_type_t type; /**< Buffer type for storage mechanism */
    uintptr_t hw_address; /**< Future: Hardware backing address */
} BlockBuffer;

/**
 * @brief Registers all block and buffer words with the virtual machine
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_block_words(VM * vm);

/**
 * @brief Initializes the block system
 * @param vm Pointer to the Forth virtual machine instance
 */
void init_block_system(VM * vm);

/**
 * @brief Gets a buffer containing the specified block
 * @param vm Pointer to the Forth virtual machine instance
 * @param block_num Block number to retrieve
 * @return Pointer to the block buffer data
 */
unsigned char *get_block_buffer(VM *vm, int block_num);

/**
 * @brief Gets an empty buffer for the specified block
 * @param vm Pointer to the Forth virtual machine instance
 * @param block_num Block number to allocate
 * @return Pointer to the empty buffer data
 */
unsigned char *get_empty_buffer(VM *vm, int block_num);

/**
 * @brief Marks the current buffer as modified
 * @param vm Pointer to the Forth virtual machine instance
 */
void mark_buffer_dirty(VM * vm);

/**
 * @brief Saves all modified buffers to storage
 * @param vm Pointer to the Forth virtual machine instance
 */
void save_all_buffers(VM * vm);

/**
 * @brief Marks all buffers as empty
 * @param vm Pointer to the Forth virtual machine instance
 */
void empty_all_buffers(VM * vm);

#endif /* BLOCK_WORDS_H */