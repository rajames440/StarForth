/*
                                  ***   StarForth   ***

  memory_management.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.981-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/memory_management.c
 */

#include "../include/vm.h"
#include "../include/log.h"

/**
 * @brief Allocates memory from the VM's dictionary space
 * @param vm Pointer to the VM instance
 * @param bytes Number of bytes to allocate
 * @return Pointer to the allocated memory or NULL if allocation fails
 * @note The allocation is done from the VM's dictionary space which is limited to DICTIONARY_MEMORY_SIZE
 */
void *vm_allot(VM *vm, size_t bytes) {
    if (!vm || !vm->memory) {
        log_message(LOG_ERROR, "vm_allot: VM or memory is NULL");
        return NULL;
    }

    /* Ensure we don't allocate beyond dictionary space (first 1024 blocks = 1MB) */
    log_message(LOG_DEBUG, "vm_allot: Requesting %zu bytes, current here=%zu, limit=%d",
                bytes, vm->here, DICTIONARY_MEMORY_SIZE);

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
        unsigned char *bytes = (unsigned char *) pad;
        for (size_t i = 0; i < padding; ++i) {
            bytes[i] = 0;
        }

        log_message(LOG_DEBUG, "vm_align: Added %zu byte(s) of padding", padding);
    }
}

/**
 * @brief Gets the memory address for a specified block number
 * @param vm Pointer to the VM instance
 * @param block_num Block number to get the address for
 * @return Pointer to the start of the block or NULL if block number is invalid
 * @note Blocks are fixed-size memory regions of BLOCK_SIZE bytes
 */
void *vm_get_block_addr(VM *vm, int block_num) {
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        log_message(LOG_ERROR, "vm_get_block_addr: Invalid block number %d", block_num);
        return NULL;
    }
    return vm->memory + (block_num * BLOCK_SIZE);
}

/* Convert address to block number */
/**
 * @brief Converts a memory address to its corresponding block number
 * @param vm Pointer to the VM instance
 * @param addr Memory address to convert
 * @return Block number (0 to MAX_BLOCKS-1) or -1 if address is outside VM memory
 * @note Used for determining which block a memory address belongs to
 */
int vm_addr_to_block(VM *vm, void *addr) {
    if (addr < (void *) vm->memory ||
        addr >= (void *) (vm->memory + VM_MEMORY_SIZE)) {
        return -1; /* Outside VM memory */
    }

    uintptr_t offset = (uintptr_t) addr - (uintptr_t) vm->memory;
    return (int) (offset / BLOCK_SIZE);
}