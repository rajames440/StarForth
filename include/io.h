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

#ifndef IO_H
#define IO_H

#include <stdint.h>
#include "vm.h"

/* BLOCK_SIZE is defined in vm.h */

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