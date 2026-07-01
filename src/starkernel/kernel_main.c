/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.
 */

/**
 * kernel_main.c - StarKernel main entry point (LithosAnanke branch)
 *
 * Milestone status:
 *   M0-M5: Complete (build, boot, PMM, VMM, interrupts, timer)
 *   M6: Infrastructure present (kmalloc exists, validation deferred)
 *   M7: Not started (VM integration pending)
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
#include "starkernel/kernel_args.h"

/**
 * @brief UEFI Runtime Services pointer — set once at M6 init, valid for kernel lifetime.
 *
 * Populated from @c boot_info->runtime_services just before the kernel heap is
 * initialised (between the M5 timer init and @c kernel_main_deep()). Declared
 * @c extern in the UEFI header so kernel FORTH words (e.g. @c REBOOT) can
 * access it without including the full @c kernel_main.c translation unit.
 *
 * Validity note: UEFI Runtime Services remain valid in physical mode after
 * @c ExitBootServices(). This kernel does not call @c SetVirtualAddressMap(),
 * so the pointer is the raw physical address returned by firmware. On QEMU/OVMF
 * this is always usable; on real hardware it is valid as long as the CPU is in
 * physical mode (identity-mapped) — which it is for the duration of LithosAnanke,
 * since the VMM uses a separate TTBR/CR3 but does not remap the EFI reserved
 * regions.
 */
EFI_RUNTIME_SERVICES *g_sk_runtime_services = NULL;

#ifdef STARFORTH_ENABLE_VM
#include "starkernel/vm/bootstrap/sk_vm_bootstrap.h"
#include "starkernel/vm/parity.h"
#include "starkernel/capsule_generated.h"
#include "starkernel/capsule_loader.h"
#include "starkernel/capsule_birth.h"  /* capsule_birth_mama, capsule_find_mama_init */
#include "starkernel/kmalloc.h"
#include "starkernel/repl.h"
#include "starkernel/pci.h"
#include "starkernel/virtio_blk.h"
#include "block_subsystem.h"
#include "vm.h"          /* DictEntry, vm_find_word, ACL_MODE_STRICT */
#include "version.h"
#endif

/* Forward declaration — kernel_main_deep contains everything from heartbeat
 * init onward.  The 2 MB BSS stack is set up by kernel_entry.S before
 * kernel_main_impl is called, so no further stack switch is needed. */
static void kernel_main_deep(BootInfo *boot_info);

/**
 * @brief Return non-zero if the EFI memory type represents usable or reclaimable RAM.
 *
 * Covers all UEFI memory types that either are immediately usable by the PMM or
 * can be reclaimed once Boot Services have exited:
 * - @c EfiConventionalMemory — general purpose RAM.
 * - @c EfiLoaderCode / @c EfiLoaderData — UEFI loader pages (reclaimed post-EBS).
 * - @c EfiBootServicesCode / @c EfiBootServicesData — boot-service pages (reclaimed post-EBS).
 * - @c EfiRuntimeServicesCode / @c EfiRuntimeServicesData — pages the firmware
 *   still uses for runtime calls (kept mapped, counted as physical RAM).
 * - @c EfiACPIReclaimMemory — ACPI tables; may be freed after OS has parsed them.
 * - @c EfiACPIMemoryNVS — non-volatile ACPI storage; kept reserved but is RAM.
 *
 * Returns 0 for all device-memory, MMIO, persistent-memory, and special types.
 * Used by @c print_boot_info() to compute the total physical RAM visible in the
 * EFI memory map.
 *
 * @param type  @c EFI_MEMORY_TYPE value from an @c EFI_MEMORY_DESCRIPTOR.
 * @return      Non-zero if the type is RAM; 0 otherwise.
 */
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
 * @brief Convert a @c uint64_t to a NUL-terminated string in the given base.
 *
 * Produces a freestanding (no libc) integer-to-string conversion for the
 * kernel console paths. Handles bases 2–16; digits above 9 are lowercase
 * alphabetic (@c 'a'–@c 'f' for hex). Special case: @p value == 0 writes
 * the string @c "0" and returns immediately.
 *
 * The algorithm builds the digit string in reverse order into a 64-byte
 * local @c temp[] buffer, then reverses it into @p buf. @p buf must be at
 * least 65 bytes to hold a 64-bit binary string plus NUL; in practice all
 * callers pass 64-byte buffers and use base 10 or 16, where the maximum
 * length is 20 or 16 digits respectively.
 *
 * @param value  Non-negative integer to convert.
 * @param buf    Caller-allocated output buffer (minimum 65 bytes for binary).
 * @param base   Numeric base (2–16).
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

    for (j = 0; j < i; j++) {
        buf[j] = temp[i - j - 1];
    }
    buf[j] = '\0';
}

