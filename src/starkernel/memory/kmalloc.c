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
 * kmalloc.c - Simple kernel heap allocator
 *
 * Reserves a fixed heap from PMM and serves aligned allocations with a
 * doubly-linked free list plus coalescing. The allocator avoids libc helpers
 * and keeps metadata inside the heap itself.
 */

#include <stdint.h>
#include <stddef.h>
#include "kmalloc.h"
#include "pmm.h"

#define KMALLOC_DEFAULT_HEAP_SIZE   (16u * 1024u * 1024u) /* 16 MiB */
#define KMALLOC_MIN_ALIGN           16u
#define KMALLOC_MAGIC               0x4B4D414Cu /* 'KMAL' */

typedef struct heap_block {
    size_t size;           /* payload bytes available in this block */
    size_t requested;      /* user-requested size (for stats) */
    int free;              /* 1 if free, 0 if allocated */
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

typedef struct {
    uint32_t magic;
    heap_block_t *block;
} kmalloc_prefix_t;

static uint8_t *heap_base = NULL;
static uint64_t heap_size = 0;
static heap_block_t *heap_head = NULL;
static kmalloc_stats_t heap_stats = {0};
static int heap_initialized = 0;

uintptr_t kmalloc_heap_base_addr(void)
{
    return (uintptr_t)heap_base;
}

uintptr_t kmalloc_heap_end_addr(void)
{
    if (!heap_base) {
        return 0;
    }
    return (uintptr_t)(heap_base + heap_size);
}

static size_t align_up_size(size_t value, size_t align)
{
    if (align == 0) {
        return value;
    }
    return (value + align - 1u) & ~(align - 1u);
}

static size_t normalize_alignment(size_t align)
{
    if (align < KMALLOC_MIN_ALIGN) {
        align = KMALLOC_MIN_ALIGN;
    }
    size_t v = KMALLOC_MIN_ALIGN;
    while (v < align) {
        v <<= 1;
    }
    return v;
}

static void update_free_bytes(void)
{
    if (heap_stats.total_bytes >= heap_stats.used_bytes) {
        heap_stats.free_bytes = heap_stats.total_bytes - heap_stats.used_bytes;
    } else {
        heap_stats.free_bytes = 0;
    }
}

static void account_allocation(size_t requested)
{
    heap_stats.used_bytes += requested;
    if (heap_stats.used_bytes > heap_stats.peak_bytes) {
        heap_stats.peak_bytes = heap_stats.used_bytes;
    }
    update_free_bytes();
}

static void account_free(size_t requested)
{
    if (heap_stats.used_bytes >= requested) {
        heap_stats.used_bytes -= requested;
    } else {
        heap_stats.used_bytes = 0;
    }
    update_free_bytes();
}

static int pointer_within_heap(const void *ptr)
{
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)heap_base;
    uintptr_t end = start + (uintptr_t)heap_size;
    return addr >= start && addr < end;
}

