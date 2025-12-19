/**
 * memory.c - HAL memory wrappers
 */

#include <stddef.h>
#include "hal_memory.h"
#include "kmalloc.h"

void *hal_mem_alloc(size_t size)
{
    return kmalloc(size);
}

void *hal_mem_alloc_aligned(size_t size, size_t align)
{
    return kmalloc_aligned(size, align);
}

void hal_mem_free(void *ptr)
{
    kfree(ptr);
}
