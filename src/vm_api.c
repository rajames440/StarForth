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

#include "../include/vm_api.h"
#include "../include/vm.h"       // internal details, not re-exposed to word modules
#include <string.h>

/** @name Stack Operations
 * @{
 */
/** @brief Push value onto data stack
 * @param vm VM instance
 * @param v Value to push
 */
void vm_api_push(VM *vm, intptr_t v) { vm_push(vm, v); }

/** @brief Pop value from data stack
 * @param vm VM instance
 * @return Popped value
 */
intptr_t vm_api_pop(VM *vm) { return vm_pop(vm); }

/** @brief Push value onto return stack
 * @param vm VM instance
 * @param v Value to push
 */
void vm_api_rpush(VM *vm, intptr_t v) { vm_rpush(vm, v); }

/** @brief Pop value from return stack
 * @param vm VM instance
 * @return Popped value
 */
intptr_t vm_api_rpop(VM *vm) { return vm_rpop(vm); }
/** @} */

/** @name Dictionary Operations
 * @{
 */
/** @brief Create new word in dictionary
 * @param vm VM instance
 * @param n Word name
 * @param len Name length
 * @param fn Word implementation function
 * @return Pointer to created dictionary entry
 */
void *vm_api_create_word(VM *vm, const char *n, size_t len, word_func_t fn) {
    return (void *) vm_create_word(vm, n, len, (void(*)(struct VM *)) fn);
}

/** @brief Find word in dictionary
 * @param vm VM instance
 * @param n Word name to find
 * @param len Name length
 * @return Pointer to found dictionary entry or NULL
 */
void *vm_api_find_word(VM *vm, const char *n, size_t len) {
    return (void *) vm_find_word(vm, n, len);
}

/** @brief Hide last defined word
 * @param vm VM instance
 */
void vm_api_hide_word(VM *vm) { vm_hide_word(vm); }

/** @brief Toggle smudge bit of last word
 * @param vm VM instance
 */
void vm_api_smudge_word(VM *vm) { vm_smudge_word(vm); }
/** @} */

/** @brief Ensure input buffers are allocated
 * @param vm VM instance
 * @return 0 on success, 1 on error
 * @details Allocates TIB buffer and input variables if not already allocated
 */
int vm_input_ensure(VM *vm) {
    if (!vm) return 1;
    if (!vm->tib_buf) {
        vm->tib_buf = (unsigned char *) vm_allot(vm, INPUT_BUFFER_SIZE);
        if (!vm->tib_buf) {
            vm->error = 1;
            return 1;
        }
        vm->tib_cap = INPUT_BUFFER_SIZE;
        memset(vm->tib_buf, 0, vm->tib_cap);
    }
    if (!vm->in_var) {
        vm->in_var = (cell_t *) vm_allot(vm, sizeof(cell_t));
        if (!vm->in_var) {
            vm->error = 1;
            return 1;
        }
        *vm->in_var = 0;
    }
    if (!vm->span_var) {
        vm->span_var = (cell_t *) vm_allot(vm, sizeof(cell_t));
        if (!vm->span_var) {
            vm->error = 1;
            return 1;
        }
        *vm->span_var = 0;
    }
    return 0;
}

/** @brief Get pointer to terminal input buffer (TIB)
 * @param vm VM instance
 * @return Pointer to TIB or NULL on error
 * @details Returns pointer to VM's terminal input buffer after ensuring it exists
 */
unsigned char *vm_input_tib(VM *vm) {
    if (vm_input_ensure(vm)) return NULL;
    return vm->tib_buf; /* VM address */
}

/** @brief Get pointer to input offset variable
 * @param vm VM instance
 * @return Pointer to input offset or NULL on error
 * @details Returns pointer to VM's input offset variable after ensuring it exists
 */
cell_t *vm_input_in(VM *vm) {
    if (vm_input_ensure(vm)) return NULL;
    return vm->in_var; /* VM address */
}

/** @brief Get pointer to input span variable
 * @param vm VM instance
 * @return Pointer to span variable or NULL on error
 * @details Returns pointer to VM's input span variable after ensuring it exists
 */
cell_t *vm_input_span(VM *vm) {
    if (vm_input_ensure(vm)) return NULL;
    return vm->span_var; /* VM address */
}

/** @brief Load input line into TIB
 * @param vm VM instance
 * @param src Source string to load
 * @param n Number of characters to load
 * @details Loads up to n characters from src into TIB, zero-pads remainder
 *          Resets input offset and updates span. Handles NULL src gracefully.
 */
void vm_input_load_line(VM *vm, const char *src, size_t n) {
    if (vm_input_ensure(vm)) return;
    if (!src) {
        *vm->span_var = 0;
        *vm->in_var = 0;
        return;
    }
    size_t nclamp = n;
    if (nclamp > vm->tib_cap) nclamp = vm->tib_cap;
    memcpy(vm->tib_buf, src, nclamp);
    if (nclamp < vm->tib_cap) memset(vm->tib_buf + nclamp, 0, vm->tib_cap - nclamp);
    *vm->span_var = (cell_t) nclamp;
    *vm->in_var = 0;
}

/** @brief Get current input source buffer info
 * @param vm VM instance
 * @param out_addr Output parameter for buffer address
 * @param out_len Output parameter for buffer length
 * @details Returns current input buffer address and length through parameters
 *          Both parameters are optional (can be NULL)
 */
void vm_input_source(VM *vm, cell_t *out_addr, cell_t *out_len) {
    if (vm_input_ensure(vm)) {
        if (out_addr) *out_addr = 0;
        if (out_len) *out_len = 0;
        return;
    }
    if (out_addr) *out_addr = (cell_t)(uintptr_t)
    vm->tib_buf;
    if (out_len) *out_len = (*vm->span_var < 0) ? 0 : *vm->span_var;
}

/** @name Memory Operations
 * @{
 */
/** @brief Convert native pointer to Forth memory address
 * @param vm VM instance
 * @param p Native pointer to convert
 * @return Forth memory address (offset from VM memory base)
 * @details Converts a native C pointer to a Forth memory address by calculating
 *          offset from VM memory base. Returns 0 for invalid pointers.
 */
cell_t vm_addr_from_ptr(VM *vm, void *p) {
    if (!vm || !p) return 0;
#if defined(VM_STRICT_PTR) && VM_STRICT_PTR
    /* Forth addresses are real pointers */
    return (cell_t)(uintptr_t)p;
#else
    /* Forth addresses are offsets into vm->memory */
    if (!vm->memory) return 0;
    unsigned long base = (unsigned long) vm->memory;
    unsigned long addr = (unsigned long) p;
    if (addr < base) return 0;
    return (cell_t)(addr - base);
#endif
}

/** @} */