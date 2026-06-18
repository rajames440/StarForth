# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **Note:** There is also a `docs/CLAUDE.md` (older, hosted-only snapshot). This file at `.claude/CLAUDE.md` is the authoritative reference.

---

## Word-Level ACL System — Complete through Phase 7

**Design doc:** `docs/03-architecture/word-acl/DESIGN.md`

The word-level ACL system is fully implemented and validated. Read the design
doc before touching any ACL-related code. Key constraints:

- All policy logic in `ACL.4th` — no new C primitives for policy
- Four C fields: `acl_ttl` + `acl_allow` + `acl_mode` + `acl_pinned` in `DictEntry`
- Two VM flags: `emergency_console` (fault handler active) + `zuse_session` (superuser authenticated)
- `ACL.4th` is self-activating — `init.4th` only needs `S" ACL.4th" EXEC` (commented out by default)
- Pin (`ACL-PIN`) is one-way; inheritance clears pin, copies mode
- Two permanent console layers: emergency (`ok>`) and zuse (`zuse)ok>`)
- Superuser `zuse` defined by `capsules/zuse.4th`, loaded by `ACL.4th` at boot
- `emergency_console` bypass applies ONLY to bare `ok>` REPL — zuse sessions are subject to ACL

**Implementation phases:**
1. ✅ C Infrastructure — `DictEntry` fields + interpreter hook + `acl_recheck()`
2. ✅ `ACL.4th` — FORTH policy words + `ACL-INIT-PRIMITIVES` + self-activation
3. ✅ `capsules/zuse.4th` — bootstrap superuser skeleton; CA root placeholder in `ACL.4th`
4. ✅ `init.4th` opt-in toggle — `\ S" ACL.4th" EXEC` (comment out = no security)
5. ✅ POST tests (800/800) + Isabelle/HOL proofs (5 theory files)
6. ✅ `EMERGENCY_CONSOLE_ENABLED` build flag + `vm_fault_handler` extension point
7. ✅ LithosAnanke parity — kernel ACL hook wired; all three ISAs boot to `zuse)ok>` (confirmed in log)
8. ⬜ PKI / thumbdrive — Ed25519 challenge-response; user minting by zuse

**Bug resolutions (all fixed, branch `feature/acl-rwt`):**
- Bug 1 ✅: Kernel ACL interpreter hook ported to `src/starkernel/vm/vm_core.c`
- Bug 2 ✅: `emergency_console` set per-iteration in `src/starkernel/repl.c` (`zuse_session ? 0 : 1`)
- Bug 3 ✅: `!vm->zuse_session` bypass removed from `src/vm.c` ACL checks

**ACL-RWT DoE campaign (branch `feature/acl-rwt`, June 15–16 2026):**
- 3×3 Latin square: seeds 12345/67890/13579 × amd64/aarch64/riscv64, 30 reps each
- This IS the first true ACL-active campaign (Bug 1 fixed before runs)
- Results: +0.0054%–+0.0088% overhead across all 9 cells; CV = 0.000%
- ISA gap closed: ACL-RWT vs Floor=16 is three orders of magnitude improvement
- Baseline (no ACL): amd64=261,098 ticks; aarch64/riscv64=261,095 ticks
- Report: `experiments/bare_metal/analysis/report/bare_metal_doe_report.pdf` (19 pages)
  - 5 R-generated vector figure pairs (10 SVGs via ggplot2/svglite)
  - All data from confirmed measured tick counts — patent support material

**Pick up here next session:**
1. Phase 8 — PKI / Ed25519 thumbdrive (challenge-response; user minting by zuse)

---

## Project Overview

StarForth is a FORTH-79 compliant virtual machine written in strict ANSI C99,
featuring a physics-driven adaptive runtime. It is the primary execution engine
for **StarshipOS** and runs standalone on Linux, L4Re/Fiasco.OC, and bare metal
via **LithosAnanke** (StarKernel).

**Key distinguishing features:**

- Physics-grounded self-adaptive runtime with formally proven deterministic behavior
  (0% algorithmic variance across 90 experimental runs)
- 7 feedback loops driving runtime optimization while preserving determinism
- Formally verified with 19 Isabelle/HOL theory files covering all loops and word categories
- Bare-metal UEFI kernel (LithosAnanke v1.5.3) enabling native execution without Linux
- Patent pending on adaptive runtime mechanisms
- Published SSRN paper: `papers/James_Steady-State_Convergence_Adaptive_Runtime.pdf`

