/**
 * pmm.c - Physical Memory Manager (bitmap allocator)
 *
 * Builds a 1-bit-per-page bitmap from the UEFI memory map and provides
 * primitive page allocation for early kernel bring-up. The allocator favors
 * simplicity and debuggability; performance tuning can happen once paging and
 * interrupts are in place.
 */

#include "pmm.h"
#include "console.h"

#define PMM_MAX_BITMAP_BYTES (2u * 1024u * 1024u) /* Tracks up to ~64 GiB */
#define PMM_BITMAP_BITS      (PMM_MAX_BITMAP_BYTES * 8ull)

static uint8_t pmm_bitmap[PMM_MAX_BITMAP_BYTES];
static uint64_t pmm_tracked_pages = 0;
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_pages = 0;
static int pmm_initialized = 0;

extern char __kernel_start[];
extern char __kernel_end[];

static void pmm_memset(void *dst, uint8_t value, size_t size) {
    uint8_t *ptr = (uint8_t *)dst;
    for (size_t i = 0; i < size; ++i) {
        ptr[i] = value;
    }
}

static inline uint64_t pmm_bytes_to_pages(uint64_t bytes) {
    uint64_t pages = bytes / PMM_PAGE_SIZE;
    if (bytes % PMM_PAGE_SIZE) {
        pages += 1;
    }
    return pages;
}

static inline void bitmap_set(uint64_t bit) {
    pmm_bitmap[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
}

static inline void bitmap_clear(uint64_t bit) {
    pmm_bitmap[bit / 8u] &= (uint8_t)~(1u << (bit % 8u));
}

static inline int bitmap_test(uint64_t bit) {
    return (pmm_bitmap[bit / 8u] >> (bit % 8u)) & 1u;
}

static void bitmap_mark_range(uint64_t start_page, uint64_t page_count, int used) {
    for (uint64_t i = 0; i < page_count; ++i) {
        uint64_t page = start_page + i;
        if (page >= pmm_tracked_pages) {
            break;
        }

        if (used) {
            if (!bitmap_test(page)) {
                bitmap_set(page);
                if (pmm_free_pages > 0) {
                    pmm_free_pages--;
                }
            }
        } else {
            if (bitmap_test(page)) {
                bitmap_clear(page);
                pmm_free_pages++;
            }
        }
    }
}

static uint64_t find_contiguous_free(uint64_t pages_needed) {
    uint64_t run_start = 0;
    uint64_t run_length = 0;

    for (uint64_t page = 0; page < pmm_tracked_pages; ++page) {
        if (!bitmap_test(page)) {
            if (run_length == 0) {
                run_start = page;
            }
            run_length++;

            if (run_length == pages_needed) {
                return run_start;
            }
        } else {
            run_length = 0;
        }
    }

    return UINT64_MAX;
}

static int is_usable_memory(uint32_t type) {
    return type == EfiConventionalMemory ||
           type == EfiBootServicesCode ||
           type == EfiBootServicesData;
}

int pmm_init(BootInfo *boot_info) {
    if (!boot_info || !boot_info->memory_map || boot_info->memory_map_descriptor_size == 0) {
        return -1;
    }

    EFI_MEMORY_DESCRIPTOR *map = boot_info->memory_map;
    UINTN map_size = boot_info->memory_map_size;
    UINTN desc_size = boot_info->memory_map_descriptor_size;
    UINTN entry_count = map_size / desc_size;

    uint64_t max_phys_end = 0;

    /* Determine total tracked pages (bounded by bitmap capacity) */
    for (UINTN i = 0; i < entry_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);
        uint64_t region_end = desc->PhysicalStart + desc->NumberOfPages * (uint64_t)PMM_PAGE_SIZE;

        if (region_end > max_phys_end) {
            max_phys_end = region_end;
        }
    }

    pmm_tracked_pages = pmm_bytes_to_pages(max_phys_end);
    if (pmm_tracked_pages > PMM_BITMAP_BITS) {
        pmm_tracked_pages = PMM_BITMAP_BITS;
    }

    if (pmm_tracked_pages == 0) {
        return -1;
    }

    /* Start with all pages marked as used */
    uint64_t bitmap_bytes = (pmm_tracked_pages + 7u) / 8u;
    pmm_memset(pmm_bitmap, 0xFF, (size_t)bitmap_bytes);

    pmm_total_pages = pmm_tracked_pages;
    pmm_free_pages = 0;

    /* Mark usable regions as free */
    for (UINTN i = 0; i < entry_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);
        if (!is_usable_memory(desc->Type)) {
            continue;
        }

        uint64_t start_page = desc->PhysicalStart / PMM_PAGE_SIZE;
        uint64_t pages = desc->NumberOfPages;

        if (start_page >= pmm_tracked_pages) {
            continue;
        }

        if (start_page + pages > pmm_tracked_pages) {
            pages = pmm_tracked_pages - start_page;
        }

        bitmap_mark_range(start_page, pages, 0);
    }

    /* Reserve the kernel image so allocations do not overlap our own code/data */
    uint64_t kernel_start = (uint64_t)__kernel_start;
    uint64_t kernel_end = (uint64_t)__kernel_end;
    uint64_t kernel_pages = pmm_bytes_to_pages(kernel_end - kernel_start);
    bitmap_mark_range(kernel_start / PMM_PAGE_SIZE, kernel_pages, 1);

    /* Keep the zero page unavailable to catch null dereferences early */
    bitmap_mark_range(0, 1, 1);

    pmm_initialized = 1;
    return 0;
}

