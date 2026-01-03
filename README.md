# LithosAnanke

```
   ██╗     ██╗████████╗██╗  ██╗ ██████╗ ███████╗
   ██║     ██║╚══██╔══╝██║  ██║██╔═══██╗██╔════╝
   ██║     ██║   ██║   ███████║██║   ██║███████╗
   ██║     ██║   ██║   ██╔══██║██║   ██║╚════██║
   ███████╗██║   ██║   ██║  ██║╚██████╔╝███████║
   ╚══════╝╚═╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
    █████╗ ███╗   ██╗ █████╗ ███╗   ██╗██╗  ██╗███████╗
   ██╔══██╗████╗  ██║██╔══██╗████╗  ██║██║ ██╔╝██╔════╝
   ███████║██╔██╗ ██║███████║██╔██╗ ██║█████╔╝ █████╗
   ██╔══██║██║╚██╗██║██╔══██║██║╚██╗██║██╔═██╗ ██╔══╝
   ██║  ██║██║ ╚████║██║  ██║██║ ╚████║██║  ██╗███████╗
   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝
```

<p align="center">
  <strong>UEFI-Bootable FORTH Microkernel</strong><br>
  The Foundation for StarshipOS
</p>

<p align="center">
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-roadmap">Roadmap</a> •
  <a href="docs/lithosananke/">Documentation</a>
</p>

---

## Status

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
| M7.1 | Capsule System | In Progress |
| M8 | REPL + Interactive Forth | Planned |

### POST Results (Power-On Self Test)

```
FINAL TEST SUMMARY:
  Total tests: 800
  Passed: 717
  Failed: 32
  Skipped: 49
  Errors: 0

PARITY:M7.1a word_count=295 here=0x30 latest_id=294 hash=0x684bbf2fa1d96d55
PARITY:OK
POST: PASSED
```

---

## Quick Start

```bash
# Build kernel with VM integration
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1

# Run in QEMU
make -f Makefile.starkernel qemu

# Clean build
make -f Makefile.starkernel clean-kernel
```

**Output artifacts:**
- `build/amd64/kernel/starkernel_loader.efi` — UEFI PE loader
- `build/amd64/kernel/starkernel_kernel.elf` — Kernel binary

---

## What is LithosAnanke?

LithosAnanke is a UEFI-bootable microkernel with the StarForth VM as its primary userspace runtime.

**Etymology:** *Lithos* (stone/foundation) + *Ananke* (necessity/inevitability)

The kernel boots from UEFI firmware, initializes memory management and interrupts, then launches the StarForth FORTH-79 virtual machine in userspace. The kernel provides minimal services (memory, time, console) while the VM handles all application logic. POST validates that the VM dictionary matches the expected parity hash, ensuring reproducible behavior across boots.

### Relationship to StarForth

| Branch | Purpose |
|--------|---------|
| `master` | Hosted StarForth VM (Linux, L4Re) |
| `lithosananke` | Microkernel + StarForth VM in userspace |

