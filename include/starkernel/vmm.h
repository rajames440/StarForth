/**
 * vmm.h - Virtual Memory Manager interface (x86_64 4-level paging)
 */

#ifndef STARKERNEL_VMM_H
#define STARKERNEL_VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE         4096ull

/* Page table entry flags */
#define VMM_FLAG_PRESENT   (1ull << 0)
#define VMM_FLAG_WRITABLE  (1ull << 1)
#define VMM_FLAG_USER      (1ull << 2)
#define VMM_FLAG_NX        (1ull << 63)

#include "uefi.h"

typedef struct {
    int present;
    int writable;
    int executable;
} vmm_page_info_t;

int vmm_init(BootInfo *boot_info);
int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);
int vmm_unmap_page(uint64_t vaddr);
uint64_t vmm_get_paddr(uint64_t vaddr);
int vmm_map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags);
int vmm_query_page(uint64_t vaddr, vmm_page_info_t *info);

#endif /* STARKERNEL_VMM_H */
