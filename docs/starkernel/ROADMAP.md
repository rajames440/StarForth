# StarKernel Implementation Roadmap

**Status:** Planning → Implementation
**Last Updated:** 2025-12-17
**Goal:** Boot StarKernel to `ok` prompt on bare metal (QEMU/OVMF)

**Primary Architecture:** amd64 (canonical reference)
**Secondary Architectures:** aarch64 (Raspberry Pi 4B), riscv64 (QEMU-only)

---

## Serial Console Connectivity Guide

### amd64 (QEMU)
```bash
# Serial output automatically appears on stdio
make ARCH=amd64 TARGET=kernel qemu
# Type directly in terminal for input
```

### amd64 (Real Hardware)
```bash
# USB-to-Serial adapter (DB9/RS-232)
screen /dev/ttyUSB0 115200
# Or minicom:
minicom -D /dev/ttyUSB0 -b 115200

# IPMI Serial-over-LAN (server hardware):
ipmitool -I lanplus -H <bmc-ip> -U <user> -P <pass> sol activate
```

### Raspberry Pi 4B (aarch64)
```bash
# Hardware Required:
# - USB-to-TTL serial adapter (3.3V logic, NOT 5V!)
# - 3 jumper wires

# Wiring (RasPi 4B GPIO header):
#   USB-TTL GND  → Pin 6  (GND)
#   USB-TTL TX   → Pin 10 (GPIO 15, UART RX)
#   USB-TTL RX   → Pin 8  (GPIO 14, UART TX)
#   DO NOT connect VCC (Pi powers itself via USB-C)

# Connect:
screen /dev/ttyUSB0 115200
# Or:
minicom -D /dev/ttyUSB0 -b 115200

# Enable UART in config.txt (already done by tools/make-rpi4-image.sh):
# enable_uart=1
# uart_2ndstage=1
```

### RISC-V (QEMU)
```bash
# QEMU virt machine includes UART16550
make ARCH=riscv64 TARGET=kernel qemu
# Serial on stdio like amd64
```

**Troubleshooting:**
- No output: Check baud rate (must be 115200)
- Garbage characters: Wrong baud rate or voltage levels
- RasPi: Ensure `enable_uart=1` in config.txt
- Permissions: Add user to `dialout` group: `sudo usermod -a -G dialout $USER`

---

## Milestone 0: Foundation Setup (Week 1)

### Directory Structure
- [x] Create `src/starkernel/` directory
- [x] Create `src/starkernel/boot/` for UEFI loader
- [x] Create `src/starkernel/vm_arbiter/` for scheduler
- [x] Create `src/starkernel/hal/` for kernel HAL implementations
- [x] Create `src/starkernel/memory/` for PMM/VMM
- [x] Create `src/starkernel/arch/` for architecture-specific code
  - [x] `src/starkernel/arch/amd64/` - x86_64 boot, interrupts, page tables
  - [x] `src/starkernel/arch/aarch64/` - ARM64 boot, GIC, page tables
  - [x] `src/starkernel/arch/riscv64/` - RISC-V boot, PLIC, page tables
- [x] Create `include/starkernel/` for kernel headers
- [x] Create `tools/` for build scripts and image generation

### Build System - Multi-Architecture Support

**Architecture Variables:**
- [x] Support `ARCH=amd64|aarch64|riscv64` (matches StarForth convention)
- [x] Support `TARGET=kernel` (StarKernel build type)
- [x] Auto-detect architecture if not specified (uname -m)
- [x] Output path: `build/${ARCH}/${TARGET}/starkernel.efi`

**Toolchain Configuration:**
- [x] Detect/configure toolchains for each architecture:
  - [x] **amd64**: `x86_64-elf-gcc` or native gcc with `-m64`
  - [x] **aarch64**: `aarch64-linux-gnu-gcc` or `aarch64-none-elf-gcc`
  - [x] **riscv64**: `riscv64-unknown-elf-gcc` or `riscv64-linux-gnu-gcc`
- [x] Add freestanding flags: `-ffreestanding -nostdlib -nostdinc -fno-builtin`
- [x] Add UEFI-specific flags: `-fPIC -fno-stack-protector -mno-red-zone` (amd64)
- [x] Detect GNU-EFI or use manual UEFI headers

### Build Targets - amd64

**Makefile.starkernel targets:**
- [x] `make ARCH=amd64 TARGET=kernel` → `build/amd64/kernel/starkernel.efi`
  - [x] Compile with `-m64 -march=x86-64 -mno-red-zone`
  - [x] Link with `linker/starkernel-amd64.ld`
  - [x] Output PE32+ UEFI executable
- [x] `make ARCH=amd64 TARGET=kernel iso` → `build/amd64/kernel/starkernel.iso`
  - [x] Create EFI System Partition (ESP) structure
  - [x] Copy `starkernel.efi` to `ESP/EFI/BOOT/BOOTX64.EFI`
  - [x] Generate bootable ISO with `xorriso` or `mkisofs`
- [x] `make ARCH=amd64 TARGET=kernel qemu`
  - [x] Launch QEMU with OVMF firmware
  - [x] Command: `qemu-system-x86_64 -bios OVMF.fd -hda fat:rw:build/amd64/kernel/esp/ -serial stdio`
  - [x] Enable KVM acceleration if available
- [x] `make ARCH=amd64 TARGET=kernel qemu-gdb`
  - [x] Launch QEMU with GDB server on port 1234
  - [x] Wait for GDB connection before starting

### Build Targets - aarch64 (Raspberry Pi 4B)

**Makefile.starkernel targets:**
- [x] `make ARCH=aarch64 TARGET=kernel` → `build/aarch64/kernel/starkernel.efi`
  - [x] Compile with `-march=armv8-a -mcpu=cortex-a72`
  - [x] Link with `linker/starkernel-aarch64.ld`
  - [x] Output PE32+ UEFI executable for ARM64
- [x] `make ARCH=aarch64 TARGET=kernel rpi4-image` → `build/aarch64/kernel/starkernel-rpi4.img`
  - [x] Create FAT32 partition (boot partition)
  - [x] Add Raspberry Pi 4B firmware files:
    - [x] `bootcode.bin`, `start4.elf`, `fixup4.dat`
    - [x] `config.txt` with `arm_64bit=1, kernel=starkernel.efi`
  - [x] Copy `starkernel.efi` to boot partition
  - [x] Create GPT partition table
  - [x] Output raw disk image for SD card: `dd if=starkernel-rpi4.img of=/dev/sdX bs=4M`
- [x] `make ARCH=aarch64 TARGET=kernel qemu`
  - [x] Launch QEMU aarch64 with Raspberry Pi 3 emulation (Pi 4 not fully supported)
  - [x] Command: `qemu-system-aarch64 -M raspi3b -kernel starkernel.efi -serial stdio`
  - [x] Note: Full Pi 4B testing requires real hardware
- [ ] `make ARCH=aarch64 TARGET=kernel test-rpi4`
  - [ ] Write image to SD card (requires sudo)
  - [ ] Provide instructions for serial console connection (GPIO pins 14/15)

### Build Targets - riscv64

**Makefile.starkernel targets:**
- [x] `make ARCH=riscv64 TARGET=kernel` → `build/riscv64/kernel/starkernel.efi`
  - [x] Compile with `-march=rv64gc -mabi=lp64d`
  - [x] Link with `linker/starkernel-riscv64.ld`
  - [x] Output UEFI executable for RISC-V 64-bit
- [x] `make ARCH=riscv64 TARGET=kernel qemu`
  - [x] Launch QEMU riscv64 with virt machine
  - [x] Command: `qemu-system-riscv64 -M virt -bios fw_payload.elf -kernel starkernel.efi -serial stdio`
  - [x] Use OpenSBI + U-Boot as firmware
- [x] `make ARCH=riscv64 TARGET=kernel qemu-gdb`
  - [x] Launch QEMU with GDB server for RISC-V debugging

### Linker Scripts

- [x] Create `linker/starkernel-amd64.ld` for x86_64 UEFI
  - [x] Base address: `0x100000` (1MB)
  - [x] Sections: `.text`, `.rodata`, `.data`, `.bss`
  - [x] Output format: `elf64-x86-64` → convert to PE32+ with `objcopy`
- [x] Create `linker/starkernel-aarch64.ld` for ARM64 UEFI
  - [x] Base address: `0x40000000` (1GB)
  - [x] Output format: `elf64-littleaarch64` → convert to PE32+
- [x] Create `linker/starkernel-riscv64.ld` for RISC-V UEFI
  - [x] Base address: `0x80200000`
  - [x] Output format: `elf64-littleriscv` → convert to PE32+

### Image Generation Tools

