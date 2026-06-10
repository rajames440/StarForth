# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Mandatory AI Agent Reading

**Before touching any physics runtime, kernel, or capsule code, read these in order:**

1. `docs/AI_AGENT_MANDATORY_README.md` — Non-negotiable rules for all AI assistants
2. `docs/TASK_LIST.md` — Task scope, hard rules, and working protocol
3. `docs/NEXT_SESSION.md` — Current task queue and next-session plan (read this first to orient)

These documents are project law. Violations produce invalid output.

## Build and Process Rules — NON-NEGOTIABLE

**One process at a time. Always.**

- Never run more than one `make`, `qemu-system`, or build process simultaneously in this workspace.
- Never use `run_in_background: true` for builds or QEMU runs while another build or QEMU process is active.
- Never launch parallel Agent tasks that each spawn builds.
- Wait for the current process to fully complete (task notification received, exit code checked) before starting the next one.
- Concurrent builds corrupt shared state (`build/`, `logs2/`, `experiments/`) and produce invalid results.

## Next Feature: Word-Level ACL System

**Design doc:** `docs/03-architecture/word-acl/DESIGN.md`

The next major feature is a word-level ACL system implemented entirely in
FORTH (`capsules/ACL.4th`). Work begins on `master`; `lithosananke` is
brought to parity after. Read the design doc before touching any ACL-related
code. Key constraints:

- All policy logic in `ACL.4th` — no new C primitives for policy
- Two C fields only: `acl_ttl` (counter) + `acl_allow` (bit) in `DictEntry`
- Emergency console (`vm->emergency_console`) always bypasses ACL — 100%
- TTL is statistically adaptive (heat + rolling window + decay + inference)
- Pin (`ACL-PIN`) is one-way; inheritance clears pin, copies mode

## What This Branch Is

**Branch: `lithosananke`** — This is the **bare-metal FORTH microkernel** branch of StarForth.

LithosAnanke (*lithos* = foundation, *ananke* = necessity) is a UEFI-bootable microkernel that uses the StarForth VM as its sole userspace runtime. It is **not** a variant of the hosted Linux VM — it is a complete system that boots from UEFI firmware, manages its own memory and interrupts, and runs FORTH as the native control plane.

**Current Milestone:** M7.1 — Init Capsule Architecture (design complete, implementation in progress)

**Architecture Vision:**
```
StarForth (hosted VM, Linux/L4Re) — this repo, master branch
    ↓
LithosAnanke (this branch) — UEFI kernel + StarForth VM
    ↓
StarshipOS (future) — full OS with storage, networking, process model
```

## Project Overview

StarForth v3.0.1 on the lithosananke branch is a FORTH-79 compliant VM with a **physics-driven adaptive runtime (SSM — Steady-State Machine)**, packaged as a bare-metal microkernel. The kernel:

- Boots via UEFI (ExitBootServices path)
- Manages physical and virtual memory (PMM + VMM)
- Runs POST (936+ FORTH word tests) at boot
- Births a "Mama" VM that spawns child VMs from init capsules
- Runs FORTH as the only userspace runtime — no processes, no syscalls

**Formal validation:** 0% algorithmic variance across 90 experimental runs. Validated via 2^7 DoE (128 configs × 300 reps = 38,400 runs). Provisional patent filed December 2025.

## Build Commands

### Hosted VM (Linux) — same as master

```bash
make                  # Standard optimized build
make fastest          # Maximum performance (ASM + LTO + direct threading)
make fast             # Fast without LTO
make turbo            # Assembly-only optimizations
make pgo              # Profile-guided optimization (6-stage)
make debug            # Debug build (-O0 -g)
make TARGET=asan all  # ASan/UBSan memory error detection
make test             # Run full test suite (936+ tests)
make bench            # Quick benchmark
make benchmark        # Full benchmark suite
make clean            # Clean build artifacts
make clean-obj        # Clean object files only
```

### Kernel (LithosAnanke) — `Makefile.starkernel`

```bash
# Build kernel for amd64 with VM enabled
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1

# Boot in QEMU (requires OVMF firmware)
make -f Makefile.starkernel qemu

# Build for aarch64
make -f Makefile.starkernel ARCH=aarch64

# Build for riscv64
make -f Makefile.starkernel ARCH=riscv64

# Clean kernel artifacts
make -f Makefile.starkernel clean
```

