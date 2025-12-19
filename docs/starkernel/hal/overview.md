# Hardware Abstraction Layer (HAL) Architecture

> **⚠️ Important Note**: This document describes the **target HAL architecture** with standardized `hal_*` function naming. The current codebase uses `sf_time_backend_t` and similar abstractions with different naming conventions. See [migration-plan.md](migration-plan.md) for the refactoring steps to migrate existing platform code to the HAL interfaces described here.

## Executive Summary

The Hardware Abstraction Layer (HAL) is the architectural foundation that enables StarForth to evolve from a hosted VM into StarKernel while preserving the physics-driven adaptive runtime's deterministic behavior across all platforms.

**Critical insight:** The HAL is not an afterthought—it's the linchpin that makes StarKernel → StarshipOS possible without compromising StarForth's experimental integrity.

---

## The Problem

StarForth currently runs on multiple platforms:
- **Linux** (POSIX, hosted)
- **L4Re/Fiasco.OC** (microkernel)
- **Bare metal** (limited, experimental)

Each platform has its own:
- Timing mechanisms (POSIX timers vs. hardware timers)
- Interrupt handling (signals vs. IDT/APIC)
- Memory allocation (malloc vs. physical page frames)
- I/O mechanisms (stdin/stdout vs. UART/framebuffer)

**Without a HAL:** Platform-specific code bleeds into the VM core, physics subsystems, and word implementations. This creates:
- ❌ Fragile #ifdef PLATFORM_X conditionals throughout codebase
- ❌ Platform-specific bugs in supposedly portable code
- ❌ Inability to test kernel code on hosted platforms
- ❌ Risk to deterministic behavior guarantees (0% algorithmic variance)

**With a HAL:** Clean separation between VM logic and platform implementation:
- ✅ VM and physics subsystems are platform-agnostic
- ✅ Platform code is isolated and testable
- ✅ New platforms (StarKernel) can be added without touching VM core
- ✅ Deterministic behavior guaranteed by HAL contract, not platform quirks

---

## Architecture Overview

```
┌────────────────────────────────────────────────────────────┐
│  StarForth VM Core + Physics Subsystems                    │
│  • Interpreter loop (vm.c)                                 │
│  • Execution heat tracking                                 │
│  • Rolling window of truth                                 │
│  • Hot-words cache                                         │
│  • Pipelining metrics                                      │
│  • Inference engine                                        │
│  • Heartbeat coordination                                  │
│                                                             │
│  ↓ Calls HAL interfaces (platform-agnostic)                │
├────────────────────────────────────────────────────────────┤
│  Hardware Abstraction Layer (HAL)                          │
│  • hal_time.h      - Monotonic time, timers, calibration   │
│  • hal_interrupt.h - IRQ management, ISR registration      │
│  • hal_memory.h    - Allocation, page mapping, heap        │
│  • hal_console.h   - Character I/O (serial, framebuffer)   │
│  • hal_cpu.h       - CPU ID, relax/halt, SMP coordination  │
│                                                             │
│  ↓ Platform-specific implementations                       │
├────────────────────────────────────────────────────────────┤
│  Platform Implementations                                  │
│  ┌──────────────┬──────────────┬──────────────────────┐   │
│  │ Linux        │ L4Re         │ Kernel (StarKernel)  │   │
│  │ (POSIX)      │ (microkernel)│ (freestanding)       │   │
│  ├──────────────┼──────────────┼──────────────────────┤   │
│  │ clock_gettime│ L4Re::Clock  │ TSC + HPET + APIC    │   │
│  │ timerfd      │ L4Re::IrqEoi │ IDT + APIC IRQ       │   │
│  │ malloc/free  │ dataspaces   │ PMM + VMM + kmalloc  │   │
│  │ stdin/stdout │ L4Re::Console│ UART + framebuffer   │   │
│  │ pthread      │ L4Re::Thread │ SMP bring-up         │   │
│  └──────────────┴──────────────┴──────────────────────┘   │
└────────────────────────────────────────────────────────────┘
```

---

## Design Principles

### 1. **VM Purity**
The VM core must never know which platform it's running on. All platform awareness lives in HAL implementations.

**Anti-pattern (current):**
```c
#ifdef PLATFORM_LINUX
    clock_gettime(CLOCK_MONOTONIC, &ts);
#elif PLATFORM_L4RE
    l4re_kip_clock(kip);
#elif PLATFORM_KERNEL
    rdtsc();
#endif
```

