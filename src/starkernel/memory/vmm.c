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
 * vmm.c - 4-level paging Virtual Memory Manager (x86_64)
 *
 * Minimal bring-up: identity maps the first 16 MiB, supports single-page
 * map/unmap/query, and switches to the new page table root.
 */

#include <stddef.h>
#include <stdint.h>
#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include "uefi.h"

#define PTE_PRESENT   (1ull << 0)
#define PTE_WRITABLE  (1ull << 1)
#define PTE_USER      (1ull << 2)
#define PTE_PWT       (1ull << 3)
#define PTE_PCD       (1ull << 4)
#define PTE_ACCESSED  (1ull << 5)
#define PTE_DIRTY     (1ull << 6)
#define PTE_PS        (1ull << 7)
#define PTE_GLOBAL    (1ull << 8)
#define PTE_NX        (1ull << 63)

#define ADDR_MASK 0x000FFFFFFFFFF000ull

static uint64_t vmm_root_pml4_phys = 0;
static const uint64_t lapic_phys_base = 0xFEE00000ull;
static const uint64_t hpet_phys_base  = 0xFED00000ull;

static int is_ram_type(uint32_t type) {
    return type == EfiConventionalMemory ||
           type == EfiLoaderCode ||
           type == EfiLoaderData ||
           type == EfiBootServicesCode ||
           type == EfiBootServicesData ||
           type == EfiRuntimeServicesCode ||
           type == EfiRuntimeServicesData ||
           type == EfiACPIReclaimMemory ||
           type == EfiACPIMemoryNVS;
}

static inline uint64_t *paddr_to_virt(uint64_t paddr) {
    return (uint64_t *)(uintptr_t)paddr;
}

static uint64_t alloc_page_zeroed(void) {
    uint64_t paddr = pmm_alloc_page();
    if (paddr == 0) {
        return 0;
    }
    uint64_t *ptr = paddr_to_virt(paddr);
    for (size_t i = 0; i < (VMM_PAGE_SIZE / sizeof(uint64_t)); ++i) {
        ptr[i] = 0;
    }
    return paddr;
}

static inline uint16_t pml4_index(uint64_t vaddr) { return (uint16_t)((vaddr >> 39) & 0x1FF); }
static inline uint16_t pdpt_index(uint64_t vaddr) { return (uint16_t)((vaddr >> 30) & 0x1FF); }
static inline uint16_t pd_index(uint64_t vaddr)   { return (uint16_t)((vaddr >> 21) & 0x1FF); }
static inline uint16_t pt_index(uint64_t vaddr)   { return (uint16_t)((vaddr >> 12) & 0x1FF); }

static uint64_t *get_or_alloc_table(uint64_t *parent, uint16_t idx) {
    uint64_t entry = parent[idx];
    if (entry & PTE_PRESENT) {
        return paddr_to_virt(entry & ADDR_MASK);
    }

    uint64_t child_phys = alloc_page_zeroed();
    if (child_phys == 0) {
        return NULL;
    }

    parent[idx] = child_phys | PTE_PRESENT | PTE_WRITABLE;
    return paddr_to_virt(child_phys);
}

static uint64_t *get_table(uint64_t *parent, uint16_t idx) {
    uint64_t entry = parent[idx];
    if (entry & PTE_PRESENT) {
        return paddr_to_virt(entry & ADDR_MASK);
    }
    return NULL;
}

static uint64_t make_pte(uint64_t paddr, uint64_t flags) {
    uint64_t pte = paddr & ADDR_MASK;
    if (flags & VMM_FLAG_PRESENT) pte |= PTE_PRESENT;
    if (flags & VMM_FLAG_WRITABLE) pte |= PTE_WRITABLE;
    if (flags & VMM_FLAG_USER) pte |= PTE_USER;
    if (!(flags & VMM_FLAG_NX)) pte &= ~PTE_NX;
    else pte |= PTE_NX;
    return pte;
}

static inline void invlpg(uint64_t vaddr) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
#else
    (void)vaddr;
#endif
}

static inline void load_cr3(uint64_t pml4_phys) {
#if defined(__x86_64__)
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
#else
    (void)pml4_phys;
#endif
}

static uint64_t *resolve_pt(uint64_t vaddr, uint16_t *idx_out) {
    if (vmm_root_pml4_phys == 0) {
        return NULL;
    }

    uint64_t *pml4 = paddr_to_virt(vmm_root_pml4_phys);
    uint64_t *pdpt = get_table(pml4, pml4_index(vaddr));
    if (!pdpt) return NULL;

    uint64_t *pd = get_table(pdpt, pdpt_index(vaddr));
    if (!pd) return NULL;

    uint64_t *pt = get_table(pd, pd_index(vaddr));
    if (!pt) return NULL;

    if (idx_out) {
        *idx_out = pt_index(vaddr);
    }

    return pt;
}