- [x] Create `tools/make-iso.sh` (amd64)
  - [x] Creates ESP directory structure
  - [x] Copies EFI binary to `EFI/BOOT/BOOTX64.EFI`
  - [x] Generates ISO with `xorriso`: `xorriso -as mkisofs -R -f -e EFI/BOOT/BOOTX64.EFI -no-emul-boot -o starkernel.iso esp/`
- [x] Create `tools/make-rpi4-image.sh` (aarch64)
  - [x] Downloads Raspberry Pi 4B firmware from GitHub
  - [x] Creates 256MB FAT32 partition
  - [x] Copies firmware + config.txt + starkernel.efi
  - [x] Outputs bootable SD card image
  - [x] Validates with `fdisk -l starkernel-rpi4.img`
- [x] Create `tools/flash-rpi4.sh` (helper script)
  - [x] Detects SD card device
  - [x] Warns user about data loss
  - [x] Uses `dd` to write image: `sudo dd if=starkernel-rpi4.img of=/dev/sdX bs=4M status=progress`
  - [x] Syncs and ejects card

### Build Convenience Targets

- [x] `make kernel` - Build for current architecture (auto-detect)
- [x] `make kernel-all` - Build for all architectures (amd64, aarch64, riscv64)
- [x] `make iso` - Build ISO for current architecture (amd64 only)
- [x] `make rpi4-image` - Build Raspberry Pi 4B image (aarch64 only)
- [x] `make qemu` - Launch QEMU for current architecture (amd64 verified; aarch64 runs raspi3b emulation)
- [x] `make qemu-gdb` - Launch QEMU with GDB for current architecture
- [x] `make clean-kernel` - Clean StarKernel build artifacts (keep StarForth builds)

### Testing Matrix

- [x] **amd64 + QEMU**: `make ARCH=amd64 TARGET=kernel qemu` → boots to serial output (OVMF pflash + ESP; ExitBootServices succeeds; “UEFI BootServices: EXITED” logged)
- [ ] **amd64 + ISO**: Burn ISO to USB, boot real x86_64 machine (deferred: needs physical host/USB)
- [x] **aarch64 + QEMU**: `make ARCH=aarch64 TARGET=kernel qemu` → boots (virt + UEFI edk2; loader runs, logs UEFI services state, jumps to kernel; console output stubbed)
- [ ] **aarch64 + RasPi 4B**: Flash SD card, boot on real Pi 4B, serial console (deferred: hardware unavailable)
- [ ] **riscv64 + QEMU**: `make ARCH=riscv64 TARGET=kernel qemu` → boots with OpenSBI (blocked: missing riscv64 edk2/UEFI firmware on host; will revisit after firmware install)

### Documentation

- [x] Create `docs/starkernel/BUILD.md` with:
  - [x] Toolchain installation for each architecture
  - [x] Build commands for all targets
  - [x] QEMU setup (OVMF, OpenSBI, firmware downloads)
  - [x] Raspberry Pi 4B setup (serial console wiring, SD card flashing)
  - [x] Troubleshooting common build errors
- [x] Create `docs/starkernel/QEMU.md` with QEMU testing procedures
- [x] Create `docs/starkernel/RPI4.md` with Raspberry Pi 4B deployment guide (mark downstream RasPi validation as deferred until hardware returns)
- [ ] Create GDB debugging guide for each architecture (pending; add once per-arch bring-up proceeds)

### Architecture Abstractions

- [x] Define `include/starkernel/arch.h` with arch-specific interfaces:
  ```c
  void arch_early_init(void);           // Early CPU setup
  void arch_enable_interrupts(void);    // Enable IRQs
  void arch_disable_interrupts(void);   // Disable IRQs
  void arch_halt(void);                 // HLT/WFI/WFI instruction
  uint64_t arch_read_timestamp(void);   // TSC/CNTPCT/RDTIME
  void arch_mmu_init(void);             // Page table setup
  ```
- [x] Stub implementations per arch in `src/starkernel/arch/<arch>/arch.c` (enable/disable IRQs, halt, timestamp, relax)
- [ ] Implement architecture-specific files:
  - [x] `src/starkernel/arch/amd64/boot.S` - x86_64 entry point (stub placeholder)
  - [x] `src/starkernel/arch/amd64/interrupts.c` - x86_64 interrupt handling (stub placeholder)
  - [x] `src/starkernel/arch/aarch64/boot.S` - ARM64 entry point (stub placeholder)
  - [x] `src/starkernel/arch/aarch64/interrupts.c` - GIC initialization (stub placeholder)
  - [x] `src/starkernel/arch/riscv64/boot.S` - RISC-V entry point (stub placeholder)
  - [x] `src/starkernel/arch/riscv64/interrupts.c` - PLIC/CLINT initialization (stub placeholder)

**Exit Criteria (Week 1 - amd64 ONLY):**
- ✅ `make ARCH=amd64 TARGET=kernel` produces `build/amd64/kernel/starkernel.efi`
- ✅ `make ARCH=amd64 TARGET=kernel iso` produces bootable `build/amd64/kernel/starkernel.iso`
- ✅ `make ARCH=amd64 TARGET=kernel qemu` boots and prints "StarKernel" to serial
- ✅ Build convention `build/${ARCH}/${TARGET}/` established
- ✅ Documentation complete for amd64 toolchain and QEMU testing
- ✅ Serial console connectivity validated (QEMU + real hardware if available)
- ❗ Explicit Non-Goal: No kernel semantics, no ExitBootServices(), no VM execution in Milestone 0.

**Week 2-3 (aarch64 + riscv64 ports):**
- ✅ `make ARCH=aarch64 TARGET=kernel rpi4-image` produces `build/aarch64/kernel/starkernel-rpi4.img`
- ✅ `make ARCH=riscv64 TARGET=kernel qemu` boots with OpenSBI
- ✅ All architectures print to serial console
- ⚠️ **Note:** amd64 defines correctness; other architectures chase it

---

### STATUS: Milestone 0 (Verified 2025-12-24)

**Result:** ⚠️ **ARCHITECTURE MISMATCH** - Functional but not per specification

**What Works:**
- ✅ Build system complete (Makefile.starkernel, 334 lines)
- ✅ Multi-architecture support (amd64, aarch64, riscv64)
- ✅ Artifacts build successfully:
  ```bash
  $ file build/amd64/kernel/starkernel.efi
  build/amd64/kernel/starkernel.efi: PE32+ executable (EFI application) x86-64 (stripped to external PDB)

  $ file build/amd64/kernel/starkernel.elf
  build/amd64/kernel/starkernel.elf: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV)
  ```
- ✅ Linker scripts for all architectures
- ✅ QEMU targets working
- ✅ Build outputs to `build/${ARCH}/${TARGET}/`

**Build Output:**
```
CC src/starkernel/kernel_main.c
CC src/starkernel/boot/reloc_stub.c
CC src/starkernel/boot/uefi_loader.c
CC src/starkernel/hal/console.c
CC src/starkernel/hal/memory.c
CC src/starkernel/memory/kmalloc.c
CC src/starkernel/memory/pmm.c
CC src/starkernel/memory/vmm.c
CC src/starkernel/arch/amd64/apic.c
CC src/starkernel/arch/amd64/arch.c
CC src/starkernel/arch/amd64/interrupts.c
CC src/starkernel/arch/amd64/timer.c
AS src/starkernel/arch/amd64/boot.S
AS src/starkernel/arch/amd64/isr.S
LD build/amd64/kernel/starkernel.elf
OBJCOPY build/amd64/kernel/starkernel.efi
StarKernel built successfully: build/amd64/kernel/starkernel.efi
```

**Critical Issues:**
- ❌ **NO `__STARKERNEL__` build fence** - Flag not defined in Makefile or source
- ❌ **WRONG ARCHITECTURE** - Monolithic UEFI app, NOT "loader→kernel" split
  - Current: `efi_main()` directly calls `kernel_main()` (line 208 of uefi_loader.c)
  - Required: Separate loader loads ELF kernel from ESP
- ❌ **NO artifact split** - Only ONE binary in two formats
  - Current: `starkernel.efi` contains both loader + kernel
  - Required: `starkernel_loader.efi` + `starkernel_kernel.elf`

**BLOCKER:** ROADMAP_ADDENDUM.md "Non-Negotiable #1" violated: "Boot path is: UEFI PE/COFF LOADER → loads ELF KERNEL"

**Recommendation:** Defer architecture refactoring; proceed with monolithic build for M7, document deviation

---

## Milestone 1: Minimal Boot (Week 2)