**Kernel build artifacts:**
- `build/amd64/kernel/starkernel_loader.efi` — UEFI loader (PE32+ format)
- `build/amd64/kernel/starkernel_kernel.elf` — Kernel ELF binary

### Build Targets and Architectures

Supported 64-bit architectures (32-bit explicitly rejected):
- `amd64` (x86_64) — Full feature set, asm optimizations
- `arm64` (aarch64) — Full feature set, SIMD optimizations
- `raspi` — Raspberry Pi 4+ (arm64 + cortex-a72 tuning)
- `riscv64` — RISC-V 64-bit (QEMU)

Cross-compile shortcuts:
```bash
make rpi4-cross    # Cross-compile for RPi4 from x86_64
make rpi4          # Native RPi4 build
make rpi4-fastest  # Max optimization for RPi4
make minimal       # Freestanding/embedded build
```

### Key Build Flags

Physics subsystem defaults (note: defaults are conservative for research):
- `STRICT_PTR=1` — Pointer safety checks (default: **on**)
- `ENABLE_HOTWORDS_CACHE=0` — Physics cache (default: **off**; enable for production)
- `ENABLE_PIPELINING=0` — Word transition prediction (default: **off**; enable for production)
- `HEARTBEAT_THREAD_ENABLED=1` — Background heartbeat (default: **on**; auto-disabled on L4Re/kernel)
- `USE_ASM_OPT=1` — Architecture asm (set automatically by TARGET)

Tuning knobs:
- `ROLLING_WINDOW_SIZE=4096` — Execution history capture depth
- `TRANSITION_WINDOW_SIZE=8` — Pipelining context depth
- `ADAPTIVE_SHRINK_RATE=50` — Window shrink aggressiveness
- `ADAPTIVE_MIN_WINDOW_SIZE=256` — Rolling window floor
- `INITIAL_DECAY_SLOPE_Q48=21845` — Cold-start decay slope (1/3 in Q48.16)
- `DECAY_RATE_PER_US_Q16=1` — Heat decay rate (half-life ~6.5s at default)
- `HEARTBEAT_TICK_NS=10000` — Heartbeat wake frequency (10µs)
- `HEARTBEAT_INFERENCE_FREQUENCY=1000` — Ticks between inference engine runs

### Important: Linker Configuration

`fastest` uses `-flto=auto -fuse-linker-plugin` (not plain `-flto`) to avoid "ELF section name out of range" errors on large codebases.

## Running

### Hosted VM
```bash
./build/amd64/standard/starforth              # Interactive REPL
./build/amd64/standard/starforth --run-tests  # Run all tests
./build/amd64/standard/starforth -c "1 2 + . BYE"  # Inline code
./build/amd64/fastest/starforth --doe         # Design of Experiments mode
```

### Kernel via QEMU

**ACCEPTANCE CRITERIA — non-negotiable:**
The ONLY valid acceptance test for any kernel change is booting all three
architectures in QEMU and capturing the serial log. There is no other test.
`make test` (hosted VM) is NEVER used to validate kernel changes.

```bash
# Run in this exact order for every kernel change:
make -f Makefile.starkernel ARCH=amd64   clean qemu
make -f Makefile.starkernel ARCH=aarch64 clean qemu
make -f Makefile.starkernel ARCH=riscv64 clean qemu
```

Always pass `clean` before `qemu` — never build-only without clean.

**What the amd64 `qemu` target now does (three steps in one command):**
1. **Host render test** (`fbtest`) — compiles `framebuffer.c`/`vt100.c`/`font_8x16.c` with
   host gcc, renders an ANSI test page into RAM, asserts 8 pixel-level properties (ANSI colors,
   truecolor, reverse video, cursor positioning, scroll), writes PNG renders to `build/host/`.
   Exits non-zero on any assertion failure — do not proceed to QEMU if this fails.
2. **Serial acceptance boot** — boots the kernel under QEMU, captures serial output,
   verifies `ok>` prompt, prints the full log.
