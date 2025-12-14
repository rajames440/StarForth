# StarForth → StarshipOS Hybrid Kernel Transformation Plan

## Vision

Transform StarForth from a FORTH-79 virtual machine into a UEFI-bootable microkernel for StarshipOS. **StarForth.efi IS the kernel** - there is no separate kernel image. The FORTH VM runs in ring 0 as the microkernel.

**Architecture**: StarForth FORTH VM is a ring-0 resident microkernel. FORTH words implement kernel services (memory, scheduling, interrupts). C code provides low-level hardware access when needed, but FORTH is the primary kernel interface.

**Key insight**: The VM interpreter itself IS the microkernel. Kernel services are FORTH words, not a separate C layer.

**Deployment**: Both QEMU/virtual machines and real hardware (x86_64, ARM64, RISC-V64).

**Philosophy**: Pure FORTH environment - no ELF/POSIX compatibility. All system services exposed as FORTH words.

## Key Requirements (from Captain Bob)

- **UEFI native**: StarForth.efi boots via UEFI, calls ExitBootServices(), becomes the kernel
- **StarForth IS the kernel**: No separate kernel image - the FORTH VM runs in ring 0 as the microkernel
- **Ring 0 FORTH**: VM interpreter executes in ring 0, FORTH words are kernel services
- **Strict ANSI C99**: No GNU extensions
- **Architecture priority**: x86_64 → ARM64 → RISC-V64
- **Replaces L4Re**: This is the new StarshipOS kernel foundation
- **Pure FORTH**: No ELF loader, no POSIX compatibility, FORTH processes only
- **Target deployment**: Both virtual hardware (QEMU) and real hardware equally
- **Scope**: Full vision including multi-arch, networking (Phase 6), process management

## Current StarForth Architecture (Key Insights)

### Initialization Flow
1. `main()` → platform init → block I/O → VM initialization (5MB memory, stacks, mutexes)
2. FORTH-79 word registration (23 modules, ~600 words)
3. Physics subsystems: hot-words cache, rolling window, heartbeat, pipelining, SSM L8
4. System bootstrap: `vm_interpret("INIT")` loads init.4th
5. Test suite (936+ tests) → REPL

### Memory Model
- `vaddr_t` abstraction: VM addresses are byte offsets, not C pointers (excellent for kernel!)
- 5MB fixed address space: Dictionary (2MB) | User blocks (1MB) | Logs (2MB)
- Stacks: C arrays in VM struct (not in VM memory)
- Memory accessors: `vm_load_cell()`, `vm_store_cell()` provide clean abstraction

### Current Kernel Blockers
- VM memory via `malloc()` → needs kernel allocator
- Dictionary headers via `malloc()` → outside VM memory
- Physics subsystems via `malloc()` → fragmented allocation
- Assumes contiguous C pointer space → needs page-aware access

### Platform Abstraction (Already Kernel-Ready!)
- Atomic spinlocks already implemented (L4Re/minimal mode)
- RAM block I/O backend requires no syscalls
- Assembly optimizations all unprivileged (no syscalls, hardware I/O)
- No dynamic allocation in VM core (fixed stacks, bump allocator)

---

# Implementation Phases

## Phase 1: UEFI Boot Stub (Minimal Kernel Bootstrap)

**Goal**: Boot StarForth as a UEFI application, obtain hardware resources, transition to kernel mode.

**Duration**: 2 weeks

### 1.1 UEFI Platform Layer

**New directory structure:**
```
src/platform/uefi/
├── uefi_boot.c          # UEFI entry point (efi_main)
├── uefi_console.c       # GOP framebuffer + serial console
├── uefi_memory.c        # UEFI memory map acquisition
├── uefi_time.c          # UEFI time services backend
└── uefi_exit.c          # ExitBootServices transition
```

**UEFI Boot Flow:**
```
efi_main(ImageHandle, SystemTable)
  ├─ Initialize UEFI console (GOP + serial)
  ├─ Get memory map (EFI_MEMORY_DESCRIPTOR array)
  ├─ Get framebuffer info (Graphics Output Protocol)
  ├─ Locate ACPI tables (for APIC/HPET)
  ├─ Call ExitBootServices()
  └─ Jump to kernel_main()
```

**Files to Create:**
- `src/platform/uefi/uefi_boot.c` - UEFI entry point
- `include/uefi_platform.h` - UEFI platform abstractions
- `src/platform/uefi/uefi_memory.c` - Memory map handling
- `src/platform/uefi/uefi_console.c` - Early console (GOP + COM1)

