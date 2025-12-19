/**
 * hal_memory.h - HAL memory allocation wrappers
 */

#ifndef STARKERNEL_HAL_MEMORY_H
#define STARKERNEL_HAL_MEMORY_H

#include <stddef.h>

void *hal_mem_alloc(size_t size);
void *hal_mem_alloc_aligned(size_t size, size_t align);
void hal_mem_free(void *ptr);

#endif /* STARKERNEL_HAL_MEMORY_H */