static void coalesce_neighbors(heap_block_t *block)
{
    while (block->next && block->next->free) {
        heap_block_t *next = block->next;
        block->size += sizeof(heap_block_t) + next->size;
        block->next = next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    while (block->prev && block->prev->free) {
        heap_block_t *prev = block->prev;
        prev->size += sizeof(heap_block_t) + block->size;
        prev->next = block->next;
        if (block->next) {
            block->next->prev = prev;
        }
        block = prev;
    }

    if (!block->prev) {
        heap_head = block;
    }
}

static size_t min_splittable_payload(void)
{
    return sizeof(kmalloc_prefix_t) + (2u * KMALLOC_MIN_ALIGN);
}

static int can_split(size_t remainder)
{
    return remainder > (sizeof(heap_block_t) + min_splittable_payload());
}

static void *allocate_from_block(heap_block_t *block, size_t requested, size_t align)
{
    size_t effective_align = normalize_alignment(align);
    size_t payload_size = align_up_size(requested, KMALLOC_MIN_ALIGN);

    uint8_t *payload_start = (uint8_t *)(block + 1);
    uintptr_t aligned_user =
        align_up_size((uintptr_t)(payload_start + sizeof(kmalloc_prefix_t)), effective_align);

    size_t prefix_offset = (size_t)(aligned_user - (uintptr_t)payload_start);
    size_t total_needed = prefix_offset + payload_size;

    if (total_needed > block->size) {
        return NULL;
    }

    size_t remainder = block->size - total_needed;
    if (can_split(remainder)) {
        uint8_t *new_block_addr = payload_start + total_needed;
        heap_block_t *new_block = (heap_block_t *)(void *)new_block_addr;
        new_block->size = remainder - sizeof(heap_block_t);
        new_block->requested = 0;
        new_block->free = 1;
        new_block->next = block->next;
        new_block->prev = block;
        if (new_block->next) {
            new_block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = total_needed;
    }

    block->free = 0;
    block->requested = requested;

    kmalloc_prefix_t *prefix = (kmalloc_prefix_t *)(void *)(aligned_user - sizeof(kmalloc_prefix_t));
    prefix->magic = KMALLOC_MAGIC;
    prefix->block = block;

    account_allocation(requested);
    return (void *)aligned_user;
}

int kmalloc_init(uint64_t heap_size_bytes)
{
    if (heap_initialized) {
        return 0;
    }

    if (!pmm_is_initialized()) {
        return -1;
    }

    uint64_t requested_size = (heap_size_bytes == 0) ? KMALLOC_DEFAULT_HEAP_SIZE : heap_size_bytes;
    if (requested_size < PMM_PAGE_SIZE) {
        requested_size = PMM_PAGE_SIZE;
    }

    uint64_t pages = requested_size / PMM_PAGE_SIZE;
    if (requested_size % PMM_PAGE_SIZE) {
        pages += 1;
    }

    uint64_t paddr = pmm_alloc_contiguous(pages);
    if (paddr == 0) {
        return -1;
    }

    heap_base = (uint8_t *)(uintptr_t)paddr;
    heap_size = pages * (uint64_t)PMM_PAGE_SIZE;

    heap_head = (heap_block_t *)(void *)heap_base;
    heap_head->size = heap_size - sizeof(heap_block_t);
    heap_head->requested = 0;
    heap_head->free = 1;
    heap_head->next = NULL;
    heap_head->prev = NULL;

    heap_stats.total_bytes = heap_head->size;
    heap_stats.used_bytes = 0;
    heap_stats.peak_bytes = 0;
    heap_stats.free_bytes = heap_head->size;

    heap_initialized = 1;
    return 0;
}

int kmalloc_is_initialized(void)
{
    return heap_initialized;
}

void *kmalloc(size_t size)
{
    return kmalloc_aligned(size, KMALLOC_MIN_ALIGN);
}

void *kmalloc_aligned(size_t size, size_t align)
{
    if (!heap_initialized || size == 0) {
        return NULL;
    }

    heap_block_t *cur = heap_head;
    size_t effective_align = normalize_alignment(align);

    while (cur) {
        if (cur->free) {
            void *ptr = allocate_from_block(cur, size, effective_align);
            if (ptr) {
                return ptr;
            }
        }
        cur = cur->next;
    }

    return NULL;
}

void kfree(void *ptr)
{
    if (!heap_initialized || !ptr) {
        return;
    }

    if (!pointer_within_heap(ptr)) {
        return;
    }

    kmalloc_prefix_t *prefix =
        (kmalloc_prefix_t *)((uint8_t *)ptr - sizeof(kmalloc_prefix_t));
    if (prefix->magic != KMALLOC_MAGIC || !prefix->block) {
        return;
    }

    heap_block_t *block = prefix->block;
    if (block->free) {
        return;
    }

    account_free(block->requested);
    block->requested = 0;
    block->free = 1;
    coalesce_neighbors(block);
}

kmalloc_stats_t kmalloc_get_stats(void)
{
    return heap_stats;
}
