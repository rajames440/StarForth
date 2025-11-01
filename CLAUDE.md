# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## 🚨 CURRENT SESSION STATUS (2025-10-31 Evening)

**WHERE WE ARE:**

- Repository in DEGRADED but RECOVERABLE state
- Git integrity: CLEAN (251 commits, no corruption)
- Critical issue: Missing submodule config (.gitmodules removed)
- Data loss risk: 8 controlled documents in backup (backed up, safe)

**WHAT'S HAPPENING:**
Captain Bob is designing a comprehensive **Workflow & Governance Plan** to establish:

- Formal roles (PROJ_MGR, QUAL_MGR, ENG_MGR)
- Version scheme (X.y.z with timestamps)
- Dual-stamp pattern (human approval + CI/CD execution)
- Document injection workflows (validated → StarForth-Governance/in_basket)

**FILES TO REVIEW:**

1. `/home/rajames/CLionProjects/RECOVERY_SESSION_2025-10-31.md` - Recovery context
2. `/home/rajames/CLionProjects/StarForth/WORKFLOW_GOVERNANCE_PLAN_DRAFT_2025-10-31.md` - Governance plan (UNREGULATED
   DRAFT)

**NEXT MORNING TASKS:**

1. Read this section + the two files above
2. Captain Bob will provide decisions on 14 gap analysis sections (see WORKFLOW_GOVERNANCE_PLAN_DRAFT_2025-10-31.md PART
   2)
3. Once gaps filled: design workflow templates and integration
4. Implement in Makefile, Jenkins, GitHub

**KEY DECISION MADE:**

- Simplified model: Two separate repos (no complex read-only submodule)
- Document flow: Validated workflows → directly inject to StarForth-Governance/in_basket
- Humans ONLY interact through validated workflows (never direct vault edits)
- Each item type (CAPA, Feature, etc.) gets own workflow definition

**BACKUP LOCATION:**
`/home/rajames/CLionProjects/in_basket_backup_1761947819.tar.gz` (verified, 88K, all data safe)

---

## Quick Start Commands

### Build
```bash
make                    # Standard optimized build (default)
make fastest           # Maximum performance (ASM + direct threading + LTO)
make fast              # Fast without LTO (easier debugging)
make pgo               # Profile-guided optimization (6-stage build)
```

### Test
```bash
make test              # Full test suite (936+ tests, fail-fast)
make smoke             # Quick sanity check (1 2 + .)
make benchmark         # Performance benchmarks
```

### Development
```bash
make clean             # Remove build artifacts
make install PREFIX=/usr/local  # Install binaries
make docs              # Generate all documentation
make book              # Generate PDF manual
make api-docs          # Generate Doxygen API docs (AsciiDoc format)
```

### Run
```bash
./build/starforth                    # Interactive REPL
./build/starforth -c "1 2 + . BYE"  # Command execution
make rpi4              # Build for Raspberry Pi 4
make rpi4-cross        # Cross-compile for ARM64
```

### Single Test Module
```bash
# Run specific test module by inspecting src/test_runner/modules/
# Tests are numbered and module-organized in the fail-fast harness
make test FILTER=arithmetic  # If supported, or see test runner code
```

---

## Architecture & Design

### High-Level Overview

StarForth is a **production-ready FORTH-79 virtual machine** written in strict ANSI C99. It features:

- **Direct-threaded interpreter** with dual-stack execution (data + return stack)
- **Dictionary-based word registry** organized across 17+ C modules
- **Block storage subsystem** supporting RAM-disk and disk-backed operation
- **Modular word definitions** separated by category (arithmetic, stack, control, etc.)
- **Comprehensive test harness** with fail-fast execution model (936+ tests, 18 modules)
- **Formal verification** via Isabelle/HOL machine-checked proofs (future phases)

### Execution Flow

1. **Outer Interpreter** (`src/vm.c: vm_interpret()`) parses whitespace-delimited words
2. **Dictionary Lookup** indexes by first character, finds matching word entry
3. **Word Dispatch**: If IMMEDIATE → execute in compile mode; else check STATE
4. **Inner Interpreter** (ITC model) handles colon definitions via return stack frames
5. **VM Execution Loop** continues until BYE or error

### Memory Layout