### UEFI Loader (src/starkernel/boot/uefi_loader.c)
- [x] Implement `efi_main()` entry point
- [x] Collect `BootInfo` structure:
  - [x] Memory map from UEFI
  - [x] ACPI tables pointer (via config table)
  - [ ] Framebuffer info (graphics output protocol) — placeholder zeroed for now
  - [x] Runtime services table
- [~] Call `ExitBootServices()` to take control (amd64 retries and logs EXITED/ENABLED; non-amd64 currently skips until platform init stabilizes)
- [x] Jump to `kernel_main()` with BootInfo

### Serial Console (src/starkernel/hal/console.c)
- [x] Implement `hal_console_init()` for UART 16550 (x86); non-x86 stubs avoid blocking
- [x] Write `hal_console_putc()` using port I/O (0x3F8)
- [x] Write `hal_console_puts()` for strings
- [x] Test: Print loader/kernel banners on amd64/aarch64 (aarch64 limited to loader text; kernel output stub)

### Kernel Entry (src/starkernel/kernel_main.c)
- [x] Implement `kernel_main(BootInfo *boot_info)`
- [x] Print boot banner to serial console
- [x] Print memory map summary
- [x] Infinite loop with `hlt` instruction

**Exit Criteria:** QEMU boots, prints "StarKernel booting..." to serial, halts cleanly

---

### STATUS: Milestone 1 (Verified 2025-12-24)

**Result:** ✅ **COMPLETE** - Functional with limitations

**What Works:**
- ✅ UEFI loader `efi_main()` boots successfully
- ✅ Real serial console (UART 16550, 171 LOC in `src/starkernel/hal/console.c`)
  - Loopback self-test at init
  - 115200 baud, 8N1 configuration
- ✅ `ExitBootServices()` succeeds (retry loop implemented)
- ✅ BootInfo populated (memory map, ACPI tables, runtime services)
- ✅ `kernel_main()` executes with formatted output

