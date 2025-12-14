# StarKernel HAL Integration Guide

## Overview

This document provides **kernel-specific implementation details** for the HAL platform layer that becomes StarKernel. It covers UEFI boot, freestanding C environment, hardware initialization, and the path to a working `ok` prompt.

**Audience:** Kernel developers implementing `src/platform/kernel/`

---

## StarKernel Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  UEFI Firmware                                              │
│  • Initializes hardware                                     │
│  • Provides boot services (memory map, ACPI, GOP)           │
│  • Loads BOOTX64.EFI                                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓ ExitBootServices()
┌─────────────────────────────────────────────────────────────┐
│  StarKernel Boot Loader (stage 0)                           │
│  • Collects BootInfo (memory map, ACPI tables, framebuffer) │
│  • Sets up initial page tables                              │
│  • Exits UEFI boot services                                 │
│  • Jumps to StarKernel proper                               │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  StarKernel HAL (stage 1)                                   │
│  • CPU initialization (GDT, IDT, interrupts)                │
│  • Memory subsystem (PMM, VMM, heap)                        │
│  • Time subsystem (TSC, HPET, APIC timer)                   │
│  • Console (UART + framebuffer)                             │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  StarForth VM                                               │
│  • vm_create()                                              │
│  • Initialize physics subsystems                            │
│  • Start REPL                                               │
│  • Print "ok" prompt                                        │
└─────────────────────────────────────────────────────────────┘
```

---

## Directory Structure

```
src/platform/kernel/
├── boot/
│   ├── uefi_loader.c         # UEFI entry point (BOOTX64.EFI)
│   ├── bootinfo.h            # BootInfo struct definition
│   └── handoff.S             # ASM trampoline (UEFI → StarKernel)
│
├── cpu/
│   ├── gdt.c                 # Global Descriptor Table
│   ├── idt.c                 # Interrupt Descriptor Table
│   ├── isr.S                 # Interrupt service routine stubs
│   └── smp.c                 # SMP bring-up (future)
│
├── mm/
│   ├── pmm.c                 # Physical Memory Manager
│   ├── vmm.c                 # Virtual Memory Manager
│   ├── kmalloc.c             # Kernel heap allocator
│   └── paging.S              # Page table manipulation ASM
│
├── time/
│   ├── tsc.c                 # Time Stamp Counter
│   ├── hpet.c                # High Precision Event Timer
│   ├── apic_timer.c          # Local APIC timer
│   └── pit.c                 # Programmable Interval Timer (fallback)
│
├── drivers/
│   ├── uart.c                # UART 16550 serial console
│   ├── framebuffer.c         # UEFI GOP framebuffer
│   ├── apic.c                # Local APIC + IOAPIC
│   ├── acpi.c                # ACPI table parsing
│   └── pci.c                 # PCI enumeration (future)
│
├── hal_time.c                # HAL time implementation
├── hal_interrupt.c           # HAL interrupt implementation
├── hal_memory.c              # HAL memory implementation
├── hal_console.c             # HAL console implementation
├── hal_cpu.c                 # HAL CPU implementation
├── hal_panic.c               # HAL panic implementation
│
└── kernel_main.c             # StarKernel entry point (after UEFI)
```

---

## Boot Sequence Detail

### 1. UEFI Loader (`boot/uefi_loader.c`)

**Responsibilities:**
- Collect system information from UEFI
- Allocate kernel memory
- Exit UEFI boot services
- Jump to StarKernel

**Implementation:**

```c
/* boot/uefi_loader.c */
#include <efi.h>
#include <efilib.h>
#include "bootinfo.h"