**Technology stack:**

```
┌─────────────────────────────────────────────────────────────────┐
│  StarshipOS (future — self-hosting OS)                          │
├─────────────────────────────────────────────────────────────────┤
│  LithosAnanke v1.5.3)    │
│  ← Milestones M0–M6 complete; M7 VM integration in progress →  │
├─────────────────────────────────────────────────────────────────┤
│  StarForth v3.1.0  (FORTH-79 VM + physics-driven adaptive RT)   │
├───────────────────────────┬─────────────────────────────────────┤
│  Linux / L4Re / Fiasco.OC │  Bare metal (amd64, aarch64, riscv) │
└───────────────────────────┴─────────────────────────────────────┘
```

---

## Build Commands

### StarForth VM (hosted)

```bash
# Standard optimized build (auto-detects architecture)
make

# Maximum performance build (ASM + LTO + direct threading)
make fastest

# Profile-guided optimization build
make pgo

# Debug build with symbols
make debug

# Run full test suite (936+ tests)
make test

# Quick benchmark
make bench

# Clean build artifacts
make clean
```

### StarKernel / LithosAnanke (bare metal)

```bash
# Build UEFI kernel (amd64, requires cross toolchain or native gcc)
make -f Makefile.starkernel

# Build for aarch64
make -f Makefile.starkernel ARCH=aarch64

# Build for riscv64
make -f Makefile.starkernel ARCH=riscv64

# Run in QEMU with OVMF
make -f Makefile.starkernel qemu

# Enable VM integration (M7)
make -f Makefile.starkernel STARFORTH_ENABLE_VM=1
```

Output: `build/amd64/kernel/starkernel_loader.efi` (UEFI PE32+ executable)

### Build Targets and Architectures

- `TARGET=standard|fast|fastest|turbo|pgo` - VM build profiles
- `ARCH=x86_64|amd64|aarch64|arm64|raspi|riscv64` - Target architecture
- Cross-compile for Raspberry Pi: `make rpi4-cross`

### Key Build Flags

- `STRICT_PTR=1` - Enforce pointer safety checks (default on)
- `USE_ASM_OPT=1` - Enable architecture-specific assembler optimizations
- `ENABLE_HOTWORDS_CACHE=1` - Physics-driven hot-words cache (default on)
- `ENABLE_PIPELINING=1` - Speculative execution via word transition prediction (default on)
- `HEARTBEAT_THREAD_ENABLED=1` - Background heartbeat thread for adaptive tuning (default on)
- `STARFORTH_ENABLE_VM=1` - Enable VM integration in StarKernel (M7)
- `PARITY_MODE=1` - Deterministic parity harness mode
- `EMERGENCY_CONSOLE_ENABLED=1` - Interactive fault handler / error recovery REPL (default on; set 0 for production or embedded builds to strip interactive fallthrough surface)

### Important: Linker Configuration

The `fastest` target uses `-flto=auto -fuse-linker-plugin` instead of plain `-flto` to avoid
"ELF section name out of range" errors with large codebases.

---

## Running

```bash
./build/amd64/standard/starforth              # Interactive REPL
./build/amd64/standard/starforth --run-tests  # Run tests then REPL
./build/amd64/standard/starforth -c "1 2 + . BYE"  # Execute inline code
```

### DoE (Design of Experiments) Mode

```bash
./build/amd64/fastest/starforth --doe
```

The `--doe` flag runs the full test harness. The CSV metrics row has been **suppressed**
as of 2025-12-08 (redundant with internal VM metrics). See `src/main.c:390-396`.
To re-enable, add a `--csv-export` flag or write to a file.

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

**QEMU rule — non-negotiable:** Only ONE QEMU instance may run at a time, always in the
foreground (never backgrounded). All three instances use `accel=tcg` (software emulation);
concurrent runs compete for host CPU and corrupt the timing signal the DoE measures.
Run each architecture to completion before starting the next.

**Session keep-alive:** When running from a mobile device, ping the session every 15–20
minutes during a QEMU run or the session will idle out. amd64 is particularly slow under
TCG and is the most likely to outlast a silent interval. Captain Bob must stay engaged
during long runs (30-rep DoE ≈ 25–30 min per ISA).

