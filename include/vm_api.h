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
 * @file vm_api.h
 * @brief StarForth Virtual Machine Public API
 *
 * This header provides the public interface for interacting with the StarForth
 * Virtual Machine, particularly focusing on input handling and memory management.
 */

#ifndef VM_API_H
#define VM_API_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup vm_input VM Input Operations
 * @{
 * Public API for managing VM input operations including Terminal Input Buffer (TIB),
 * input pointer (>IN), and input span (SPAN) handling.
 */
/**
 * @brief Ensures input buffer initialization
 * @param vm Pointer to VM instance
 * @return 0 on success, non-zero on failure
 *
 * Allocates and initializes input buffer if not already done.
 */
int vm_input_ensure(VM * vm);

/**
 * @brief Retrieves Terminal Input Buffer address
 * @param vm Pointer to VM instance
 * @return Pointer to TIB buffer
 */
unsigned char *vm_input_tib(VM * vm);

/**
 * @brief Retrieves input pointer address
 * @param vm Pointer to VM instance
 * @return Pointer to >IN cell
 */
cell_t *vm_input_in(VM * vm);

/**
 * @brief Retrieves span cell address
 * @param vm Pointer to VM instance
 * @return Pointer to SPAN cell
 */
cell_t *vm_input_span(VM * vm);

/**
 * @brief Loads a line into TIB
 * @param vm Pointer to VM instance
 * @param src Source string to copy
 * @param n Number of bytes to copy
 *
 * Copies data into TIB, updates SPAN, and resets >IN.
 */
void vm_input_load_line(VM *vm, const char *src, size_t n);

/**
 * @brief Gets current input source
 * @param vm Pointer to VM instance
 * @param out_addr Output parameter for source address
 * @param out_len Output parameter for source length
 *
 * Returns SOURCE pair (address, length) for current input.
 */
void vm_input_source(VM * vm, cell_t * out_addr, cell_t * out_len);

/**
 * @brief Converts host pointer to Forth address
 * @param vm Pointer to VM instance
 * @param p Host memory pointer
 * @return Forth memory address (offset into vm->memory)
 */
cell_t vm_addr_from_ptr(VM *vm, void *p);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_API_H */