/* BootInfo passed to StarKernel */
typedef struct {
    uint64_t memory_map_addr;
    uint64_t memory_map_size;
    uint64_t memory_map_descriptor_size;
    uint64_t acpi_rsdp_addr;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
} BootInfo;

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    BootInfo *boot_info = (BootInfo *)AllocatePool(sizeof(BootInfo));

    /* 1. Get memory map */
    UINTN map_size = 0, map_key, descriptor_size;
    UINT32 descriptor_version;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;

    uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, memory_map, &map_key,
                      &descriptor_size, &descriptor_version);

    map_size += 2 * descriptor_size;  /* Extra space for ExitBootServices */
    memory_map = (EFI_MEMORY_DESCRIPTOR *)AllocatePool(map_size);

    uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, memory_map, &map_key,
                      &descriptor_size, &descriptor_version);

    boot_info->memory_map_addr = (uint64_t)memory_map;
    boot_info->memory_map_size = map_size;
    boot_info->memory_map_descriptor_size = descriptor_size;

    /* 2. Get ACPI RSDP */
    EFI_GUID acpi_20_guid = ACPI_20_TABLE_GUID;
    void *rsdp = NULL;
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        if (CompareGuid(&ST->ConfigurationTable[i].VendorGuid, &acpi_20_guid) == 0) {
            rsdp = ST->ConfigurationTable[i].VendorTable;
            break;
        }
    }
    boot_info->acpi_rsdp_addr = (uint64_t)rsdp;

    /* 3. Get framebuffer from GOP (Graphics Output Protocol) */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    uefi_call_wrapper(BS->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);

    if (gop) {
        boot_info->framebuffer_addr = gop->Mode->FrameBufferBase;
        boot_info->framebuffer_width = gop->Mode->Info->HorizontalResolution;
        boot_info->framebuffer_height = gop->Mode->Info->VerticalResolution;
        boot_info->framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    }

    /* 4. Exit boot services */
    uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, map_key);

    /* 5. Jump to StarKernel */
    extern void kernel_main(BootInfo *boot_info);
    kernel_main(boot_info);

    /* Never returns */
    while (1) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
```

### 2. StarKernel Entry Point (`kernel_main.c`)

**Responsibilities:**
- Parse BootInfo
- Initialize CPU (GDT, IDT)
- Initialize memory subsystem
- Initialize HAL
- Call `main()` to start VM

**Implementation:**

```c
/* kernel_main.c */
#include "bootinfo.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "drivers/uart.h"
#include "drivers/framebuffer.h"

/* Global BootInfo (available to all kernel code) */
BootInfo *g_boot_info = NULL;

void kernel_main(BootInfo *boot_info) {
    g_boot_info = boot_info;

    /* 1. Initialize serial console ASAP (for debug output) */
    uart_early_init(0x3F8, 115200);  /* COM1 */
    uart_puts("StarKernel booting...\r\n");

    /* 2. Initialize CPU structures */
    gdt_init();
    idt_init();

    /* 3. Initialize physical memory manager */
    pmm_init(boot_info);
    uart_puts("PMM initialized\r\n");

    /* 4. Initialize virtual memory manager */
    vmm_init();
    uart_puts("VMM initialized\r\n");

    /* 5. Initialize kernel heap */
    kmalloc_init();
    uart_puts("Heap initialized\r\n");

    /* 6. Initialize framebuffer */
    fb_init(boot_info);

    /* 7. Initialize HAL */
    hal_time_init();
    hal_interrupt_init();
    hal_mem_init();
    hal_console_init();
    hal_cpu_init();

    uart_puts("HAL initialized\r\n");

    /* 8. Call standard main() to start VM */
    extern int main(int argc, char **argv);
    char *argv[] = {"starforth", NULL};
    main(1, argv);

    /* Never returns, but halt if it does */
    hal_panic("main() returned unexpectedly");
}
```

---

## HAL Implementation Details

### 1. HAL Time (`hal_time.c`)

**Hardware:** TSC (Time Stamp Counter), HPET (High Precision Event Timer), APIC timer

**Key challenges:**
- TSC calibration against HPET
- TSC frequency varies on old CPUs (need invariant TSC check)
- APIC timer setup for periodic interrupts

**Implementation outline:**

```c
/* hal_time.c */
#include "hal/hal_time.h"
#include "time/tsc.h"
#include "time/hpet.h"
#include "time/apic_timer.h"

static uint64_t tsc_hz = 0;
static uint64_t boot_tsc = 0;

