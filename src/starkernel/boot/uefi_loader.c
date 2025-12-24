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

/* Kernel entry point signature */
typedef void (*KernelEntry)(BootInfo *boot_info);

static BootInfo g_boot_info = {0};

/* Buffer to hold loaded kernel ELF (8 MB should be enough) */
static uint8_t kernel_elf_buffer[8 * 1024 * 1024] __attribute__((aligned(4096)));
static uint64_t kernel_elf_size = 0;

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

/* Load kernel.elf from ESP using Simple File System Protocol */
static EFI_STATUS load_kernel_from_esp(EFI_HANDLE ImageHandle, EFI_BOOT_SERVICES *BS)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *kernel_file = NULL;

    RAW_LOG("Loading kernel.elf from ESP...\n");

    /* Get loaded image protocol */
    status = BS->HandleProtocol(ImageHandle, &EFI_LOADED_IMAGE_PROTOCOL_GUID,
                                (void **)&loaded_image);
    if (status != EFI_SUCCESS) {
        RAW_LOG("Failed to get LoadedImageProtocol\n");
        return status;
    }

    /* Get file system protocol from device handle */
    status = BS->HandleProtocol(loaded_image->DeviceHandle,
                                &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
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
    status = kernel_file->GetInfo(kernel_file, &EFI_FILE_INFO_GUID,
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
    if (kernel_elf_size > sizeof(kernel_elf_buffer)) {
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

    /* Load kernel.elf from ESP BEFORE ExitBootServices */
    status = load_kernel_from_esp(ImageHandle, BS);
    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"FATAL: Failed to load kernel.elf\r\n");
        RAW_LOG("FATAL: Failed to load kernel.elf\n");
        while (1) arch_halt();
        return status;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Collecting boot information...\r\n");

    /* Fill BootInfo fields that do NOT require EBS first */
    g_boot_info.runtime_services = SystemTable->RuntimeServices;
    g_boot_info.acpi_table = NULL;
    g_boot_info.framebuffer.base = NULL;
    g_boot_info.framebuffer.size = 0;
    g_boot_info.framebuffer.width = 0;
    g_boot_info.framebuffer.height = 0;
    g_boot_info.framebuffer.pixels_per_scanline = 0;
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

    /* Should never reach here */
    RAW_LOG("FATAL: Kernel returned\n");
    while (1) {
        arch_halt();
    }

    return EFI_SUCCESS;
}