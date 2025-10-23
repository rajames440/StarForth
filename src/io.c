/*
                                  ***   StarForth   ***

  io.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.411-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/io.c
 */

#include "io.h"
#include "log.h"

#include <string.h>

/** @} */

/**
 * @brief Initialize the IO subsystem
 * @param vm Pointer to VM instance
 *
 * Initializes the block device view over the unified VM memory.
 * Zeros out the entire VM memory space.
 */
void io_init(VM *vm) {
    if (!vm || !vm->memory) {
        log_message(LOG_ERROR, "io_init: VM or VM memory not initialized");
        return;
    }

    /* Zero the entire VM address space so blocks start clean. */
    memset(vm->memory, 0, VM_MEMORY_SIZE);

    log_message(LOG_INFO,
                "IO subsystem initialized with %d blocks of %d bytes",
                (int) MAX_BLOCKS, (int) BLOCK_SIZE);
}

/**
 * @brief Write a block to VM memory
 * @param vm Pointer to VM instance
 * @param block_num Block number to write (0 to MAX_BLOCKS-1)
 * @param buffer Source buffer containing block data
 * @return 0 on success, -1 on error
 */
/**
 * @brief Write a block to VM memory
 * @param vm Pointer to VM instance
 * @param block_num Block number to write (0 to MAX_BLOCKS-1)
 * @param buffer Source buffer containing block data
 * @return 0 on success, -1 on error
 *
 * Writes a BLOCK_SIZE chunk of data from the provided buffer into the VM's memory
 * at the specified block number offset. The block number must be within valid range.
 * The entire block is written atomically.
 *
 * @note The buffer must contain at least BLOCK_SIZE bytes of valid data
 */
int io_write_block(VM *vm, int block_num, const unsigned char *buffer) {
    if (!vm || !vm->memory || !buffer) {
        log_message(LOG_ERROR, "io_write_block: bad args");
        return -1;
    }
    if (block_num < 0 || block_num >= (int) MAX_BLOCKS) {
        log_message(LOG_ERROR, "io_write_block: invalid block number %d", block_num);
        return -1;
    }
    size_t off = (size_t) block_num * (size_t) BLOCK_SIZE;
    memcpy(&vm->memory[off], buffer, BLOCK_SIZE);
    log_message(LOG_DEBUG, "io_write_block: wrote block %d", block_num);
    return 0;
}