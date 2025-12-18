/**
 * kernel_main.c - StarKernel main entry point
 * Called after UEFI boot services have been exited
 */

#include "uefi.h"
#include "console.h"
#include "arch.h"
#include "pmm.h"
#include "vmm.h"

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

/**
 * Convert integer to string (simple implementation)
 */
static void itoa_simple(uint64_t value, char *buf, int base) {
    char temp[64];
    int i = 0;
    int j;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0) {
        int digit = value % base;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }

    /* Reverse the string */
    for (j = 0; j < i; j++) {
        buf[j] = temp[i - j - 1];
    }
    buf[j] = '\0';
}

static void print_uint(const char *label, uint64_t value) {
    char buf[64];
    if (label) {
        console_puts(label);
    }
    itoa_simple(value, buf, 10);
    console_println(buf);
}

static void print_hex64(const char *label, uint64_t value) {
    char buf[64];
    if (label) {
        console_puts(label);
    }
    console_puts("0x");
    itoa_simple(value, buf, 16);
    console_println(buf);
}

/**
 * Print boot information summary
 */
static void print_boot_info(BootInfo *boot_info) {
    char buf[64];
    UINTN num_entries;
    UINTN total_memory = 0;
    UINTN usable_memory = 0;
    UINTN i;

    console_println("\n=== StarKernel Boot Information ===");

    /* Calculate memory statistics */
    num_entries = boot_info->memory_map_size / boot_info->memory_map_descriptor_size;

    for (i = 0; i < num_entries; i++) {
        EFI_MEMORY_DESCRIPTOR *desc =
            (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)boot_info->memory_map +
                                      i * boot_info->memory_map_descriptor_size);

        UINTN size = desc->NumberOfPages * 4096;
        if (is_ram_type(desc->Type)) {
            total_memory += size;
        }

        if (desc->Type == EfiConventionalMemory) {
            usable_memory += size;
        }
    }

    console_puts("Memory map entries: ");
    itoa_simple(num_entries, buf, 10);
    console_println(buf);

    console_puts("Total memory: ");
    itoa_simple(total_memory / (1024 * 1024), buf, 10);
    console_puts(buf);
    console_println(" MB");

    console_puts("Usable memory: ");
    itoa_simple(usable_memory / (1024 * 1024), buf, 10);
    console_puts(buf);
    console_println(" MB");

    console_println("===================================\n");
}

static void print_pmm_stats(void) {
    pmm_stats_t stats = pmm_get_stats();

    console_println("PMM statistics:");
    print_uint("  Total pages: ", stats.total_pages);
    print_uint("  Free pages : ", stats.free_pages);
    print_uint("  Used pages : ", stats.used_pages);
    print_uint("  Total MB   : ", stats.total_bytes / (1024 * 1024));
    print_uint("  Free  MB   : ", stats.free_bytes / (1024 * 1024));
    print_uint("  Used  MB   : ", stats.used_bytes / (1024 * 1024));
    console_println("");
}

static void pmm_smoke_test(void) {
    uint64_t pages[10] = {0};
    int unique = 1;

    console_println("PMM smoke test: allocating 10 pages...");

    for (int i = 0; i < 10; ++i) {
        pages[i] = pmm_alloc_page();
        if (pages[i] == 0) {
            console_println("  Allocation failed.");
            return;
        }

        for (int j = 0; j < i; ++j) {
            if (pages[i] == pages[j]) {
                unique = 0;
            }
        }
    }

    if (!unique) {
        console_println("  Duplicate page detected!");
        return;
    }

    console_println("  Pages allocated successfully:");
    for (int i = 0; i < 10; ++i) {
        print_hex64("    ", pages[i]);
    }

    console_println("  Freeing pages...");
    for (int i = 0; i < 10; ++i) {
        pmm_free_page(pages[i]);
    }

    uint64_t reused = pmm_alloc_page();
    if (reused == 0) {
        console_println("  Re-allocation failed.");
        return;
    }

    int matched_previous = 0;
    for (int i = 0; i < 10; ++i) {
        if (reused == pages[i]) {
            matched_previous = 1;
            break;
        }
    }

    console_puts("  Re-allocated page: ");
    print_hex64("", reused);

    if (matched_previous) {
        console_println("  Re-allocation reused a freed page (expected).");
    } else {
        console_println("  Re-allocation succeeded but did not reuse a prior page.");
    }

    pmm_free_page(reused);

    uint64_t contiguous = pmm_alloc_contiguous(4);
    if (contiguous != 0) {
        console_println("  Allocated 4 contiguous pages:");
        print_hex64("    Start: ", contiguous);
        print_hex64("    End  : ", contiguous + (4 * PMM_PAGE_SIZE) - 1);
        pmm_free_contiguous(contiguous, 4);
    } else {
        console_println("  Contiguous allocation of 4 pages failed.");
    }

    console_println("PMM smoke test complete.\n");
}

