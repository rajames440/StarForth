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

#ifdef STARFORTH_ENABLE_VM
#include "starkernel/vm/bootstrap/sk_vm_bootstrap.h"
#include "starkernel/vm/parity.h"
#endif

/* Helper: check if memory type is RAM */
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

/* Simple integer to string conversion */
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

static void print_uint(const char *label, uint64_t value) {
    char buf[64];
    if (label) {
        console_puts(label);
    }
    itoa_simple(value, buf, 10);
    console_println(buf);
}

/* Print boot information summary */
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

/* Print the StarKernel banner */
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
    console_println("StarKernel v0.2.0-lithosananke - FORTH Microkernel");
    console_println("Architecture: amd64");
    console_puts("Build: ");
    console_puts(__DATE__);
    console_puts(" ");
    console_println(__TIME__);
    console_println("");
    console_println("UEFI BootServices: EXITED");
}

/**
 * kernel_main - Main entry point after UEFI handoff
 *
 * This is called by uefi_loader.c after ExitBootServices().
 * At this point we own the machine - no UEFI services available.
 */
void kernel_main(BootInfo *boot_info) {
    /* M1: Console initialization */
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

    /* M6: Kernel heap */
    #define KERNEL_HEAP_SIZE (16 * 1024 * 1024)  /* 16 MB */
    kmalloc_init(KERNEL_HEAP_SIZE);
    console_println("Kernel heap initialized.");
    print_heap_stats();

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
    /* M7: VM Bootstrap and Parity Validation */
    console_println("VM: bootstrap parity...");
    ParityPacket parity_pkt;
    int vm_rc = sk_vm_bootstrap_parity(&parity_pkt);
    if (vm_rc != 0) {
        console_println("VM: parity bootstrap FAILED");
    } else {
        console_println("VM: parity bootstrap complete");
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
    console_println("Heartbeat running. (QEMU: Press Ctrl+A, then X to exit)");

    /* Idle loop */
    for (;;) {
        arch_halt();
    }
}