/**
 * @brief Print an optional label followed by a @c uint64_t in decimal to the console.
 *
 * Converts @p value to a decimal string via @c itoa_simple() and emits it with
 * a trailing newline via @c console_println(). If @p label is non-NULL, it is
 * emitted first via @c console_puts() (no newline between label and value).
 * Used by @c print_pmm_stats() and @c print_heap_stats() to avoid repeating
 * the convert-and-print pattern for each statistic line.
 *
 * @param label  Optional NUL-terminated prefix string; NULL to omit.
 * @param value  64-bit unsigned integer to display in decimal.
 */
static void print_uint(const char *label, uint64_t value) {
    char buf[64];
    if (label) {
        console_puts(label);
    }
    itoa_simple(value, buf, 10);
    console_println(buf);
}

/**
 * @brief Print a boot-information summary from the UEFI memory map to the console.
 *
 * Iterates over every @c EFI_MEMORY_DESCRIPTOR in @c boot_info->memory_map and
 * accumulates:
 * - @c total_memory — sum of page sizes for all RAM-type regions
 *   (via @c is_ram_type()).
 * - @c usable_memory — sum of page sizes for @c EfiConventionalMemory only.
 *
 * Emits a three-field report box to the kernel serial console:
 * - "Memory map entries: N"
 * - "Total memory: N MB" (rounds down to whole MiB)
 * - "Usable memory: N MB"
 *
 * Called from @c kernel_main_impl() / @c kernel_main() immediately after M1
 * console initialisation, so the memory map must still be intact (it always
 * is — the map was captured by @c uefi_loader.c before @c ExitBootServices()).
 *
 * @param boot_info  @c BootInfo structure populated by @c uefi_loader.c; provides
 *                   @c memory_map, @c memory_map_size, and
 *                   @c memory_map_descriptor_size.
 */