**Correct pattern (HAL):**
```c
// In VM code (platform-agnostic)
uint64_t now = hal_time_now_ns();

// In platform/linux/hal_time.c
uint64_t hal_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// In platform/kernel/hal_time.c
uint64_t hal_time_now_ns(void) {
    return tsc_to_ns(rdtsc());
}
```

### 2. **Contract-First Design**
HAL interfaces are **contracts**, not convenience wrappers. Each HAL function has:
- **Precise semantics** - What it does, guaranteed across all platforms
- **Error handling** - When it can fail and how
- **Performance expectations** - Allowed latency/overhead
- **Concurrency model** - Thread-safe? ISR-safe?

### 3. **Testability on Hosted Platforms**
StarKernel code must be developable and testable on Linux/L4Re before deploying to bare metal.

**Example:** Heartbeat ISR development
- **Kernel implementation:** APIC timer interrupt → ISR → ring buffer
- **Linux test implementation:** timerfd + signal handler → ISR → ring buffer
- **Same HAL interface, same VM code, different platform layer**

### 4. **Zero Overhead When Possible**
HAL calls should compile to direct hardware access on kernel platforms, not add abstraction tax.

**Good:** `hal_time_now_ns()` inlines to `rdtsc()` with `-O2`
**Bad:** Function pointer indirection adds 5-10 cycles per call

### 5. **Fail-Fast Validation**
HAL implementations validate platform assumptions at init time, not during execution.

**Example:**
```c
void hal_time_init(void) {
    // Calibrate TSC frequency at boot
    tsc_calibrate_hpet();

    // Validate monotonicity
    uint64_t t1 = hal_time_now_ns();
    hal_cpu_relax();
    uint64_t t2 = hal_time_now_ns();

    if (t2 < t1) {
        hal_panic("hal_time: TSC not monotonic!");
    }
}
```

---

## HAL Subsystems

### 1. **Time & Timers** (`hal_time.h`)
**Purpose:** Monotonic time, periodic/oneshot timers, calibration

**Critical for StarForth:** The heartbeat subsystem and physics feedback loops depend on precise, jitter-free timing. HAL must guarantee:
- Monotonic time (never goes backward)
- Sub-microsecond resolution
- Calibrated frequency (for TSC-based timing)

**Platform challenges:**
- **Linux:** `clock_gettime()` is good, but signal-based timers have latency
- **Kernel:** TSC drift, HPET/PIT fallback, per-core calibration

### 2. **Interrupts** (`hal_interrupt.h`)
**Purpose:** Enable/disable IRQs, register ISRs, query interrupt context

**Critical for StarForth:** Heartbeat ISR must run at precise intervals without VM involvement.

**Platform challenges:**
- **Linux:** Signals are "interrupt-like" but not true IRQs
- **Kernel:** IDT setup, APIC configuration, spurious interrupt handling

### 3. **Memory** (`hal_memory.h`)
**Purpose:** Allocate/free memory, page mapping, heap management

**Critical for StarForth:** Dictionary allocation, stack allocation, heap allocator must work identically across platforms.

**Platform challenges:**
- **Linux:** malloc/free are simple
- **Kernel:** Physical memory manager, virtual memory manager, heap allocator—all from scratch

### 4. **Console** (`hal_console.h`)
**Purpose:** Character I/O for REPL and diagnostics

**Critical for StarForth:** The REPL must work on all platforms for interactive experimentation.

**Platform challenges:**
- **Linux:** stdin/stdout are perfect
- **Kernel:** UART 16550 is well-documented but framebuffer is tricky

### 5. **CPU** (`hal_cpu.h`)
**Purpose:** CPU ID, relax/halt, SMP coordination

**Critical for StarForth:** Per-core execution heat tracking (future), SMP scalability (future).

**Platform challenges:**
- **Linux:** pthread local storage
- **Kernel:** Local APIC, per-core stacks, CPU-local storage

---

## HAL and the Physics Subsystems

The physics-driven adaptive runtime is the **primary beneficiary** of the HAL:

| Physics Subsystem | HAL Dependency | Why |
|-------------------|----------------|-----|
| **Execution Heat** | None (pure VM state) | Tracks word execution frequency |
| **Rolling Window** | `hal_time_now_ns()` | Timestamps for window entries |
| **Hot-Words Cache** | None (pure dictionary state) | Frequency-based reordering |
| **Pipelining Metrics** | None (pure transition tracking) | Word-to-word prediction |
| **Inference Engine** | `hal_time_now_ns()` | Calibration timing, ANOVA |
| **Heartbeat** | `hal_timer_periodic()`, `hal_interrupt.*` | ISR-based sampling |

