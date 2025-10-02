/*

                                 ***   StarForth   ***
  io.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef IO_H
#define IO_H

#include <stdint.h>
#include "vm.h"

/** @brief Size of a single block in bytes */
#define BLOCK_SIZE 1024
/** @brief Total number of blocks available */
#define BLOCK_COUNT 1024
/** @brief Total memory size in bytes */
#define MEMORY_SIZE (BLOCK_SIZE * BLOCK_COUNT)

/**
 * @brief Initialize the I/O subsystem
 * @param vm Pointer to the virtual machine instance
 */
void io_init(VM * vm);

/**
 * @brief Read a block from storage
 * @param vm Pointer to the virtual machine instance
 * @param block_num Block number to read
 * @param buffer Buffer to store the read data
 * @return 0 on success, non-zero on failure
 */
int io_read_block(VM *vm, int block_num, unsigned char *buffer);

/**
 * @brief Write a block to storage
 * @param vm Pointer to the virtual machine instance
 * @param block_num Block number to write
 * @param buffer Buffer containing data to write
 * @return 0 on success, non-zero on failure
 */
int io_write_block(VM *vm, int block_num, const unsigned char *buffer);

#endif /* IO_H */