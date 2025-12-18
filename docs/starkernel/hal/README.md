# Hardware Abstraction Layer (HAL) Documentation

This directory contains comprehensive documentation for StarForth's Hardware Abstraction Layer (HAL), which enables the evolution from hosted VM to StarKernel to StarshipOS.

---

## Document Organization

Read in this order for full understanding:

### 1. **[overview.md](overview.md)** - Start Here
**What:** High-level HAL architecture, design principles, and rationale
**Who:** Everyone (developers, architects, contributors)
**Why read:** Understand the problem HAL solves and how it fits into StarForth → StarKernel → StarshipOS

**Key sections:**
- The Problem (why HAL is necessary)
- Architecture Overview (3-layer model)
- Design Principles (VM purity, contract-first, testability)
- HAL Subsystems (time, interrupts, memory, console, CPU)
- HAL and Physics Subsystems
- Success Criteria

---

### 2. **[interfaces.md](interfaces.md)** - The Contract
**What:** Detailed HAL interface specifications (function signatures, semantics, guarantees)
**Who:** VM developers, platform implementers
**Why read:** Understand exactly what each HAL function does and requires

**Key sections:**
- Time & Timers interface (`hal_time.h`)
- Interrupts interface (`hal_interrupt.h`)
- Memory interface (`hal_memory.h`)
- Console interface (`hal_console.h`)
- CPU interface (`hal_cpu.h`)
- Panic/Error interface (`hal_panic.h`)
- Interface safety summary (ISR-safe, thread-safe)

**Critical details:**
- Function semantics (what it does, guaranteed behavior)
- Error handling (when it can fail, how to handle)
- Performance expectations (latency, overhead)
- Concurrency model (thread-safe? ISR-safe?)

---

### 3. **[platform-implementations.md](platform-implementations.md)** - How to Implement
**What:** Platform-specific implementation examples and guidance
**Who:** Platform developers adding HAL support for new targets
**Why read:** Learn how to implement HAL for a specific platform

**Key sections:**
- Platform directory structure
- Linux HAL implementation (reference platform)
- Kernel HAL implementation (StarKernel)
- Platform testing strategy
- Common implementation pitfalls

**Includes complete code examples for:**
- Linux (POSIX, hosted)
- Kernel (freestanding, bare metal)

---

### 4. **[migration-plan.md](migration-plan.md)** - Refactoring Guide
**What:** Step-by-step plan for refactoring existing StarForth to use HAL
**Who:** Core developers doing the migration work
**Why read:** Execute the HAL migration safely with zero regressions

**Key sections:**
- 7-phase migration strategy
- Incremental refactoring approach
- Test validation after each phase
- Rollback strategy
- Timeline estimates (2-4 weeks)

**Phases:**
1. Define HAL interfaces (headers only)
2. Implement HAL for Linux
3. Migrate VM core to use HAL
4. Migrate physics subsystems to use HAL
5. Migrate REPL and word implementations
6. Implement HAL for L4Re (optional validation)
7. Validate deterministic behavior

---

### 5. **[starkernel-integration.md](starkernel-integration.md)** - Kernel Details
**What:** StarKernel-specific HAL implementation and boot sequence
**Who:** Kernel developers building StarKernel
**Why read:** Understand UEFI boot, freestanding C, hardware initialization

**Key sections:**
- StarKernel architecture (boot sequence)
- Directory structure (`src/platform/kernel/`)
- Boot sequence detail (UEFI → StarKernel → VM)
- HAL implementation details (time, interrupts, memory, console, CPU)
- Build system (freestanding toolchain, linker script)
- Testing on QEMU/OVMF
- Debugging techniques
- Roadmap to `ok` prompt

**Includes:**
- Complete UEFI loader implementation
- Kernel entry point
- Hardware initialization sequence
- QEMU testing guide
- Debugging strategies

---

## Quick Reference

### For Different Roles

**I'm a VM developer:**
- Read: `overview.md`, `interfaces.md`
- Use HAL functions in VM code
- Never call platform-specific APIs directly

**I'm adding a new platform:**
- Read: `overview.md`, `interfaces.md`, `platform-implementations.md`
- Implement all HAL interfaces for your platform
- Test against VM test suite (936+ tests must pass)

**I'm migrating existing code to HAL:**
- Read: `migration-plan.md`
- Follow 7-phase incremental approach
- Test after each phase

**I'm building StarKernel:**
- Read: All documents, especially `starkernel-integration.md`
- Start with UEFI boot + serial output
- Build incrementally to `ok` prompt

---

## Key Concepts

### HAL Principles

1. **VM Purity:** VM code is platform-agnostic, never knows which platform it's on
2. **Contract-First:** HAL interfaces are contracts with precise semantics
3. **Testability:** Develop/test on Linux, deploy on kernel
4. **Zero Overhead:** HAL inlines to direct hardware access when optimized
5. **Fail-Fast:** Validate platform assumptions at init, not during execution

### Success Criteria

The HAL is successful if:
- ✅ VM core has zero platform-specific code
- ✅ All 936+ tests pass on all platforms
- ✅ 0% algorithmic variance maintained
- ✅ No measurable performance regression
- ✅ StarKernel boots to `ok` prompt
- ✅ Physics subsystems work identically on all platforms

---

## Document Status

| Document | Status | Last Updated |
|----------|--------|--------------|
| overview.md | ✅ Complete | 2025-12-14 |
| interfaces.md | ✅ Complete | 2025-12-14 |
| platform-implementations.md | ✅ Complete | 2025-12-14 |
| migration-plan.md | ✅ Complete | 2025-12-14 |
| starkernel-integration.md | ✅ Complete | 2025-12-14 |

---

## Next Steps

### Immediate (HAL Migration)
1. Review and approve HAL interface specifications
2. Execute migration plan Phase 1 (define interfaces)
3. Execute migration plan Phase 2 (implement Linux HAL)
4. Continue through Phase 7 (validation)

### Near-Term (StarKernel)
1. Implement UEFI boot loader
2. Implement kernel HAL subsystems
3. Boot to `ok` prompt on QEMU
4. Validate physics subsystems on bare metal

### Long-Term (StarshipOS)
1. Add storage drivers (AHCI, NVMe)
2. Add networking stack (TCP/IP)
3. Implement process model (Forth tasks)
4. Build device model (block/net/char)

---

## References

### StarForth Documentation
- `docs/CLAUDE.md` - Project overview and build instructions
- `docs/03-architecture/heartbeat-system/` - Heartbeat subsystem details
- `docs/FEEDBACK_LOOPS.md` - Physics feedback loops
- `README.md` - Project quick start

### External Resources
- [UEFI Specification](https://uefi.org/specifications)
- [GNU-EFI](https://sourceforge.net/projects/gnu-efi/)
- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel Software Developer Manual](https://www.intel.com/sdm)
- [ACPI Specification](https://uefi.org/acpi)

---

## Contributing

When updating HAL documentation:

1. **Keep documents synchronized** - Changes to interfaces require updates to implementation guides
2. **Maintain code examples** - Examples must compile and work
3. **Update this README** - Reflect new sections or document reorganization
4. **Version documentation** - Note "Last Updated" dates
5. **Test instructions** - Verify all commands and code snippets work

---

## Questions?

For questions about HAL architecture or StarKernel implementation, see:
- GitHub Issues: https://github.com/anthropics/starforth/issues (if public)
- Project maintainer: (contact info)
- Documentation feedback: (preferred method)

---

*The HAL is not just a kernel bootstrapping tool—it's the architectural foundation for StarForth → StarKernel → StarshipOS.*