Always pass `clean` before `qemu` — never build-only without clean.
Serial output is automatically captured to:
```
logs2/qemu-amd64-YYYYMMDD-HHMMSS.log
logs2/qemu-aarch64-YYYYMMDD-HHMMSS.log
logs2/qemu-riscv64-YYYYMMDD-HHMMSS.log
```
These logs are for Captain Bob's manual inspection. Do not delete them.
`logs2/` is gitignored — logs are local artifacts, not committed to the repo.
Do not claim a change is accepted until all three architectures have booted
to `zuse)ok>` and their logs are present in `logs2/`.

---

## Architecture

### Source Tree

```
src/
├── main.c                         # Entry point, CLI, VM init, DoE mode
├── vm.c                           # Interpreter loop, stacks, dictionary state
├── vm_api.c                       # External VM API
├── vm_bootstrap.c                 # VM bootstrap initialization
├── vm_debug.c                     # Debugging utilities
├── vm_time.c                      # Time-related VM operations
├── repl.c                         # REPL read-eval-print loop
├── cli.c                          # CLI parsing
├── io.c                           # I/O operations
├── log.c                          # Logging infrastructure
├── memory_management.c            # Dictionary allocator
├── dictionary_management.c        # Dictionary allocation & search
├── dictionary_heat_optimization.c # Heat tracking (Loop #1)
├── word_registry.c                # Word registration system
├── block_subsystem.c              # Logical→physical block mapper
├── blkio_*.c                      # Block I/O backends (file, RAM, factory)
├── stack_management.c             # Stack operations
├── math_portable.c                # Portable math functions
├── profiler.c                     # Performance profiling
├── heartbeat_export.c             # Heartbeat metrics export
├── ssm_jacquard.c                 # L8 Jacquard steady-state machine
├── doe_metrics.c                  # Design of Experiments metrics (2^7 factorial)
│
├── Physics Engine (7 Feedback Loops):
├── physics_runtime.c              # Main physics coordinator
├── physics_hotwords_cache.c       # Loop #1: Hot-words caching
├── physics_metadata.c             # Metadata tracking
├── physics_pipelining_metrics.c   # Loop #4: Word transition prediction
├── physics_execution_hooks.c      # Execution instrumentation
├── rolling_window_of_truth.c      # Loop #2: Circular execution history
├── inference_engine.c             # Loops #5/#6: ANOVA + statistical inference
│
├── word_source/                   # 25 FORTH-79 word implementation files
│   ├── arithmetic_words.c         # + - * / MOD ABS MIN MAX
│   ├── stack_words.c              # DUP DROP SWAP ROT OVER NIP TUCK
│   ├── control_words.c            # IF ELSE THEN DO LOOP BEGIN UNTIL WHILE
│   ├── defining_words.c           # : ; CREATE DOES> VARIABLE CONSTANT
│   ├── memory_words.c             # @ ! C@ C! MOVE FILL
│   ├── return_stack_words.c       # >R R> R@ RDROP 2>R 2R@ 2R>
│   ├── double_words.c             # 2DUP 2DROP 2SWAP 2@ 2! D+ D-
│   ├── logical_words.c            # AND OR XOR NOT INVERT LSHIFT RSHIFT
│   ├── io_words.c                 # EMIT KEY TYPE CR TAB SPACE ACCEPT
│   ├── string_words.c             # S" SLITERAL string operations
│   ├── block_words.c              # BLOCK BUFFER LOAD THRU FLUSH
│   ├── format_words.c             # .( .R .S HEX DECIMAL BASE
│   ├── system_words.c             # BYE ABORT INCLUDE STATE
│   ├── dictionary_words.c         # FIND SEARCH-WORDLIST WORDS
│   ├── vocabulary_words.c         # VOCABULARY DEFINITIONS FORTH-WORDLIST
│   ├── q48_16_words.c             # Q48.16 fixed-point word definitions
│   ├── starforth_words.c          # StarForth-specific extensions
│   ├── physics_benchmark_words.c  # Benchmark harness (L1-L7)
│   ├── physics_diagnostic_words.c # Physics diagnostics (WORD-ENTROPY)
│   ├── physics_freeze_words.c     # PHYSICS-FREEZE / PHYSICS-THAW
│   └── physics_pipelining_diagnostic_words.c
│
├── test_runner/                   # 936+ test cases
│   ├── test_runner.c              # Test harness orchestration
│   ├── test_common.c              # Shared test utilities
│   ├── test_contracts.c           # Contract-based testing
│   └── modules/                   # 22 per-category test files
│       ├── arithmetic_words_test.c
│       ├── stack_words_test.c ... (22 files, incl. integration & stress)
│
├── platform/                      # Platform abstraction (hosted)
│   ├── linux/time.c               # POSIX timing
│   └── l4re/time.c                # L4Re timing
│
└── starkernel/                    # LithosAnanke bare-metal kernel (37 files)
    ├── kernel_main.c              # Kernel entry point (M0–M7 milestones)
    ├── repl.c                     # Kernel REPL
    ├── arch/amd64/                # AMD64: arch.c apic.c timer.c interrupts.c boot.S isr.S
    ├── boot/                      # uefi_loader.c elf_loader.c reloc_stub.c reloc.S
    ├── capsule/                   # capsule_birth.c capsule_run.c capsule_loader.c
    │                              #   capsule_find.c capsule_validate.c capsule_vm_hooks.c
    │                              #   mama_forth_words.c
    ├── hal/                       # hal.c console.c memory.c host_services.c
    ├── memory/                    # kmalloc.c pmm.c vmm.c
    ├── math/q48_16.c              # Q48.16 fixed-point (kernel build)
    ├── hash/xxhash64.c            # XXHash64 (content addressing)
    └── vm/                        # Kernel VM subsystem
        ├── bootstrap/sk_vm_bootstrap.c
        ├── host/shim.c
        ├── vm_core.c vm_runtime.c vm_bootstrap.c
        ├── parity.c               # Birth/execution parity logging
        ├── arena.c                # Capsule arena allocator
        └── alloc_kernel.c
```