```
Total VM Memory: 5 MB (DICTIONARY_MEMORY_SIZE = 2097152 bytes)
- Dictionary blocks: 0-2047 (2 MB for word definitions)
- User blocks: 2048-5119 (3 MB for block storage)
- Each block: 1024 bytes (BLOCK_SIZE = 1024)
- Data stack: 1024 cells
- Return stack: 1024 cells
```

### Core VM Files

| File | Purpose |
|------|---------|
| `src/vm.c` | Main interpreter, execution loop, bootstrap |
| `src/main.c` | Entry point, CLI parsing, REPL integration |
| `src/block_subsystem.c` | Logical→physical block mapping, storage management |
| `src/memory_management.c` | Dictionary allocator, heap management |
| `src/stack_management.c` | Data and return stack operations |
| `include/vm.h` | Core VM structure, configuration constants |
| `include/vm_api.h` | Public C embedding API |

### Word Implementation Modules (17 total)

Located in `src/word_source/`:

- `arithmetic_words.c` - +, -, *, /, MOD, ABS, MIN, MAX, etc.
- `stack_words.c` - DUP, DROP, SWAP, OVER, ROT, >R, R>, R@, etc.
- `control_words.c` - IF/THEN/ELSE, DO/LOOP, RECURSE, BEGIN/UNTIL, etc.
- `defining_words.c` - : (colon definitions), CREATE, VARIABLE, CONSTANT
- `dictionary_words.c` - FIND, WORDS, FORGET
- `memory_words.c` - @ (fetch), ! (store), C@ (char fetch), C! (char store)
- `io_words.c` - . (print), CR, EMIT, KEY, etc.
- `block_words.c` - BLOCK, BUFFER, UPDATE, SAVE-BUFFERS, etc.
- `double_words.c` - Double-precision arithmetic (D+, D-, D*, D/)
- `logical_words.c` - AND, OR, XOR, NOT, shift operations
- `format_words.c` - Base conversion, number formatting
- `editor_words.c` - Block editor words (LIST, EDIT)
- `return_stack_words.c` - Return stack explicit access
- `mixed_arithmetic_words.c` - Mixed-precision operations
- `starforth_words.c` - StarForth-specific extensions
- Plus string and system word modules

### Test Infrastructure

**Location:** `src/test_runner/`

**Structure:**
- Fail-fast harness in `test_runner.c` - stops on first failure
- 22 test modules in `src/test_runner/modules/`
- 936+ test cases covering all word categories
- Adversarial "break_me" tests for stress validation

**Key Categories:**
- Arithmetic operations
- Stack manipulation
- Control flow (IF/THEN, DO/LOOP)
- Dictionary operations and word definitions
- Block I/O and storage
- String handling
- System integration

### Build System

**Makefile: 40+ targets** organized by purpose:

- **Standard builds**: `all`, `fast`, `fastest`, `turbo`
- **Optimization builds**: `pgo` (6-stage profile-guided), `pgo-perf`, `pgo-valgrind`
- **Platform-specific**: `rpi4`, `rpi4-cross`, `rpi4-fastest`, `minimal`, `fake-l4re`
- **Testing**: `test`, `smoke`, `bench`, `benchmark`, `bench-compare`
- **Documentation**: `docs`, `api-docs`, `docs-latex`, `docs-isabelle`, `book`, `book-html`
- **Profiling**: `profile`, `asm` (show assembly)

**Key Compiler Flags:**
```makefile
CFLAGS = -std=c99 -Wall -Werror          # Strict C99, all warnings
CFLAGS += -fomit-frame-pointer           # Performance
CFLAGS += -march=native                  # Architecture optimization (x86/ARM)
STRICT_PTR = 1                           # Pointer safety enforcement
USE_ASM_OPT = 1                          # Assembly optimizations
```

**Architecture Detection:**
- Auto-detects x86_64, ARM64, generic
- Conditional compilation for platform-specific code
- L4Re microkernel bindings (optional, in `src/platform/l4re/`)

### CI/CD Pipeline

**Jenkinsfiles:**
- `/Jenkinsfile` - Main torture-test pipeline (23 stages)
  - Builds 6 configurations (debug, standard, fastest, fast, RPi4)
  - Runs smoke tests, stress tests, benchmarks
  - Collects artifacts and reports
- `jenkinsfiles/test/` - Strict test validation
- `jenkinsfiles/qual/` - Quality assurance (compliance)
- `jenkinsfiles/prod/` - Production release pipeline

**GitHub Actions:**
- `claude.yml` - Claude Code integration for AI-assisted development
- `claude-code-review.yml` - Automated code review
- `makefile.yml` - Basic validation

