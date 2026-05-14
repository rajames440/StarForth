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
typedef UINTN EFI_TPL;

#define TRUE  ((BOOLEAN)1)
#define FALSE ((BOOLEAN)0)

#define TPL_APPLICATION 4
#define TPL_CALLBACK    8
#define TPL_NOTIFY      16
#define TPL_HIGH_LEVEL  31

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
#define EFI_ABORTED               (21 | (1UL << 63))

/* EFI Handle */
typedef void* EFI_HANDLE;

/* EFI Physical Address */
typedef UINT64 EFI_PHYSICAL_ADDRESS;

/* Allocation types for AllocatePages */
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

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

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    EFI_ALLOCATE_TYPE Type,
    EFI_MEMORY_TYPE MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN Pages
);

typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    void **Interface
);

/* Search types for LocateHandle */
typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE)(
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    void *SearchKey,
    UINTN *BufferSize,
    EFI_HANDLE *Buffer
);

typedef EFI_TPL (EFIAPI *EFI_RAISE_TPL)(
    EFI_TPL NewTpl
);

typedef void (EFIAPI *EFI_RESTORE_TPL)(
    EFI_TPL OldTpl
);

typedef struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Task Priority Services */
    EFI_RAISE_TPL RaiseTPL;
    EFI_RESTORE_TPL RestoreTPL;

    /* Memory Services */
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES FreePages;
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
    EFI_HANDLE_PROTOCOL HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    EFI_LOCATE_HANDLE LocateHandle;
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

/* Configuration Table */
typedef struct {
    EFI_GUID VendorGuid;
    void *VendorTable;
} EFI_CONFIGURATION_TABLE;

/* ACPI GUIDs */
static const EFI_GUID EFI_ACPI_20_TABLE_GUID = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
static const EFI_GUID EFI_ACPI_TABLE_GUID    = {0xeb9d2d30,0x2d88,0x11d3,{0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}};

/* Framebuffer info (GOP) */
typedef struct {
    void   *base;
    UINTN   size;
    UINT32  width;
    UINT32  height;
    UINT32  pixels_per_scanline;
} FramebufferInfo;

/* Boot Info Structure (passed to kernel) */
typedef struct {
    EFI_MEMORY_DESCRIPTOR *memory_map;
    UINTN memory_map_size;
    UINTN memory_map_descriptor_size;
    EFI_RUNTIME_SERVICES *runtime_services;
    void *acpi_table;
    FramebufferInfo framebuffer;
    UINT8 uefi_boot_services_exited;
} BootInfo;

/* File System Protocols (needed for loader) */
static const EFI_GUID EFI_LOADED_IMAGE_PROTOCOL_GUID =
    {0x5B1B31A1,0x9562,0x11d2, {0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B}};

static const EFI_GUID EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID =
    {0x0964e5b22,0x6459,0x11d2, {0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};

static const EFI_GUID EFI_FILE_INFO_GUID =
    {0x09576e92,0x6d3f,0x11d2, {0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

struct _EFI_FILE_PROTOCOL;
struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    struct _EFI_FILE_PROTOCOL *This,
    struct _EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(
    struct _EFI_FILE_PROTOCOL *This
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    struct _EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    struct _EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer
);

typedef struct _EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    VOID *Delete;
    EFI_FILE_READ Read;
    VOID *Write;
    VOID *GetPosition;
    VOID *SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    VOID *SetInfo;
    VOID *Flush;
} EFI_FILE_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    EFI_FILE_PROTOCOL **Root
);

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[256];
} EFI_FILE_INFO;

typedef struct {
    EFI_HANDLE DeviceHandle;
    VOID *FilePath;
    VOID *Reserved;
    UINT32 LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    VOID *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

#endif /* STARKERNEL_UEFI_H */