### LithosAnanke / StarKernel

LithosAnanke ("stone inevitability") is the bare-metal UEFI kernel that boots
StarForth directly on hardware. Version **1.5.3**, monolithic UEFI PE32+ executable.

**Boot sequence:**
```
UEFI Firmware → uefi_loader.c (BOOTX64.EFI)
    ExitBootServices() → owns hardware
    BootInfo{memory map, ACPI, framebuffer}
    → kernel_main()
        M1: Console init (UART 16550 + framebuffer)
        M2: PMM (physical memory manager, bitmap)
        M3: VMM (4-level x86_64 paging)
        M4: IDT + APIC interrupts
        M5: TSC + HPET + APIC timer (100 Hz heartbeat)
        M6: kmalloc heap
        M7: StarForth VM bootstrap + capsule loading → "ok" REPL
```

**Milestone status (as of v1.5.3):**
- ✅ M0–M5: complete (verified with QEMU/OVMF, three-arch tested)
- ✅ M6: kmalloc infrastructure present (full validation deferred)
- 🔄 M7: VM integration in progress (capsule execution pipeline partially wired)

**Kernel memory layout:**
- Kernel heap: 16 MB (`kmalloc`)
- Block RAM (LBN 0–991): 1 MB dedicated RAM blocks
- Kernel ramdrive (LBN 2048–3071): 1 MB for capsule loading
- LBN 2048 = entry point for `init.4th`

**LinkerScripts in `linker/`:**
- `starkernel-loader-amd64.ld` — UEFI loader
- `starkernel-loader-amd64-pe.ld` — PE32+ format
- `starkernel-kernel-amd64.ld` — ELF kernel (split build)
- `starkernel-amd64.ld` — monolithic build

### Capsule System

Capsules are immutable, content-addressed VM initialization payloads. A capsule
ID is its XXHash64 content hash — any mutation is detectable.

**Capsule types:**
- `(m) MAMA_INIT` — exactly one Mama VM initialization capsule
- `(p) PRODUCTION` — truth-bearing baby VM initializers
- `(e) EXPERIMENT` — DoE workload-only initializers

**Birth protocol:** locate by name → validate hash → allocate VM ID →
execute IDENTITY (capsule code) → execute PERSONALITY (block 1 from ramdrive)
→ log parity record (VM ID + capsule hash + dict hash).

