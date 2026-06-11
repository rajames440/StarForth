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
 * uefi_loader.c - UEFI boot loader for StarKernel
 *
 * This loader loads the StarKernel ELF binary from the ESP,
 * parses it, loads segments, applies relocations, and jumps to entry.
 */

#include "uefi.h"
#include "arch.h"
#include "elf_loader.h"
#include "elf64.h"
#include "starkernel/cmdline.h"
#include "starkernel/boot_info_offsets.h"
#include <stddef.h>

/* Verify BootInfo field offsets match boot_info_offsets.h (BOOT_INFO_OFFSETS) */
_Static_assert(offsetof(BootInfo, kernel_stack_base) == BOOT_INFO_KERNEL_STACK_BASE_OFFSET,
    "BOOT_INFO_KERNEL_STACK_BASE_OFFSET mismatch — update boot_info_offsets.h");
_Static_assert(offsetof(BootInfo, kernel_stack_size) == BOOT_INFO_KERNEL_STACK_SIZE_OFFSET,
    "BOOT_INFO_KERNEL_STACK_SIZE_OFFSET mismatch — update boot_info_offsets.h");

/* For monolithic build: kernel_main is linked directly */
#ifdef MONOLITHIC_BUILD
extern void kernel_main(BootInfo *boot_info);
#endif

#if defined(ARCH_AMD64)
#define COM1_BASE 0x3F8
static inline void raw_outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t raw_inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void raw_serial_init(void)
{
    /* Disable interrupts */
    raw_outb(COM1_BASE + 1, 0x00);
    /* Enable DLAB */
    raw_outb(COM1_BASE + 3, 0x80);
    /* Divisor 1 = 115200 baud */
    raw_outb(COM1_BASE + 0, 0x01);
    raw_outb(COM1_BASE + 1, 0x00);
    /* 8N1 */
    raw_outb(COM1_BASE + 3, 0x03);
    /* Enable FIFO, clear, 14-byte threshold */
    raw_outb(COM1_BASE + 2, 0xC7);
    /* RTS/DSR set */
    raw_outb(COM1_BASE + 4, 0x0B);
}

static void raw_serial_putc(char c)
{
    while ((raw_inb(COM1_BASE + 5) & 0x20) == 0) { }
    raw_outb(COM1_BASE + 0, (uint8_t)c);
}

static void raw_serial_puts(const char *s)
{
    while (*s)
    {
        char c = *s++;
        if (c == '\n') raw_serial_putc('\r');
        raw_serial_putc(c);
    }
}
#define RAW_LOG(str) raw_serial_puts(str)
#else
#define RAW_LOG(str) ((void)0)
#endif

static BootInfo g_boot_info = {0};

static int guid_equals(const EFI_GUID* a, const EFI_GUID* b)
{
    return a->Data1 == b->Data1 &&
           a->Data2 == b->Data2 &&
           a->Data3 == b->Data3 &&
           a->Data4[0] == b->Data4[0] &&
           a->Data4[1] == b->Data4[1] &&
           a->Data4[2] == b->Data4[2] &&
           a->Data4[3] == b->Data4[3] &&
           a->Data4[4] == b->Data4[4] &&
           a->Data4[5] == b->Data4[5] &&
           a->Data4[6] == b->Data4[6] &&
           a->Data4[7] == b->Data4[7];
}

#ifndef MONOLITHIC_BUILD
/* Kernel entry point signature */
typedef void (*KernelEntry)(BootInfo *boot_info);

/* Kernel ELF buffer - dynamically allocated via UEFI Boot Services */
#define KERNEL_MAX_SIZE (8 * 1024 * 1024)
static uint8_t *kernel_elf_buffer = NULL;
static uint64_t kernel_elf_size = 0;

