/**
 * uefi.h - UEFI definitions for StarKernel
 * Minimal UEFI interface for bare-metal boot
 */

#ifndef STARKERNEL_UEFI_H
#define STARKERNEL_UEFI_H

#include <stdint.h>
#include <stddef.h>

/* UEFI Basic Types */
typedef uint64_t UINTN;
typedef int64_t INTN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef uint8_t BOOLEAN;
typedef void VOID;
typedef uint16_t CHAR16;

#define TRUE  ((BOOLEAN)1)
#define FALSE ((BOOLEAN)0)

/* Calling convention */
#if defined(__x86_64__) || defined(__i386__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

/* EFI Status Codes */
typedef UINTN EFI_STATUS;
#define EFI_SUCCESS               0
#define EFI_LOAD_ERROR            (1 | (1UL << 63))
#define EFI_INVALID_PARAMETER     (2 | (1UL << 63))
#define EFI_UNSUPPORTED           (3 | (1UL << 63))
#define EFI_BAD_BUFFER_SIZE       (4 | (1UL << 63))
#define EFI_BUFFER_TOO_SMALL      (5 | (1UL << 63))
#define EFI_NOT_READY             (6 | (1UL << 63))
#define EFI_DEVICE_ERROR          (7 | (1UL << 63))
#define EFI_WRITE_PROTECTED       (8 | (1UL << 63))
#define EFI_OUT_OF_RESOURCES      (9 | (1UL << 63))
#define EFI_NOT_FOUND             (14 | (1UL << 63))

/* EFI Handle */
typedef void* EFI_HANDLE;

/* EFI GUID */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* EFI Memory Types */
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/* EFI Memory Descriptor */
typedef struct {
    UINT32           Type;
    UINT64           PhysicalStart;
    UINT64           VirtualStart;
    UINT64           NumberOfPages;
    UINT64           Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* Memory Attributes */
#define EFI_MEMORY_UC   0x0000000000000001ULL
#define EFI_MEMORY_WC   0x0000000000000002ULL
#define EFI_MEMORY_WT   0x0000000000000004ULL
#define EFI_MEMORY_WB   0x0000000000000008ULL
#define EFI_MEMORY_UCE  0x0000000000000010ULL
#define EFI_MEMORY_WP   0x0000000000001000ULL
#define EFI_MEMORY_RP   0x0000000000002000ULL
#define EFI_MEMORY_XP   0x0000000000004000ULL
#define EFI_MEMORY_RUNTIME 0x8000000000000000ULL

/* EFI Table Header */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* Forward declarations */
struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct _EFI_BOOT_SERVICES;
struct _EFI_RUNTIME_SERVICES;

/* Simple Text Output Protocol */
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/* Boot Services */
typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
);

typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINTN MapKey
);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    EFI_MEMORY_TYPE PoolType,
    UINTN Size,
    void **Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(
    void *Buffer
);

typedef struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Task Priority Services */
    void *RaiseTPL;
    void *RestoreTPL;

    /* Memory Services */
    void *AllocatePages;
    void *FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;

    /* Event & Timer Services */
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;

    /* Protocol Handler Services */
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    void *HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;

    /* Image Services */
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;

    /* Misc Services */
    void *GetNextMonotonicCount;
    void *Stall;
    void *SetWatchdogTimer;
} EFI_BOOT_SERVICES;

/* Runtime Services */
typedef struct _EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Time Services */
    void *GetTime;
    void *SetTime;
    void *GetWakeupTime;
    void *SetWakeupTime;

    /* Virtual Memory Services */
    void *SetVirtualAddressMap;
    void *ConvertPointer;

    /* Variable Services */
    void *GetVariable;
    void *GetNextVariableName;
    void *SetVariable;

    /* Misc Services */
    void *GetNextHighMonotonicCount;
    void *ResetSystem;
} EFI_RUNTIME_SERVICES;

/* System Table */
typedef struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* Boot Info Structure (passed to kernel) */
typedef struct {
    EFI_MEMORY_DESCRIPTOR *memory_map;
    UINTN memory_map_size;
    UINTN memory_map_descriptor_size;
    EFI_RUNTIME_SERVICES *runtime_services;
    void *acpi_table;
    void *framebuffer;
} BootInfo;

#endif /* STARKERNEL_UEFI_H */