int pmm_is_initialized(void) {
    return pmm_initialized;
}

uint64_t pmm_alloc_page(void) {
    if (!pmm_initialized) {
        return 0;
    }

    uint64_t start = find_contiguous_free(1);
    if (start == UINT64_MAX) {
        return 0;
    }

    bitmap_mark_range(start, 1, 1);
    return start * (uint64_t)PMM_PAGE_SIZE;
}

uint64_t pmm_alloc_contiguous(uint64_t num_pages) {
    if (!pmm_initialized || num_pages == 0) {
        return 0;
    }

    uint64_t start = find_contiguous_free(num_pages);
    if (start == UINT64_MAX) {
        return 0;
    }

    bitmap_mark_range(start, num_pages, 1);
    return start * (uint64_t)PMM_PAGE_SIZE;
}

void pmm_free_page(uint64_t paddr) {
    if (!pmm_initialized || (paddr % PMM_PAGE_SIZE) != 0) {
        return;
    }

    pmm_free_contiguous(paddr, 1);
}

void pmm_free_contiguous(uint64_t paddr, uint64_t num_pages) {
    if (!pmm_initialized || (paddr % PMM_PAGE_SIZE) != 0 || num_pages == 0) {
        return;
    }

    uint64_t start_page = paddr / PMM_PAGE_SIZE;
    if (start_page >= pmm_tracked_pages) {
        return;
    }

    if (start_page + num_pages > pmm_tracked_pages) {
        num_pages = pmm_tracked_pages - start_page;
    }

    bitmap_mark_range(start_page, num_pages, 0);
}

pmm_stats_t pmm_get_stats(void) {
    pmm_stats_t stats = {0};
    stats.total_pages = pmm_total_pages;
    stats.free_pages = pmm_free_pages;
    stats.used_pages = (pmm_total_pages > pmm_free_pages) ? (pmm_total_pages - pmm_free_pages) : 0;
    stats.total_bytes = stats.total_pages * (uint64_t)PMM_PAGE_SIZE;
    stats.used_bytes = stats.used_pages * (uint64_t)PMM_PAGE_SIZE;
    stats.free_bytes = stats.free_pages * (uint64_t)PMM_PAGE_SIZE;
    return stats;
}