LithosAnanke uses the same VM core as hosted StarForth. The kernel provides minimal HAL services while the VM runs in userspace — a freestanding environment with no libc and kernel-mediated hardware access.

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
│  • PMM: Physical Memory Manager (bitmap allocator)          │
│  • VMM: Virtual Memory Manager (4-level paging)             │
│  • IDT: Interrupt Descriptor Table (256 entries)            │
│  • APIC: Local APIC timer + IRQ routing                     │
│  • Heap: kmalloc/kfree (16 MB)                              │
│  • Console: Serial UART output                              │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  StarForth VM (M7)                                          │
│  • 295 FORTH-79 words                                       │
│  • 5 MB arena allocation                                    │
│  • Parity validation (reproducible dictionary hash)         │
│  • POST: 800 tests run at boot                              │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  Capsule System (M7.1)                                      │
│  • Content-addressed INIT capsules                          │
│  • Birth protocol for child VMs                             │
│  • DOMAIN/PERSONALITY semantics                             │
└─────────────────────────────────────────────────────────────┘
```

### Directory Structure

```
src/starkernel/
├── boot/               # UEFI loader, ELF loader
├── hal/                # Console, host services
├── memory/             # PMM, VMM, kmalloc
├── arch/amd64/         # x86_64 specific (APIC, IDT, timer, ISR)
├── vm/                 # VM integration
│   ├── arena.c         # VM memory arena
│   ├── parity.c        # Parity validation
│   ├── bootstrap/      # sk_vm_bootstrap (POST runner)
│   └── host/           # VMHostServices shim
└── kernel_main.c       # Entry point
```

---

## Current Capabilities

- **UEFI Boot** — Boots on real hardware and QEMU (OVMF)
- **Serial Console** — All output via COM1 (115200 baud)
- **Memory Management** — PMM tracks ~1GB RAM, VMM with 4-level paging
- **Interrupts** — Full IDT, exception handlers, APIC timer
- **Heartbeat Timer** — 100 Hz tick with TIME-TRUST metric
- **KRELTSC Logging** — Relative TSC timestamps on all log messages
- **StarForth VM** — Complete FORTH-79 dictionary (295 words)
- **POST Validation** — 800 tests run at boot, parity hash verified

---

## Roadmap

### Near Term

| Milestone | Goal |
|-----------|------|
| **M7.1** | Capsule system — content-addressed INIT capsules, birth protocol |
| **M8** | REPL — keyboard input, `ok` prompt, interactive Forth |
| **M9** | Block storage — AHCI driver, persistent blocks |

### Future

| Milestone | Goal |
|-----------|------|
| M10 | Networking — VirtIO-net, TCP/IP stack |
| M11 | Process model — Forth tasks, scheduling |
| M12 | Self-hosting — compile Forth on LithosAnanke |

**Full roadmap:** [docs/lithosananke/ROADMAP.md](docs/lithosananke/ROADMAP.md)

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/lithosananke/README.md](docs/lithosananke/README.md) | Architecture overview |
| [docs/lithosananke/ROADMAP.md](docs/lithosananke/ROADMAP.md) | Full milestone roadmap |
| [docs/lithosananke/M7.1.md](docs/lithosananke/M7.1.md) | Capsule system design |
| [docs/lithosananke/hal/](docs/lithosananke/hal/) | HAL documentation |

---

## Patent Pending

**This technology is subject to a provisional patent application filed with the USPTO.**

**Patent Application:** Physics-Grounded Self-Adaptive Runtime System for Virtual Machines
**Filing Date:** December 2025
**Applicant:** Robert A. James
**Status:** Provisional Patent Pending

The adaptive runtime system, frequency-based optimization mechanisms, rolling window of truth, and deterministic inference algorithms described in this repository are proprietary innovations covered by pending patent protection.

**For licensing inquiries, contact:** rajames440@gmail.com

---

## License

**LithosAnanke is released under the Starship License 1.0 (SL-1.0).**

- Use for personal, research, and educational purposes
- Modify and create derivative works
- Distribute in source or binary form

**Commercial use requires a separate license.** Contact rajames440@gmail.com for licensing.

**Attribution requirement:** Permanent attribution to **R.A. James (Captain Bob)** must be preserved in all distributions.

**Patent Notice:** This license does **not** grant patent rights. See Patent Pending section above.

**Full license:** See [LICENSE](LICENSE)

---

## Author

**Robert A. James (Captain Bob)**
Systems Engineer, Hacking Since 1973

---

## Related

- **StarForth** (`master` branch) — Hosted FORTH-79 VM with adaptive runtime
- **StarshipOS** — Future: Full operating system built on LithosAnanke

---

<p align="center">
  <em>Lithos</em> — The foundation is laid.<br>
  <em>Ananke</em> — The necessity is clear.
</p>