**Capsule files in `capsules/` (17 `.4th` files):**
- `init.4th` — default Mama VM personality
- `init-0.4th` through `init-9.4th` — numbered variants
- `init-l8-{stable,volatile,diverse,temporal,transition,omni}.4th` — L8 Jacquard variants

Tool `tools/mkcapsule.c` assembles `.4th` files into the binary capsule directory
format (`capsule_generated.c`) baked into the kernel image.

**ACL capsules (implemented):**
- `capsules/ACL.4th` — word-level ACL system; self-activating; contains CA root placeholder
- `capsules/zuse.4th` — bootstrap superuser; loaded by `ACL.4th` at boot

**MANDATORY: Read before writing capsules.**
Before writing or modifying any `.4th` capsule file, read `experiments/bare_metal/README.md`
in full. The block namespace is shared across all loaded capsules; violations cause silent
word-definition collisions and corrupt the DoE. Block ranges are:
- `2048–2099` — `init.4th` only
- `2100–2199` — `doe.4th` only
- `3000–3999` — workload capsules
- `4000+` — user-defined capsules (ACL.4th, zuse.4th, etc.)
Each block header line counts against the 1024-byte limit. Any block exceeding 1024 bytes
is truncated silently at load time — verify with `wc -c` before committing.

### Physics-Driven Adaptive Runtime

Uses thermodynamic metaphors as modeling language (see `ONTOLOGY.md`):

1. **Loop #1 — Execution Heat** (`dictionary_heat_optimization.c`) — frequency counter per word
2. **Loop #2 — Rolling Window** (`rolling_window_of_truth.c`) — circular buffer, execution history
3. **Loop #3 — Linear Decay** — quiescent words lose heat over time
4. **Loop #4 — Pipelining** (`physics_pipelining_metrics.c`) — word-to-word transition prediction
5. **Loop #5 — Window Width Inference** (`inference_engine.c`) — Levene's test, binary chop
6. **Loop #6 — Decay Slope Inference** (`inference_engine.c`) — exponential regression
7. **Loop #7 — Adaptive Heartrate** (`HeartbeatState`) — background tick coordinator

**L8 Jacquard Mode Selector** (`ssm_jacquard.c`) — additional steady-state machine layer
that switches between mode configurations based on attractor bucket statistics. Modes:
stable, volatile, diverse, temporal, transition, omni. Controlled by `vm->ssm_l8_state`.

All loops are independently togglable via Makefile build flags.

### Key Data Structures (`include/vm.h`)

- **`VM` struct** — entire VM state: stacks, dictionary, memory, physics, heartbeat, SSM
- **`DictEntry`** — `execution_heat`, `physics` (DictPhysics), `transition_metrics`, `word_id`, `acl_default`
- **`DictPhysics`** — `temperature_q8`, `last_active_ns`, `mass_bytes`, `avg_latency_ns`, `acl_hint`
- **`RollingWindowOfTruth`** — circular buffer, double-buffered snapshots, adaptive sizing
- **`HeartbeatState`** — tick coordinator, DoE observation counters, L8 bucket stats, M5 time trust
- **`HeartbeatTickSnapshot`** — per-tick: cache hits, heat, window width, jitter, L8 mode
- **`PipelineGlobalMetrics`** — prefetch accuracy, binary-chop window tuning state
- **`CapsuleDesc`** — 64-byte cache-aligned capsule descriptor with content hash

### Memory Model

- `vaddr_t` — VM addresses are byte offsets, not C pointers
- `vm_load_cell()` / `vm_store_cell()` — canonical memory accessors
- `VM_ADDR(cell)` / `CELL(vaddr)` — explicit stack↔offset conversions
- Dictionary occupies first 2MB (`DICTIONARY_BLOCKS=2048`), user blocks start at 2048
- Total VM memory: 5MB (`VM_MEMORY_SIZE`)
- Log blocks: 3072–5120 (2MB for persistent log, 32768 max lines at 64 bytes/line)

---

## Testing

Tests are organized in POST (Power-On Self Test) order:

1. **Unit tests:** Q48.16 fixed-point, inference statistics, decay slope inference
2. **Dictionary tests:** FORTH-79 word validation across 22 categories (not 18)
3. **Integration tests, stress tests, adversarial/fuzzing tests** (break_me_tests.c)