3. **Framebuffer screendump** — boots the kernel a second time with a HMP monitor socket,
   waits for `ok>`, issues `screendump` to capture the live GOP framebuffer surface, and
   saves a PNG proving that `console_fb_init → BootInfo.framebuffer → VRAM` works end-to-end.

Output artifacts captured to `logs/`:
```
logs/qemu-amd64-YYYYMMDD-HHMMSS.log          ← serial acceptance log
logs/qemu-screenshot-YYYYMMDD-HHMMSS.log      ← screendump serial log (amd64 only)
logs/qemu-screenshot-YYYYMMDD-HHMMSS.png      ← framebuffer screendump PNG (amd64 only)
logs/qemu-aarch64-YYYYMMDD-HHMMSS.log
logs/qemu-riscv64-YYYYMMDD-HHMMSS.log
```
These artifacts are for Captain Bob's manual inspection. Do not delete them.
`logs/` is tracked in git — it is a canonical optimistic history of all QEMU acceptance
runs. Commit and push new logs (including PNG screendumps) after every acceptance test run.
Do not claim a change is accepted until all three architectures have booted
to `ok>` and their logs are present in `logs/`.

The standalone `fbtest` and `qemu-screenshot` targets remain available:
```bash
make -f Makefile.starkernel fbtest             # host render test only (no QEMU)
make -f Makefile.starkernel ARCH=amd64 qemu-screenshot  # screendump only
```

## Architecture

### System Architecture: Three Layers

