/**
 * uefi_loader.c - UEFI boot loader for StarKernel
 */

#include "uefi.h"
#include "arch.h"

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

/* Forward declarations */
extern void kernel_main(BootInfo *boot_info);

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

#if defined(ARCH_AMD64)
static inline void raw_outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void raw_serial_puts(const char *s)
{
    const uint16_t port = 0x3F8; /* COM1 data */
    while (*s) {
        raw_outb(port, (uint8_t)*s++);
    }
}
#define RAW_LOG(str) raw_serial_puts(str)
#else
#define RAW_LOG(str) ((void)0)
#endif

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

    /* Phase A: safe to use UEFI console */
    SystemTable->ConOut->Reset(SystemTable->ConOut, FALSE);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"StarKernel UEFI Loader\r\n");
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

#if defined(ARCH_AMD64)
    raw_serial_init();
    RAW_LOG("RAW SERIAL UP\n");
#endif

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
     * No ConOut, no extra services, no protocol opens, no “verification” GetMemoryMap.
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

    /* Jump to kernel (BootServices may be gone) */
    g_boot_info.uefi_boot_services_exited = exited_boot_services ? 1u : 0u;
    kernel_main(&g_boot_info);

    while (1) {
        arch_halt();
    }

    return EFI_SUCCESS;
}