void hal_time_init(void) {
    /* Check for invariant TSC */
    if (!tsc_is_invariant()) {
        hal_panic("hal_time: TSC not invariant, falling back to HPET");
        /* TODO: Use HPET directly if TSC unreliable */
    }

    /* Calibrate TSC frequency using HPET */
    tsc_hz = tsc_calibrate_hpet();
    if (tsc_hz == 0) {
        hal_panic("hal_time: TSC calibration failed");
    }

    boot_tsc = rdtsc();

    /* Initialize APIC timer for periodic interrupts */
    apic_timer_init();
}

uint64_t hal_time_now_ns(void) {
    uint64_t tsc = rdtsc() - boot_tsc;
    return (tsc * 1000000000ULL) / tsc_hz;
}

int hal_timer_periodic(uint64_t period_ns, hal_timer_callback_t callback, void *ctx) {
    return apic_timer_periodic(period_ns, callback, ctx);
}
```

**See:** `time/tsc.c`, `time/hpet.c`, `time/apic_timer.c` for low-level implementations.

---

### 2. HAL Interrupt (`hal_interrupt.c`)

**Hardware:** IDT (Interrupt Descriptor Table), Local APIC, IOAPIC

**Key challenges:**
- Setting up 256 IDT entries
- Routing hardware IRQs through IOAPIC
- Tracking interrupt nesting depth (for `hal_in_interrupt_context()`)

**Implementation outline:**

```c
/* hal_interrupt.c */
#include "hal/hal_interrupt.h"
#include "cpu/idt.h"
#include "drivers/apic.h"

/* Per-CPU interrupt depth (thread-local on SMP) */
static __thread unsigned int irq_depth = 0;

void hal_interrupt_init(void) {
    /* Set up IDT with 256 entries */
    idt_init();

    /* Disable legacy PIC (use APIC instead) */
    outb(0x21, 0xFF);  /* Mask all PIC1 IRQs */
    outb(0xA1, 0xFF);  /* Mask all PIC2 IRQs */

    /* Initialize Local APIC */
    apic_init();

    /* Initialize IOAPIC */
    ioapic_init();
}

void hal_irq_enable(void) {
    __asm__ volatile("sti");
}

unsigned long hal_irq_disable(void) {
    unsigned long flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags));
    return flags;
}

void hal_irq_restore(unsigned long flags) {
    __asm__ volatile("pushq %0; popfq" :: "r"(flags));
}

int hal_in_interrupt_context(void) {
    return irq_depth > 0;
}

/* Common ISR entry (called from IDT stubs) */
void isr_common_handler(uint64_t vector, uint64_t error_code) {
    irq_depth++;

    /* Dispatch to registered handler */
    extern void isr_dispatch(uint64_t vector, uint64_t error_code);
    isr_dispatch(vector, error_code);

    irq_depth--;

    /* Send EOI to APIC */
    apic_eoi();
}
```

**See:** `cpu/idt.c`, `cpu/isr.S`, `drivers/apic.c` for low-level implementations.

---

### 3. HAL Memory (`hal_memory.c`)

**Components:**
- **PMM (Physical Memory Manager):** Tracks free/used physical page frames
- **VMM (Virtual Memory Manager):** Manages page tables (4-level paging on x86_64)
- **kmalloc:** Kernel heap allocator (slab or buddy allocator)

**Implementation outline:**

```c
/* hal_memory.c */
#include "hal/hal_memory.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/kmalloc.h"

void hal_mem_init(void) {
    /* PMM and VMM initialized in kernel_main() before HAL */
    /* Just verify they're ready */
    if (!pmm_is_initialized()) {
        hal_panic("hal_mem_init: PMM not initialized");
    }
}

void *hal_mem_alloc(size_t size) {
    return kmalloc(size);
}

void hal_mem_free(void *ptr) {
    kfree(ptr);
}

uint64_t hal_mem_alloc_pages(size_t count) {
    return pmm_alloc_pages(count);
}

void hal_mem_free_pages(uint64_t paddr, size_t count) {
    pmm_free_pages(paddr, count);
}

int hal_mem_map(uint64_t vaddr, uint64_t paddr, size_t size, unsigned int flags) {
    return vmm_map_range(vaddr, paddr, size, flags);
}

