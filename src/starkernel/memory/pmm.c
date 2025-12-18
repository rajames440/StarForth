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

/* Do not hand out pages below this floor (1 MiB) */
static const uint64_t pmm_alloc_floor_page = 0x100000u / PMM_PAGE_SIZE;

static uint8_t pmm_bitmap[PMM_MAX_BITMAP_BYTES];
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_pages = 0;
static int pmm_initialized = 0;
static uint64_t pmm_tracked_pages = 0;

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

    for (uint64_t page = pmm_alloc_floor_page; page < pmm_tracked_pages; ++page) {
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
    return type == EfiConventionalMemory;
}

static int is_ram_type(uint32_t type) {
    return is_usable_memory(type) ||
           type == EfiRuntimeServicesCode ||
           type == EfiRuntimeServicesData ||
           type == EfiACPIReclaimMemory ||
           type == EfiACPIMemoryNVS;
}

int pmm_init(BootInfo *boot_info)
{
    if (!boot_info ||
        !boot_info->memory_map ||
        boot_info->memory_map_descriptor_size == 0)
    {
        return -1;
    }

    EFI_MEMORY_DESCRIPTOR *map = boot_info->memory_map;
    UINTN map_size = boot_info->memory_map_size;
    UINTN desc_size = boot_info->memory_map_descriptor_size;
    UINTN entry_count = map_size / desc_size;

    uint64_t max_ram_end = 0;

    /* ------------------------------------------------------------
     * Pass 1: determine highest physical address of RAM-like regions
     * ------------------------------------------------------------ */
    for (UINTN i = 0; i < entry_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc =
            (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);

        if (!is_ram_type(desc->Type))
            continue;

        uint64_t region_end =
            desc->PhysicalStart +
            desc->NumberOfPages * (uint64_t)PMM_PAGE_SIZE;

        if (region_end > max_ram_end)
            max_ram_end = region_end;
    }

    if (max_ram_end == 0)
        return -1;

    pmm_tracked_pages = pmm_bytes_to_pages(max_ram_end);
    if (pmm_tracked_pages > PMM_BITMAP_BITS)
        pmm_tracked_pages = PMM_BITMAP_BITS;

    if (pmm_tracked_pages == 0)
        return -1;

    uint64_t bitmap_bytes = (pmm_tracked_pages + 7) / 8;
    pmm_memset(pmm_bitmap, 0xFF, (size_t)bitmap_bytes);

    pmm_total_pages = 0;
    pmm_free_pages  = 0;

    /* ------------------------------------------------------------
     * Pass 2: count RAM pages (tracked universe)
     * ------------------------------------------------------------ */
    for (UINTN i = 0; i < entry_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc =
            (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);

        if (!is_ram_type(desc->Type))
            continue;

        uint64_t start_page = desc->PhysicalStart / PMM_PAGE_SIZE;
        uint64_t end_page   = start_page + desc->NumberOfPages;

        if (start_page >= pmm_tracked_pages)
            continue;

        if (end_page > pmm_tracked_pages)
            end_page = pmm_tracked_pages;

        if (end_page > start_page)
            pmm_total_pages += (end_page - start_page);
    }

    /* ------------------------------------------------------------
     * Pass 3: free usable memory (respect allocation floor)
     * ------------------------------------------------------------ */
    for (UINTN i = 0; i < entry_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc =
            (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);

        if (!is_usable_memory(desc->Type))
            continue;

        uint64_t start_page = desc->PhysicalStart / PMM_PAGE_SIZE;
        uint64_t end_page   = start_page + desc->NumberOfPages;

        if (start_page >= pmm_tracked_pages)
            continue;

        if (end_page > pmm_tracked_pages)
            end_page = pmm_tracked_pages;

        if (end_page <= pmm_alloc_floor_page)
            continue;

        if (start_page < pmm_alloc_floor_page)
            start_page = pmm_alloc_floor_page;

        if (end_page > start_page) {
            uint64_t pages = end_page - start_page;
            bitmap_mark_range(start_page, pages, 0);
            pmm_free_pages += pages;
        }
    }

    /* ------------------------------------------------------------
     * Reserve low memory (defensive)
     * ------------------------------------------------------------ */
    if (pmm_alloc_floor_page > 0) {
        bitmap_mark_range(0, pmm_alloc_floor_page, 1);
    }

    /* ------------------------------------------------------------
     * Reserve kernel image (page-aligned, physical)
     * ------------------------------------------------------------ */
    uint64_t kstart = (uint64_t)__kernel_start;
    uint64_t kend   = (uint64_t)__kernel_end;

    uint64_t kstart_page = kstart / PMM_PAGE_SIZE;
    uint64_t kend_page =
        (kend + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    if (kend_page > kstart_page) {
        uint64_t kpages = kend_page - kstart_page;
        bitmap_mark_range(kstart_page, kpages, 1);
        if (pmm_free_pages >= kpages)
            pmm_free_pages -= kpages;
    }

    /* ------------------------------------------------------------
     * Keep page 0 reserved (NULL guard)
     * ------------------------------------------------------------ */
    bitmap_mark_range(0, 1, 1);

    if (pmm_free_pages > pmm_total_pages)
        pmm_free_pages = pmm_total_pages;

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