**QEMU Boot Output (amd64):**
```
StarKernel UEFI Loader
Collecting boot information...
IDT installed.


   _____ _             _  __                    _
  / ____| |           | |/ /                   | |
 | (___ | |_ __ _ _ __| ' / ___ _ __ _ __   ___| |
  \___ \| __/ _` | '__|  < / _ \ '__| '_ \ / _ \ |
  ____) | || (_| | |  | . \  __/ |  | | | |  __/ |
 |_____/ \__\__,_|_|  |_|\_\___|_|  |_| |_|\___|_|

StarKernel v0.1.0 - FORTH Microkernel
Architecture: amd64
Build: Dec 24 2025 09:09:58

UEFI BootServices: EXITED

=== StarKernel Boot Information ===
Memory map entries: 118
Total memory: 1023 MB
Usable memory: 973 MB
===================================
```

**Limitations:**
- ⚠️ No centralized string library (no `strlen`/`memcpy`/`memset`, using compiler builtins)
- ⚠️ No formal logging API (using `console_puts()` directly, `RAW_LOG()` is loader-only)
- ⚠️ Non-x86 architectures have console stubs (aarch64/riscv64 need UART drivers)

**Recommendation:** Sufficient for M7; string/logging infrastructure can be added later

---

## Milestone 2: Physical Memory Manager (Week 3)

### PMM Bitmap Allocator (src/starkernel/memory/pmm.c)
- [x] Parse UEFI memory map
- [x] Build free page bitmap (1 bit per 4KB page)
- [x] Implement `pmm_alloc_page()` - returns physical address
- [x] Implement `pmm_free_page(paddr)`
- [x] Implement `pmm_alloc_contiguous(num_pages)`
- [x] Track total/free/used memory stats

### Testing
- [x] Allocate 10 pages, verify uniqueness
- [x] Free pages, verify they can be reallocated
- [x] Print PMM stats to serial console
- [x] Smoke test exercised at boot (serial output logged: docs/starkernel/PMM-smoke-2025-12-18.log)

**Exit Criteria:** PMM allocates/frees physical pages correctly, no overlaps

---

### STATUS: Milestones 2-3 (ELF Loader) (Verified 2025-12-24)

**Result:** ❌ **NOT APPLICABLE** - Monolithic architecture

**Analysis:**
These milestones describe a separate ELF loader that parses ELF headers, loads PT_LOAD segments, and applies relocations before jumping to kernel entry. The current implementation is monolithic - `efi_main()` directly calls `kernel_main()` at line 208 of `uefi_loader.c`.

**No code exists for:**
- ELF header parsing
- Program header iteration
- Segment loading from ESP
- REL/RELA relocation application
- Separate kernel binary artifact

**Recommendation:** These milestones cannot be assessed without first refactoring to the loader+kernel split architecture per ROADMAP_ADDENDUM.md "Non-Negotiable #1"

---

## Milestone 3: Virtual Memory Manager (Week 4)

### Page Tables (src/starkernel/memory/vmm.c)
- [x] Implement 4-level page table structures (PML4, PDPT, PD, PT)
- [x] Write `vmm_map_page(vaddr, paddr, flags)` - maps single 4KB page
- [x] Write `vmm_unmap_page(vaddr)` - unmaps and frees page table entry
- [x] Write `vmm_get_paddr(vaddr)` - translates virtual → physical
- [x] Map physical RAM from UEFI map; switch to new CR3

### Higher-Half Kernel
- [x] Map higher-half alias for validation (`0xFFFF800000000000`) and verify round-trip
- [x] Load CR3 with new page table root
- [x] VMM self-test exercises map/unmap/translate

**Exit Criteria:** Kernel runs from higher-half virtual addresses, page faults handled ✅

---

### STATUS: Milestones 2-3 (PMM/VMM) (Verified 2025-12-24)

**Result:** ✅ **COMPLETE** (Combined M2+M3 assessment)

**PMM (Physical Memory Manager) - `memory/pmm.c` (300 LOC):**
- ✅ Bitmap allocator (1 bit per 4KB page)
- ✅ Parses UEFI memory map
- ✅ Tracks up to 64 GiB (2MB bitmap)
- ✅ Allocation floor at 1 MiB
- ✅ `pmm_alloc_page()`, `pmm_free_page()`, `pmm_alloc_contiguous()`
- ✅ Statistics tracking

**PMM Boot Output:**
```
PMM initialized.
PMM statistics:
  Total pages: 250602
  Free pages : 249073
  Used pages : 1529
  Total MB   : 978
  Free  MB   : 972
  Used  MB   : 5

PMM smoke test: allocating 10 pages...
  Pages allocated successfully:
    0x100000
    0x101000
    0x102000
    0x103000
    0x104000
    0x105000
    0x106000
    0x107000
    0x108000
    0x109000
  Freeing pages...
  Re-allocated page: 0x100000
  Re-allocation reused a freed page (expected).
  Allocated 4 contiguous pages:
    Start: 0x100000
    End  : 0x103fff
PMM smoke test complete.
```

**VMM (Virtual Memory Manager) - `memory/vmm.c` (400 LOC):**
- ✅ 4-level page tables (PML4, PDPT, PD, PT)
- ✅ `vmm_map_page()`, `vmm_unmap_page()`, `vmm_get_paddr()`
- ✅ Identity mapping of first 16 MiB
- ✅ Higher-half mapping at `0xFFFF800000000000` (validated)
- ✅ CR3 switched successfully

**VMM Boot Output:**
```
VMM initialized (mapped RAM, CR3 switched)
VMM self-test: mapped OK at 0xffff800000000000
VMM self-test complete.
```

**Heap Allocator - `memory/kmalloc.c` (350 LOC):**
- ✅ Slab-based allocator
- ✅ 16 MiB default heap
- ✅ `kmalloc()`, `kfree()`, stats tracking

**Heap Boot Output:**
```
Kernel heap initialized.
Heap statistics:
  Total bytes: 16777176
  Free  bytes: 16777176
  Used  bytes: 0
  Peak  bytes: 0

Heap smoke test: allocating blocks...
  Allocations succeeded.
  Re-allocation succeeded (coalescing validated).
Heap smoke test complete.
```

---

## Milestone 4: Interrupt Handling (Week 5)

### IDT Setup (src/starkernel/hal/interrupt.c)
- [x] Define IDT structure (256 entries)
- [x] Implement gate setup per vector
- [x] Write assembly stubs for ISR entry (vector + error code normalization)
- [x] Load IDT with `lidt` instruction
- [x] Disable PIC (legacy 8259A)

### Exception Handlers
- [~] Implement handlers for CPU exceptions (0-31): default fatal handler prints info
  - [x] #DE (Divide Error) - vector 0 (gated self-test available)
  - [x] #PF (Page Fault) - vector 14 (gated self-tests available)
  - [x] #GP (General Protection) - vector 13
- [x] Print exception info: vector, error code, RIP, CR2
- [ ] Test: Trigger divide-by-zero, verify handler fires (self-test gated by DIV0_SELF_TEST)
- [ ] Test: Trigger page-fault read/write, verify handler fires (gated by PF_SELF_TEST_READ/WRITE)

### APIC Initialization
- [x] Detect APIC via ACPI MADT table (fallback to 0xFEE00000)
- [x] Map APIC MMIO region (0xFEE00000)
- [x] Enable Local APIC
- [x] Set spurious interrupt vector (0xFF)

**Exit Criteria:** Kernel handles exceptions, prints diagnostic info, doesn't triple-fault ✅

---

### STATUS: Milestone 4 (Verified 2025-12-24)

**Result:** ✅ **COMPLETE**

**What Works:**
- ✅ IDT setup (`arch/amd64/interrupts.c`) - 256 entries
- ✅ Exception handlers implemented (#DE, #PF, #GP)
- ✅ `isr_common_handler()` with diagnostic output
- ✅ APIC initialized (SIVR=0xFF)
- ✅ No triple-faults during boot

**Boot Output:**
```
IDT installed.

APIC: init...
APIC enabled (SIVR=0xFF).
APIC: init done
```

**Exception Handlers:**
- Gated self-tests available via compile flags:
  - `DIV0_SELF_TEST` - Divide-by-zero (#DE)
  - `PF_SELF_TEST_READ` - Page fault read test
  - `PF_SELF_TEST_WRITE` - Page fault write test
- Exception info printed: vector, error code, RIP, CR2

**Architecture Coverage:**
- **amd64:** ✅ Full implementation
- **aarch64:** ⚠️ Stub (needs GIC setup)
- **riscv64:** ⚠️ Stub (needs PLIC/CLINT)

---

## Milestone 5: Timer Subsystem — Authoritative Heartbeat (Week 6)

⚠️ **CRITICAL:** Timer errors silently poison DoE data and determinism claims.  
This milestone must FAIL HARD on unacceptable variance. No degraded success states.

⚠️ **CRITICAL:** There is exactly ONE heartbeat.  
All VMs observe the same authoritative timebase.  
No per-VM timers exist.

---

## 5.0 Purpose (Read This First)

The purpose of this milestone is to:

- Establish a **single, authoritative heartbeat**
- Drive **all kernel activity** from that heartbeat
- Measure **time quality honestly**
- Expose **TIME-TRUST as data**, not control flow

**Deliverable:**  
A running periodic heartbeat interrupt on QEMU *and* real hardware.

---

## 5.1 Non-Negotiable Time Model (Normative)

### Fundamental Principles

1. **One Heartbeat**
   - Started by the kernel
   - Never stops
   - Drives scheduling, VM execution, physics, DoE

2. **Time Is Always Relative**
   - No absolute time
   - No modes
   - No ABS/REL switches

3. **Truth Is a Measurement**
   - Time correctness is continuously measured
   - Never binary
   - Never gates execution

---

## 5.2 Canonical Time Vocabulary (Frozen)

These words MUST exist:

- `TIME-TICKS` (Q64.0)
  - Monotonic execution index
  - Ground truth of progression

- `TIME-RELATIVE` (Q64.0)
  - Elapsed ticks
  - Always valid

- `TIME-TRUST` (Q48.16)
  - Continuous confidence metric
  - Derived from variance, drift, agreement

NO additional time modes are permitted.

---

## 5.3 Numeric & Math Constraints (HARD)

- Floating-point is **FORBIDDEN**
- Allowed formats only:
  - Q64.0
  - Q48.16
- All statistics must be deterministic fixed-point math

Build must FAIL if FP instructions are emitted.

---

## 5.4 Heartbeat Architecture (What Must Exist)

### Heartbeat Responsibilities

The heartbeat ISR MUST:

- Increment `TIME-TICKS`
- Sample high-resolution timestamp
- Feed variance & drift estimators
- Update TIME-TRUST
- Drive scheduler tick

### What the Heartbeat Is NOT

- Not simulated
- Not stubbed
- Not VM-local
- Not optional

---

## 5.5 Time Sources & Calibration (Initialization Phase)

### Allowed Sources (amd64)

- TSC (preferred)
- HPET (bare metal)
- PIT / PM Timer (VM-safe fallback)

### Calibration Rules

1. Calibrate TSC via **at least two independent sources**
2. Cross-check results
3. If disagreement > 0.1% → **HALT**
4. Log all values to serial

There is NO “calibration complete” moment.  
Calibration continues as feedback.

---

## 5.6 Virtualization Reality (Design Rule)

- Same logic for QEMU, KVM, real hardware
- VM instability is EXPECTED
- Instability reduces TIME-TRUST
- Execution NEVER changes

---

## 5.7 DoE Integration (Why This Exists)

For every DoE run, record:

- TIME-TICKS (Q64.0)
- TIME-RELATIVE (Q64.0)
- TIME-TRUST (Q48.16)
- p-value (fixed-point)

This allows peers to explain variance as:
- timing quality vs
- execution divergence

---

## 5.8 IMPLEMENTATION ORDER (BUILD THIS IN THIS ORDER)

### Step 1 — Timer Init (Already Mostly Done)

- Detect VM vs bare metal
- Select available time sources
- Perform calibration
- Print calibration report

**Exit:** calibration succeeds or kernel halts

---

### Step 2 — APIC Timer Bring-Up (CORE WORK)

You MUST now do the following:

1. Program APIC timer divisor
2. Set initial count
3. Configure LVT timer:
   - periodic mode
   - vector 0x20
4. Install ISR in IDT

NO VM OR KERNEL CODE may run periodically until this works.

---

### Step 3 — Heartbeat ISR

ISR MUST:

- Increment global `time_ticks`
- Call heartbeat update function
- Acknowledge APIC interrupt

Keep ISR minimal. No printing inside ISR.

---

### Step 4 — Heartbeat Loop Logic (Non-ISR)

On heartbeat update:

- Accumulate timestamp samples
- Update variance & drift (Q48.16)
- Update TIME-TRUST
- Optionally log summary every N ticks

---

### Step 5 — Visibility (MANDATORY)

Every N ticks (e.g. 1000):

- Print:
  - ticks
  - ns elapsed
  - TIME-TRUST
  - variance

This is required for debugging and review.

---

## 5.9 Validation Tests (MANDATORY)

- 100-tick APIC vs TSC comparison
- 1000 × 10ms sleep variance test
- Variance must be < 0.01%

Failure → HALT

---

## 5.10 LLM RULES (MANDATORY)

**Claude / Codex / All LLMs**

- Do NOT add time modes
- Do NOT stub heartbeat
- Do NOT fake timers
- Do NOT introduce FP math
- Do NOT proceed past this milestone without review

---

## 5.11 Review Gate (MANDATORY)

Before moving on:

1. Capture QEMU output
2. Capture real hardware output (if available)
3. Paste logs into ROADMAP.md (code block)
4. Await Captain Bob approval

NO further work without approval.

---

## 5.12 Current Status (2025-12-24)

**State:** ⚠️ PARTIALLY COMPLETE

### Working
- Calibration logic
- Multi-source detection
- Trust computation logic

### Missing (BLOCKERS)
- APIC timer not started
- No periodic interrupt
- No heartbeat ISR
- Kernel halts after init

### Next Action
👉 Implement APIC periodic timer + ISR  
👉 Prove heartbeat ticks on QEMU **and** hardware


---

## Milestone 6: Heap Allocator (Week 7)

### Kernel Heap (src/starkernel/memory/kmalloc.c)
- [x] Reserve virtual address range for heap (16MB contiguous from PMM)
- [x] Implement simple bump allocator or slab allocator
- [x] Write `kmalloc(size)` - allocates kernel memory
- [x] Write `kfree(ptr)` - frees kernel memory
- [x] Track heap usage stats

### HAL Memory Implementation
- [x] Implement `hal_mem_alloc(size)` → calls `kmalloc()`
- [x] Implement `hal_mem_free(ptr)` → calls `kfree()`
- [x] Implement `hal_mem_alloc_aligned(size, align)`

**Exit Criteria:** Kernel can allocate/free dynamic memory, VM can use HAL memory functions
## Milestone 7: VM Integration + INIT Capsules (Week 8)

### Architectural Contract (Frozen)
1. **Mama Forth governs VM birth.**
  - VM creation is initiated by the primordial VM ("mama").
  - Kernel provides primitives; policy lives in mama.

2. **INIT is a capsule artifact.**
  - INIT content is delivered as a *byte blob* plus metadata.
  - Delivery method is abstract: today = local repo; later = UDP via service VM.
  - Kernel does NOT “download”. It only ingests a provided capsule via a driver/HAL call.

3. **Transport is pluggable, semantics are not.**
  - Local repo is a mock transport that must behave like future UDP transport:
    - chunking-friendly (even if not used yet)
    - explicit capsule ids
    - signature/hashes (can be stubbed for now ONLY if explicitly marked)

4. **Heartbeat/timebase is global.**
  - No per-VM time. VM observes TIME-TICKS/TIME-RELATIVE/TIME-TRUST only.

### Definitions

**INIT Capsule**
- A capsule is identified by a deterministic string id:
  - e.g. `init/core/mama@1`, `init/user/fred@1`
- Stored locally for now in:
  - `ESP:/STARKERNEL/INIT/` (FAT)
  - or `build/.../esp/STARKERNEL/INIT/` in QEMU setup
- The capsule format is initially a raw blob with a tiny header:

#### Capsule Header (v0)
All fields are little-endian.

- magic[8]      = "SKINIT00"
- version u32   = 1
- payload_len u32
- payload_sha256[32]  (optional in v0, but RESERVED bytes must exist)
- flags u32     (bit0 = HEADLESS_OK, bit1 = REPL_OK, etc.)
- reserved[64]  (future signing metadata)

Followed by:
- payload[payload_len]

**NOTE:** signature enforcement may be deferred in Week 8, but the file format MUST reserve the fields.

### Kernel Interfaces (C level)

Implement in kernel (no stubs):

- `int sk_capsule_load_from_repo(const char *capsule_id, void **out_buf, u64 *out_len);`
  - For now this reads from local repo (ESP/disk).
  - Must return deterministic errors and print clear logs.

- `int sk_vm_birth(const void *init_blob, u64 init_len, struct VMInstance **out_vm);`
  - Creates VM control block and memory space.
  - Injects init blob into VM in a defined location.
  - Prepares VM IP to init entrypoint (or sets a flag "init pending").

- `int sk_vm_eval_boot(struct VMInstance *vm);`
  - Executes the init entry path in the VM context until it yields/returns.

### VM Words (Forth level) – Mama Governance

Add kernel words (minimal) that mama can call:

- `VM-BIRTH ( capsule_id_addr capsule_id_len -- vmid ior )`
  - Loads capsule by id via capsule loader
  - Verifies header/payload shape (at minimum)
  - Creates VM, injects INIT, starts init eval
  - Returns vmid, ior=0 success

- `VM-INJECT-INIT ( vmid capsule_id_addr capsule_id_len -- ior )`
  - Requires capability later; for now allowed only for mama
  - Injects init payload; does not auto-run unless explicitly asked

- `VM-RUN ( vmid -- ior )`
  - Marks VM runnable (scheduler later)
  - For Week 8 single-VM: can be a no-op or immediate run

### Week 8 Exit Criteria (Explicit)
1. Kernel boots to mama Forth prompt on serial.
2. Mama can execute:
  - `VM-BIRTH` with a capsule id pointing at local repo file.
3. The new VM runs its INIT and prints a deterministic banner (serial).
4. No git actions are performed by any LLM during this milestone.

### Local Repo Mock Layout (REQUIRED)
- `STARKERNEL/INIT/`
  - `core_mama.skinit`
  - `user_fred.skinit`
  - `test_hello.skinit`

Capsule id mapping rule (v0):
- id "init/test/hello@1" maps to file `test_hello.skinit`
- mapping may be hardcoded in Week 8; must be documented in ROADMAP.md

---

### LLM Instructions (MANDATORY)

**All LLMs**
- DO NOT run any git commands.
- DO NOT commit, push, branch, or reset.
- All changes must be local edits only.
- After completing Milestone 7 tasks, capture QEMU output and paste it into ROADMAP.md in a code block.
- Captain Bob must review ROADMAP.md output before any work continues.

**Claude**
- Work in small atomic edits (one file at a time).
- Before modifying build scripts or boot flow, write a short change plan in ROADMAP.md.
- Do not change timer/heartbeat code unless explicitly instructed.

**Codex**
- Only implement the capsule loader and the kernel word wiring.
- Do not refactor unrelated subsystems.
- Do not modify the interrupt/timer subsystems.
- Produce compile-ready code with full file outputs (no fragments).


---

### STATUS: Milestone 6 (Verified 2025-12-24)

**Result:** ✅ **COMPLETE**

**Implementation** (`memory/kmalloc.c`, 350 LOC):
- ✅ Slab-based allocator
- ✅ 16 MiB default heap region
- ✅ `kmalloc(size)`, `kfree(ptr)`
- ✅ Statistics tracking (total/free/used/peak)
- ✅ Coalescing validated

**Boot Output:**
```
Kernel heap initialized.
Heap statistics:
  Total bytes: 16777176
  Free  bytes: 16777176
  Used  bytes: 0
  Peak  bytes: 0

Heap smoke test: allocating blocks...
  Allocations succeeded.
  Re-allocation succeeded (coalescing validated).
Heap smoke test complete.
```

**HAL Memory Functions:**
- ⚠️ **HAL stub** - `src/starkernel/hal/memory.c` is only 323 bytes
- Current: Architecture-specific allocators used directly
- Missing: `hal_mem_alloc()`, `hal_mem_free()`, `hal_mem_alloc_aligned()` wrappers

**Actual vs Expected:**
The roadmap expected HAL wrappers calling `kmalloc()`, but the implementation directly uses architecture memory managers. This is functionally equivalent but bypasses the HAL abstraction layer.

**Recommendation:** HAL wrappers can be added during M7 VM integration when needed

---

## Milestone 7: VM Integration (Week 8)

### VM Initialization
- [ ] Allocate VM structure via `kmalloc()`
- [ ] Call `vm_init_dictionary()` to populate built-in words
- [ ] Call `vm_init_physics()` to initialize heat/window/cache
- [ ] Verify dictionary links correctly
- [ ] Test: Execute `1 2 + .` via C, verify output "3"

### VM Arbiter Stubs
- [ ] Create `VMInstance` control block
- [ ] Implement `vm_create_primordial()` - creates VM[0]
- [ ] Grant VM[0] capabilities (CAP_SPAWN, CAP_MMIO, CAP_IRQ)
- [ ] Store VM[0] in global `current_vm` pointer

### Kernel Words (Minimal Set)
- [ ] Implement `TICKS` - returns `hal_time_now_ns()`
- [ ] Implement `YIELD` - stub (single VM, no-op for now)
- [ ] Test: Execute `TICKS .` in VM, verify timestamp printed

**Exit Criteria:** VM executes Forth words correctly in kernel context

---

### STATUS: Milestone 7 (Verified 2025-12-24)

**Result:** ❌ **NOT STARTED** - No VM integration yet

**Current State:**
- ❌ NO VM allocated - `kmalloc()` not called for VM structure
- ❌ NO dictionary initialization - `vm_init_dictionary()` not called
- ❌ NO physics initialization - `vm_init_physics()` not called
- ❌ NO VM arbiter - No `VMInstance` control block exists
- ❌ NO kernel words - `TICKS`, `YIELD` not implemented

**Complete Boot Output (All Milestones 0-6):**
```
StarKernel UEFI Loader
Collecting boot information...
IDT installed.


   _____ _             _  __                    _
  / ____| |           | |/ /                   | |
 | (___ | |_ __ _ _ __| ' / ___ _ __ _ __   ___| |
  \___ \| __/ _` | '__|  < / _ \ '__| '_ \ / _ \ |
  ____) | || (_| | |  | . \  __/ |  | | | |  __/ |
 |_____/ \__\__,_|_|  |_|\_\___|_|  |_| |_|\___|_|

StarKernel v0.1.0 - FORTH Microkernel
Architecture: amd64
Build: Dec 24 2025 09:09:58

UEFI BootServices: EXITED

=== StarKernel Boot Information ===
Memory map entries: 118
Total memory: 1023 MB
Usable memory: 973 MB
===================================

PMM initialized.
PMM statistics:
  Total pages: 250602
  Free pages : 249073
  Used pages : 1529
  Total MB   : 978
  Free  MB   : 972
  Used  MB   : 5

PMM smoke test: allocating 10 pages...
  Pages allocated successfully:
    0x100000
    0x101000
    0x102000
    0x103000
    0x104000
    0x105000
    0x106000
    0x107000
    0x108000
    0x109000
  Freeing pages...
  Re-allocated page: 0x100000
  Re-allocation reused a freed page (expected).
  Allocated 4 contiguous pages:
    Start: 0x100000
    End  : 0x103fff
PMM smoke test complete.

VMM initialized (mapped RAM, CR3 switched)
VMM self-test: mapped OK at 0xffff800000000000
VMM self-test complete.

Kernel heap initialized.
Heap statistics:
  Total bytes: 16777176
  Free  bytes: 16777176
  Used  bytes: 0
  Peak  bytes: 0

Heap smoke test: allocating blocks...
  Allocations succeeded.
  Re-allocation succeeded (coalescing validated).
Heap smoke test complete.

APIC: init...
APIC enabled (SIVR=0xFF).
APIC: init done
Timer: init...
Timer: init start
Timer: VM mode detected (hypervisor present).
Timer: HPET calibration disabled (VM-exit MMIO would poison timing).
Timer: WARNING: invariant TSC not present under hypervisor.
Timer:          continuing in RELATIVE mode (no determinism guarantees).
Timer: RDTSCP not present; using RDTSC (less serialized).
Timer: CPUID frequency unavailable; trying PM Timer...
Timer: WARNING: could not derive TSC frequency; PM Timer will be used for RELATIVE ns.
Timer: trust=1 (0=NONE,1=REL,2=ABS), TSC=0 Hz
Timer: init done
Kernel initialization complete.
Boot successful!

Kernel halted. (QEMU: Press Ctrl+A, then X to exit)
```

**Recommendation:** Begin M7 VM integration as the next milestone

---

## Milestone 8: REPL on Serial Console (Week 9)

### Console Input
- [ ] Implement `hal_console_getc()` - polls UART for input
- [ ] Implement `hal_console_poll()` - non-blocking check
- [ ] Handle backspace, newline, basic line editing

### REPL Integration
- [ ] Port `repl.c` to work with kernel HAL
- [ ] Read line from serial console
- [ ] Pass to `vm_eval()` for execution
- [ ] Print result or error to serial
- [ ] Test: Type `1 2 +` [Enter], verify "ok" response

### Boot Sequence
- [ ] kernel_main() calls:
  1. HAL initialization (memory, console, timer, interrupts)
  2. VM initialization (dictionary, physics)
  3. Print "ok" prompt
  4. Enter REPL loop (never returns)

**Exit Criteria:** Boot to `ok` prompt, accept Forth input, execute words, print results

---

## Milestone 9: Multi-VM Support (Week 10-11)

### VM Arbiter Scheduler
- [ ] Implement FIFO run queue (`SchedQueue`)
- [ ] Implement `vm_scheduler_enqueue(vm)`
- [ ] Implement `vm_scheduler_dequeue()` → next VM
- [ ] Implement `vm_arbiter_next()` - selects runnable VM
- [ ] Implement quantum-based preemption (10ms default)

### VM Transitions
- [ ] Implement `vm_transition(from, to)`:
  - [ ] Save `from` VM state (IP, SP, RP)
  - [ ] Restore `to` VM state
  - [ ] Switch page tables (if separate address spaces)
  - [ ] Update `current_vm` global
- [ ] Hook APIC timer to preempt current VM

### SPAWN-VM Kernel Word
- [ ] Implement `word_spawn_vm()`:
  - [ ] Check CAP_SPAWN capability
  - [ ] Allocate new VMInstance
  - [ ] Attenuate capabilities (child ⊆ parent)
  - [ ] Initialize child VM dictionary (clone or fresh)
  - [ ] Enqueue child as Runnable
- [ ] Test: VM[0] spawns VM[1], both execute concurrently

**Exit Criteria:** Two VMs time-slice correctly, SPAWN-VM creates new VMs

---

## Milestone 10: Message Passing IPC (Week 12)

### Message Queues
- [ ] Define `MessageQueue` structure (circular buffer)
- [ ] Implement `msg_queue_init(vm)`
- [ ] Implement `msg_queue_enqueue(vm, msg)`
- [ ] Implement `msg_queue_dequeue(vm)` → msg or NULL

### SEND/RECV Kernel Words
- [ ] Implement `SEND ( vmid msg -- )`:
  - [ ] Validate target vmid exists
  - [ ] Check CAP_SEND capability
  - [ ] Enqueue message to target VM
  - [ ] Wake target VM if blocked on RECV
- [ ] Implement `RECV ( -- msg )`:
  - [ ] Dequeue message from current VM's queue
  - [ ] If empty, block current VM (state = Blocked)
  - [ ] Yield to scheduler
- [ ] Implement `RECV? ( -- msg flag )` - non-blocking variant

**Exit Criteria:** VM[0] sends message to VM[1], VM[1] receives and processes it

---

## Milestone 11: Capability System (Week 13)

### Capability Enforcement
- [ ] Implement `capability_has(vm, cap)` → bool
- [ ] Implement `capability_grant(vm, cap)`
- [ ] Implement `capability_revoke(vm, cap)`
- [ ] Update all kernel words to check capabilities:
  - [ ] SPAWN-VM requires CAP_SPAWN
  - [ ] KILL-VM requires CAP_KILL
  - [ ] MMIO-READ/WRITE require CAP_MMIO

### Capability Kernel Words
- [ ] Implement `HAS-CAP? ( cap -- flag )`
- [ ] Implement `GRANT-CAP ( vmid cap -- )`
- [ ] Implement `REVOKE-CAP ( vmid cap -- )`
- [ ] Test: VM without CAP_SPAWN fails to spawn child

**Exit Criteria:** Capability checks enforced, privilege escalation impossible

---

## Milestone 12: ACL System with TTL Caching (Week 14)

### ACL Data Structures
- [ ] Implement `ACList` (linked list of ACLEntry)
- [ ] Implement `acl_check(acl, vmid, perm)` → bool
- [ ] Implement `acl_grant(acl, vmid, perm)`
- [ ] Implement `acl_revoke(acl, vmid, perm)`

### ACL Cache
- [ ] Implement `ACLCache` (hash table with TTL entries)
- [ ] Implement `acl_check_cached(cache, acl, vmid, perm, now)`
  - [ ] Check cache, validate TTL not expired
  - [ ] On cache miss or expiry, perform full check
  - [ ] Cache result with absolute expiration time
- [ ] Set `ACL_TTL_NS = 1000000000` (1 second)

### ACL Kernel Words
- [ ] Implement `ACL-CREATE ( -- acl )`
- [ ] Implement `ACL-GRANT ( acl vmid perm -- )`
- [ ] Implement `ACL-REVOKE ( acl vmid perm -- )`
- [ ] Implement `ACL-CHECK ( acl perm -- flag )`

**Exit Criteria:** ACL revocation bounded by 1 second, cache provides 10-20x speedup

---

## Milestone 13: Block Device Support (Week 15)

### Virtual Block Device
- [ ] Create `BlockDevice` structure with read/write ops
- [ ] Implement RAM-backed block device (for testing)
- [ ] Implement block device registry (vmid → device mapping)

### Block I/O Kernel Words
- [ ] Implement `BLOCK-READ ( blk addr -- )`
  - [ ] Check ACL permissions for block range
  - [ ] Call device read operation
  - [ ] Copy data to VM memory
- [ ] Implement `BLOCK-WRITE ( blk addr -- )`
  - [ ] Check ACL permissions for block range
  - [ ] Copy data from VM memory
  - [ ] Call device write operation

### ACL on Blocks
- [ ] Extend ACL to support block ranges
- [ ] Implement `BLOCK-ACL-GRANT-RANGE ( device start end vmid perm -- )`
- [ ] Test: VM[0] grants blocks 0-99 to VM[1], VM[1] reads/writes

**Exit Criteria:** VMs can read/write blocks with ACL enforcement

---

## Milestone 14: Runtime Verification (Week 16)

### Runtime Monitors
- [ ] Implement `runtime_check_capability(vm, cap)` from Isabelle
- [ ] Implement `runtime_check_bounds(vm, addr)` from Isabelle
- [ ] Implement `runtime_check_scheduler_invariant()`
- [ ] Add verification points at:
  - [ ] Every kernel word entry
  - [ ] Every VM transition
  - [ ] Every memory access

### Violation Logging
- [ ] Create `ViolationLog` circular buffer
- [ ] Implement `runtime_violation(vm, type, message)`
- [ ] Print violations to serial console
- [ ] Count violations per VM

### Verification Modes
- [ ] Implement `VERIFICATION_MODE` flag:
  - [ ] `FULL` - Check everything (development)
  - [ ] `SAMPLING` - Check 1% random sample (production)
  - [ ] `DISABLED` - No checks (benchmarking)

**Exit Criteria:** Runtime monitors detect and log capability violations, memory bounds errors

---

## Milestone 15: Formal Verification Integration (Week 17-18)

**⚠️ REALISTIC SCOPE:** Complete proofs will take longer than v0.1.0 timeline.
Focus on proof coverage and structural soundness, not completion.

### Isabelle Proof Coverage (Best Effort)
- [ ] **Attempt** 3 `sorry` proofs in Scheduler.thy:
  - [ ] `fifo_guarantees_progress` (induction on queue length) - **HARD**
  - [ ] `no_starvation` (follows from FIFO property) - **MEDIUM**
  - [ ] `bounded_waiting` (induction on queue position) - **MEDIUM**
- [ ] Add Message Passing theory (MessageQueue.thy) - **skeleton + key theorems**
- [ ] Add Kernel Words safety theory (KernelWords.thy) - **skeleton + key theorems**
- [ ] **Document proof structure** even if incomplete
- [ ] **Tag remaining `sorry`s** with difficulty estimates and proof sketches

### Code Generation from Isabelle (Prioritize)
- [ ] Export `runtime_check_bounds` to C (already proven complete)
- [ ] Export `acl_check` to C (proven sound)
- [ ] Export `capability_has` to C (proven sound)
- [ ] Integrate generated code into kernel build
- [ ] **Validate:** Generated code matches manual implementation

### Trace Validation (Stretch Goal)
- [ ] Implement execution trace capture (basic format)
- [ ] Export trace to file
- [ ] **Defer:** Full trace theorem validation to post-v0.1.0
- [ ] Document trace format for future validation

### Proof Status Documentation
- [ ] Create `formal/isabelle/PROOF_STATUS.md`:
  - [ ] List all theorems with proof status (proven / sorry / partial)
  - [ ] Estimate effort to complete remaining proofs
  - [ ] Prioritize critical safety properties
  - [ ] Document proof dependencies

**Exit Criteria (Revised for v0.1.0):**
- ✅ Proof **structure** in place (4 theories, key theorems stated)
- ✅ Critical theorems proven: `capability_attenuation`, `acl_revocation_bounded`, `at_most_one_vm_per_core`
- ✅ Runtime monitors generated from proven theorems
- ✅ Remaining `sorry`s documented with proof sketches
- ⚠️ **NOT REQUIRED:** All proofs complete (defer to post-v0.1.0)
- ✅ **REQUIRED:** No known violations of stated theorems in practice

---

## Milestone 16: Performance & Determinism Validation (Week 19)

**⚠️ CRITICAL:** This milestone validates the core claim: 0% algorithmic variance.
If determinism fails on any architecture, the kernel is not shippable.

**DoE Trust Gating:** DoE runs collected while timer_trust < ABSOLUTE must be flagged as untrusted and excluded from determinism and variance calculations. Such runs may be used for qualitative diagnostics only (functional correctness, ordering, qualitative behavior), but must not contribute to determinism claims. In RELATIVE trust mode, determinism validation is skipped and the kernel proceeds without making timing claims. Any DoE results collected in RELATIVE mode are diagnostic-only and must not be used to support determinism assertions.

### Per-Architecture DoE Validation
- [ ] Port DoE test suite to run on bare metal (amd64, aarch64, riscv64)
- [ ] Run full 780+ test suite on each architecture
- [ ] **HARD REQUIREMENT:** 0% algorithmic variance across:
  - [ ] 10 runs on amd64 (QEMU + real hardware)
  - [ ] 10 runs on aarch64 (Raspberry Pi 4B)
  - [ ] 10 runs on riscv64 (QEMU only)
- [ ] Calculate variance metrics:
  - [ ] Rolling window contents (must be identical)
  - [ ] Execution heat values (must be identical)
  - [ ] Pipelining transition counts (must be identical)
  - [ ] Test pass/fail results (must be identical)

### Cross-Architecture Comparison
- [ ] Compare amd64 vs aarch64 vs riscv64 DoE results
- [ ] **Expected:** Absolute values differ (different ISAs, different clock speeds)
- [ ] **Required:** Variance within architecture = 0%
- [ ] **Acceptable:** Cross-arch variance due to ISA differences (document)
- [ ] **Unacceptable:** Non-zero variance within same architecture

### Performance Benchmarking
- [ ] Measure on each architecture:
  - [ ] VM transition latency (cycles)
  - [ ] Kernel word invocation overhead (cycles)
  - [ ] ACL cache hit rate (%)
  - [ ] Message passing throughput (msgs/sec)
- [ ] Compare to Linux-hosted StarForth baseline (per architecture)
- [ ] **Target:** ≤10% overhead vs Linux-hosted

### Per-Architecture Physics Tuning (REQUIRED)
**Rationale:** Different architectures have different optimal tuning parameters while maintaining determinism within architecture.

- [ ] **Window Width Tuning (per architecture):**
  - [ ] amd64: Tune rolling window size (baseline: 1024 samples)
  - [ ] aarch64: Tune rolling window size (may differ from amd64)
  - [ ] riscv64: Tune rolling window size (may differ from amd64)
  - [ ] Validate: 0% variance with tuned parameters
  - [ ] Document rationale for each architecture's window size

- [ ] **Adaptive Heartbeat Tuning (per architecture):**
  - [ ] amd64: Tune heartbeat frequency (baseline: 100 Hz)
  - [ ] aarch64: Tune heartbeat frequency (may differ due to lower CPU freq)
  - [ ] riscv64: Tune heartbeat frequency (may differ due to QEMU emulation)
  - [ ] Validate: Timer calibration <0.1% jitter
  - [ ] Document rationale for each architecture's heartbeat rate

- [ ] **Decay Slope Tuning (per architecture):**
  - [ ] amd64: Tune heat decay rate
  - [ ] aarch64: Tune heat decay rate (may differ)
  - [ ] riscv64: Tune heat decay rate (may differ)
  - [ ] Validate: Cache effectiveness maintained

- [ ] **Hot-Words Cache Tuning (per architecture):**
  - [ ] amd64: Tune cache size and eviction policy
  - [ ] aarch64: Tune cache size (may differ due to ARM cache hierarchy)
  - [ ] riscv64: Tune cache size (may differ)
  - [ ] Validate: >90% cache hit rate on realistic workloads

- [ ] **Create Tuning Configuration Files:**
  - [ ] `include/starkernel/arch/amd64/tuning.h`
  - [ ] `include/starkernel/arch/aarch64/tuning.h`
  - [ ] `include/starkernel/arch/riscv64/tuning.h`
  - [ ] Example:
    ```c
    /* amd64 tuning parameters */
    #define WINDOW_WIDTH 1024
    #define HEARTBEAT_HZ 100
    #define DECAY_SLOPE 0.95f
    #define HOTWORDS_CACHE_SIZE 64
    ```

### Optimization (If Needed)
- [ ] Profile hot paths using TSC (per architecture)
- [ ] Optimize VM arbiter (reduce context switch overhead)
- [ ] Optimize dictionary search (hot-words cache effectiveness)
- [ ] Tune ACL cache size and TTL
- [ ] **Re-validate:** Determinism after any optimization

### Determinism Report
- [ ] Generate formal report for each architecture:
  ```
  Architecture: amd64
  Test Runs: 10
  Total Tests: 780
  Variance: 0.000% ✓
  Heat Delta: 0.000% ✓
  Window Delta: 0.000% ✓
  Transition Delta: 0.000% ✓
  STATUS: DETERMINISTIC
  ```

**Exit Criteria:**
- ✅ **MANDATORY:** 0% algorithmic variance on amd64, aarch64, riscv64
- ✅ Performance within 10% of Linux-hosted (per architecture)
- ✅ DoE test suite passes 100% on all architectures
- ✅ Determinism report generated and verified
- ❌ **BLOCKER:** Any non-zero variance within architecture

---

## Milestone 17: Hardware Testing (Week 20)

### Real Hardware Boot
- [ ] Create bootable USB with BOOTX64.EFI
- [ ] Test on physical x86_64 machine
- [ ] Verify serial output on real UART
- [ ] Test APIC timer on real hardware

### Debugging
- [ ] Set up QEMU GDB stub for kernel debugging
- [ ] Create GDB script to load symbols
- [ ] Test single-stepping through VM arbiter
- [ ] Document common debugging scenarios

**Exit Criteria:** StarKernel boots on real hardware, passes all tests

---

## Milestone 18: Documentation & Release (Week 21)

### Documentation
- [ ] Complete `docs/starkernel/BUILD.md`
- [ ] Complete `docs/starkernel/DEBUGGING.md`
- [ ] Complete `docs/starkernel/ARCHITECTURE.md`
- [ ] **Create `docs/starkernel/TUNING.md`:**
  - [ ] Document per-architecture tuning parameters
  - [ ] Explain rationale for each architecture's values
  - [ ] Include tuning methodology and validation criteria
  - [ ] Example:
    ```
    # amd64 Tuning
    - Window Width: 1024 (cache-optimized for L2 size)
    - Heartbeat: 100 Hz (timer resolution limit)
    - Decay Slope: 0.95 (validated via DoE)

    # aarch64 Tuning (Raspberry Pi 4B)
    - Window Width: 512 (smaller L2, lower memory bandwidth)
    - Heartbeat: 50 Hz (ARM timer precision, lower CPU freq)
    - Decay Slope: 0.92 (validated via DoE)
    ```
- [ ] Record screencast: Boot to `ok`, create VM, send message (all architectures)
- [ ] Write blog post announcing StarKernel

### Release Checklist
- [ ] All 780+ tests pass on bare metal (all architectures)
- [ ] 0% algorithmic variance maintained (per architecture)
- [ ] Per-architecture tuning parameters documented
- [ ] ~~All Isabelle proofs complete~~ ⚠️ Proof coverage documented, critical theorems proven
- [ ] Runtime verification catches test violations
- [ ] Documentation complete and accurate
- [ ] ISO (amd64), RasPi image (aarch64), QEMU binary (riscv64) all boot to `ok`

**Exit Criteria:**
- ✅ StarKernel v0.1.0 released
- ✅ Boots to `ok` on QEMU (all architectures) and real hardware (amd64 + RasPi 4B)
- ✅ Multi-VM scheduling working
- ✅ IPC message passing validated
- ✅ Capability + ACL security enforced
- ✅ 0% algorithmic variance per architecture
- ⚠️ Isabelle proof coverage documented (not blocking release)

---

## Post-Release: Service VMs (Future)

### UART Service VM
- [ ] Move UART driver into dedicated VM
- [ ] Other VMs send messages to UART VM for I/O
- [ ] Test isolation: crash UART VM, kernel survives

### Storage Service VM
- [ ] Implement AHCI driver in VM
- [ ] Expose block device via IPC
- [ ] Test: App VM reads/writes disk via storage VM

### Network Service VM
- [ ] Implement virtio-net driver in VM
- [ ] TCP/IP stack in Forth
- [ ] Test: HTTP server in Forth

---

## Critical Path Summary

**Week 1:** Multi-architecture build system (amd64, aarch64, riscv64) + ISO/RasPi images
**Weeks 2-9:** Foundation → Boot to `ok` prompt (single VM, all architectures)
**Weeks 10-13:** Multi-VM + IPC + Security (capabilities + ACLs)
**Weeks 14-18:** Verification + Performance + Hardware testing
**Week 21:** Release (ISO + RasPi 4B images for all features)

**Total Estimated Time:** 21 weeks (5 months)

**Parallelization Opportunities:**
- Week 1: Develop amd64 first, then parallelize aarch64 and riscv64 ports
- Weeks 10-13: One person on VM arbiter, another on ACL system
- Weeks 17-18: Isabelle proofs parallel to performance optimization
- Weeks 19-20: Test amd64 + aarch64 + riscv64 in parallel on different hardware

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Timer calibration inaccurate** | **High** | **HIGH** | **MANDATORY:** HPET+PIT cross-check, fail hard on >0.1% variance |
| Multi-arch toolchain issues | High | High | Test builds early, document toolchain versions, amd64 first |
| Page fault handler bugs | High | Critical | Extensive testing, QEMU `-d int` logging, single-step debugging |
| Per-arch determinism divergence | Medium | Critical | DoE validation on all architectures, fail if variance ≠ 0% |
| RasPi 4B firmware incompatibility | Medium | Medium | Test multiple firmware versions, use latest stable |
| UEFI boot issues on hardware | Medium | Medium | Test on multiple machines, provide ISO fallback |
| Isabelle proofs blocking release | Medium | Medium | Mark as "in-progress coverage", do not block v0.1.0 |
| RISC-V QEMU limitations | Low | Low | Explicitly second-class citizen, QEMU-only validation |
| Performance regression | Low | Medium | Continuous benchmarking, DoE validation |

---

## Dependencies

**External Tools:**
- **QEMU 7.0+**:
  - `qemu-system-x86_64` with OVMF firmware (amd64)
  - `qemu-system-aarch64` for ARM64 testing
  - `qemu-system-riscv64` for RISC-V testing
- **Toolchains**:
  - `x86_64-elf-gcc` or native GCC (amd64)
  - `aarch64-linux-gnu-gcc` or `aarch64-none-elf-gcc` (aarch64)
  - `riscv64-unknown-elf-gcc` or `riscv64-linux-gnu-gcc` (riscv64)
- **Image Tools**:
  - `xorriso` or `mkisofs` for ISO generation (amd64)
  - `parted`, `mkfs.vfat` for RasPi image generation (aarch64)
- **Debugging**:
  - GDB 10.0+ with multi-arch support
  - OpenOCD (optional, for RasPi 4B JTAG debugging)
- **Verification**:
  - Isabelle2024 for formal verification

**Hardware (Testing):**
- x86_64 machine or VM for amd64 ISO testing
- Raspberry Pi 4B (4GB or 8GB) for aarch64 testing
- USB-to-TTL serial adapter (3.3V) for RasPi serial console
- RISC-V hardware (optional, QEMU sufficient)

**Internal:**
- StarForth VM must pass all 780+ tests
- HAL interfaces must be stable and frozen
- Physics subsystem must maintain 0% variance within each architecture
- Per-architecture tuning parameters documented with rationale
- Determinism validation completed before any tuning changes

---

## Success Metrics

**Per-Architecture Goals (amd64, aarch64, riscv64):**
1. **Boot to `ok`**: All architectures boot to Forth REPL
2. **ISO/Image**: Bootable artifacts for each architecture
3. **Multi-VM**: 2+ VMs time-slicing correctly on all architectures
4. **IPC**: Message passing <100 cycles (architecture-normalized)
5. **Security**: 0 capability violations in 1M operations
6. **Performance**: ≤10% overhead vs Linux-hosted (per architecture)
7. **Tuning**: Per-architecture physics parameters (window width, heartbeat, decay slope)
8. **Verification**: Critical Isabelle theorems proven (arch-agnostic proofs)
9. **Determinism**: 0% algorithmic variance within each architecture (tuning may differ)

**Release Artifacts (v0.1.0):**
- ✅ `build/amd64/kernel/starkernel.iso` - Bootable x86_64 ISO
- ✅ `build/aarch64/kernel/starkernel-rpi4.img` - Raspberry Pi 4B SD card image
- ✅ `build/riscv64/kernel/starkernel.efi` - RISC-V UEFI binary (QEMU tested)

---

## Next Actions (This Week - Milestone 0)

**Day 1-2: Directory Structure & Build System (4 hours)**
1. Create `src/starkernel/` directory tree with arch/ subdirectories (30 min)
2. Create `tools/` directory with placeholder scripts (15 min)
3. Set up `Makefile.starkernel` with ARCH/TARGET detection (2 hours)
   - Support `ARCH=amd64|aarch64|riscv64`
   - Support `TARGET=kernel`
   - Toolchain detection for all architectures
   - Output to `build/${ARCH}/${TARGET}/`
4. Create linker scripts for all architectures (1 hour)
5. Test: `make ARCH=amd64 TARGET=kernel` compiles empty stub (15 min)

**Day 3-4: Minimal amd64 Boot (6 hours)**
6. Write minimal UEFI loader (`src/starkernel/boot/uefi_loader.c`) (2 hours)
7. Implement serial console output for amd64 (1 hour)
8. Test on QEMU: `make ARCH=amd64 TARGET=kernel qemu` prints "StarKernel" (1 hour)
9. Create `tools/make-iso.sh` script (1 hour)
10. Test: `make ARCH=amd64 TARGET=kernel iso` produces bootable ISO (1 hour)

**Day 5: Documentation & Testing (3 hours)**
11. Document toolchain setup in `docs/starkernel/BUILD.md` (1.5 hours)
12. Document QEMU testing in `docs/starkernel/QEMU.md` (1 hour)
13. Test on real x86_64 hardware (if available) (30 min)

**Week 1 Deliverables:**
- ✅ Build system supports `build/${ARCH}/${TARGET}/` convention
- ✅ `make ARCH=amd64 TARGET=kernel` → `build/amd64/kernel/starkernel.efi`
- ✅ `make ARCH=amd64 TARGET=kernel iso` → bootable ISO
- ✅ QEMU boots and prints to serial console
- ✅ Documentation complete for amd64

**Total estimated time Week 1: 13 hours (amd64 only)**

**Week 2 (Optional):** Port to aarch64 and riscv64 (8 hours each = 16 hours total)

---

*This roadmap is a living document. Update status weekly and adjust timeline based on actual progress.*