**Files to Modify:**
- `src/main.c` - Add conditional compilation:
  ```c
  #ifdef UEFI_BUILD
  EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
  #else
  int main(int argc, char* argv[]);
  #endif
  ```

### 1.2 Build System for UEFI

**New Makefile targets:**
```make
TARGET_UEFI = uefi
UEFI_OUTPUT = StarForth.efi

# UEFI-specific flags
UEFI_CFLAGS = -ffreestanding -fno-stack-protector -fno-builtin \
              -mno-red-zone -DUEFI_BUILD=1 -DKERNEL_BUILD=1

# Use gnu-efi headers
UEFI_INCLUDES = -I/usr/include/efi -I/usr/include/efi/x86_64

# Link as UEFI PE/COFF
UEFI_LDFLAGS = -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main

uefi: $(UEFI_OUTPUT)
```

**New linker script**: `build/uefi.ld` (PE/COFF format for UEFI)

**Files to Create:**
- `build/uefi.ld` - UEFI linker script
- Update `Makefile` with UEFI targets

**Milestone**: StarForth.efi boots in QEMU with OVMF, prints "Hello from StarForth kernel" via serial

---

## Phase 2: Kernel Core Services (C Implementation)

**Goal**: Implement essential kernel primitives (memory, interrupts, scheduling) in C.

**Duration**: 4 weeks

### 2.1 Physical Memory Allocator

**New files:**
- `src/kernel/pmm.c` - Physical memory manager (bitmap allocator)
- `include/kernel/pmm.h`

**Design:**
- Bitmap allocator (1 bit per 4KB page)
- Bootstrap from UEFI memory map
- 4KB page granularity

```c
typedef struct {
    uint64_t* bitmap;      // 1 bit per 4KB page
    uint64_t total_pages;
    uint64_t free_pages;
} PhysicalMemoryManager;

uint64_t pmm_alloc_page(void);           // Returns physical address
void pmm_free_page(uint64_t phys_addr);
void pmm_init(EFI_MemRegion* regions, size_t count);
```

### 2.2 Virtual Memory (Paging)

**New files:**
- `src/kernel/vmm.c` - Virtual memory manager
- `include/kernel/vmm.h`

**x86_64 paging**: 4-level page tables (PML4 → PDPT → PD → PT)

**Kernel memory layout:**
```
Virtual Address Space (x86_64):
0xFFFFFFFF80000000 - 0xFFFFFFFF80200000  Kernel code/data (2MB)
0xFFFFFFFF80200000 - 0xFFFFFFFF90000000  Kernel heap (256MB)
0xFFFFFFFF90000000 - 0xFFFFFFFF95000000  StarForth VM memory (5MB per VM)
0xFFFFFFFF95000000 - 0xFFFFFFFFA0000000  Physics subsystems
0xFFFFFFFFA0000000 - 0xFFFFFFFFFFFFFFFF  MMIO mappings
```

**Functions:**
```c
void vmm_init(void);
void* vmm_map_page(uint64_t phys, uint64_t virt, uint32_t flags);
void vmm_unmap_page(uint64_t virt);
void page_fault_handler(void);  // IDT #14
```

**Preserves `vaddr_t` abstraction**: Already 64-bit in `include/vm.h:95`

### 2.3 Interrupt Descriptor Table (IDT)

**New files:**
- `src/kernel/idt.c` - IDT setup
- `src/kernel/isr.S` - Interrupt stubs (x86_64 assembly)
- `include/kernel/interrupts.h`

**IDT structure (256 entries):**
- 0-31: CPU exceptions (divide-by-zero, page fault, etc.)
- 32-47: IRQs (timer, keyboard, etc.)
- 48-255: Software interrupts

**Critical handlers:**
- **IRQ 0 (Timer)**: Scheduler tick, heartbeat coordination
- **IRQ 1 (Keyboard)**: Input handling
- **Exception #14 (Page Fault)**: Demand paging

**Heartbeat integration**: Timer IRQ triggers `vm_tick()` in background (replaces pthread)

### 2.4 GDT/TSS Setup

**New files:**
- `src/kernel/gdt.c`
- `include/kernel/gdt.h`

**GDT layout:**
```
0: Null descriptor
1: Kernel code (ring 0, 64-bit)
2: Kernel data (ring 0)
3: User code (ring 3, 64-bit)
4: User data (ring 3)
5: TSS descriptor
```