```
┌──────────────────────────────────────────────────────────┐
│  Layer 1: VM Core + SSM Physics Subsystems               │
│  • FORTH-79 interpreter, dictionary, stacks              │
│  • SSM: heat, rolling window, cache, pipelining          │
│  • L8 Jacquard mode selector (policy engine)             │
│  • Heartbeat coordinator                                 │
│  ↓ calls HAL only — never platform code directly         │
├──────────────────────────────────────────────────────────┤
│  Layer 2: Hardware Abstraction Layer (HAL)               │
│  • hal_time.h    — Monotonic time, periodic timers       │
│  • hal_interrupt.h — IRQ management, ISR registration    │
│  • hal_memory.h  — Memory allocation, page mapping       │
│  • hal_console.h — Character I/O for REPL                │
│  • hal_cpu.h     — CPU ID, relax/halt, SMP               │
│  ↓ platform-specific implementations                     │
├──────────────────────────────────────────────────────────┤
│  Layer 3: Platform Implementations                       │
│  ┌──────────────┬──────────────┬──────────────────────┐  │
│  │ Linux (POSIX)│ L4Re         │ LithosAnanke kernel  │  │
│  │ clock_gettime│ L4Re::Clock  │ TSC + HPET + APIC    │  │
│  │ malloc/free  │ dataspaces   │ PMM + VMM + kmalloc  │  │
│  │ stdin/stdout │ L4Re console │ UART + framebuffer   │  │
│  └──────────────┴──────────────┴──────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

### UEFI Boot Flow

1. UEFI firmware loads `starkernel_loader.efi`
2. Loader calls `ExitBootServices`, then initializes:
   - Serial console (UART or UEFI framebuffer)
   - PMM — parses UEFI memory map, builds page bitmaps
   - VMM — switches CR3, sets up 4-level paging
   - IDT — 256 interrupt descriptor table entries
   - APIC — local interrupt controller + timers
3. `kernel_main()` (`src/starkernel/kernel_main.c`):
   - Initializes heap allocator
   - Runs POST (936+ FORTH word tests); prints parity hash
   - Births Mama VM (executes init capsule)
4. Mama VM (FORTH):
   - Births system VMs (Messaging, Block I/O)
   - Births user session VMs on demand
   - Runs REPL in kernel mode
5. Each child VM:
   - Gets its own heap + RAM blocks (0–2047)
   - Maps disk blocks from storage (2048+)
   - Runs isolated FORTH session

### Full Source Tree

```
src/
├── main.c                    # Hosted VM entry point
├── vm.c                      # Core interpreter loop, stacks, dictionary
├── vm_api.c                  # External VM API
├── vm_bootstrap.c            # VM initialization helpers
├── vm_time.c                 # VM-level timing
├── vm_debug.c                # Debug utilities
├── repl.c                    # Interactive REPL (hosted)
├── cli.c                     # CLI argument parsing
├── stack_management.c        # Stack operations
├── memory_management.c       # Dictionary allocator
├── block_subsystem.c         # Logical→physical block mapper
├── blkio_factory.c / blkio_file.c / blkio_ram.c  # Block I/O backends
│
├── ssm_jacquard.c            # L8 Jacquard mode selector (SSM policy engine)
├── heartbeat_export.c        # Heartbeat CSV export (planned/partial)
├── dictionary_heat_optimization.c  # Execution heat tracking (L1)
├── rolling_window_of_truth.c # Circular execution history (L2)
├── physics_hotwords_cache.c  # Frequency-driven cache
├── physics_pipelining_metrics.c    # Word transition prediction (L4)
├── physics_runtime.c         # Physics runtime coordination
├── physics_metadata.c        # Per-word physics metadata
├── physics_execution_hooks.c # Execution interception
├── inference_engine.c        # ANOVA early-exit + regression (L5/L6)
├── doe_metrics.c             # DoE metrics extraction
├── dictionary_management.c   # Dictionary search + registration
├── word_registry.c           # Word registration API
├── math_portable.c           # Q48.16 fixed-point math
├── io.c, log.c, profiler.c
│
├── word_source/              # FORTH-79 words (25+ modules, ~295 words)
│   ├── arithmetic_words.c    # + - * / MOD
│   ├── stack_words.c         # DUP DROP SWAP
│   ├── control_words.c       # IF ELSE THEN DO LOOP
│   ├── defining_words.c      # : ; CREATE DOES> VARIABLE CONSTANT
│   ├── memory_words.c        # @ ! +! ALLOT
│   ├── logical_words.c       # AND OR XOR
│   ├── double_words.c / mixed_arithmetic_words.c
│   ├── string_words.c / format_words.c
│   ├── io_words.c / block_words.c / editor_words.c
│   ├── dictionary_words.c / dictionary_manipulation_words.c
│   ├── return_stack_words.c / system_words.c / vocabulary_words.c
│   ├── starforth_words.c     # StarForth extensions + version word
│   ├── q48_16_words.c        # Q48.16 fixed-point words
│   ├── log_words.c           # Module 24: LOG-DEBUG LOG-INFO LOG-WARN LOG-ERROR
│   ├── physics_benchmark_words.c / physics_diagnostic_words.c
│   ├── physics_freeze_words.c / physics_pipelining_diagnostic_words.c
│   └── dictionary_heat_diagnostic_words.c
│
├── test_runner/              # Test harness
│   ├── test_runner.c         # Orchestration + POST integration
│   └── modules/              # 22 test modules (936+ tests)
│       ├── break_me_tests.c  # Adversarial edge cases
│       ├── integration_tests.c
│       ├── stress_tests.c
│       └── *_test.c          # One per word category
│
├── platform/                 # Platform abstraction (hosted)
│   ├── linux/time.c          # POSIX timing
│   ├── l4re/time.c           # L4Re timing
│   ├── platform_init.c / threading.c
│   ├── alloc_host.c / alloc_kernel.c
│
└── starkernel/               # KERNEL-SPECIFIC SOURCE (LithosAnanke)
    ├── kernel_main.c         # Kernel entry point (POST + Mama VM boot)
    ├── repl.c                # Kernel REPL (no OS I/O — direct hardware)
    ├── doe_log.c             # Per-tick heartbeat CSV via serial (DoE logging)
    │
    ├── arch/                 # Architecture-specific implementations
    │   ├── amd64/            # x86_64: IDT, TSC, APIC, GDT
    │   ├── aarch64/          # ARM64: system timers, exception vectors
    │   └── riscv64/          # RISC-V: CLINT, PLIC, trap handling
    │
    ├── boot/                 # UEFI bootloader
    │   └── (UEFI loader, ExitBootServices, serial init)
    │
    ├── capsule/              # M7.1 Init Capsule System
    │   ├── capsule_registry.c    # Content-addressed capsule storage
    │   ├── capsule_loader.c      # Load capsule by hash, execute
    │   ├── capsule_birth.c       # Create new VM from capsule
    │   └── capsule_run.c         # Capsule execution runtime
    │
    ├── hal/                  # Hardware Abstraction Layer implementations
    │   ├── console.c         # Serial/framebuffer output
    │   ├── hal.c             # Generic HAL functions
    │   └── hal_panic.c       # Kernel panic handler
    │
    ├── memory/               # Memory management
    │   ├── pmm.c             # Physical Memory Manager (page bitmaps)
    │   ├── vmm.c             # Virtual Memory Manager (4-level paging)
    │   └── heap.c            # Kernel heap allocator
    │
    ├── math/                 # Freestanding fixed-point math
    ├── hash/                 # xxHash64 (content-addressed capsule IDs)
    └── vm/                   # Kernel-side VM wrapper
