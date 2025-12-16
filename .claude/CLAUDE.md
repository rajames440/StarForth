# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

StarForth is a FORTH-79 compliant virtual machine written in strict ANSI C99, featuring a physics-driven adaptive runtime. It serves as the primary userland execution engine for StarshipOS and can run standalone on Linux, L4Re/Fiasco.OC, and bare-metal targets.

**Key distinguishing feature:** Physics-grounded self-adaptive runtime with formally proven deterministic behavior (0% algorithmic variance across 90 experimental runs).

## Build Commands

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

# Quick smoke test (verifies basic operation)
make smoke

# Quick benchmark
make bench

# Clean build artifacts
make clean
```

### Build Targets and Architectures

- `TARGET=standard|fast|fastest|turbo|pgo` - Build profiles
- `ARCH=x86_64|amd64|aarch64|arm64|raspi|riscv64` - Target architecture
- Cross-compile for Raspberry Pi: `make rpi4-cross`

### Key Build Flags

- `STRICT_PTR=1` - Enforce pointer safety checks (default on)
- `USE_ASM_OPT=1` - Enable architecture-specific assembler optimizations
- `ENABLE_HOTWORDS_CACHE=1` - Physics-driven hot-words cache (default on)
- `ENABLE_PIPELINING=1` - Speculative execution via word transition prediction (default on)
- `HEARTBEAT_THREAD_ENABLED=1` - Background heartbeat thread for adaptive tuning (default on)

### Important: Linker Configuration

The `fastest` target uses `-flto=auto -fuse-linker-plugin` instead of plain `-flto` to avoid "ELF section name out of range" errors with large codebases. If you encounter linker errors with LTO, this is the correct fix.

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

**Important:** The `--doe` flag runs the full test harness and outputs test results to stdout. As of the latest changes, the CSV metrics row has been **suppressed** because it was redundant with internal VM metrics. If you need to re-enable CSV export, see `src/main.c:395` which has detailed comments explaining the rationale for suppression.

## Architecture

### Core Components

```
src/
├── main.c                    # Entry point, CLI parsing, VM initialization
├── vm.c                      # Core interpreter loop, stacks, dictionary state
├── vm_api.c                  # External VM API functions
├── repl.c                    # Read-eval-print loop
├── memory_management.c       # Dictionary allocator
├── block_subsystem.c         # Logical→physical block mapper
├── blkio_*.c                 # Block I/O backends (file, RAM)
│
├── word_source/              # Modular FORTH-79 word implementations
│   ├── arithmetic_words.c    # + - * / MOD etc.
│   ├── stack_words.c         # DUP DROP SWAP etc.
│   ├── control_words.c       # IF ELSE THEN DO LOOP etc.
│   ├── defining_words.c      # : ; CREATE DOES> VARIABLE CONSTANT
│   └── ...                   # 18+ word modules
│
├── test_runner/              # Test framework
│   ├── test_runner.c         # Test harness orchestration
│   └── modules/              # Per-category test files
│
└── platform/                 # Platform abstraction
    ├── linux/time.c          # POSIX timing
    └── l4re/time.c           # L4Re timing
