/**
 * kmalloc.h - Kernel heap allocator interface
 */

#ifndef STARKERNEL_KMALLOC_H
#define STARKERNEL_KMALLOC_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t peak_bytes;
    uint64_t free_bytes;
} kmalloc_stats_t;

int kmalloc_init(uint64_t heap_size_bytes);
int kmalloc_is_initialized(void);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t align);
void kfree(void *ptr);
kmalloc_stats_t kmalloc_get_stats(void);
uintptr_t kmalloc_heap_base_addr(void);
uintptr_t kmalloc_heap_end_addr(void);

#endif /* STARKERNEL_KMALLOC_H */
