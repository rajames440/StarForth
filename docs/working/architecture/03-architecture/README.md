<!-- Moved from docs/03-architecture/README.md to docs/working/architecture/03-architecture/README.md on 2026-06-16 (docs reorg Phase 2) -->
# Architecture

This directory contains system architecture documentation for StarForth's core subsystems and the path to StarKernel and StarshipOS.

## Subsystems

- **[hal/](hal/)** - Hardware Abstraction Layer (enables StarForth → StarKernel → StarshipOS)
- **[heartbeat-system/](heartbeat-system/)** - Centralized time-based tuning coordinator
- **[physics-engine/](physics-engine/)** - Physics-driven adaptive runtime
- **[pipelining/](pipelining/)** - Speculative execution via word transition prediction
- **[adaptive-systems/](adaptive-systems/)** - Window inference and decay algorithms

## Architecture Overview

StarForth's architecture consists of three layers:

```
┌──────────────────────────────────────┐
│  StarForth VM + Physics Subsystems   │  ← Platform-agnostic
├──────────────────────────────────────┤
│  HAL (Hardware Abstraction Layer)    │  ← Clean interfaces
├──────────────────────────────────────┤
│  Platform Implementation             │  ← Linux | L4Re | Kernel
└──────────────────────────────────────┘
```

### Core Components

1. **VM Core** (`src/vm.c`) - FORTH-79 interpreter loop, stacks, dictionary
2. **Physics Subsystems** - Execution heat, rolling window, hot-words cache, pipelining
3. **Heartbeat System** - Time-driven tuning coordinator (Loop #3 + Loop #5)
4. **HAL** - Platform abstraction enabling portability

### Key Innovation

**Physics-grounded self-adaptive runtime** with formally proven deterministic behavior:
- 0% algorithmic variance across 90 experimental runs
- Execution heat tracking drives dictionary optimization
- Rolling window of truth captures execution history
- Inference engine adapts window width and decay slope

## Roadmap: StarForth → StarKernel → StarshipOS

### Phase 1: StarForth VM ✅ COMPLETE
- FORTH-79 compliant interpreter
- Physics-driven adaptive runtime
- Runs on Linux, L4Re, bare metal

### Phase 2: HAL Migration 🔄 IN PROGRESS
- Introduce Hardware Abstraction Layer
- Refactor VM to use HAL interfaces
- Preserve deterministic behavior guarantees

### Phase 3: StarKernel 📋 PLANNED
- UEFI bootable kernel
- Freestanding C environment
- Forth as native control plane
- Boot to `ok` prompt on bare metal

### Phase 4: StarshipOS 🔮 FUTURE
- Full operating system
- Storage, networking, device drivers
- Process model (Forth tasks)
- Self-hosting development environment

See `hal/starkernel-integration.md` for the detailed path to StarKernel.

## Key Documents

- **System Ontology:** `../ONTOLOGY.md`
- **Computational Framework:** `../COMPUTATIONAL_PHYSICS_FRAMEWORK.md`
- **Feedback Loops:** `physics-engine/feedback-loops.md`

## For Developers

- **Adding a new subsystem:** Follow the pattern in `hal/` (include README, overview, implementation guide)
- **Understanding interactions:** See physics-engine/ for how subsystems coordinate via heartbeat
- **Platform porting:** See `hal/platform-implementations.md`