/* Load kernel.elf from ESP using Simple File System Protocol */
static EFI_STATUS load_kernel_from_esp(EFI_HANDLE ImageHandle, EFI_BOOT_SERVICES *BS)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *kernel_file = NULL;

    RAW_LOG("Loading kernel.elf from ESP...\n");

    /* Allocate buffer for kernel ELF using UEFI Boot Services */
    UINTN pages_needed = (KERNEL_MAX_SIZE + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS buffer_addr = 0;
    status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages_needed, &buffer_addr);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to allocate kernel buffer\n");
        return status;
    }
    kernel_elf_buffer = (uint8_t *)(uintptr_t)buffer_addr;
    RAW_LOG("Kernel buffer allocated\n");

    /* Get loaded image protocol */
    status = BS->HandleProtocol(ImageHandle, (EFI_GUID *)&EFI_LOADED_IMAGE_PROTOCOL_GUID,
                                (void **)&loaded_image);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to get LoadedImageProtocol\n");
        return status;
    }

    /* Get file system protocol from device handle */
    status = BS->HandleProtocol(loaded_image->DeviceHandle,
                                (EFI_GUID *)&EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
                                (void **)&fs);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to get FileSystemProtocol\n");
        return status;
    }

    /* Open volume (root directory) */
    status = fs->OpenVolume(fs, &root);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to open volume\n");
        return status;
    }

    /* Open kernel.elf file */
    status = root->Open(root, &kernel_file, L"kernel.elf",
                       EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to open kernel.elf\n");
        root->Close(root);
        return status;
    }

    /* Get file size */
    EFI_FILE_INFO file_info_buffer[128]; /* Should be enough for file info */
    UINTN file_info_size = sizeof(file_info_buffer);
    status = kernel_file->GetInfo(kernel_file, (EFI_GUID *)&EFI_FILE_INFO_GUID,
                                  &file_info_size, file_info_buffer);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to get file info\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }

    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *)file_info_buffer;
    kernel_elf_size = file_info->FileSize;

    /* Check if kernel fits in buffer */
    if (kernel_elf_size > KERNEL_MAX_SIZE) {
        RAW_LOG("Kernel too large for buffer\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return EFI_BUFFER_TOO_SMALL;
    }

    /* Read kernel into buffer */
    UINTN read_size = kernel_elf_size;
    status = kernel_file->Read(kernel_file, &read_size, kernel_elf_buffer);
    if (status != EFI_SUCCESS || read_size != kernel_elf_size) {
        RAW_LOG("Failed to read kernel.elf\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status != EFI_SUCCESS ? status : EFI_ABORTED;
    }

    RAW_LOG("Kernel loaded successfully\n");

    /* Close files */
    kernel_file->Close(kernel_file);
    root->Close(root);

    return EFI_SUCCESS;
}
#endif /* !MONOLITHIC_BUILD */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;

    /* Static map buffer (256 KiB) to avoid allocations in Phase B */
    static EFI_MEMORY_DESCRIPTOR memory_map_static[256 * 1024 / sizeof(EFI_MEMORY_DESCRIPTOR)];
    EFI_MEMORY_DESCRIPTOR *memory_map = memory_map_static;
    UINTN memory_map_capacity = sizeof(memory_map_static);

    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
    int exited_boot_services = 0;

    /* Phase A: safe to use UEFI console and file services */
    SystemTable->ConOut->Reset(SystemTable->ConOut, FALSE);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"StarKernel UEFI Loader\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Loading kernel from ESP...\r\n");

#if defined(ARCH_AMD64)
    raw_serial_init();
    RAW_LOG("RAW SERIAL UP\n");
#endif

#ifndef MONOLITHIC_BUILD
    /* Load kernel.elf from ESP BEFORE ExitBootServices */
    status = load_kernel_from_esp(ImageHandle, BS);
    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"FATAL: Failed to load kernel.elf\r\n");
        RAW_LOG("FATAL: Failed to load kernel.elf\n");
        while (1) arch_halt();
        return status;
    }
#else
    RAW_LOG("Monolithic build - kernel linked directly\n");