```

### Physics-Driven Adaptive Runtime

The VM features a unique physics-grounded optimization system:

1. **Execution Heat Model** (`dictionary_heat_optimization.c`) - Tracks word execution frequency
2. **Rolling Window of Truth** (`rolling_window_of_truth.c`) - Circular buffer capturing execution history
3. **Hot-Words Cache** (`physics_hotwords_cache.c`) - Frequency-driven dictionary acceleration
4. **Pipelining Metrics** (`physics_pipelining_metrics.c`) - Word-to-word transition prediction
5. **Inference Engine** (`inference_engine.c`) - Adaptive tuning via ANOVA early-exit + window width + decay slope inference
6. **Heartbeat System** - Background thread coordinating Loop #3 (heat decay) and Loop #5 (window tuning)

### Key Data Structures

- **`VM` struct** (`include/vm.h`) - Main VM state: stacks, dictionary, memory, execution state
- **`DictEntry`** - Dictionary entry with `execution_heat`, `physics` metadata, and `transition_metrics`
- **`RollingWindowOfTruth`** - Execution history for deterministic metric seeding
- **`HeartbeatState`** - Centralized time-driven tuning dispatcher

### Memory Model

- `vaddr_t` - VM addresses are byte offsets, not C pointers
- `vm_load_cell()` / `vm_store_cell()` - Canonical memory accessors
- Dictionary occupies first 2MB, user blocks start at block 2048

## Testing

Tests are organized in POST (Power-On Self Test) order:

1. **Unit tests (POST):** Q48.16 fixed-point, inference statistics, decay slope inference
2. **Dictionary tests:** FORTH-79 word validation across 18 modules

```bash
make test                     # Run all 936+ tests
make test PROFILE=1           # With basic profiling
make test FAIL_FAST=1         # Stop on first failure
```

Test files are in `src/test_runner/modules/` - each `*_test.c` file tests one word category.

## Code Standards

- **Strict ANSI C99** - No GNU extensions, no C++ features
- **Zero warnings** - Build with `-Wall -Werror`
- **No hidden state** - All VM state is explicit in the `VM` struct
- **Platform-agnostic** - Abstract platform-specific code via `src/platform/`

## Important Conventions

- Stack values on data stack are VM offsets, not C pointers
- Use `VM_ADDR()` and `CELL()` macros for offset↔cell conversions
- Dictionary words with `WORD_IMMEDIATE` flag execute during compilation
- `STRICT_PTR=1` enforces bounds checking (disable only for benchmarking)

## Physics Feedback Loops

The adaptive runtime has 7 configurable feedback loops (toggled via Makefile):

- Loop #1: Execution Heat Tracking
- Loop #2: Rolling Window History
- Loop #3: Linear Decay
- Loop #4: Pipelining Metrics
- Loop #5: Window Width Inference (Levene's test)
- Loop #6: Decay Slope Inference (exponential regression)
- Loop #7: Adaptive Heartrate

See `docs/FEEDBACK_LOOPS.md` for detailed descriptions of each loop's positive/negative feedback mechanisms.

## Critical Implementation Details

### DoE CSV Output Suppression

The CSV output at the end of `--doe` runs has been **intentionally suppressed** as of 2025-12-08. See `src/main.c:390-396` for the rationale:
- Metrics are still collected via `metrics_from_vm()`
- The CSV row was redundant with internal VM metrics
- Test harness output is now clean (no trailing CSV line)
- If CSV export is needed, consider writing to a file instead of stdout or adding a `--csv-export` flag

### Heartbeat Instrumentation (Planned)

The VM has data structures for heartbeat per-tick metrics (`HeartbeatTickSnapshot`, `tick_buffer`) declared in `include/vm.h`, but the export function `heartbeat_export_csv()` is **not yet implemented**. This is a planned feature documented in `docs/HEARTBEAT_INSTRUMENTATION_PLAN.md`.

### Word Statistics Output

The `WORD-ENTROPY` Forth word prints execution heat statistics to stdout. This output is intentionally **kept enabled** even in DoE mode because it provides valuable diagnostic information about VM behavior during test runs.

## Documentation

Comprehensive documentation is available in multiple formats:

```bash
make book         # src → PDF (gold standard)
make book-html    # HTML single-page + multi-page with dark.css
```

Key documentation files:
- `README.md` - Project overview and quick start
- `docs/DEVELOPER.md` - Development environment setup
- `docs/FEEDBACK_LOOPS.md` - Physics feedback loop details
- `docs/HEARTBEAT_INSTRUMENTATION_PLAN.md` - Future instrumentation work
- `FINAL_REPORT/book.adoc` - 121-page peer-review ready academic paper

## License

See ./LICENSE