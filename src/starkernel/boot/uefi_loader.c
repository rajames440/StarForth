/**
 * uefi_loader.c - UEFI boot loader for StarKernel
 * Entry point from UEFI firmware
 */

#include "uefi.h"
#include "arch.h"

/* Forward declaration of kernel_main */
extern void kernel_main(BootInfo *boot_info);

/* Global boot info structure */
static BootInfo g_boot_info = {0};

/**
 * UEFI Entry Point
 * Called by UEFI firmware when the EFI application starts
 *
 * @param ImageHandle - Handle to this EFI application
 * @param SystemTable - Pointer to UEFI System Table
 * @return EFI_STATUS - Should not return (we exit boot services and jump to kernel)
 */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN memory_map_size = 0;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;

    /* Print boot message via UEFI console (before exiting boot services) */
    SystemTable->ConOut->Reset(SystemTable->ConOut, FALSE);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"StarKernel UEFI Loader\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Collecting boot information...\r\n");
    (void)ImageHandle; /* currently unused after removing ExitBootServices */

    /* Size the memory map once and allocate with headroom */
    status = SystemTable->BootServices->GetMemoryMap(
        &memory_map_size,
        NULL,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    if (status != EFI_BUFFER_TOO_SMALL && status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"ERROR: Failed to size memory map\r\n");
        return status;
    }

    memory_map_size += descriptor_size * 16; /* headroom */
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, memory_map_size, (void **)&memory_map);
    if (status != EFI_SUCCESS || memory_map == NULL) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"ERROR: Failed to allocate memory map buffer\r\n");
        return status;
    }

    /* Fetch memory map and exit boot services with retries if key invalidates */
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        UINTN map_sz = memory_map_size;
        status = SystemTable->BootServices->GetMemoryMap(
            &map_sz,
            memory_map,
            &map_key,
            &descriptor_size,
            &descriptor_version
        );

        if (status == EFI_BUFFER_TOO_SMALL) {
            /* Expand buffer and retry sizing */
            memory_map_size = map_sz + descriptor_size * 4;
            SystemTable->BootServices->FreePool(memory_map);
            status = SystemTable->BootServices->AllocatePool(EfiLoaderData, memory_map_size, (void **)&memory_map);
            if (status != EFI_SUCCESS || memory_map == NULL) {
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"ERROR: Failed to grow memory map buffer\r\n");
                return status;
            }
            continue;
        }

        if (status != EFI_SUCCESS) {
            SystemTable->ConOut->OutputString(SystemTable->ConOut, L"ERROR: Failed to get memory map\r\n");
            return status;
        }

        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Memory map ready; jumping to kernel...\r\n");

        /* Store memory map info in boot info */
        g_boot_info.memory_map = memory_map;
        g_boot_info.memory_map_size = map_sz;
        g_boot_info.memory_map_descriptor_size = descriptor_size;
        g_boot_info.runtime_services = SystemTable->RuntimeServices;
        g_boot_info.acpi_table = NULL;  /* TODO: Find ACPI tables */
        g_boot_info.framebuffer = NULL; /* TODO: Get framebuffer info */

        status = EFI_SUCCESS;
        break;
    }

    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"ERROR: Failed to transition to kernel.\r\n");
        return status;
    }

    /* We are now in control - UEFI boot services are no longer available */
    /* Jump to kernel proper */
    kernel_main(&g_boot_info);

    /* Should never reach here */
    while (1) {
        arch_halt();
    }

    return EFI_SUCCESS;
}