size_t hal_mem_page_size(void) {
    return 4096;
}
```

**PMM details** (`mm/pmm.c`):
- Bitmap allocator (1 bit per 4KB page)
- Parse UEFI memory map to mark usable vs. reserved regions
- Reserve kernel image, ACPI tables, framebuffer

**VMM details** (`mm/vmm.c`):
- 4-level page tables (PML4 → PDPT → PD → PT)
- Recursive mapping trick for page table access
- Identity-map kernel, higher-half kernel optional

**kmalloc details** (`mm/kmalloc.c`):
- Slab allocator for common sizes (16, 32, 64, ..., 4096 bytes)
- Buddy allocator for large allocations
- Zero-initialization required by HAL contract

---

### 4. HAL Console (`hal_console.c`)

**Hardware:** UART 16550 (serial), UEFI GOP framebuffer (video)

**Implementation outline:**

```c
/* hal_console.c */
#include "hal/hal_console.h"
#include "drivers/uart.h"
#include "drivers/framebuffer.h"

void hal_console_init(void) {
    /* UART already initialized in kernel_main() for early debug */
    /* Framebuffer initialized from BootInfo */
}

void hal_console_putc(char c) {
    uart_putc(c);         /* Always output to serial */
    fb_putc(c);           /* Also output to framebuffer if available */
}

void hal_console_puts(const char *s) {
    while (*s) {
        hal_console_putc(*s++);
    }
}

int hal_console_getc(void) {
    /* Block until UART has data */
    return uart_getc();
}

int hal_console_has_input(void) {
    return uart_has_data();
}
```

**UART details** (`drivers/uart.c`):
- 16550 compatible (COM1 = 0x3F8, COM2 = 0x2F8)
- 115200 baud default
- Polling mode (no interrupts for simplicity)

**Framebuffer details** (`drivers/framebuffer.c`):
- Linear framebuffer from UEFI GOP
- 32-bit RGB pixels
- Software text rendering (8x16 font)
- Scrolling, cursor, ANSI escape codes (optional)

---

### 5. HAL CPU (`hal_cpu.c`)

**Implementation outline:**

```c
/* hal_cpu.c */
#include "hal/hal_cpu.h"
#include "drivers/apic.h"

static unsigned int num_cpus = 1;

void hal_cpu_init(void) {
    /* Detect CPU features (CPUID) */
    /* TODO: Bring up secondary CPUs (SMP) */
}

unsigned int hal_cpu_id(void) {
    return apic_get_id();  /* Local APIC ID */
}

void hal_cpu_relax(void) {
    __asm__ volatile("pause");
}

void hal_cpu_halt(void) {
    __asm__ volatile("hlt");
}

unsigned int hal_cpu_count(void) {
    return num_cpus;
}
```

---

## Build System

### Toolchain Requirements

**Freestanding C environment:**
- `gcc` or `clang` with `-ffreestanding`
- `ld` with custom linker script
- `objcopy` to create PE32+ executable (for UEFI)

### Makefile Additions

```makefile
# Platform: kernel
ifeq ($(PLATFORM),kernel)
    CC = gcc
    LD = ld
    OBJCOPY = objcopy

    CFLAGS += -ffreestanding -nostdlib -mno-red-zone -mcmodel=large
    CFLAGS += -mno-mmx -mno-sse -mno-sse2  # No FP in kernel
    CFLAGS += -I/usr/include/efi -I/usr/include/efi/x86_64

    LDFLAGS += -nostdlib -static -T src/platform/kernel/linker.ld

    # Platform sources
    PLATFORM_SOURCES = \
        src/platform/kernel/boot/uefi_loader.c \
        src/platform/kernel/kernel_main.c \
        src/platform/kernel/cpu/gdt.c \
        src/platform/kernel/cpu/idt.c \
        src/platform/kernel/cpu/isr.S \
        src/platform/kernel/mm/pmm.c \
        src/platform/kernel/mm/vmm.c \
        src/platform/kernel/mm/kmalloc.c \
        src/platform/kernel/drivers/uart.c \
        src/platform/kernel/drivers/framebuffer.c \
        src/platform/kernel/drivers/apic.c \
        src/platform/kernel/hal_time.c \
        src/platform/kernel/hal_interrupt.c \
        src/platform/kernel/hal_memory.c \
        src/platform/kernel/hal_console.c \
        src/platform/kernel/hal_cpu.c \
        src/platform/kernel/hal_panic.c

    # Build UEFI PE32+ executable
    starforth.efi: $(OBJECTS)
        $(LD) $(LDFLAGS) -o starforth.so $(OBJECTS)
        $(OBJCOPY) -j .text -j .data -j .rodata -j .reloc \
                   --target=efi-app-x86_64 starforth.so $@
