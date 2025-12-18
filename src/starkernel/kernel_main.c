/**
 * kernel_main.c - StarKernel main entry point
 * Called after UEFI boot services have been exited
 */

#include "uefi.h"
#include "console.h"
#include "arch.h"

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
        total_memory += size;

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