**Key insight:** Only 2 of 6 subsystems touch the HAL directly, and only via clean interfaces. This preserves deterministic behavior while enabling kernel deployment.

---

## HAL and StarKernel

StarKernel is a **new platform implementation** of the HAL:

```
src/platform/kernel/
├── boot/
│   └── uefi_loader.c       # UEFI entry point → BootInfo handoff
├── hal_time.c              # TSC + HPET + APIC timer
├── hal_interrupt.c         # IDT + Local APIC + IOAPIC
├── hal_memory.c            # PMM + VMM + kmalloc
├── hal_console.c           # UART 16550 + framebuffer
├── hal_cpu.c               # SMP + CPU-local storage
└── drivers/                # PCI, AHCI, NVMe, VirtIO, etc.
```

**StarKernel does NOT modify:**
- VM core (`src/vm.c`)
- Physics subsystems (`src/dictionary_heat_optimization.c`, etc.)
- Word implementations (`src/word_source/*.c`)

**StarKernel ONLY implements:**
- HAL interfaces (`hal_*.c`)
- UEFI boot loader (`boot/uefi_loader.c`)
- Device drivers (`drivers/*.c`)

This is the **proof** that the HAL abstraction works: StarKernel is a pure platform layer, not a VM fork.

---

## HAL and StarshipOS

StarshipOS builds on StarKernel by adding:
- **Process model** - Forth tasks vs. traditional processes
- **Filesystem** - FAT32, ext2, or log-structured
- **Networking** - TCP/IP stack, DHCP, DNS
- **Device model** - Unified block/net/char device interfaces
- **Security model** - Capabilities, ACL, Forth-based access control

All of these **still use the HAL** for low-level access:
- Filesystem → `hal_memory` for caching, `hal_interrupt` for async I/O
- Networking → `hal_interrupt` for packet RX, `hal_time` for timeouts
- Device model → HAL as the common substrate

**The HAL is not just a kernel bootstrapping tool—it's the foundation for the entire OS.**

---

## Migration Strategy

Current StarForth codebase must be refactored to introduce the HAL:

### Phase 1: Define HAL Interfaces
- Write `include/hal/*.h` headers with contracts
- Document semantics, error handling, performance expectations
- **No implementation yet**

### Phase 2: Refactor Existing Platforms
- Create `src/platform/linux/hal_*.c` implementing HAL
- Create `src/platform/l4re/hal_*.c` implementing HAL
- Update VM code to call HAL instead of platform-specific APIs
- **VM must still build and pass all 936+ tests**

### Phase 3: Validate HAL
- Build and test on Linux
- Build and test on L4Re (if available)
- Verify deterministic behavior (0% algorithmic variance) still holds
- Benchmark: HAL must not add measurable overhead

### Phase 4: Implement StarKernel Platform
- Create `src/platform/kernel/` with HAL implementations
- UEFI boot loader
- Basic MM, console, timing
- **Goal:** Boot to `ok` prompt on QEMU/OVMF

### Phase 5: Full StarKernel
- Complete HAL implementations (interrupts, SMP, drivers)
- Heartbeat ISR running at kernel level
- Physics subsystems operational
- **Goal:** Reproduce DoE results on bare metal

### Phase 6: StarshipOS
- Process model, filesystem, networking
- Forth as native control plane
- **Goal:** Self-hosting OS

---

## Success Criteria

The HAL is successful if:

1. ✅ **VM core has zero platform-specific code**
2. ✅ **All 936+ tests pass on Linux, L4Re, and StarKernel**
3. ✅ **0% algorithmic variance maintained across platforms**
4. ✅ **No measurable performance regression from HAL abstraction**
5. ✅ **StarKernel boots to `ok` prompt and runs REPL**
6. ✅ **Heartbeat subsystem works identically on all platforms**
7. ✅ **New platforms can be added without touching VM code**

---

## Next Steps

See companion documentation:
- `interfaces.md` - Detailed HAL interface specifications
- `platform-implementations.md` - How to implement HAL for new platforms
- `migration-plan.md` - Step-by-step refactoring guide
- `starkernel-integration.md` - StarKernel-specific implementation details

---

## References

- StarForth VM core: `src/vm.c`
- Heartbeat system: `docs/03-architecture/heartbeat-system/`
- Physics feedback loops: `docs/FEEDBACK_LOOPS.md`
- Platform abstraction (current): `src/platform/`