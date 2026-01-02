# LithosAnanke Hardware Abstraction Layer (HAL)

This directory contains HAL documentation for the LithosAnanke kernel branch.

---

## Document Overview

| Document | Purpose |
|----------|---------|
| [overview.md](overview.md) | HAL architecture and design principles |
| [interfaces.md](interfaces.md) | HAL function specifications and contracts |
| [lithosananke-integration.md](lithosananke-integration.md) | Kernel-specific HAL implementation |
| [platform-implementations.md](platform-implementations.md) | How to implement HAL for a platform |
| [migration-plan.md](migration-plan.md) | Migration from hosted to kernel |

---

## Reading Order

1. **overview.md** — Understand why HAL exists
2. **interfaces.md** — Learn the contracts
3. **lithosananke-integration.md** — See kernel implementation
4. **platform-implementations.md** — Reference implementations

---

## HAL Subsystems

```
hal_time.h       — Monotonic time, timers
hal_interrupt.h  — IRQ management, ISR registration
hal_memory.h     — Allocation, page mapping
hal_console.h    — Character I/O (UART, framebuffer)
hal_cpu.h        — CPU ID, relax/halt, SMP
hal_panic.h      — Fatal error handling
```

---

## Current State (lithosananke branch)

The HAL is **partially implemented** in LithosAnanke:

| Subsystem | Status | Location |
|-----------|--------|----------|
| Time | Implemented | `src/starkernel/arch/amd64/timer.c` |
| Interrupts | Implemented | `src/starkernel/arch/amd64/interrupts.c` |
| Memory | Implemented | `src/starkernel/memory/` |
| Console | Implemented | `src/starkernel/hal/console.c` |
| CPU | Partial | `src/starkernel/arch/amd64/apic.c` |
| Panic | Implemented | `src/starkernel/hal/hal.c` |

---

## Relationship to Hosted StarForth

The HAL enables the same VM code to run on:

- **Linux** (hosted, POSIX)
- **LithosAnanke** (freestanding, bare metal)
- **L4Re** (microkernel, optional)

```
┌─────────────────────────────────────┐
│  StarForth VM + Physics Subsystems  │
│         (platform-agnostic)         │
├─────────────────────────────────────┤
│              HAL Layer              │
├──────────┬──────────┬───────────────┤
│  Linux   │  L4Re    │  LithosAnanke │
│ (POSIX)  │ (micro)  │   (kernel)    │
└──────────┴──────────┴───────────────┘
```

---

## See Also

- [../README.md](../README.md) — LithosAnanke overview
- [../ROADMAP.md](../ROADMAP.md) — Milestone roadmap
- [../M7.1.md](../M7.1.md) — Init Capsule Architecture
