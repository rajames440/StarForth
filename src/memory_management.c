/*

                                 ***   StarForth   ***
  memory_management.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "vm.h"
#include "log.h"

/**
 * @brief Allocates memory from the VM's dictionary space
 * @param vm Pointer to the VM instance
 * @param bytes Number of bytes to allocate
 * @return Pointer to the allocated memory or NULL if allocation fails
 * @note The allocation is done from the VM's dictionary space which is limited to DICTIONARY_MEMORY_SIZE
 */
void* vm_allot(VM *vm, size_t bytes) {
    /* Ensure we don't allocate beyond dictionary space (first 64 blocks) */
    if (vm->here + bytes >= DICTIONARY_MEMORY_SIZE) {
        log_message(LOG_ERROR, "Dictionary space full (here=%zu, bytes=%zu, dict_limit=%d)",
                    vm->here, bytes, DICTIONARY_MEMORY_SIZE);
        vm->error = 1;
        return NULL;
    }

    void *ptr = vm->memory + vm->here;
    vm->here += bytes;

    /* Log which block we're using */
    int current_block = vm->here / BLOCK_SIZE;
    log_message(LOG_DEBUG, "ALLOT: Allocated %zu bytes at offset %zu (block %d)",
                bytes, vm->here - bytes, current_block);

    return ptr;
}

/**
 * @brief Aligns the VM's dictionary pointer (here) to the cell boundary
 * @param vm Pointer to the VM instance
 * @note Adds padding bytes if necessary to ensure proper alignment for cell_t type
 */
void vm_align(VM *vm) {
    size_t align = sizeof(cell_t);
    size_t misalignment = vm->here % align;

    if (misalignment != 0) {
        size_t padding = align - misalignment;
        void *pad = vm_allot(vm, padding);
        if (pad == NULL) {
            log_message(LOG_ERROR, "vm_align: Out of memory while aligning");
            vm->error = 1;
            return;
        }

        /* Optional: zero out the padding */
        unsigned char *bytes = (unsigned char *)pad;
        for (size_t i = 0; i < padding; ++i) {
            bytes[i] = 0;
        }

        log_message(LOG_DEBUG, "vm_align: Added %zu byte(s) of padding", padding);
    }
}

void* vm_get_block_addr(VM *vm, int block_num) {
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        log_message(LOG_ERROR, "vm_get_block_addr: Invalid block number %d", block_num);
        return NULL;
    }
    return vm->memory + (block_num * BLOCK_SIZE);
}

/* Convert address to block number */
int vm_addr_to_block(VM *vm, void *addr) {
    if (addr < (void*)vm->memory ||
        addr >= (void*)(vm->memory + VM_MEMORY_SIZE)) {
        return -1;  /* Outside VM memory */
        }

    uintptr_t offset = (uintptr_t)addr - (uintptr_t)vm->memory;
    return (int)(offset / BLOCK_SIZE);
}