```bash
make test                     # Run all 936+ tests
make test PROFILE=1           # With basic profiling
make test FAIL_FAST=1         # Stop on first failure
```

Test files: `src/test_runner/modules/` — 22 `*_test.c` files (including
`mama_forth_words_test.c`, `integration_tests.c`, `stress_tests.c`, `break_me_tests.c`).

Kernel baseline: `QEMU_BASELINE.log` captures reference QEMU/OVMF output for regression detection.

---

## Acceptance Tests — LithosAnanke Three-Arch

The acceptance gate for LithosAnanke (Phase 7 and any kernel-touching change) is a
clean QEMU boot on all three supported architectures. These run **serially** — only
one QEMU instance at a time. `clean` is mandatory before each arch to prevent stale
cross-arch objects from leaking.

```bash
make -f Makefile.starkernel ARCH=amd64    clean qemu
make -f Makefile.starkernel ARCH=aarch64  clean qemu
make -f Makefile.starkernel ARCH=riscv64  clean qemu
```

**Pass criteria — each arch must:**
- Boot to the `ok>` REPL without fault or hang
- Print the expected milestone banner (M0–M7 sequence)
- Exit QEMU cleanly (no panic, no triple fault)

**Round 1 constraints:**
- **No DoE execution** — `--doe` flag must not be set
- **No ACL activated** — `init.4th` must have `S" ACL.4th" EXEC` commented out
- **Serial only** — never run two QEMU instances in parallel; they share OVMF state
- `clean` before each arch — cross-arch object contamination causes silent miscompiles

**Round 1 pass criteria — each arch must:**
- Boot to the bare `ok>` REPL without fault or hang
- Print the expected milestone banner (M0–M7 sequence)
- Exit QEMU cleanly (no panic, no triple fault)

### Round 2 — ACL Activated, No DoE

Once Round 1 passes on all three arches, repeat with ACL enabled:

```bash
make -f Makefile.starkernel ARCH=amd64    clean qemu
make -f Makefile.starkernel ARCH=aarch64  clean qemu
make -f Makefile.starkernel ARCH=riscv64  clean qemu
```

Enable ACL by uncommenting `S" ACL.4th" EXEC` in `init.4th` before building.

**Round 2 constraints:**
- **No DoE execution** — `--doe` flag must not be set
- **ACL activated** — `S" ACL.4th" EXEC` uncommented in `init.4th`
- Same serial / clean rules apply

**Round 2 pass criteria — each arch must:**
- Boot through ACL self-activation without fault
- Present the `zuse)ok>` prompt (or `ok>` if zuse boot is deferred)
- ACL enforcement active — unprivileged words blocked as configured
- Exit QEMU cleanly

**Reference output:** `QEMU_BASELINE.log`

---

## Formal Verification

The `proof/` directory contains 19 Isabelle/HOL theory files providing
machine-checkable proofs of determinism and correctness:

```
proof/
├── ROOT                           # Isabelle project manifest
├── StarForth_Base.thy             # Base definitions and type system
├── StarForth_Loop1_Heat.thy       # Execution heat tracking
├── StarForth_Loop2_Window.thy     # Rolling window
├── StarForth_Loop3_Decay.thy      # Linear decay
├── StarForth_Loop4_Pipeline.thy   # Pipelining metrics
├── StarForth_Loop5_WinInf.thy     # Window width inference
├── StarForth_Loop6_DecayInf.thy   # Decay slope inference
├── StarForth_Loop7_Heartrate.thy  # Adaptive heartrate
├── StarForth_Arithmetic_Words.thy
├── StarForth_Stack_Words.thy
├── StarForth_Logical_Words.thy
├── StarForth_Memory_Words.thy
├── StarForth_Return_Stack_Words.thy
├── StarForth_Q48_16.thy           # Q48.16 fixed-point
├── StarForth_Correctness.thy      # Overall correctness
├── StarForth_Concurrent.thy       # Concurrency properties
├── StarForth_Transition.thy       # State transitions
└── StarForth_Mutex.thy            # Mutual exclusion
```

To check proofs: `isabelle build -D proof/` (requires Isabelle installation,
see `docs/01-getting-started/DEVELOPER.md`).