#endif

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Collecting boot information...\r\n");

    /* ------------------------------------------------------------------ */
    /* Phase A-1: Parse kernel command line                                */
    /*                                                                     */
    /* Priority (highest first):                                           */
    /*   1. StarForthBootArgs NVRAM variable (one-shot, set by REBOOT)    */
    /*   2. EFI_LOADED_IMAGE_PROTOCOL->LoadOptions (firmware/boot mgr)    */
    /*   3. Empty string → all KernelArgs defaults                        */
    /* ------------------------------------------------------------------ */
    {
        char cmdline_buf[KERNEL_ARGS_CMDLINE_MAX];
        int  used_nvram = 0;

        cmdline_buf[0] = '\0';

        /* Check one-shot NVRAM variable first */
        {
            EFI_GUID vendor_guid = STARFORTH_VENDOR_GUID;
            EFI_GET_VARIABLE GetVariable =
                (EFI_GET_VARIABLE)SystemTable->RuntimeServices->GetVariable;
            UINTN data_size = (UINTN)(KERNEL_ARGS_CMDLINE_MAX - 1);
            UINT32 attrs = 0;

            EFI_STATUS vs = GetVariable(
                (CHAR16 *)SF_VAR_BOOT_ARGS,
                &vendor_guid,
                &attrs,
                &data_size,
                cmdline_buf);

            if (vs == EFI_SUCCESS && data_size > 0) {
                cmdline_buf[data_size] = '\0';
                used_nvram = 1;
                RAW_LOG("CmdLine: from NVRAM StarForthBootArgs\n");

                /* One-shot: clear the variable immediately */
                EFI_SET_VARIABLE SetVariable =
                    (EFI_SET_VARIABLE)SystemTable->RuntimeServices->SetVariable;
                SetVariable(
                    (CHAR16 *)SF_VAR_BOOT_ARGS,
                    &vendor_guid,
                    EFI_VARIABLE_NON_VOLATILE |
                    EFI_VARIABLE_BOOTSERVICE_ACCESS |
                    EFI_VARIABLE_RUNTIME_ACCESS,
                    0, NULL);
            }
        }

        /* Fall back to LoadOptions if NVRAM was empty */
        if (!used_nvram) {
            EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
            EFI_STATUS li_status = BS->HandleProtocol(
                ImageHandle,
                (EFI_GUID *)&EFI_LOADED_IMAGE_PROTOCOL_GUID,
                (void **)&loaded_image);

            if (li_status == EFI_SUCCESS && loaded_image &&
                loaded_image->LoadOptionsSize > 0 && loaded_image->LoadOptions) {
                CHAR16 *opts = (CHAR16 *)loaded_image->LoadOptions;
                UINTN   n    = loaded_image->LoadOptionsSize / sizeof(CHAR16);
                UINTN   i;
                for (i = 0; i + 1 < (UINTN)(KERNEL_ARGS_CMDLINE_MAX) && i < n && opts[i]; i++)
                    cmdline_buf[i] = (char)(opts[i] & 0xFFu);
                cmdline_buf[i] = '\0';
                RAW_LOG("CmdLine: from LoadOptions\n");
            }
        }

        cmdline_parse_ascii(cmdline_buf, &g_boot_info.args);
        RAW_LOG("CmdLine: parsed OK\n");
    }

    /* ------------------------------------------------------------------ */
    /* Phase A-2: Allocate dynamic kernel stack if --stack= was given     */
    /* ------------------------------------------------------------------ */
    {
        uint64_t stack_sz = g_boot_info.args.stack_size
                          ? g_boot_info.args.stack_size
                          : (uint64_t)0;

        g_boot_info.kernel_stack_base = NULL;
        g_boot_info.kernel_stack_size = 0;

        if (stack_sz > 0) {
            UINTN pages = (UINTN)((stack_sz + 4095u) / 4096u);
            EFI_PHYSICAL_ADDRESS stack_addr = 0;
            EFI_STATUS sa = BS->AllocatePages(
                AllocateAnyPages, EfiLoaderData, pages, &stack_addr);
            if (sa == EFI_SUCCESS) {
                g_boot_info.kernel_stack_base = (void *)(uintptr_t)stack_addr;
                g_boot_info.kernel_stack_size = (uint64_t)(pages * 4096u);
                RAW_LOG("Stack: dynamic stack allocated\n");
            } else {
                RAW_LOG("Stack: dynamic alloc failed — using BSS fallback\n");
            }
        }
    }

    /* Fill BootInfo fields that do NOT require EBS first */
    g_boot_info.runtime_services = SystemTable->RuntimeServices;
    g_boot_info.acpi_table = NULL;
    g_boot_info.framebuffer.base = NULL;
    g_boot_info.framebuffer.size = 0;
    g_boot_info.framebuffer.width = 0;
    g_boot_info.framebuffer.height = 0;
    g_boot_info.framebuffer.pixels_per_scanline = 0;
    g_boot_info.framebuffer.pixel_format = (UINT32)PixelBltOnly;
    g_boot_info.uefi_boot_services_exited = 0;

    /* Locate ACPI table (safe pre-EBS) */
    {
        EFI_CONFIGURATION_TABLE *config_tables =
            (EFI_CONFIGURATION_TABLE *)SystemTable->ConfigurationTable;

        for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
            if (guid_equals(&config_tables[i].VendorGuid, &EFI_ACPI_20_TABLE_GUID) ||
                guid_equals(&config_tables[i].VendorGuid, &EFI_ACPI_TABLE_GUID))
            {
                g_boot_info.acpi_table = config_tables[i].VendorTable;
                break;
            }
        }
    }

    /* Locate GOP and populate framebuffer info (safe pre-EBS) */
    {
        EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
        EFI_STATUS gop_status = BS->LocateProtocol(&gop_guid, NULL, (void **)&gop);
        if (gop_status == EFI_SUCCESS && gop && gop->Mode && gop->Mode->Info) {
            EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->Mode;
            if (mode->Info->PixelFormat != PixelBltOnly) {
                g_boot_info.framebuffer.base             = (void *)(UINTN)mode->FrameBufferBase;
                g_boot_info.framebuffer.size             = (UINTN)mode->FrameBufferSize;
                g_boot_info.framebuffer.width            = mode->Info->HorizontalResolution;
                g_boot_info.framebuffer.height           = mode->Info->VerticalResolution;
                g_boot_info.framebuffer.pixels_per_scanline = mode->Info->PixelsPerScanLine;
                g_boot_info.framebuffer.pixel_format     = (UINT32)mode->Info->PixelFormat;
                RAW_LOG("GOP: linear framebuffer found\n");
            } else {
                RAW_LOG("GOP: PixelBltOnly - no linear framebuffer\n");
            }
        } else {
            RAW_LOG("GOP: protocol not found\n");
        }
    }

    /*
     * Phase B: ExitBootServices loop
     * RULE: Between the final GetMemoryMap and ExitBootServices => DO NOTHING.
     * No ConOut, no extra services, no protocol opens, no "verification" GetMemoryMap.
     */
    for (int attempt = 0; attempt < 16; ++attempt) {
        UINTN map_sz = memory_map_capacity;

        status = BS->GetMemoryMap(
            &map_sz,
            memory_map,
            &map_key,
            &descriptor_size,
            &descriptor_version
        );

        if (status == EFI_BUFFER_TOO_SMALL) {
            RAW_LOG("GetMemoryMap: BUFFER_TOO_SMALL\r\n");
            return status;
        }

        if (status != EFI_SUCCESS) {
            RAW_LOG("GetMemoryMap: ERROR\r\n");
            return status;
        }

        /* Stash map into BootInfo (still pre-EBS) */
        g_boot_info.memory_map = memory_map;
        g_boot_info.memory_map_size = map_sz;
        g_boot_info.memory_map_descriptor_size = descriptor_size;
        g_boot_info.runtime_services = SystemTable->RuntimeServices;

#if defined(ARCH_AMD64)
        RAW_LOG("EBS...\r\n");
#endif

        /* Call ExitBootServices immediately after GetMemoryMap */
        status = BS->ExitBootServices(ImageHandle, map_key);
        if (status == EFI_SUCCESS) {
            exited_boot_services = 1;
#if defined(ARCH_AMD64)
            RAW_LOG("EBS OK\r\n");
#endif
            break;
        }

        if (status != EFI_INVALID_PARAMETER) {
#if defined(ARCH_AMD64)
            RAW_LOG("EBS fail non-invalid\r\n");
#endif
            return status;
        }

#if defined(ARCH_AMD64)
        RAW_LOG("EBS invalid -> retry\r\n");
#endif
        /* loop will retry */
    }

    g_boot_info.uefi_boot_services_exited = exited_boot_services ? 1u : 0u;

#ifdef MONOLITHIC_BUILD
    /*
     * Monolithic build: kernel is linked directly, call kernel_main
     */
    RAW_LOG("Calling kernel_main (monolithic)...\n");
    kernel_main(&g_boot_info);
#else
    /*
     * Phase C: Post-ExitBootServices - Parse and load kernel
     * BootServices are GONE, can only use runtime services
     */
    RAW_LOG("Parsing kernel ELF...\n");

    Elf64_Addr entry_point = 0;
    if (!elf_load_kernel(kernel_elf_buffer, kernel_elf_size, &entry_point)) {
        RAW_LOG("FATAL: Failed to load kernel ELF\n");
        while (1) arch_halt();
    }

    RAW_LOG("Jumping to kernel entry point...\n");

    /* Jump to kernel entry point */
    KernelEntry kernel_entry = (KernelEntry)entry_point;
    kernel_entry(&g_boot_info);
#endif

    /* Should never reach here */
    RAW_LOG("FATAL: Kernel returned\n");
    while (1) {
        arch_halt();
    }

    return EFI_SUCCESS;
}