### Formal Verification

**Location:** `docs/src/internal/formal/`

**Isabelle/HOL theories** (machine-checked proofs):
- `VM_Core.thy` - Core VM semantics
- `VM_Stacks.thy` - Stack operations (data & return)
- `VM_Words.thy` - Word definitions
- `Physics_StateMachine.thy` - Runtime state machine (observability)
- Additional theories for refinement tracking

**Status:** 5 phases of refinement planned; currently in early phases

**Documentation:**
- `docs/REFINEMENT_CAPA.adoc` - Defect tracking for C ⊑ Isabelle refinement
- `docs/REFINEMENT_ROADMAP.adoc` - Formal verification phases

---

## Development Workflow

### Code Style & Standards

- **Language:** Strict ANSI C99 (no C11+ features)
- **Compilation:** `-std=c99 -Wall -Werror` (zero warnings)
- **Naming:** Clear, descriptive identifiers following FORTH conventions
- **Error Handling:** Explicit error flags (e.g., `error_flag` in vm.h)
- **Comments:** Document non-obvious logic; assume readers know FORTH

### Adding a New Word

1. **Choose the appropriate module** in `src/word_source/` based on word category
2. **Implement the C function:**
   ```c
   void forth_MYWORD(VM *vm) {
       if (vm->data_stack_ptr < 1) {
           vm->error_flag = 1;
           return;
       }
       Cell n = vm->data_stack[--vm->data_stack_ptr];
       // Process n...
       vm->data_stack[vm->data_stack_ptr++] = result;
   }
   ```
3. **Register in word registry** (`include/word_registry.h`):
   ```c
   REGISTER_WORD("MYWORD", forth_MYWORD),
   ```
4. **Add tests** in `src/test_runner/modules/` corresponding to the module
5. **Test:** `make test` (ensure fail-fast harness passes)
6. **Update docs** if new functionality warrants documentation

### Running a Single Test Module

Tests are organized in `src/test_runner/modules/`. To run the full suite:
```bash
make test
```

To examine a specific module (e.g., arithmetic):
```bash
# Inspect tests in src/test_runner/modules/test_arithmetic_words.c
# Then run full suite - fail-fast harness stops on first error
make test
```

### Debugging

1. **Build with debug symbols:**
   ```bash
   make clean && make CFLAGS="-std=c99 -Wall -Werror -O0 -g"
   ```
2. **Run with GDB:**
   ```bash
   gdb ./build/starforth
   ```
3. **Run with Valgrind (memory check):**
   ```bash
   valgrind --leak-check=full ./build/starforth
   ```
4. **Check stack traces via profiler:** `make pgo-valgrind`

### Performance Profiling

**Quick benchmark:**
```bash
make benchmark
```

**Profile-guided optimization:**
```bash
make pgo           # Builds with profiling, then optimizes
make pgo-perf      # With perf analysis
make pgo-valgrind  # With Valgrind callgrind profiling
```

**Compare regular vs. PGO:**
```bash
make bench-compare
```

### Assembly Inspection

To see generated assembly for optimization verification:
```bash
make asm
# Check build/*.s files
```

---

## Key Configuration Constants

Located in `include/vm.h`:

```c
#define STACK_SIZE              1024       /* Cells per stack */
#define DICTIONARY_SIZE         1024       /* Dictionary entries */
#define VM_MEMORY_SIZE          (5 * 1024 * 1024)  /* 5 MB total */
#define BLOCK_SIZE              1024       /* Bytes per block */
#define MAX_BLOCKS              5120       /* 5 MB / 1 KB blocks */
#define DICTIONARY_MEMORY_SIZE  2097152    /* 2 MB for dictionary */
#define USER_BLOCKS_START       2048       /* After dictionary blocks */
```

Modify these before building to adjust VM capacity.

---

## Important File Locations

| Purpose | Path |
|---------|------|
| Main VM | `src/vm.c`, `include/vm.h` |
| Entry point | `src/main.c` |
| Word registry | `include/word_registry.h` |
| Test harness | `src/test_runner/test_runner.c` |
| Test modules | `src/test_runner/modules/` |
| Word implementations | `src/word_source/` |
| Block system | `src/block_subsystem.c`, `include/block_subsystem.h` |
| Memory management | `src/memory_management.c` |
| Platform layer | `src/platform/linux/`, `src/platform/l4re/` |
| Documentation | `docs/` (AsciiDoc sources) |
| Formal proofs | `docs/src/internal/formal/` (Isabelle/HOL) |
| Governance | `StarForth-Governance/` (read-only submodule) |