int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (vmm_root_pml4_phys == 0) {
        return -1;
    }

    uint64_t *pml4 = paddr_to_virt(vmm_root_pml4_phys);
    uint64_t *pdpt = get_or_alloc_table(pml4, pml4_index(vaddr));
    if (!pdpt) return -1;

    uint64_t *pd = get_or_alloc_table(pdpt, pdpt_index(vaddr));
    if (!pd) return -1;

    uint64_t *pt = get_or_alloc_table(pd, pd_index(vaddr));
    if (!pt) return -1;

    uint16_t idx = pt_index(vaddr);
    if (pt[idx] & PTE_PRESENT) {
        uint64_t existing_pa = pt[idx] & ADDR_MASK;
        uint64_t new_pte = make_pte(paddr, flags | VMM_FLAG_PRESENT);
        /* Idempotent mapping: same PA + compatible flags => success */
        if (existing_pa == (paddr & ADDR_MASK)) {
            /* Require WRITABLE compatibility (no silent downgrade/upgrade) */
            int was_writable = (pt[idx] & PTE_WRITABLE) != 0;
            int want_writable = (new_pte & PTE_WRITABLE) != 0;
            if (was_writable == want_writable) {
                return 0;
            }
        }
        console_println("VMM map conflict");
        return -1;
    }

    pt[idx] = make_pte(paddr, flags | VMM_FLAG_PRESENT);
    invlpg(vaddr);
    return 0;
}

int vmm_unmap_page(uint64_t vaddr) {
    if (vmm_root_pml4_phys == 0) {
        return -1;
    }

    uint64_t *pml4 = paddr_to_virt(vmm_root_pml4_phys);
    uint64_t *pdpt = get_table(pml4, pml4_index(vaddr));
    if (!pdpt) return -1;

    uint64_t *pd = get_table(pdpt, pdpt_index(vaddr));
    if (!pd) return -1;

    uint64_t *pt = get_table(pd, pd_index(vaddr));
    if (!pt) return -1;

    uint16_t idx = pt_index(vaddr);
    if (!(pt[idx] & PTE_PRESENT)) {
        return -1;
    }

    pt[idx] = 0;
    invlpg(vaddr);
    return 0;
}

uint64_t vmm_get_paddr(uint64_t vaddr) {
    if (vmm_root_pml4_phys == 0) {
        return 0;
    }

    uint64_t *pml4 = paddr_to_virt(vmm_root_pml4_phys);
    uint64_t *pdpt = get_table(pml4, pml4_index(vaddr));
    if (!pdpt) return 0;

    uint64_t *pd = get_table(pdpt, pdpt_index(vaddr));
    if (!pd) return 0;

    uint64_t *pt = get_table(pd, pd_index(vaddr));
    if (!pt) return 0;

    uint16_t idx = pt_index(vaddr);
    if (!(pt[idx] & PTE_PRESENT)) {
        return 0;
    }

    return (pt[idx] & ADDR_MASK) | (vaddr & 0xFFF);
}

int vmm_map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags) {
    uint64_t pages = (size + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE;
    for (uint64_t i = 0; i < pages; ++i) {
        if (vmm_map_page(vaddr + i * VMM_PAGE_SIZE, paddr + i * VMM_PAGE_SIZE, flags) != 0) {
            return -1;
        }
    }
    return 0;
}

int vmm_query_page(uint64_t vaddr, vmm_page_info_t *info) {
    if (info) {
        info->present = 0;
        info->writable = 0;
        info->executable = 0;
    }

    uint16_t idx = 0;
    uint64_t *pt = resolve_pt(vaddr, &idx);
    if (!pt) {
        return 0;
    }

    uint64_t entry = pt[idx];
    if (!(entry & PTE_PRESENT)) {
        return 0;
    }

    if (info) {
        info->present = 1;
        info->writable = (entry & PTE_WRITABLE) ? 1 : 0;
        info->executable = (entry & PTE_NX) ? 0 : 1;
    }

    return 1;
}

int vmm_init(BootInfo *boot_info) {
    if (!pmm_is_initialized()) {
        console_println("VMM init failed: PMM not ready");
        return -1;
    }

    if (!boot_info || !boot_info->memory_map || boot_info->memory_map_descriptor_size == 0) {
        console_println("VMM init failed: missing BootInfo");
        return -1;
    }

    vmm_root_pml4_phys = alloc_page_zeroed();
    if (vmm_root_pml4_phys == 0) {
        console_println("VMM init failed: no memory for PML4");
        return -1;
    }

    EFI_MEMORY_DESCRIPTOR *map = boot_info->memory_map;
    UINTN map_size = boot_info->memory_map_size;
    UINTN desc_size = boot_info->memory_map_descriptor_size;
    UINTN entry_count = map_size / desc_size;

    for (UINTN i = 0; i < entry_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);
        if (!is_ram_type(desc->Type)) {
            continue;
        }

        uint64_t start = desc->PhysicalStart;
        uint64_t size = desc->NumberOfPages * VMM_PAGE_SIZE;

        if (vmm_map_range(start, start, size, VMM_FLAG_WRITABLE) != 0) {
            console_println("VMM init failed: mapping RAM region");
            return -1;
        }
    }

    /* Map LAPIC and HPET identity */
    if (vmm_map_range(lapic_phys_base, lapic_phys_base, VMM_PAGE_SIZE, VMM_FLAG_WRITABLE) != 0) {
        console_println("VMM init failed: map LAPIC MMIO");
        return -1;
    }
    if (vmm_map_range(hpet_phys_base, hpet_phys_base, VMM_PAGE_SIZE, VMM_FLAG_WRITABLE) != 0) {
        console_println("VMM init failed: map HPET MMIO");
        return -1;
    }

    load_cr3(vmm_root_pml4_phys);
    console_println("VMM initialized (mapped RAM, CR3 switched)");
    return 0;
}