```

### SSM: Steady-State Machine (Physics-Driven Adaptive Runtime)

8-loop feedback architecture validated via 2^7 DoE (38,400 runs):

| Loop | Name | Default State | Notes |
|------|------|--------------|-------|
| L1 | Execution Heat Tracking | **Disabled** | Harmful in 86% of top configs |
| L2 | Rolling Window History | L8-controlled | On when entropy > 0.75 |
| L3 | Linear Decay | L8-controlled | On when temporal_decay > 0.5 |
| L4 | Pipelining Metrics | **Disabled** | Harmful in 100% of top configs |
| L5 | Window Width Inference | L8-controlled | On when CV > 0.15 |
| L6 | Decay Slope Inference | L8-controlled | On when CV > 0.15 AND temporal > 0.3 |
| L7 | Adaptive Heartrate | **Always on** | Beneficial in 71% of top configs |
| L8 | Jacquard Mode Selector | **Policy engine** | 4-bit selector (C0–C15) for L2/L3/L5/L6 |

**L8 Jacquard** (`src/ssm_jacquard.c`) dynamically switches L2/L3/L5/L6 based on runtime metrics with hysteresis. Top 5% validated modes: C4, C7, C9, C11, C12. See `include/ssm_jacquard.h` for the mode table.

### M7.1 Init Capsule System

The capsule system is the VM birth/identity mechanism. Key invariants:

- Each capsule is content-addressed: `capsule_id == xxHash64(FORTH source code)`
- Exactly one production `(p)` INIT defines each child VM's personality
- No inheritance between capsules; no runtime selection of base INIT
- **DOMAIN** (roles, composition) is Mama's concern
- **PERSONALITY** (worldview, behavior) is the child VM's concern

Capsule files live in `capsules/` (formerly `conf/` — do not recreate `conf/`):
- `capsules/init.4th` — Default hosted init
- `capsules/init-0.4th` … `init-9.4th` — Numbered variants
- `capsules/init-l8-*.4th` — L8 Jacquard experiment capsules

### Key Data Structures

**Hosted VM:**
- `VM` struct (`include/vm.h`) — All VM state; nothing global, nothing hidden
- `DictEntry` — Word with `execution_heat`, `physics` metadata, `transition_metrics`
- `RollingWindowOfTruth` — Self-tuning circular execution history buffer
- `HeartbeatState` — Background thread coordinating L3 + L5
- `ssm_l8_state_t` (`include/ssm_jacquard.h`) — Current Jacquard mode + hysteresis counter

**Kernel:**
- `include/starkernel/capsule.h` — Capsule descriptor + content-address
- `include/starkernel/capsule_birth.h` — VM birth protocol
- PMM/VMM state — managed via `src/starkernel/memory/`
- HAL interfaces — `include/starkernel/` headers

### Memory Model

- `vaddr_t` — VM addresses are byte offsets, never raw C pointers
- `vm_load_cell()` / `vm_store_cell()` — Always use these, never pointer arithmetic
- Dictionary occupies first 2MB; user blocks start at block 2048
- `VM_ADDR()` / `CELL()` macros for offset↔cell conversions
- Kernel: PMM manages physical pages; VMM maps them; heap sits on top

## Testing

Tests run in POST (Power-On Self Test) order — same on hosted VM and bare-metal kernel:

1. **Unit tests:** Q48.16 fixed-point, inference statistics, decay slope inference
2. **Dictionary tests:** FORTH-79 word validation across 22 modules (936+ tests)

```bash
# Hosted
make test                                         # Full suite
make test PROFILE=1                               # With profiling
make test FAIL_FAST=1                             # Stop on first failure
./build/amd64/standard/starforth --run-tests      # Direct (faster iteration)