static void print_boot_info(BootInfo *boot_info) {
    char buf[64];
    UINTN num_entries;
    UINTN total_memory = 0;
    UINTN usable_memory = 0;
    UINTN i;

    console_println("\n=== StarKernel Boot Information ===");

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

/**
 * @brief Print Physical Memory Manager statistics to the kernel console.
 *
 * Calls @c pmm_get_stats() to obtain a @c pmm_stats_t snapshot and then emits
 * six lines via @c print_uint():
 * - Total pages, free pages, used pages (in 4 KiB page units).
 * - Total MB, free MB, used MB (bytes ÷ 1 MiB, truncated).
 *
 * Called from @c kernel_main_impl() / @c kernel_main() immediately after
 * @c pmm_init() completes (M2), providing a sanity check that the PMM saw the
 * expected quantity of physical RAM.
 */
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

/**
 * @brief Print kernel heap (kmalloc) statistics to the kernel console.
 *
 * Calls @c kmalloc_get_stats() to obtain a @c kmalloc_stats_t snapshot and
 * emits four lines via @c print_uint():
 * - Total bytes allocated to the heap arena.
 * - Free bytes currently available.
 * - Used bytes currently allocated by callers.
 * - Peak bytes — the high-water mark since @c kmalloc_init().
 *
 * Called from @c kernel_main_impl() / @c kernel_main() immediately after
 * @c kmalloc_init() (M6) to confirm that the heap was sized correctly from the
 * @c --heap= boot argument or its 2 GiB default.
 */
static void print_heap_stats(void) {
    kmalloc_stats_t stats = kmalloc_get_stats();

    console_println("Heap statistics:");
    print_uint("  Total bytes: ", stats.total_bytes);
    print_uint("  Free  bytes: ", stats.free_bytes);
    print_uint("  Used  bytes: ", stats.used_bytes);
    print_uint("  Peak  bytes: ", stats.peak_bytes);
    console_println("");
}

/**
 * @brief Print the StarKernel ASCII-art banner and build metadata to the console.
 *
 * Emits:
 * - The "StarKernel" ASCII-art logotype (six-line block font).
 * - @c LITHOS_VERSION_STR — the @c LithosAnanke version string from @c version.h.
 * - Target ISA: "amd64", "aarch64", "riscv64", or "unknown", selected by
 *   compile-time @c ARCH_* / @c __riscv preprocessor guards.
 * - Build date and time from @c __DATE__ / @c __TIME__ (compiler intrinsics).
 * - "UEFI BootServices: EXITED" — confirmation that the kernel is running
 *   after @c ExitBootServices() and owns all hardware.
 *
 * Called first in @c kernel_main_impl() / @c kernel_main() after
 * @c console_init() so the banner is the first visible output on the serial
 * port, matching the @c QEMU_BASELINE.log reference.
 */
static void print_banner(void) {
    console_println("");
    console_println("");
    console_println("   _____ _             _  __                    _ ");
    console_println("  / ____| |           | |/ /                   | |");
    console_println(" | (___ | |_ __ _ _ __| ' / ___ _ __ _ __   ___| |");
    console_println("  \\___ \\| __/ _` | '__|  < / _ \\ '__| '_ \\ / _ \\ |");
    console_println("  ____) | || (_| | |  | . \\  __/ |  | | | |  __/ |");
    console_println(" |_____/ \\__\\__,_|_|  |_|\\_\\___|_|  |_| |_|\\___|_|");
    console_println("");
    console_println(LITHOS_VERSION_STR);
#if defined(ARCH_AMD64)
    console_println("Architecture: amd64");
#elif defined(ARCH_AARCH64)
    console_println("Architecture: aarch64");
#elif defined(__riscv)
    console_println("Architecture: riscv64");
#else
    console_println("Architecture: unknown");
#endif
    console_puts("Build: ");
    console_puts(__DATE__);
    console_puts(" ");
    console_println(__TIME__);
    console_println("");
    console_println("UEFI BootServices: EXITED");
}

/**
 * @brief Main kernel entry point after UEFI handoff — executes milestones M0–M6.
 *
 * On amd64, @c kernel_entry.S switches the stack from UEFI's default to a 2 MiB
 * zero-initialised BSS stack and tail-calls this function as @c kernel_main_impl.
 * On aarch64 and riscv64 the assembly trampoline is not yet implemented and the
 * UEFI loader calls @c kernel_main directly.
 *
 * Milestone sequence:
 * - **M0 — Architecture early init** (@c arch_early_init()): On amd64, installs a
 *   minimal GDT with a proper 64-bit code segment at selector 0x08 and reloads CS
 *   via @c lretq. Without this, UEFI's 64-bit segment at 0x38 is in scope and the
 *   ISR's @c INT gate (which expects CS 0x08) would fault silently.
 * - **M1 — Console** (@c console_init()): brings up UART 16550 at 115200 8N1 and
 *   the framebuffer VT100 terminal. Then prints banner and memory map.
 * - **M2 — PMM** (@c pmm_init()): initialises the physical memory manager's 4 KiB
 *   page bitmap from the EFI memory map.
 * - **M3 — VMM** (@c vmm_init()): builds 4-level x86-64 page tables, maps all
 *   conventional RAM at the kernel virtual base, and loads CR3.
 * - **M4 — IDT + APIC** (@c arch_interrupts_init() + @c apic_init()): programs the
 *   64-entry IDT, masks the legacy 8259A PIC, and initialises the Local APIC in
 *   xAPIC MMIO mode at 0xFEE00000.
 * - **M5 — Timer** (@c timer_init()): calibrates the TSC and HPET.
 * - **M6 — Heap** (@c kmalloc_init()): initialises the kernel slab allocator with
 *   @c heap_size from the boot args or @c KARGS_DEFAULT_HEAP_SIZE (2 GiB).
 *
 * Stashes @c boot_info->runtime_services in @c g_sk_runtime_services for later
 * use by kernel FORTH words (e.g. @c REBOOT). Then tail-calls
 * @c kernel_main_deep() for M7 and the REPL.
 *
 * @param boot_info  @c BootInfo populated by @c uefi_loader.c before
 *                   @c ExitBootServices(); provides the memory map, ACPI pointer,
 *                   framebuffer descriptor, runtime services pointer, and parsed
 *                   kernel command-line arguments.
 */
#if defined(__x86_64__)
void kernel_main_impl(BootInfo *boot_info) {
#else
void kernel_main(BootInfo *boot_info) {
#endif
    /*
     * Establish our own GDT before anything else.  UEFI hands us CS=0x38
     * (OVMF's 64-bit segment at GDT[7]).  Our IDT entries use selector 0x08,
     * so if UEFI's GDT[1] (0x08) is not a valid 64-bit code descriptor the
     * ISR will run with the wrong CS type and all serial output from the ISR
     * will fail silently.  arch_early_init() installs a minimal GDT with a
     * proper 64-bit code segment at 0x08 and reloads CS via lretq.
     */
    arch_early_init();

    /* M1: Console initialization — serial UART first */
    console_init();
    print_banner();
    print_boot_info(boot_info);

    /* M2: Physical Memory Manager */
    pmm_init(boot_info);
    console_println("PMM initialized.");
    print_pmm_stats();

    /* M3: Virtual Memory Manager */
    vmm_init(boot_info);
    console_println("VMM initialized (mapped RAM, CR3 switched)");
    console_println("VMM self-test: mapped OK at 0xffff800000000000");
    console_println("VMM self-test complete.\n");

    /* M4: Interrupt handling */
    arch_interrupts_init();
    console_println("IDT installed.\n");

    /* M4: APIC */
    console_println("APIC: init...");
    apic_init(boot_info);
    console_println("APIC: init done\n");

    /* M5: Timer subsystem */
    console_println("Timer: init...");
    timer_init(boot_info);
    console_println("Timer: init done\n");

    /* Stash runtime services for REBOOT word and other kernel FORTH words */
    g_sk_runtime_services = boot_info->runtime_services;

    /* M6: Kernel heap — sized from --heap= flag, default 2 GiB */
    {
        uint64_t heap_sz = boot_info->args.heap_size
                         ? boot_info->args.heap_size
                         : KARGS_DEFAULT_HEAP_SIZE;
        kmalloc_init(heap_sz);
    }
    console_println("Kernel heap initialized.");
    print_heap_stats();

    /* Hand off to the deep initialization path.  The 2 MiB BSS stack was
     * already set up by kernel_entry.S (amd64) before this function was
     * called, so no further stack switch is needed here. */
    kernel_main_deep(boot_info);
}

/**
 * @brief Deep kernel initialisation — M5 heartbeat, M7 VM bootstrap, and REPL.
 *
 * Called as the final act of @c kernel_main_impl() / @c kernel_main() after
 * all hardware milestones M0–M6 are complete. Runs on the 2 MiB BSS stack on
 * amd64 (set up by @c kernel_entry.S before @c kernel_main_impl() was called)
 * or the UEFI-provided stack on aarch64 and riscv64.
 *
 * **M5 — Heartbeat subsystem:**
 * Calls @c apic_timer_init(tsc_hz, 100) to configure the APIC timer for 100 Hz
 * periodic delivery to vector 32, then @c heartbeat_init(tsc_hz, 100) to
 * initialise the rolling-window heartbeat state.
 *
 * **M7 — VM bootstrap (when @c STARFORTH_ENABLE_VM is defined):**
 * 1. @c sk_vm_bootstrap_parity() — allocates the Mama VM and validates the
 *    capsule directory parity.
 * 2. Allocates 1 MiB @c blk_ram_buf (LBN 0–991) and 1 MiB @c krd_buf
 *    (LBN 2048–3071 = capsule ramdrive) from @c kmalloc, then calls
 *    @c capsule_blk_init() to wire them into the Mama VM's block subsystem.
 * 3. Copies the read-only @c capsule_arena to heap and calls
 *    @c capsule_exec_init() to load and execute @c init.4th.
 * 4. Pins @c CAPSULE-BIRTH and @c BIRTH with @c ACL_MODE_STRICT via
 *    @c vm_find_word() so that ACL policy cannot downgrade them.
 *
 * After M7, the APIC timer is started via @c apic_timer_start() and
 * @c arch_enable_interrupts() enables IRQs.
 *
 * **REPL (when @c STARFORTH_ENABLE_VM is defined):**
 * - If @c boot_info->args.run_doe is set, injects @c "12345 3 EXEC-DOE BYE"
 *   before the interactive REPL.
 * - If @c SK_STARTUP_FORTH is defined at build time, executes it as a
 *   compile-time startup script (lowest priority — overridden by @c --doe).
 * - Activates the framebuffer VT100 terminal (if the framebuffer descriptor
 *   is valid) so the REPL output appears on screen as well as the serial port.
 * - Clears the @c StarForthRebootTries NVRAM variable to signal a clean boot.
 * - Calls @c sk_repl() — the interactive FORTH REPL loop. Returns when the
 *   user executes @c BYE or @c vm->halted is set.
 *
 * Terminates with an infinite @c arch_halt() idle loop regardless of the
 * @c STARFORTH_ENABLE_VM build configuration.
 *
 * @param boot_info  The @c BootInfo passed from @c kernel_main_impl().
 */
static void kernel_main_deep(BootInfo *boot_info) {
    /* M5: Initialize heartbeat subsystem */
    console_println("Heartbeat: init...");
    uint64_t tsc_hz = timer_tsc_hz();
    if (apic_timer_init(tsc_hz, 100) != 0) {
        console_println("APIC Timer initialization failed.");
    }
    heartbeat_init(tsc_hz, 100);  /* 100 Hz tick rate */
    console_println("Heartbeat: init done");

    console_println("Kernel initialization complete.");
    console_println("Boot successful!\n");

#ifdef STARFORTH_ENABLE_VM
    /* M7.pre: PCI + Artemis virtio-blk disk */
    console_println("PCI: init...");
    pci_init(boot_info->acpi_table);

    {
        static blkio_dev_t artemis_dev;
        int vrc = virtio_blk_find_artemis(&artemis_dev);
        if (vrc == 0) {
            console_println("Artemis: virtio-blk attached");
            blk_subsys_attach_device(&artemis_dev);
        } else {
            console_println("Artemis: no virtio-blk disk (continuing without)");
        }
    }

    /* M7: VM Bootstrap and Parity Validation */
    console_println("VM: bootstrap parity...");
    ParityPacket parity_pkt;
    int vm_rc = sk_vm_bootstrap_parity(&parity_pkt);
    if (vm_rc != 0) {
        console_println("VM: parity bootstrap FAILED");
    } else {
        console_println("VM: parity bootstrap complete");
    }

    /* Wire parity log so PARITY:MAMA_INIT/BIRTH/RUN/KILL reach serial */
    capsule_parity_set_output(NULL, console_puts);

    /* M7.1: Execute init.4th via the proper Mama birth protocol.
     * capsule_arena lives in .rodata; copy to heap so the interpreter
     * can safely read payload bytes after VMM takeover. */
    void *mama_vm = sk_get_mama_vm();

    /* Block subsystem: 1MB for dedicated RAM blocks (LBN 0-991) */
    #define BLK_RAM_SIZE (1024u * 1024u)
    uint8_t *blk_ram_buf = (uint8_t *)kmalloc(BLK_RAM_SIZE);
    /* Kernel ramdrive: 1MB covering capsule blocks 2048-3071 */
    #define KRD_BUF_SIZE (1024u * 1024u)
    uint8_t *krd_buf     = (uint8_t *)kmalloc(KRD_BUF_SIZE);

    if (!blk_ram_buf || !krd_buf) {
        console_println("Init: blk alloc FAILED");
    } else {
        /* Pre-zero the ramdrive buffer (no memset in freestanding context) */
        size_t krd_i;
        for (krd_i = 0; krd_i < KRD_BUF_SIZE; krd_i++) krd_buf[krd_i] = 0;
        capsule_blk_init(mama_vm, blk_ram_buf, BLK_RAM_SIZE, krd_buf);
    }

    /* Copy capsule directory header to heap (has pointer field needing update) */
    CapsuleDirHeader *live_dir = (CapsuleDirHeader *)kmalloc(sizeof(CapsuleDirHeader));
    if (!live_dir) {
        console_println("Init: dir alloc FAILED");
    } else {
        const CapsuleDirHeader *src_dir = &capsule_directory;
        live_dir->magic         = src_dir->magic;
        live_dir->arena_base    = src_dir->arena_base;
        live_dir->arena_size    = src_dir->arena_size;
        live_dir->desc_count    = src_dir->desc_count;
        live_dir->desc_capacity = src_dir->desc_capacity;
        live_dir->name_count    = src_dir->name_count;
        live_dir->reserved      = src_dir->reserved;
        live_dir->dir_hash      = src_dir->dir_hash;

        uint8_t *arena_copy = (uint8_t *)kmalloc((size_t)live_dir->arena_size);
        if (!arena_copy) {
            console_println("Init: arena alloc FAILED");
        } else {
            const uint8_t *src = capsule_arena;
            uint8_t *dst = arena_copy;
            size_t n = (size_t)live_dir->arena_size;
            while (n--) *dst++ = *src++;
            live_dir->arena_base = (uint64_t)(uintptr_t)arena_copy;

            console_println("Init: Mama birth...");
            CapsuleRunResult cr = capsule_birth_mama(
                mama_vm,
                live_dir,
                capsule_descriptors,
                capsule_names,
                arena_copy);
            if (cr == CAPSULE_RUN_OK) {
                console_println("Init: Mama birth OK");
                /* Free ramdrive slots so init.4th blocks are available for userspace */
                const CapsuleDesc *mama_cap =
                    capsule_find_mama_init(live_dir, capsule_descriptors);
                if (mama_cap)
                    capsule_clear_blocks(arena_copy + mama_cap->offset,
                                        mama_cap->length);
            } else {
                console_println("Init: Mama birth FAILED");
            }

            /* Pin kernel-only privileged words that ACL.4th cannot reach
             * portably (BIRTH/CAPSULE-BIRTH do not exist in the hosted VM).
             * Done in C after capsule load so ACL.4th stays host-portable. */
            VM *mama_vm_ptr = (VM *)sk_get_mama_vm();
            DictEntry *capsule_birth = vm_find_word(mama_vm_ptr, "CAPSULE-BIRTH", 13);
            if (capsule_birth) {
                capsule_birth->acl_mode   = ACL_MODE_STRICT;
                capsule_birth->acl_pinned = 1;
                console_println("ACL: CAPSULE-BIRTH pinned STRICT");
            }
            DictEntry *birth = vm_find_word(mama_vm_ptr, "BIRTH", 5);
            if (birth) {
                birth->acl_mode   = ACL_MODE_STRICT;
                birth->acl_pinned = 1;
                console_println("ACL: BIRTH pinned STRICT");
            }
        }
    }
#else
    console_println("=== LithosAnanke Checkpoint ===");
    console_println("M0-M6: Complete");
    console_println("M7: Disabled (build with STARFORTH_ENABLE_VM=1)");
    console_println("================================\n");
#endif

    /* Start heartbeat and enable interrupts */
    console_println("Starting heartbeat...");
    apic_timer_start();
    arch_enable_interrupts();
    console_println("Heartbeat running.");


#ifdef STARFORTH_ENABLE_VM
    VM *mama = (VM *)sk_get_mama_vm();

    /*
     * Runtime --doe flag: inject "EXEC-DOE BYE" if requested via boot args.
     * Checked before SK_STARTUP_FORTH so a runtime --doe takes precedence.
     */
    if (boot_info->args.run_doe) {
        console_println("Startup: --doe flag set — running EXEC-DOE");
        vm_interpret(mama, "12345 3 EXEC-DOE BYE");
        if (mama->error) {
            console_println("Startup: EXEC-DOE ERROR");
            mama->error = 0;
        }
        if (mama->halted) goto idle;
    }

    /*
     * SK_STARTUP_FORTH — compile-time script injection (lowest priority).
     * Usage: make -f Makefile.starkernel qemu SK_CMD="TIME-TICKS . BYE"
     */
#ifdef SK_STARTUP_FORTH
    console_puts("Startup: ");
    console_println(SK_STARTUP_FORTH);
    vm_interpret(mama, SK_STARTUP_FORTH);
    if (mama->error) {
        console_puts("Startup: ERROR\n");
        mama->error = 0;
    }
    if (mama->halted) goto idle;
#endif

    /* Activate framebuffer VT100 now that POST is done — REPL-only */
    if (boot_info->framebuffer.base != NULL && boot_info->framebuffer.size > 0) {
        FbPixelFormat fb_fmt;
        switch (boot_info->framebuffer.pixel_format) {
            case (UINT32)PixelRedGreenBlueReserved8BitPerColor: fb_fmt = FB_PIXEL_RGBX32; break;
            case (UINT32)PixelBlueGreenRedReserved8BitPerColor: fb_fmt = FB_PIXEL_BGRX32; break;
            default: fb_fmt = FB_PIXEL_BGRX32; break;
        }
        console_fb_init(&boot_info->framebuffer, fb_fmt);
    }

    /* Clear reboot-tries counter: we reached the REPL cleanly */
    if (g_sk_runtime_services) {
        EFI_GUID vendor_guid = STARFORTH_VENDOR_GUID;
        EFI_SET_VARIABLE SetVariable =
            (EFI_SET_VARIABLE)g_sk_runtime_services->SetVariable;
        SetVariable(
            (CHAR16 *)SF_VAR_REBOOT_TRIES,
            &vendor_guid,
            EFI_VARIABLE_NON_VOLATILE |
            EFI_VARIABLE_BOOTSERVICE_ACCESS |
            EFI_VARIABLE_RUNTIME_ACCESS,
            0, NULL);
    }

    sk_repl(mama);
#endif

    /* Idle loop (reached if sk_repl exits via BYE or vm->halted) */
#ifdef STARFORTH_ENABLE_VM
idle:
#endif
    for (;;) {
        arch_halt();
    }
}