static void vmm_self_test(void) {
    /* Map a single PMM page at a high alias and verify round-trip */
    const uint64_t test_vaddr = 0xFFFF800000000000ull;
    uint64_t paddr = pmm_alloc_page();
    if (paddr == 0) {
        console_println("VMM self-test: failed to allocate page.");
        return;
    }

    if (vmm_map_page(test_vaddr, paddr, VMM_FLAG_WRITABLE) != 0) {
        console_println("VMM self-test: map failed.");
        pmm_free_page(paddr);
        return;
    }

    uint64_t resolved = vmm_get_paddr(test_vaddr);
    if (resolved != paddr) {
        console_println("VMM self-test: translation mismatch.");
    } else {
        console_puts("VMM self-test: mapped OK at ");
        print_hex64("", test_vaddr);
    }

    /* Write via alias to ensure it is accessible */
    volatile uint64_t *ptr = (uint64_t *)(uintptr_t)test_vaddr;
    *ptr = 0xdeadbeefULL;

    if (vmm_unmap_page(test_vaddr) != 0) {
        console_println("VMM self-test: unmap failed.");
    }

    pmm_free_page(paddr);
    console_println("VMM self-test complete.\n");
}

/**
 * Kernel main entry point
 * Called from UEFI loader after boot services have been exited
 */
void kernel_main(BootInfo *boot_info) {
    /* Perform early arch setup (stubs until implemented) */
    arch_early_init();

    /* Initialize serial console */
    console_init();

    /* Print boot banner */
    console_println("\n");
    console_println("   _____ _             _  __                    _ ");
    console_println("  / ____| |           | |/ /                   | |");
    console_println(" | (___ | |_ __ _ _ __| ' / ___ _ __ _ __   ___| |");
    console_println("  \\___ \\| __/ _` | '__|  < / _ \\ '__| '_ \\ / _ \\ |");
    console_println("  ____) | || (_| | |  | . \\  __/ |  | | | |  __/ |");
    console_println(" |_____/ \\__\\__,_|_|  |_|\\_\\___|_|  |_| |_|\\___|_|");
    console_println("");
    console_println("StarKernel v0.1.0 - FORTH Microkernel");
    console_println("Architecture: amd64");
    console_println("Build: " __DATE__ " " __TIME__);
    console_println("");

    console_puts("UEFI BootServices: ");
    if (boot_info && boot_info->uefi_boot_services_exited) {
        console_println("EXITED");
    } else {
        console_println("ENABLED");
    }

    /* Print boot information */
    if (boot_info) {
        print_boot_info(boot_info);
    }

    /* Initialize physical memory manager */
    if (boot_info) {
        if (pmm_init(boot_info) == 0) {
            console_println("PMM initialized.");
            print_pmm_stats();
            pmm_smoke_test();
        } else {
            console_println("PMM initialization failed.");
        }
    } else {
        console_println("No BootInfo provided; skipping PMM initialization.");
    }

    /* Initialize virtual memory (identity-map first 16 MiB and switch CR3) */
    if (vmm_init(boot_info) != 0) {
        console_println("VMM initialization failed.");
    }
    else {
        vmm_self_test();
    }

    /* Success message */
    console_println("Kernel initialization complete.");
    console_println("Boot successful!");
    console_println("");

    /* Infinite loop - kernel is running */
    console_println("Kernel halted. (QEMU: Press Ctrl+A, then X to exit)");

    while (1) {
        arch_halt();
    }
}
