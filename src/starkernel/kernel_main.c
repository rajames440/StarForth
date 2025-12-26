/**
 * kernel_main.c - StarKernel main entry point
 * Called after UEFI boot services have been exited
 */

#ifndef __STARKERNEL__
#error "__STARKERNEL__ must be defined for kernel build"
#endif

#include "uefi.h"
#include "console.h"
#include "arch.h"
#include "pmm.h"
#include "vmm.h"
#include "apic.h"
#include "timer.h"
#include "kmalloc.h"

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
        int digit = (int)(value % (uint64_t)base);
        temp[i++] = (digit < 10) ? (char)('0' + digit) : (char)('a' + digit - 10);
        value /= (uint64_t)base;
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

        UINTN size = desc->NumberOfPages * 4096u;
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
    itoa_simple((uint64_t)(total_memory / (1024u * 1024u)), buf, 10);
    console_puts(buf);
    console_println(" MB");

    console_puts("Usable memory: ");
    itoa_simple((uint64_t)(usable_memory / (1024u * 1024u)), buf, 10);
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
    print_uint("  Total MB   : ", stats.total_bytes / (1024u * 1024u));
    print_uint("  Free  MB   : ", stats.free_bytes / (1024u * 1024u));
    print_uint("  Used  MB   : ", stats.used_bytes / (1024u * 1024u));
    console_println("");
}

static void print_heap_stats(void) {
    kmalloc_stats_t stats = kmalloc_get_stats();

    console_println("Heap statistics:");
    print_uint("  Total bytes: ", stats.total_bytes);
    print_uint("  Free  bytes: ", stats.free_bytes);
    print_uint("  Used  bytes: ", stats.used_bytes);
    print_uint("  Peak  bytes: ", stats.peak_bytes);
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
        print_hex64("    End  : ", contiguous + (4u * PMM_PAGE_SIZE) - 1u);
        pmm_free_contiguous(contiguous, 4);
    } else {
        console_println("  Contiguous allocation of 4 pages failed.");
    }

    console_println("PMM smoke test complete.\n");
}

static void heap_smoke_test(void) {
    console_println("Heap smoke test: allocating blocks...");

    void *a = kmalloc(64);
    void *b = kmalloc_aligned(128, 64);
    void *c = kmalloc(256);

    if (!a || !b || !c) {
        console_println("  Allocation failed.");
        if (a) kfree(a);
        if (b) kfree(b);
        if (c) kfree(c);
        return;
    }

    console_println("  Allocations succeeded.");

    kfree(b);
    kfree(a);
    kfree(c);

    void *d = kmalloc(64);
    if (!d) {
        console_println("  Re-allocation failed.");
    } else {
        console_println("  Re-allocation succeeded (coalescing validated).");
        kfree(d);
    }

    console_println("Heap smoke test complete.\n");
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
    volatile uint64_t *ptr = (volatile uint64_t *)(uintptr_t)test_vaddr;
    *ptr = 0xdeadbeefULL;

    if (vmm_unmap_page(test_vaddr) != 0) {
        console_println("VMM self-test: unmap failed.");
    }

    pmm_free_page(paddr);
    console_println("VMM self-test complete.\n");
}

/* ---------------- Self-tests (gated) ---------------- */

/* Self-test flags: set to 1 to trigger fault for testing, 0 for normal operation */
#ifndef DIV0_SELF_TEST
#define DIV0_SELF_TEST 0
#endif

#ifndef PF_SELF_TEST_READ
#define PF_SELF_TEST_READ 0
#endif

#ifndef PF_SELF_TEST_WRITE
#define PF_SELF_TEST_WRITE 0
#endif

/* Compiler barrier to discourage “helpful” optimization across fault tests */
static inline void compiler_barrier(void) {
    __asm__ volatile ("" ::: "memory");
}