**TSS**: Required for privilege level switches, interrupt stacks

### 2.5 Timer (APIC/HPET)

**New files:**
- `src/kernel/timer.c`
- `include/kernel/timer.h`

**Timer options** (priority order):
1. Local APIC timer (per-CPU)
2. HPET (High Precision Event Timer)
3. PIT (8254 legacy fallback)

**Integration**: Implements `sf_time_backend_t` from `include/platform_time.h`

### 2.6 Basic Scheduler

**New files:**
- `src/kernel/sched.c`
- `include/kernel/sched.h`

**Task structure:**
```c
typedef struct Task {
    uint64_t pid;
    uint64_t rsp;       // Stack pointer
    uint64_t rip;       // Instruction pointer
    uint64_t cr3;       // Page table base
    uint32_t state;     // RUNNING, READY, BLOCKED
    VM* vm_ctx;         // StarForth VM context (FORTH task)
    struct Task* next;
} Task;
```

**Algorithm**: Round-robin, 1ms quantum

**FORTH VM as kernel task**: Initial boot creates single FORTH VM task (PID 1)

**Milestone**: Kernel manages memory, handles timer IRQs, schedules tasks

---

## Phase 3: Kernel Services as FORTH Words

**Goal**: Implement kernel services as FORTH words. The VM already runs in ring 0 - now make it a full microkernel.

**Duration**: 4 weeks

### 3.1 Kernel Memory Allocator

**New files:**
- `src/kernel/kmalloc.c` - Kernel heap allocator
- `include/kernel/kmalloc.h`

**Design**: Simple bump allocator or slab allocator backed by `pmm_alloc_page()`

**Replace malloc everywhere:**
```c
#ifdef KERNEL_BUILD
    #define malloc(n) kmalloc(n)
    #define free(p) kfree(p)
#endif
```

**Files to modify** (all files with malloc/free calls):
- `src/vm.c:176` - VM memory allocation
- `src/vm.c:255-336` - Physics subsystems
- All word modules that use malloc

### 3.2 Memory Model Transformation

**Current issue**: `vm->memory = malloc(VM_MEMORY_SIZE)` in `src/vm.c:176`

**Solution**:
```c
#ifdef KERNEL_BUILD
    vm->memory = (uint8_t*)kmalloc(VM_MEMORY_SIZE);
#else
    vm->memory = (uint8_t*)malloc(VM_MEMORY_SIZE);
#endif
```

