# LithosAnanke

**UEFI-bootable FORTH microkernel.** Boots from firmware, initialises memory and interrupts, then runs the StarForth VM as its sole userspace runtime. No libc. No OS. Just stone and necessity.

*Lithos* (foundation) + *Ananke* (necessity) — the kernel under StarshipOS.

---

## Status — M7.1 (Capsule System)

| Milestone | | Status |
|---|---|---|
| M0–M6 | UEFI boot · PMM · VMM · IDT · APIC · heap | Complete |
| M7 | StarForth VM integration + parity validation | Complete |
| **M7.1** | **Capsule birth protocol · Mama FORTH vocabulary** | **In Progress** |
| M8 | REPL — keyboard input, interactive Forth | Planned |
| M9 | Block storage — AHCI driver | Planned |

POST at boot: **800 tests · parity hash verified · 295 FORTH-79 words**

---

## Quick Start

```bash
# Build kernel (requires cross-compilation toolchain for amd64)
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1

# Run in QEMU with OVMF
make -f Makefile.starkernel qemu
```

Artifacts: `build/amd64/kernel/starkernel_loader.efi` · `build/amd64/kernel/starkernel_kernel.elf`

For the hosted VM (Linux, no cross-compiler needed): see [`master` branch](../../tree/master).

---

## Documentation

| | |
|---|---|
| [System Architecture](docs/lithosananke/SYSTEM_ARCHITECTURE.md) | Full kernel + VM design |
| [HAL Reference](docs/lithosananke/hal/) | Hardware abstraction layer interfaces |
| [Capsule System — M7.1](docs/lithosananke/M7.1.md) | Capsule birth protocol design |
| [Roadmap](docs/lithosananke/ROADMAP.md) | Milestone plan through self-hosting |

---

## License

[Starship License 1.0 (SL-1.0)](LICENSE) — free for personal, research, and educational use.
Commercial use requires a separate agreement.
Attribution to **R.A. James (Captain Bob)** must be preserved in all distributions.

**Patent pending.** USPTO provisional filed December 2025 — physics-grounded self-adaptive runtime system. This license does not grant patent rights. Licensing inquiries: rajames440@gmail.com

---

*Robert A. James (Captain Bob) · Systems Engineer · Hacking since 1973*