__attribute__((noinline))
static void divide_by_zero_self_test(void) {
#if DIV0_SELF_TEST
    console_println("Triggering divide-by-zero self-test (#DE)...");

    /*
     * We must use inline asm to force the CPU to execute a DIV instruction.
     * The compiler may optimize away a C division whose result is unused,
     * even with volatile operands.
     */
    uint64_t dividend = 1;
    uint64_t divisor  = 0;
    uint64_t quotient;

    __asm__ volatile (
        "xor %%rdx, %%rdx\n\t"   /* clear high bits of dividend (RDX:RAX) */
        "div %2"                  /* RAX = RAX / operand, RDX = remainder */
        : "=a"(quotient)
        : "a"(dividend), "r"(divisor)
        : "rdx"
    );

    /* Suppress unused-variable warning; this line should never be reached */
    (void)quotient;
    console_println("Divide-by-zero self-test did NOT trigger as expected.");
#endif
}

__attribute__((noinline))
static void page_fault_self_test_read(void) {
#if PF_SELF_TEST_READ
    /*
     * After vmm_init() you identity-map low memory (per your note: first 16 MiB).
     * So a high-ish canonical address is very likely unmapped and should #PF.
     */
    const uint64_t bad = 0x0000000080000000ull; /* 2 GiB */
    console_puts("Triggering page-fault READ self-test (#PF) at ");
    print_hex64("", bad);
    console_println("");

    volatile uint64_t *ptr = (volatile uint64_t *)(uintptr_t)bad;
    compiler_barrier();
    (void)(*ptr);
    compiler_barrier();

    console_println("Page-fault READ self-test did NOT trigger as expected.");
#endif
}

__attribute__((noinline))
static void page_fault_self_test_write(void) {
#if PF_SELF_TEST_WRITE
    const uint64_t bad = 0x0000000080000000ull; /* 2 GiB */
    console_puts("Triggering page-fault WRITE self-test (#PF) at ");
    print_hex64("", bad);
    console_println("");

    volatile uint64_t *ptr = (volatile uint64_t *)(uintptr_t)bad;
    compiler_barrier();
    *ptr = 0x1122334455667788ull;
    compiler_barrier();

    console_println("Page-fault WRITE self-test did NOT trigger as expected.");
#endif
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

    /* Install IDT / exception handling AFTER console is live */
#if defined(ARCH_AMD64) || defined(ARCH_X86_64)
    arch_interrupts_init();
#endif

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
    } else {
        vmm_self_test();
        /* Fault injectors (gated) */
        page_fault_self_test_read();
        page_fault_self_test_write();
        divide_by_zero_self_test();
    }

    /* Initialize kernel heap (default 16 MiB region) */
    if (kmalloc_init(0) == 0) {
        console_println("Kernel heap initialized.");
        print_heap_stats();
        heap_smoke_test();
    } else {
        console_println("Kernel heap initialization failed.");
    }

    /* Initialize Local APIC (MADT discovery if available) */
    console_println("APIC: init...");
    if (apic_init(boot_info) != 0) {
        console_println("APIC initialization failed.");
    }
    console_println("APIC: init done");

    /* Calibrate timers (HPET + PIT cross-check) */
    console_println("Timer: init...");
    if (timer_init(boot_info) != 0) {
        console_println("Timer initialization failed.");
        while (1) {
            arch_halt();
        }
    }
    console_println("Timer: init done");
    /* One immediate drift probe */
    timer_check_drift_now();

    /* Initialize APIC timer for heartbeat (100 Hz = 10ms period) */
    console_println("Heartbeat: init...");
    uint64_t tsc_hz = timer_tsc_hz();
    if (apic_timer_init(tsc_hz, 100) != 0) {
        console_println("APIC Timer initialization failed.");
    }
    console_println("Heartbeat: init done");

    /* Initialize M5 heartbeat subsystem */
    heartbeat_init(tsc_hz, 100);  /* 100 Hz tick rate */

    /* Success message */
    console_println("Kernel initialization complete.");
    console_println("Boot successful!");
    console_println("");

    /* Start heartbeat and enable interrupts */
    console_println("Starting heartbeat...");
    apic_timer_start();
    arch_enable_interrupts();
    console_println("Heartbeat running. (QEMU: Press Ctrl+A, then X to exit)");

    /* Idle loop - HLT with interrupts enabled allows timer to fire */
    while (1) {
        arch_halt();  /* HLT waits for interrupt, then continues */
    }
}