**Dictionary headers**: Keep outside VM memory initially (kmalloc'd), optimize later

**Stacks**: Keep as C arrays in VM struct (simplest approach)

### 3.3 Kernel Services as FORTH Words

**New word module:**
- `src/word_source/kernel_words.c`

**Exposed primitives:**
```forth
PMEM-ALLOC  ( n -- addr )         \ Allocate n pages of physical memory
PMEM-FREE   ( addr -- )           \ Free physical page
VMAP        ( phys virt flags -- )\ Map virtual → physical
VUNMAP      ( virt -- )           \ Unmap virtual page
IRQ-ENABLE  ( n -- )              \ Enable IRQ n
IRQ-DISABLE ( n -- )              \ Disable IRQ n
TASK-CREATE ( xt stack -- pid )   \ Create new FORTH task
TASK-YIELD  ( -- )                \ Yield CPU
TASK-KILL   ( pid -- )            \ Terminate task
IOPERM      ( port len -- )       \ Grant I/O port access
```

**Implementation pattern:**
```c
static void word_PMEM_ALLOC(VM* vm) {
    cell_t npages = vm_pop(vm);
    uint64_t phys = pmm_alloc_pages(npages);
    vm_push(vm, CELL(phys));
}

void register_kernel_words(VM* vm) {
    register_word(vm, "PMEM-ALLOC", word_PMEM_ALLOC);
    register_word(vm, "VMAP", word_VMAP);
    // ... etc
}
```

**Files to modify:**
- `src/word_registry.c:98` - Add `register_kernel_words(vm);`

### 3.4 Physics Runtime Preservation

**Critical subsystems to preserve:**
- Execution heat tracking
- Rolling window of truth
- Hot-words cache
- Pipelining metrics
- Heartbeat system
- SSM L8 Jacquard mode selector

**Kernel integration:**
1. **Heartbeat via timer IRQ**: Replace pthread (`src/vm.c:40-44`) with timer IRQ → `vm_tick()`
2. **Memory allocation**: Physics subsystems use kmalloc (no functional changes)
3. **Mutexes**: Use atomic spinlocks (already implemented for L4Re)

**No changes needed**: All 7 feedback loops work as-is

**Milestone**: FORTH REPL runs in kernel mode, can allocate memory via FORTH words, all 936+ tests pass

---

## Phase 4: Platform Completion (I/O Services)

**Goal**: Kernel has functional console, block I/O, time services.

**Duration**: 2 weeks

### 4.1 Kernel Console (Framebuffer + Serial)

**New files:**
- `src/platform/kernel/kernel_console.c`
- `include/kernel/console.h`

**Pre-ExitBootServices**: UEFI Simple Text Output Protocol

**Post-ExitBootServices**:
- GOP framebuffer (graphical text console)
- Serial port (COM1, 0x3F8)

**Framebuffer rendering**:
- Store GOP framebuffer info before ExitBootServices
- Simple text renderer (PSF font or bitmap font)
- Scroll buffer

**Files to modify:**
- `src/io.c` - Add `#ifdef KERNEL_BUILD` conditional for `sf_putchar()`

### 4.2 Block I/O Backend

**Target both QEMU and real hardware:**

**Priority 1: RAM disk** (already exists in `src/blkio_ram.c`)
- No changes needed
- Usable immediately

**Priority 2: Virtio-blk** (QEMU, cloud VMs)
- `src/platform/kernel/blkio_virtio.c`
- Implement `blkio_vtable_t` for virtio-blk

**Priority 3: AHCI** (SATA drives, real hardware)
- `src/platform/kernel/blkio_ahci.c`
- AHCI controller driver

**Priority 4: NVMe** (modern SSDs)
- `src/platform/kernel/blkio_nvme.c`
- NVMe driver

**Integration**: `src/main.c:179` initializes blkio → call from `kernel_main()` after device enumeration

### 4.3 Time Services

**New files:**
- `src/platform/kernel/kernel_time.c`

**Implementation**: APIC timer or HPET backend implementing `sf_time_backend_t`

**Files to modify:**
- `src/platform/platform_init.c:47` - Add kernel backend selection

**Milestone**: Kernel has functional I/O (console, blocks, timers), can save/load FORTH blocks

---

## Phase 5: Multi-Architecture Support

**Goal**: Extend kernel to ARM64 and RISC-V64.

**Duration**: 4 weeks

### 5.1 Architecture Abstraction

**New directory structure:**
```
src/kernel/arch/
├── x86_64/
│   ├── boot.S       # Early boot assembly
│   ├── gdt.c        # GDT/TSS setup
│   ├── idt.c        # IDT/interrupts
│   ├── paging.c     # Page table management
│   └── switch.S     # Context switching
├── arm64/
│   ├── boot.S       # Early boot assembly
│   ├── mmu.c        # ARM64 MMU setup
│   ├── gic.c        # GIC (Generic Interrupt Controller)
│   ├── timer.c      # ARM Generic Timer
│   └── switch.S     # Context switching
└── riscv64/
    ├── boot.S       # Early boot assembly
    ├── mmu.c        # RISC-V SV39 paging
    ├── plic.c       # PLIC (interrupt controller)
    ├── timer.c      # RISC-V timer
    └── switch.S     # Context switching
```

### 5.2 x86_64 Kernel Flow

```
efi_main() → ExitBootServices() → kernel_main()
  ├─ gdt_init()           # Setup GDT/TSS
  ├─ idt_init()           # Setup IDT
  ├─ pmm_init()           # Physical memory
  ├─ vmm_init()           # Virtual memory
  ├─ timer_init()         # APIC timer
  ├─ sched_init()         # Scheduler
  ├─ vm_init()            # StarForth VM
  ├─ register_forth79_words()
  ├─ register_kernel_words()
  └─ vm_repl()            # Enter FORTH REPL
```

### 5.3 ARM64 Differences

**UEFI**: ARM64 UEFI spec (AARCH64)

**Architecture-specific**:
- MMU: 4-level page tables (similar to x86_64)
- Interrupts: GIC (Generic Interrupt Controller) vs. APIC
- Timer: ARM Generic Timer vs. APIC timer
- No GDT/TSS (ARM64 uses system registers)

**New files**:
- `src/kernel/arch/arm64/gic.c` - GIC setup
- `src/kernel/arch/arm64/timer.c` - ARM timer
- Assembly already exists: `include/vm_asm_opt_arm64.h` (~1200 lines, unprivileged)

### 5.4 RISC-V64 Considerations

**Boot**: UEFI (limited hardware) or OpenSBI (Supervisor Binary Interface)

**Architecture-specific**:
- MMU: SV39 (3-level page tables)
- Interrupts: PLIC (Platform-Level Interrupt Controller)
- Timer: RISC-V machine timer (mtime/mtimecmp)
- Privileged modes: M-mode (machine), S-mode (supervisor), U-mode (user)

**New files**:
- `src/kernel/arch/riscv64/plic.c`
- `src/kernel/arch/riscv64/sbi.c` - OpenSBI interface

**Milestone**: Kernel boots on x86_64, ARM64, RISC-V64; all tests pass on all architectures

---

## Phase 6: Advanced Kernel Features

**Goal**: Production-ready kernel capabilities.

**Duration**: 8 weeks

### 6.1 FORTH Process Model

**Each FORTH process**:
- Own VM instance
- Separate dictionary, stacks, memory
- Copy-on-write for shared code

**Process creation**:
```forth
: NEW-TASK ( xt -- pid )
    TASK-CREATE   \ Create task structure
    VM-CLONE      \ Clone current VM
    TASK-START    \ Begin execution
;
```

### 6.2 IPC (Inter-Process Communication)

**New word module**: `src/word_source/ipc_words.c`

**FORTH-native IPC**:
```forth
CHAN-CREATE ( size -- chan )      \ Create message channel
CHAN-SEND   ( chan msg len -- )   \ Send message
CHAN-RECV   ( chan buf max -- n ) \ Receive (blocking)
CHAN-CLOSE  ( chan -- )           \ Close channel
```

**Implementation**: Message passing via kernel buffers

### 6.3 Device Drivers

**Real hardware drivers** (for real hardware deployment):

**Priority order**:
1. **Serial (UART)**: Already needed for console
2. **Disk (AHCI/NVMe)**: For real storage
3. **Network (E1000/virtio-net)**: For networking (Phase 6)
4. **USB**: For keyboards, storage, etc.
5. **Graphics (simple framebuffer)**: Already have from UEFI GOP

**Driver model**: C drivers + FORTH extensibility

**Example**: Expose driver APIs as FORTH words
```forth
\ FORTH-based LED driver
: LED-ON  0x60 1 IO-OUT ;  \ Write to GPIO
: LED-OFF 0x60 0 IO-OUT ;
```

### 6.4 Networking Stack (Phase 6)

**Stack**: lwIP (lightweight IP stack)
- Minimal, embedded-friendly
- TCP/IP, UDP, ICMP
- Integrate as kernel module

**Drivers**:
- virtio-net (QEMU)
- E1000 (Intel Ethernet, common in VMs)

**FORTH integration**:
```forth
NET-SOCKET  ( domain type -- fd )
NET-BIND    ( fd addr port -- )
NET-LISTEN  ( fd backlog -- )
NET-ACCEPT  ( fd -- newfd )
NET-SEND    ( fd buf len -- n )
NET-RECV    ( fd buf max -- n )
```

### 6.5 File System

**Phase 1**: Block-based (current)
- Already implemented: `src/block_subsystem.c`
- 1KB blocks, simple addressing

**Phase 2**: VFS + ext2
- Virtual File System layer
- ext2 (simple, well-documented)
- Mount from virtio-blk or AHCI

**FORTH integration**:
```forth
FS-MOUNT  ( device path -- )
FS-OPEN   ( path flags -- fd )
FS-READ   ( fd buf len -- n )
FS-WRITE  ( fd buf len -- n )
FS-CLOSE  ( fd -- )
```

### 6.6 Userspace Model

**Pure FORTH environment** (no ELF loader):
- All processes are FORTH VMs
- Native FORTH execution
- System services via FORTH words

**Init process**: StarForth REPL (PID 1)
- Interactive kernel exploration
- Spawn tasks from FORTH
- All administration via FORTH

**Milestone**: Production-ready hybrid kernel with multi-process FORTH, IPC, networking, filesystems

---

## Build System Architecture

### Makefile Targets

```make
# Standard builds (unchanged)
make              # Optimized build for Linux
make fastest      # Maximum performance
make test         # Run test suite

# UEFI kernel builds (new)
make kernel-x86_64   # x86_64 UEFI kernel
make kernel-arm64    # ARM64 UEFI kernel
make kernel-riscv64  # RISC-V64 UEFI kernel

# Testing
make kernel-test     # Run tests in kernel mode (QEMU)
```

### Compiler Flags

```make
# Kernel build flags
KERNEL_CFLAGS = -ffreestanding -fno-stack-protector -fno-builtin \
                -mno-red-zone -DKERNEL_BUILD=1 -DUEFI_BUILD=1 \
                -Wall -Werror

# Architecture-specific flags
KERNEL_CFLAGS_X86_64 = -march=x86-64 -mcmodel=kernel
KERNEL_CFLAGS_ARM64 = -march=armv8-a -mcmodel=large
KERNEL_CFLAGS_RISCV64 = -march=rv64gc -mcmodel=medany
```

### Linker Scripts

**File**: `build/kernel.ld`
```ld
ENTRY(efi_main)

SECTIONS {
    . = 0xFFFFFFFF80100000;  /* Kernel base */

    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
}
```

### Conditional Compilation

**All source files use**:
```c
#ifdef KERNEL_BUILD
    // Kernel-specific code
    #include "kernel/kmalloc.h"
    void* ptr = kmalloc(size);
#else
    // Userspace code
    #include <stdlib.h>
    void* ptr = malloc(size);
#endif
```

---

## Testing Strategy

### Phase 1: UEFI Boot
- **Environment**: QEMU with OVMF UEFI firmware
- **Test**: Boot StarForth.efi, print via serial
- **Verify**: Memory map parsed, framebuffer acquired
- **Command**: `qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -hda fat:rw:esp`

### Phase 2: Kernel Primitives
- **Test**: Timer IRQ fires, PMM allocates pages
- **Verify**: Timer interrupt handler increments tick count
- **Debug**: QEMU GDB remote debugging (`-s -S`)

### Phase 3: FORTH Integration
- **Test**: Run existing test suite in kernel mode
- **Verify**: All 936+ tests pass
- **Tools**: FORTH test runner (`src/test_runner/`)

### Phase 4: Platform Completion
- **Test**: Block I/O read/write, console rendering
- **Verify**: Save/load FORTH blocks, framebuffer text
- **Tools**: virtio-blk disk image

### Phase 5: Multi-Architecture
- **Test**: Boot on QEMU ARM64 (`qemu-system-aarch64 -machine virt`)
- **Test**: Boot on QEMU RISC-V64 (`qemu-system-riscv64 -machine virt`)
- **Verify**: Same test suite passes on all architectures

### Phase 6: Integration Testing
- **Test**: Multi-process FORTH, IPC, networking
- **Verify**: Processes communicate, TCP/IP stack works
- **Tools**: Network testing tools, stress tests

---

## Critical Files Summary

### New Directories
```
src/kernel/              # Kernel core services
src/kernel/arch/         # Architecture-specific code
src/platform/uefi/       # UEFI platform layer
src/platform/kernel/     # Kernel platform backends
```

### Files to Create (Phase 1-3)
```
src/platform/uefi/uefi_boot.c
src/platform/uefi/uefi_console.c
src/platform/uefi/uefi_memory.c
src/kernel/pmm.c
src/kernel/vmm.c
src/kernel/idt.c
src/kernel/gdt.c
src/kernel/timer.c
src/kernel/sched.c
src/kernel/kmalloc.c
src/word_source/kernel_words.c
build/uefi.ld
```

### Files to Modify (Key Changes)
```
src/main.c               # Add efi_main() entry point
src/vm.c                 # Replace malloc with kmalloc
src/word_registry.c      # Register kernel_words module
src/io.c                 # Conditional kernel console
Makefile                 # Add UEFI build targets
```

---

## Design Decisions & Rationale

### 1. StarForth IS the Kernel
**Decision**: No separate kernel layer - StarForth.efi runs in ring 0 as the microkernel
**Rationale**: Captain Bob's architecture - the VM interpreter itself is the microkernel. FORTH words are kernel primitives.

### 2. Pure FORTH Environment (No ELF)
**Decision**: No ELF loader, no POSIX compatibility
**Rationale**: Captain Bob specified pure FORTH. Simplifies kernel, stays true to FORTH philosophy.

### 3. Kernel Memory Layout
**Decision**: VM memory at fixed kernel virtual address (0xFFFFFFFF90000000)
**Rationale**: Preserves existing 5MB contiguous memory model, clean separation from kernel heap.

### 4. System Call Interface
**Decision**: FORTH words ARE the system call interface
**Rationale**: VM runs in ring 0, FORTH words directly manipulate kernel state. No syscall instruction needed - just execute FORTH words.

### 5. Driver Model
**Decision**: C drivers + FORTH extensibility
**Rationale**: Performance-critical drivers in C (virtio, AHCI), simple devices can use FORTH via `IO-IN`/`IO-OUT` words.

### 6. Dictionary Allocation
**Decision**: Keep dictionary headers outside VM memory (kmalloc'd)
**Rationale**: Simpler initial implementation, can optimize later.

### 7. Heartbeat Integration
**Decision**: Timer IRQ triggers VM heartbeat (replaces pthread)
**Rationale**: Natural fit - timer already exists for scheduler, preserves all 7 feedback loops.

---

## Risks & Mitigation

### Risk 1: UEFI Complexity
**Challenge**: UEFI spec is large, error-prone
**Mitigation**: Use gnu-efi library, reference TianoCore EDK2

### Risk 2: Memory Management Bugs
**Challenge**: Virtual memory, paging, page faults are error-prone
**Mitigation**: Start with identity mapping, incremental complexity, extensive testing

### Risk 3: Interrupt Handling
**Challenge**: IDT/GDT setup, interrupt routing, race conditions
**Mitigation**: Reference existing kernels (Linux, xv6), careful locking

### Risk 4: Physics Runtime Preservation
**Challenge**: Adaptive runtime depends on timing, allocations
**Mitigation**: Preserve existing heartbeat logic, use kernel timer IRQ

### Risk 5: Multi-Architecture Porting
**Challenge**: ARM64/RISC-V64 have different boot, MMU, interrupt models
**Mitigation**: Abstract architecture-specific code early, test on QEMU

### Risk 6: Real Hardware Debugging
**Challenge**: No printf, debugger in early boot
**Mitigation**: Serial console (COM1), QEMU GDB stub for development

---

## Timeline Summary

| Phase | Duration | Milestone |
|-------|----------|-----------|
| Phase 1: UEFI Boot | 2 weeks | StarForth.efi boots, prints via serial |
| Phase 2: Kernel Primitives | 4 weeks | Memory management, interrupts, scheduler working |
| Phase 3: FORTH Integration | 4 weeks | FORTH REPL in kernel mode, tests pass |
| Phase 4: Platform Completion | 2 weeks | Console, block I/O, timers functional |
| Phase 5: Multi-Architecture | 4 weeks | Boots on x86_64, ARM64, RISC-V64 |
| Phase 6: Advanced Features | 8 weeks | Multi-process, IPC, networking, filesystems |
| **Total** | **24 weeks** | Production-ready hybrid kernel |

---

## Success Criteria

**Phase 1 Success**: StarForth.efi boots in QEMU, displays UEFI console output, acquires memory map

**Phase 2 Success**: Kernel handles timer interrupts, allocates/frees memory, schedules dummy tasks

**Phase 3 Success**: FORTH REPL runs in kernel mode, all 936+ tests pass, kernel words work

**Phase 4 Success**: Can type in framebuffer console, save/load FORTH blocks to disk

**Phase 5 Success**: Same kernel boots on x86_64 QEMU, ARM64 QEMU, RISC-V64 QEMU

**Phase 6 Success**: Multiple FORTH processes communicate via IPC, TCP/IP stack sends packets

**Final Success**: StarshipOS boots on real hardware (x86_64 PC or Raspberry Pi), runs FORTH REPL, manages processes, saves blocks to disk, connects to network

---

## Next Steps

1. **Setup development environment**:
   - Install QEMU, OVMF UEFI firmware
   - Install gnu-efi or EDK2
   - Setup serial console logging

2. **Create basic directory structure**:
   - `src/platform/uefi/`
   - `src/kernel/`
   - `include/kernel/`

3. **Implement Phase 1**:
   - UEFI entry point (`efi_main`)
   - Memory map acquisition
   - Early console (serial)
   - ExitBootServices

4. **Iterate through phases**:
   - Complete each phase fully before moving to next
   - Test thoroughly at each milestone
   - Maintain compatibility with existing Linux builds

This is an ambitious, technically challenging project that will result in a truly unique operating system: A FORTH-based hybrid kernel with physics-driven adaptive runtime, running bare-metal on multiple architectures.