endif
```

### Linker Script (`src/platform/kernel/linker.ld`)

```ld
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(efi_main)

SECTIONS {
    . = 0x100000;  /* Load at 1MB (standard kernel load address) */

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    }

    /DISCARD/ : {
        *(.eh_frame)
    }
}
```

---

## Testing on QEMU

### QEMU + OVMF Setup

```bash
# Install OVMF (UEFI firmware for QEMU)
sudo apt install ovmf

# Create ESP (EFI System Partition)
mkdir -p esp/EFI/BOOT
cp starforth.efi esp/EFI/BOOT/BOOTX64.EFI

# Run QEMU
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:esp/,format=raw \
    -serial stdio \
    -m 512M \
    -enable-kvm
```

### Expected Output

```
StarKernel booting...
PMM initialized
VMM initialized
Heap initialized
HAL initialized
StarForth VM v1.0
ok
```

---

## Debugging

### Serial Debugging

```c
/* Early debug output via UART */
void debug_puts(const char *s) {
    while (*s) {
        while (!(inb(0x3FD) & 0x20));  /* Wait for UART ready */
        outb(0x3F8, *s++);              /* Write character */
    }
}
```

### GDB Remote Debugging

```bash
# Start QEMU with GDB server
qemu-system-x86_64 ... -s -S

# In another terminal
gdb starforth.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

### Panic Handler

```c
/* hal_panic.c */
void hal_panic(const char *msg) {
    hal_irq_disable();

    hal_console_puts("\n*** KERNEL PANIC ***\n");
    hal_console_puts(msg ? msg : "unknown error");
    hal_console_puts("\nSystem halted.\n");

    /* Halt all CPUs */
    while (1) {
        hal_cpu_halt();
    }
}
```

---

## Roadmap to `ok` Prompt

### Milestone 1: Boot + Serial Output
- UEFI loader runs
- Kernel prints "StarKernel booting..." to serial
- System doesn't triple-fault

### Milestone 2: Memory Works
- PMM tracks physical pages
- VMM maps kernel
- kmalloc/kfree work

### Milestone 3: HAL Initialized
- All `hal_*_init()` functions complete
- HAL functions callable (even if not fully functional)

### Milestone 4: VM Starts
- `vm_create()` succeeds
- Dictionary allocated
- No crashes in VM init

### Milestone 5: REPL Runs
- REPL prints "`ok`" prompt
- Can type characters (echoed to serial)
- Can execute simple words (`1 2 + .` → `3`)

### Milestone 6: Physics Subsystems Work
- Execution heat tracking operational
- Heartbeat timer fires
- Rolling window captures execution history

### Milestone 7: Full Test Suite
- All 936+ tests pass on kernel
- DoE mode works
- 0% algorithmic variance on bare metal

---

## Future Work (StarshipOS)

After achieving `ok` prompt on StarKernel:

1. **Storage:** AHCI/NVMe drivers, FAT32 filesystem
2. **Networking:** VirtIO-net driver, TCP/IP stack
3. **Process model:** Forth tasks, scheduling, IPC
4. **Device model:** Unified block/net/char subsystem
5. **Security:** Capabilities, ACL, Forth-based access control

**The HAL remains the foundation throughout.**

---

## References

- UEFI Specification: https://uefi.org/specifications
- GNU-EFI: https://sourceforge.net/projects/gnu-efi/
- OSDev Wiki: https://wiki.osdev.org/
- Intel SDM: https://www.intel.com/sdm
- ACPI Specification: https://uefi.org/acpi