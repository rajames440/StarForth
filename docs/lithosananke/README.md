# LithosAnanke — StarForth Kernel Branch

**Branch:** `lithosananke`
**Status:** M7 Complete (VM Parity Validation)
**Parent:** `master` (hosted StarForth)

---

## What is LithosAnanke?

LithosAnanke is the kernel branch of StarForth — a UEFI-bootable microkernel with the StarForth VM as its native execution engine.

**Etymology:** *Lithos* (stone/foundation) + *Ananke* (necessity/inevitability)

The name reflects the project's nature: a necessary foundation for StarshipOS.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  UEFI Firmware                                              │
│  • ExitBootServices → handoff to kernel                     │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  LithosAnanke Boot (M0)                                     │
│  • UEFI loader collects BootInfo                            │
│  • Memory map, ACPI tables, framebuffer                     │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  LithosAnanke HAL (M1-M6)                                   │
│  • PMM: Physical Memory Manager (bitmap)                    │
│  • VMM: Virtual Memory Manager (4-level paging)             │
│  • IDT: Interrupt Descriptor Table                          │
│  • APIC: Timer + IRQ routing                                │
│  • Heap: kmalloc/kfree                                      │
│  • Console: Serial UART                                     │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  StarForth VM (M7+)                                         │
│  • 295 FORTH-79 words                                       │
│  • Parity validation (hash=0x684bbf2fa1d96d55)              │
│  • Physics-driven adaptive runtime                          │
│  • Init capsule system (M7.1)                               │
└─────────────────────────────────────────────────────────────┘
```

---

## Current Milestones

| Milestone | Description | Status |
|-----------|-------------|--------|
| M0 | UEFI Boot + Serial Output | Complete |
| M1 | Physical Memory Manager | Complete |
| M2 | Virtual Memory Manager | Complete |
| M3 | IDT + Interrupts | Complete |
| M4 | APIC Timer | Complete |
| M5 | Timer Calibration | Complete |
| M6 | Kernel Heap | Complete |
| **M7** | **VM Parity Validation** | **Complete** |
| M7.1 | Init Capsule Architecture | Design Complete |
| M8 | REPL + Interactive Forth | Planned |
| M9 | Block Storage | Planned |

---

## Build & Run

```bash
# Build kernel with VM integration
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1

# Run in QEMU
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1 qemu
```

**Output artifacts:**
- `build/amd64/kernel/starkernel_loader.efi` — UEFI loader
- `build/amd64/kernel/starkernel_kernel.elf` — Kernel binary

---

## Documentation

### Core Design

| Document | Description |
|----------|-------------|
| [M7.1.md](M7.1.md) | Init Capsule Architecture |
| [ROADMAP.md](ROADMAP.md) | Milestone roadmap |

### HAL (Hardware Abstraction Layer)

| Document | Description |
|----------|-------------|
| [hal/overview.md](hal/overview.md) | HAL architecture and principles |
| [hal/interfaces.md](hal/interfaces.md) | HAL interface specifications |
| [hal/lithosananke-integration.md](hal/lithosananke-integration.md) | Kernel-specific HAL implementation |
| [hal/platform-implementations.md](hal/platform-implementations.md) | Platform implementation guide |
| [hal/migration-plan.md](hal/migration-plan.md) | Migration from hosted to kernel |

---

## Key Concepts

### One Truth Per VM

Every VM has exactly one birth capsule — its identity and provenance.

```
PARITY:BIRTH vm_id=42 capsule_id=0xA13F mode=p capsule_hash=0x... dict_hash=0x...
```

### Content-Addressed Capsules

Capsules are identified by content hash, not names. Names lie. Hashes don't.

### Mama FORTH

The root VM that holds all truths, manages child VMs, and enforces the birth protocol.

---

## Directory Structure

```
src/starkernel/
├── boot/               # UEFI loader, ELF loader
├── hal/                # Console, host services
├── memory/             # PMM, VMM, kmalloc
├── arch/amd64/         # x86_64 specific (APIC, IDT, timer)
├── vm/                 # VM integration
│   ├── arena.c         # VM memory arena
│   ├── parity.c        # Parity validation
│   ├── bootstrap/      # sk_vm_bootstrap
│   └── host/           # Host services shim
└── kernel_main.c       # Entry point
```

---

## Contributing

The lithosananke branch is experimental. Changes should:

1. Maintain parity validation (M7 baseline)
2. Pass QEMU boot test
3. Not break hosted StarForth (master branch)

---

## References

- [StarForth README](../../README.md) — Main project
- [FORTH-79 Standard](http://www.forth.org/forth-79.html)
- [OSDev Wiki](https://wiki.osdev.org/)
- [UEFI Specification](https://uefi.org/specifications)