---

## Common Pitfalls & Tips

1. **Stack underflow/overflow:** The VM checks `data_stack_ptr` before operations. If operations fail silently, check `vm->error_flag` after execution.

2. **Colon definitions:** These create new dictionary entries. The block system tracks colon definition headers; inspect `block_subsystem.c` for understanding how dictionary growth is managed.

3. **Return stack:** Used by the ITC model for nested word calls. Don't manipulate directly in word implementations unless you understand the frame structure.

4. **Block I/O:** The `blkio_factory.c` pattern allows pluggable backends (file, RAM-disk, etc.). Check `blkio.h` for the interface.

5. **Pointer discipline:** Enable `STRICT_PTR=1` during development to catch address calculation errors.

6. **Cross-compilation:** Set `CC` and `CFLAGS` rather than editing Makefile; e.g., `make CC=arm-linux-gnueabihf-gcc CFLAGS="$(BASE_CFLAGS) -DARCH_ARM64=1"`.

7. **Assembly optimizations:** Optional. Disable with `USE_ASM_OPT=0` if your toolchain doesn't support inline assembly.

---

## References & Documentation

- **README.md** - Project overview, quick start
- **CONTRIBUTE.md** - Contributing guidelines
- **docs/DEVELOPER.md** - Setup and CI/CD details
- **docs/WORKFLOW.md** - Development workflow
- **docs/src/getting-started/ARCHITECTURE.adoc** - Deep architecture dive
- **docs/src/architecture-internals/** - Block storage, word development, ABI
- **docs/src/testing-quality/TESTING.adoc** - Test strategy
- **docs/src/testing-quality/GAP_ANALYSIS.adoc** - Test coverage gaps
- **.README_ADDENDUM.md** - Comprehensive codebase review (9.2/10 rating)
- **Formal verification:** `docs/src/internal/formal/` (Isabelle/HOL proofs)

---

## Testing & Validation

- **Test suite:** 936+ tests organized in 18 modules with fail-fast execution
- **Smoke test:** `make smoke` for sub-second sanity check
- **Full suite:** `make test` for comprehensive validation
- **FORTH-79 compliance:** 100% of core 70 words implemented and tested
- **Zero compiler warnings:** Enforced via `-Wall -Werror`

---

## Governance

StarForth governance is maintained in a **read-only Git submodule:**
- **Location:** `StarForth-Governance/` (cannot be modified locally)
- **Purpose:** Ensures compliance policies and standards are maintainer-controlled
- **For governance changes:** Submit GitHub issues; only maintainers can update policies

---

## Performance Characteristics

- **Direct-threaded interpreter** for fast word dispatch
- **Optional x86-64 & ARM64 assembly optimizations** (enabled by default)
- **Link-time optimization (LTO)** support for whole-program optimization
- **Profile-guided optimization (PGO)** for real-world workload optimization
- **Inline assembly** for critical paths (`vm_inner_interp_asm.h`, `vm_inner_interp_arm64.h`)

---

## Embedding StarForth in Your Project

The `vm_api.h` header provides a clean C API for embedding:

```c
#include "vm.h"
#include "vm_api.h"

VM vm;
vm_init(&vm, NULL);  // Initialize with default block backend
vm_interpret(&vm, "1 2 + . BYE");
vm_cleanup(&vm);
```

Refer to `src/main.c` for full integration example.

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Build fails with `-march=native` on older machines | Use `make CFLAGS="-std=c99 -Wall -Werror -O2"` |
| Link errors on non-GNU systems | Disable LTO: `make USE_LTO=0` |
| Assembly errors on unsupported architecture | Disable ASM opts: `make USE_ASM_OPT=0` |
| Tests fail with pointer errors | Ensure `STRICT_PTR=1` during development; check `vm->error_flag` |
| Block storage exhausted | Increase `MAX_BLOCKS` or `DICTIONARY_MEMORY_SIZE` in `vm.h` |
| REPL doesn't respond | Check stdin; run with explicit command: `./build/starforth -c "1 . BYE"` |

---

**Last Updated:** October 2025
**Maintained by:** Robert A. James (Captain Bob)
**License:** CC0 / Public Domain