# Kernel (QEMU serial captures POST output)
make -f Makefile.starkernel qemu                  # Boot + auto-POST + parity hash
```

POST parity hash: printed after all 936+ tests pass at kernel boot. A hash mismatch indicates dictionary corruption or a missing word.

Test files: `src/test_runner/modules/*_test.c`. New suites go there with a `run_<area>_tests()` entry registered in `src/test_runner/test_runner.c`.

## Quality Tools

```bash
make quality         # clang_tidy + cppcheck + gcc_analyzer
make clang_tidy      # Config: .clang-tidy, ignore: .clang-tidy-ignore
make cppcheck        # Static analysis
make gcc_analyzer    # GCC static analyzer
make compile_commands  # generate compile_commands.json
```

## Documentation

```bash
make docs            # Generate all documentation
make api-docs        # Doxygen XML → AsciiDoc
make docs-latex      # AsciiDoc → LaTeX
make isabelle-build  # Build + verify Isabelle formal theories
make isabelle-check  # Quick-check Isabelle theories
make math-companion  # SSRN math companion PDF
make sbom            # SBOM in SPDX format (requires syft)
```

Key documentation files:
- `docs/lithosananke/README.md` — Kernel status, milestones, quick start
- `docs/lithosananke/SYSTEM_ARCHITECTURE.md` — Full kernel + VM design
- `docs/lithosananke/ROADMAP.md` — Milestone breakdown M0–M10
- `docs/lithosananke/M7.1.md` — Init capsule architecture (PERSONALITY vs DOMAIN)
- `docs/lithosananke/hal/README.md` — HAL subsystems overview
- `docs/lithosananke/hal/interfaces.md` — HAL function contracts
- `docs/03-architecture/OVERVIEW.md` — Three-layer architecture diagram
- `docs/AI_AGENT_MANDATORY_README.md` — **Mandatory** AI agent rules
- `docs/TASK_LIST.md` — Task protocol, hard rules
- `docs/AGENTS.md` — Contributor guidelines
- `docs/ONTOLOGY.md` — Formal definitions and terminology

## Code Standards

- **Strict ANSI C99** — No GNU extensions, no C++ features, no C11
- **Freestanding for kernel** — Kernel code may not use libc; use `include/starkernel/freestanding/`
- **Zero warnings** — `-Wall -Werror` on all builds
- **No hidden state** — All VM state explicit in `VM` struct; kernel state in explicit global structs
- **Platform-agnostic VM** — VM core calls HAL only; never platform code directly

## Coding Conventions (from `docs/AGENTS.md`)

- **Indentation:** 4 spaces, same-line braces — `static void foo(void) {`
- **Naming:** `snake_case` functions/vars, UPPER_CASE macros, `g_` prefix for file-scoped statics
- **APIs:** Exported functions in `include/` with `/** ... */` headers; internals `static`
- **Public boundary:** `include/` for VM headers; `include/starkernel/` for kernel headers
- **Feature gating:** Gate new behavior behind existing flags; don't add undocumented flags

## Important Conventions

- Stack values on data stack are `vaddr_t` offsets — never raw C pointers
- `VM_ADDR()` / `CELL()` for offset↔pointer conversions
- Words with `WORD_IMMEDIATE` execute during compilation
- `STRICT_PTR=1` enforces bounds checking; disable only for benchmarking
- `WORD-ENTROPY` prints execution heat stats; intentionally kept on in DoE mode
- Kernel code: never call `malloc`/`free` — use `pmm_alloc_page()`, `heap_alloc()`, or the VM's own allocator

## Repository Layout

```
StarForth/
├── src/                  # All C source (see above)
│   └── starkernel/       # Kernel-specific source (this branch)
├── include/              # Public VM headers
│   └── starkernel/       # Kernel headers + freestanding libc replacements
├── capsules/             # FORTH-79 init scripts (.4th)
├── experiments/          # DoE experiment infrastructure
├── docs/                 # Documentation
│   └── lithosananke/     # Kernel-specific docs (this branch)
├── build/                # Build output (disposable)
├── lfs/                  # LFS binary copies (auto-updated on build)
├── scripts/              # Build/test/profiling automation
├── Makefile              # Hosted VM build
├── Makefile.starkernel   # Kernel cross-compile build
├── Doxyfile              # API docs config
├── .clang-tidy           # Clang-tidy config
└── .clang-tidy-ignore    # Clang-tidy ignore list
```

## Critical Implementation Details

### DoE CSV Output (Hosted)

CSV output in `--doe` mode is **intentionally suppressed** (since 2025-12-08, `src/main.c:413-419`). Metrics collected via `metrics_from_vm()` but not printed. Re-enable by uncommenting `metrics_write_csv_row()` or adding `--csv-export`.

### Kernel DoE Logging

The kernel emits DoE rows via serial (`src/starkernel/doe_log.c`). This is distinct from the hosted VM suppression — kernel DoE logging is **enabled** and captured from QEMU serial output. Validated on amd64, aarch64, and riscv64.

### Module 24: Log Level Words

`src/word_source/log_words.c` implements FORTH-level log control:
- `LOG-DEBUG`, `LOG-INFO`, `LOG-WARN`, `LOG-ERROR` — set the active log level
- Added in recent commits; validates on all three architectures via QEMU serial

### Heartbeat Instrumentation

`heartbeat_export.c` exists but `heartbeat_export_csv()` is not fully implemented. Data structures (`HeartbeatTickSnapshot`, `tick_buffer`) declared in `include/vm.h`. See `docs/heartbeat_csv_export.md`.

### SSM Architecture (Experimentally Validated)

- **L1 and L4 permanently disabled** — proven harmful across all experiments
- **L7 always on** — unconditionally beneficial
- **L8 Jacquard** controls L2/L3/L5/L6 dynamically with hysteresis
- `ENABLE_HOTWORDS_CACHE` and `ENABLE_PIPELINING` default to **0** intentionally

### Capsules (formerly `conf/`)

The `capsules/` directory holds FORTH-79 initialization blobs. Previously `conf/` — all paths updated in commit `88e8858`. Do not recreate `conf/`.

### Kernel HAL Rules

- VM core must **never** call Linux/POSIX functions directly — always route through HAL
- Adding a new platform: implement all HAL interfaces in `src/starkernel/arch/<arch>/`
- HAL panic (`hal_panic.c`) halts the CPU and should never return
- Interrupt handlers must not allocate memory or call into the FORTH interpreter

## Milestone Roadmap (Summary)

| Milestone | Status | Description |
|-----------|--------|-------------|
| M0 | Complete | UEFI boot, serial console |
| M1 | Complete | PMM (physical memory manager) |
| M2 | Complete | VMM (virtual memory manager) |
| M3 | Complete | Interrupt handling (IDT/APIC) |
| M4 | Complete | Kernel heap allocator |
| M5 | Complete | VM integration (FORTH-79 runs in kernel) |
| M6 | Complete | POST (936+ tests at boot, parity hash) |
| M7 | Complete | Mama VM + init capsule boot |
| M7.1 | In progress | Init capsule architecture (PERSONALITY model) |
| M8 | Planned | Child VM birth, isolation |
| M9 | Planned | Block I/O, thumbdrive mounting |
| M10 | Planned | Multi-architecture hardening |

## Commit and PR Guidelines

- One task per commit; sentence-style subject under 72 characters
- Include rationale and verification commands in commit body
- Run `make test` (hosted) and `make -f Makefile.starkernel qemu` (kernel) before any PR
- Never batch multiple tasks into one commit
- All changes require Captain Bob's explicit approval before merge
- Kernel changes: annotate which milestone and which HAL subsystem is affected

## Critical Git Rules

- Never force-push to `lithosananke` without explicit approval
- Kernel source (`src/starkernel/`) and kernel headers (`include/starkernel/`) are reviewed separately from hosted VM changes
- Do not mix hosted VM changes and kernel changes in the same commit

## License

See `./LICENSE` — Starship License 1.0 (SL-1.0). Free for personal/research/education; commercial use requires agreement. R.A. James (Captain Bob) must be credited in all distributions. Patent pending (provisional filed December 2025).