ACL proofs complete (Phase 6): `ACL_Pin_Monotone.thy`, `ACL_Inherit_Clears_Pin.thy`,
`ACL_TTL_Bounded.thy`, `ACL_Emergency_Bypass.thy`, `ACL_No_Escalation.thy`. Total: 24 theory files.

---

## Roadmap

Full roadmap: `ROADMAP.md`. Summary:

```
2025 (done)     2026             2027             2028
StarForth  →  StarKernel   →  StarshipOS   →   FPGA Hardware
   VM        (bare metal)   (self-hosting)     (custom silicon)
  DONE       M0-M6 done      storage,net,       feasibility
             M7 in progress  multitask,         study
                             self-compile
```

**LithosAnanke roadmap:**
- `v1.0.x` — serial-only production (current)
- `v1.5.0` — framebuffer VT100 terminal milestone
- `v2.0.0` — StarForth SDK release

---

## Code Standards

- **Strict ANSI C99** — No GNU extensions, no C++ features
- **Zero warnings** — Build with `-Wall -Werror`
- **No hidden state** — All VM state is explicit in the `VM` struct
- **Platform-agnostic** — Kernel code gated by `__STARKERNEL__` and `STARFORTH_ENABLE_VM`
- **Content-addressed immutability** — Capsule ID = content hash; any mutation is detectable

---

## Important Conventions

- Stack values are VM offsets (`vaddr_t`), not C pointers — use `VM_ADDR()` / `CELL()`
- `WORD_IMMEDIATE` flag = executes during compilation (not deferred)
- `WORD_PINNED` = execution heat cannot decay to zero
- `WORD_FROZEN` = execution heat does not decay at all
- `STRICT_PTR=1` enforces bounds checking (disable only for benchmarking)
- Kernel code uses `#ifdef __STARKERNEL__`; VM-enabled path uses `#ifdef STARFORTH_ENABLE_VM`
- Never add `acl_*` fields to `DictEntry` beyond the four already planned (see ACL design doc)

---

## Critical Implementation Details

### DoE CSV Output Suppression

Intentionally suppressed as of 2025-12-08. See `src/main.c:390-396`:
- Metrics still collected via `metrics_from_vm()`
- CSV row was redundant with internal VM metrics
- To re-enable: add `--csv-export` flag or write to file

### Heartbeat Instrumentation (Planned)

`HeartbeatTickSnapshot` and `tick_buffer` are declared in `include/vm.h`,
but `heartbeat_export_csv()` is **not yet implemented**.
See `docs/03-architecture/heartbeat-system/instrumentation-plan.md`.

### Word Statistics Output

`WORD-ENTROPY` prints execution heat statistics to stdout. Kept enabled in DoE mode
intentionally (diagnostic value outweighs noise cost).

### StarKernel M7 Parity

`parity.c` logs every VM birth and execution with: VM ID, capsule hash, dictionary hash.
This enables offline determinism verification — an independent system can load the same
capsule and compare dict hashes. Zero-deviation means 0% algorithmic variance.

---

## Documentation

```bash
make book         # LaTeX → PDF (gold standard)
make book-html    # HTML single-page + multi-page with dark.css
```

Key files:
- `README.md` — project overview and quick start
- `ROADMAP.md` — full VM→Kernel→OS→FPGA roadmap
- `ONTOLOGY.md` — formal taxonomy: thermodynamic metaphors, literal implementations, lexicon
- `docs/01-getting-started/DEVELOPER.md` — dev environment, Isabelle setup, CI/CD
- `docs/03-architecture/OVERVIEW.md` — complete architecture overview
- `docs/03-architecture/physics-engine/feedback-loops.md` — all 7 loops detailed
- `docs/03-architecture/hal/` — HAL architecture docs (6 files)
- `docs/03-architecture/heartbeat-system/` — heartbeat architecture, planned instrumentation
- `docs/03-architecture/word-acl/DESIGN.md` — ACL system design + implementation punch list
- `docs/02-experiments/` — DoE guides (factorial, heartbeat, physics-optimization)
- `papers/James_Steady-State_Convergence_Adaptive_Runtime.pdf` — published SSRN paper
- `sbom.spdx` / `sbom.spdx.json` — Software Bill of Materials
- `QEMU_BASELINE.log` — QEMU/OVMF reference output for kernel regression testing

---

## License

See `./LICENSE` (Starship License 1.0). Commercial license available.
