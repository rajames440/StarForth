# StarForth Architecture

**Version:** 1.0.0
**Last Updated:** October 2025
**Author:** Robert A. James

---

## Table of Contents

1. [Overview](#overview)
2. [Design Philosophy](#design-philosophy)
3. [Core Architecture](#core-architecture)
4. [Memory Management](#memory-management)
5. [Dictionary System](#dictionary-system)
6. [Execution Model](#execution-model)
7. [Optimization Strategies](#optimization-strategies)
8. [Module Organization](#module-organization)
9. [Platform Integration](#platform-integration)
10. [Performance Characteristics](#performance-characteristics)

---

## Overview

StarForth is a FORTH-79 standard implementation written in ANSI C99, designed for embedded systems, microkernel
environments (L4Re), and minimal OS targets. It features a clean, modular architecture with **no dependencies on libc
** (`malloc`, `printf`, or glibc), making it ideal for bare-metal and capability-based operating systems.

### Key Architectural Goals

- **Zero libc dependency**: Suitable for L4Re, StarshipOS, and bare-metal environments
- **Predictable memory usage**: Fixed 5MB memory arena, no dynamic allocation
- **High performance**: Inline assembly optimizations for critical operations
- **FORTH-79 compliance**: Full standard implementation with extensions
- **Modularity**: Clean separation of concerns across 19 word modules
- **Safety**: Comprehensive bounds checking and error handling

---

## Design Philosophy

### 1. Virtual Address Model

StarForth uses a **virtual address model** where all addresses on the data stack are VM offsets, not C pointers. This
provides:

- **Safety**: Bounds checking on all memory operations
- **Portability**: No pointer arithmetic on host machine addresses
- **L4Re compatibility**: Maps cleanly to capability-based addressing

```c
typedef uint64_t vaddr_t;  // VM offset (byte address into VM memory)
typedef signed long cell_t; // 64-bit cell for stack operations

// Conversion functions
static inline vaddr_t VM_ADDR(cell_t c) { return (vaddr_t)(uint64_t)c; }
static inline cell_t CELL(vaddr_t a) { return (cell_t)(int64_t)a; }
```

### 2. Fixed Memory Arena

All memory is pre-allocated in a single 5MB buffer:

```c
#define VM_MEMORY_SIZE (5 * 1024 * 1024)  // 5 MB total
uint8_t *memory;  // Unified VM memory buffer
```

This eliminates:

- Dynamic allocation overhead
- Memory fragmentation
- Need for `malloc`/`free`

### 3. Modular Word Organization

Words are organized into 19 semantic modules, each in its own source file:

| Module                            | Purpose                                        |
|-----------------------------------|------------------------------------------------|
| `arithmetic_words.c`              | `+` `-` `*` `/` `MOD` `*/` `*/MOD` etc.        |
| `stack_words.c`                   | `DUP` `DROP` `SWAP` `OVER` `ROT` etc.          |
| `control_words.c`                 | `IF` `ELSE` `THEN` `BEGIN` `UNTIL` `DO` `LOOP` |
| `defining_words.c`                | `:` `;` `CREATE` `DOES>` `CONSTANT` `VARIABLE` |
| `memory_words.c`                  | `@` `!` `C@` `C!` `+!` `ALLOT` `HERE`          |
| `io_words.c`                      | `EMIT` `TYPE` `CR` `SPACE` `KEY` `.`           |
| `logical_words.c`                 | `AND` `OR` `XOR` `NOT` `LSHIFT` `RSHIFT`       |
| `return_stack_words.c`            | `>R` `R>` `R@` `RDROP`                         |
| `dictionary_words.c`              | `FIND` `'` `EXECUTE` `>BODY` `IMMEDIATE`       |
| `block_words.c`                   | `BLOCK` `UPDATE` `FLUSH` `LIST` `LOAD`         |
| `vocabulary_words.c`              | `VOCABULARY` `DEFINITIONS` `CONTEXT` `CURRENT` |
| `string_words.c`                  | String manipulation and parsing                |
| `double_words.c`                  | Double-precision arithmetic                    |
| `mixed_arithmetic_words.c`        | Mixed single/double arithmetic                 |
| `format_words.c`                  | Number formatting and output                   |
| `editor_words.c`                  | Block editor commands                          |
| `dictionary_manipulation_words.c` | `FORGET` `SMUDGE` dictionary control           |
| `system_words.c`                  | `BYE` `ABORT` `QUIT` system control            |
| `starforth_words.c`               | StarForth-specific extensions                  |

---

## Core Architecture

### VM Structure

The core VM is defined in `vm.h`:

```c
typedef struct VM {
    /* Stacks */
    cell_t data_stack[STACK_SIZE];      // 1024 cells
    cell_t return_stack[STACK_SIZE];    // 1024 cells
    int dsp;                             // Data stack pointer
    int rsp;                             // Return stack pointer
    int exit_colon;                      // EXIT flag for colon definitions

    /* Memory */
    uint8_t *memory;                     // 5MB unified memory buffer
    size_t here;                         // Next free memory (byte offset)
    DictEntry *latest;                   // Most recent dictionary entry

    /* Dictionary Protection */
    DictEntry *dict_fence_latest;        // FORGET fence
    size_t dict_fence_here;              // Memory fence

    /* Input System */
    char input_buffer[INPUT_BUFFER_SIZE]; // 256 bytes
    size_t input_length;
    size_t input_pos;

    /* Compiler State */
    vm_mode_t mode;                      // INTERPRET or COMPILE
    DictEntry *compiling_word;           // Current word being compiled
    cell_t *compile_buffer;              // Compilation buffer
    size_t compile_pos;
    size_t compile_size;

    /* Execution */
    DictEntry *current_executing_entry;
    cell_t *ip;                          // Instruction pointer

    /* State */
    int error;
    int halted;
    cell_t base;                         // Numeric base (default 10)

    /* Control Flow Stack */
    struct {
        vaddr_t address;
        enum cf_type type;               // CF_BEGIN, CF_IF, CF_ELSE, etc.
    } cf_stack[STACK_SIZE];
    int cf_sp;                           // Control-flow stack pointer
    unsigned long cf_epoch;              // Prevents stale CF frames

    /* Loop Stack */
    struct {
        cell_t limit;
        cell_t index;
        cell_t *loop_start;
    } loop_stack[STACK_SIZE];
    int loop_sp;

    /* Leave Stack (for LEAVE compilation) */
    vaddr_t leave_stack[STACK_SIZE];
    int leave_sp;

    /* Block System */
    uint8_t *block_buffers[2];           // Double-buffered blocks
    int current_blk;
    int updated[2];

    /* Vocabulary System */
    DictEntry *vocabularies[8];          // Up to 8 vocabularies
    int current_vocab;
    int context_vocab;
} VM;
```

### Dictionary Entry Format

Each dictionary entry follows the FORTH-79 linked-list model:

```c
typedef struct DictEntry {
    struct DictEntry *link;  // Previous word (linked list)
    word_func_t func;        // Function pointer for execution
    uint8_t flags;           // IMMEDIATE, HIDDEN, SMUDGED, etc.
    uint8_t name_len;        // Length of name
    cell_t entropy;          // Usage counter (for optimization tracking)
    char name[];             // Variable-length name + optional code
} DictEntry;
```

**Word Flags:**

```c
#define WORD_IMMEDIATE  0x80  // Execute immediately in compile mode
#define WORD_HIDDEN     0x40  // Hidden from dictionary searches
#define WORD_SMUDGED    0x20  // Being defined (FORTH-79)
#define WORD_COMPILED   0x10  // User-compiled (not built-in)
#define WORD_PINNED     0x08  // Entropy pinned (cannot decay)
```

---

## Memory Management

### Memory Layout

StarForth uses a 5MB unified memory space divided into blocks:

```
┌──────────────────────────────────────────────────────────┐
│                   VM Memory (5 MB)                       │
├──────────────────────────────────────────────────────────┤
│  Dictionary Area (2 MB = 2048 blocks)                    │
│  - Built-in words                                        │
│  - User-defined words                                    │
│  - Compiled code                                         │
├──────────────────────────────────────────────────────────┤
│  User Block Area (3 MB = 3072 blocks)                    │
│  - Block 2048-5119                                       │
│  - 1KB per block                                         │
│  - User data and source code                             │
└──────────────────────────────────────────────────────────┘
```

**Constants:**

```c
#define VM_MEMORY_SIZE (5 * 1024 * 1024)    // 5 MB total
#define BLOCK_SIZE 1024                      // 1 KB per block
#define MAX_BLOCKS 5120                      // Total blocks
#define DICTIONARY_BLOCKS 2048               // Dictionary blocks
#define DICTIONARY_MEMORY_SIZE (2 * 1024 * 1024) // 2 MB
#define USER_BLOCKS_START 2048               // First user block
```

### Memory Access Functions

All memory access goes through bounds-checked functions:

```c
// Bounds checking
int vm_addr_ok(struct VM *vm, vaddr_t addr, size_t len);

// Pointer materialization (internal use only)
uint8_t *vm_ptr(struct VM *vm, vaddr_t addr);

// Safe memory operations
uint8_t vm_load_u8(struct VM *vm, vaddr_t addr);
void vm_store_u8(struct VM *vm, vaddr_t addr, uint8_t v);
cell_t vm_load_cell(struct VM *vm, vaddr_t addr);
void vm_store_cell(struct VM *vm, vaddr_t addr, cell_t v);
```

### Block System

The block system provides persistent storage with 1KB blocks:

```c
// Block operations
uint8_t *vm_block(VM *vm, int blk_num);  // Get block buffer
void vm_update(VM *vm);                   // Mark block as modified
void vm_flush(VM *vm);                    // Write modified blocks
```

**Block Buffer Management:**

- Double-buffered for safety
- Lazy write-back on `UPDATE` + `FLUSH`
- In-memory only (persistence planned for future)

---

## Dictionary System

### Dictionary Lookup

StarForth uses a **linked-list dictionary** with linear search:

```c
DictEntry *vm_find(VM *vm, const char *name, size_t len) {
    DictEntry *entry = vm->latest;
    while (entry) {
        if (entry->name_len == len &&
            !(entry->flags & WORD_HIDDEN) &&
            memcmp(entry->name, name, len) == 0) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}
```

### Word Creation

User-defined words are created with `:` and compiled into the dictionary:

```forth
: SQUARE  DUP * ;
```

**Compilation steps:**

1. Allocate dictionary entry
2. Link to `latest`
3. Set SMUDGE flag (hide during definition)
4. Compile threaded code
5. Clear SMUDGE on `;`

### Entropy System

Each word tracks usage for optimization analysis:

```c
cell_t entropy;  // Incremented on each execution
```

This allows:

- Identifying hot paths for optimization
- Future JIT compilation targets
- Performance profiling

---

## Execution Model

StarForth supports both **indirect-threaded** and **direct-threaded** execution:

### Indirect Threading (Default)

Each word is represented by a function pointer:

```c
typedef void (*word_func_t)(struct VM *vm);

void vm_execute_word(VM *vm, DictEntry *entry) {
    vm->current_executing_entry = entry;
    entry->entropy++;  // Track usage
    entry->func(vm);   // Call function
}
```

### Direct Threading (Optional)

With `USE_DIRECT_THREADING=1`, threaded code is executed directly:

```c
void vm_execute_threaded(VM *vm, cell_t *code) {
    vm->ip = code;
    while (!vm->error && !vm->exit_colon) {
        cell_t cfa = *vm->ip++;
        DictEntry *entry = (DictEntry *)cfa;
        entry->func(vm);
    }
}
```

**Performance:** Direct threading eliminates function call overhead in inner interpreter loops.

### Control Flow Implementation

StarForth implements structured control flow with **byte-relative branches**:

#### IF-ELSE-THEN

```forth
: TEST  5 > IF ." big" ELSE ." small" THEN ;
```

**Compilation:**

1. `IF` compiles `(0BRANCH)` with placeholder offset
2. Pushes branch address to control-flow stack (type `CF_IF`)
3. `ELSE` patches `IF` branch, compiles `(BRANCH)`, pushes new address (type `CF_ELSE`)
4. `THEN` patches forward branch

#### BEGIN-WHILE-REPEAT

```forth
: COUNTDOWN  10 BEGIN DUP WHILE DUP . 1- REPEAT DROP ;
```

**Compilation:**

1. `BEGIN` pushes loop start address (type `CF_BEGIN`)
2. `WHILE` compiles `(0BRANCH)` with placeholder (type `CF_WHILE`)
3. `REPEAT` compiles backward `(BRANCH)` to BEGIN, patches WHILE

#### DO-LOOP

```forth
: SUM  0 10 0 DO I + LOOP ;
```

**Runtime:**

- Loop parameters `(limit, index, loop_start)` pushed to **loop stack**
- `I` fetches index from loop stack
- `LOOP` increments index, branches if `index < limit`

#### LEAVE

```forth
: SEARCH  100 0 DO I DUP . 50 = IF LEAVE THEN LOOP ;
```

**Compilation:**

- `LEAVE` compiles `(BRANCH)` with placeholder offset
- Pushes branch address to **leave stack**
- Loop terminator (`LOOP` / `+LOOP`) patches all pending LEAVE branches

**Key Design:** Separate control-flow and leave stacks prevent pollution of IF/ELSE/THEN state.

---

## Optimization Strategies

### 1. Inline Assembly Optimizations

StarForth includes hand-tuned inline assembly for critical stack operations:

**x86_64 Optimizations** (`vm_asm_opt.h`):

```c
static inline void vm_push_asm(VM *vm, cell_t value, int *error) {
    __asm__ volatile (
        "movq    %[dsp], %%rax\n\t"        // Load dsp
        "cmpq    $1023, %%rax\n\t"         // Check overflow
        "jge     1f\n\t"                    // Jump if overflow
        "leaq    1(%%rax), %%rcx\n\t"      // dsp++
        "movq    %%rcx, %[dsp_out]\n\t"    // Store dsp
        "movq    %[val], %[stack](,%%rcx,8)\n\t" // stack[dsp] = value
        "movl    $0, %[err]\n\t"           // error = 0
        "jmp     2f\n\t"
        "1:\n\t"
        "movl    $1, %[err]\n\t"           // error = 1
        "2:\n\t"
        : [dsp_out]"=m"(vm->dsp), [err]"=m"(*error)
        : [dsp_in]"m"(vm->dsp), [val]"r"(value), [stack]"m"(vm->data_stack)
        : "rax", "rcx", "memory", "cc"
    );
}
```

**ARM64 Optimizations** (`vm_asm_opt_arm64.h`):

```c
static inline void vm_push_asm(VM *vm, cell_t value, int *error) {
    __asm__ volatile (
        "ldr     x0, %[dsp]\n\t"           // Load dsp
        "cmp     x0, #1023\n\t"            // Check overflow
        "b.ge    1f\n\t"
        "add     x1, x0, #1\n\t"           // dsp++
        "str     x1, %[dsp]\n\t"           // Store dsp
        "ldr     x2, %[stack]\n\t"         // Load stack base
        "str     %[val], [x2, x1, lsl #3]\n\t" // stack[dsp] = value
        "mov     w3, #0\n\t"               // error = 0
        "str     w3, %[err]\n\t"
        "b       2f\n\t"
        "1:\n\t"
        "mov     w3, #1\n\t"               // error = 1
        "str     w3, %[err]\n\t"
        "2:\n\t"
        : [dsp]"+m"(vm->dsp), [err]"=m"(*error)
        : [val]"r"(value), [stack]"m"(vm->data_stack)
        : "x0", "x1", "x2", "x3", "memory", "cc"
    );
}
```

**Performance Impact:**

- **x86_64:** ~22% speedup on tight loops
- **ARM64:** ~18% speedup (Cortex-A72)
- Enabled with `USE_ASM_OPT=1`

### 2. Compiler Optimizations

**Build Flags** (in `Makefile`):

```makefile
# Fastest configuration
CFLAGS = -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 \
         -flto -funroll-loops -finline-functions \
         -fomit-frame-pointer -fno-plt -fno-semantic-interposition \
         -march=native -mtune=native

LDFLAGS = -flto -s -Wl,--gc-sections -static
```

**Optimization Techniques:**

- **LTO (Link-Time Optimization):** Cross-module inlining
- **Loop unrolling:** Reduces branch overhead
- **Frame pointer omission:** One extra register for x86_64
- **Section garbage collection:** Removes unused code
- **Static linking:** No dynamic library overhead

### 3. Stack Management Optimizations

**Inlined stack operations:**

```c
// Fast path: no function call overhead
#if defined(USE_ASM_OPT) && (defined(ARCH_X86_64) || defined(ARCH_ARM64))
    vm_push_asm(vm, value, &error);
#else
    vm_push(vm, value);  // Standard C fallback
#endif
```

### 4. Dictionary Optimization Opportunities

**Current:** Linear search with entropy tracking
**Future:**

- Hash table for O(1) lookup
- Binary search for sorted dictionary
- JIT compilation for high-entropy words

---

## Module Organization

### Source Tree Layout

```
src/
├── vm.c                    # Core VM implementation
├── io.c                    # Input/output system
├── log.c                   # Logging and debugging
├── main.c                  # Entry point and REPL
├── word_registry.c         # Word registration system
├── stack_management.c      # Stack operations (with asm opts)
└── word_source/            # Modular word implementations
    ├── arithmetic_words.c
    ├── stack_words.c
    ├── control_words.c
    ├── defining_words.c
    ├── memory_words.c
    ├── io_words.c
    ├── logical_words.c
    ├── return_stack_words.c
    ├── dictionary_words.c
    ├── block_words.c
    ├── vocabulary_words.c
    ├── string_words.c
    ├── double_words.c
    ├── mixed_arithmetic_words.c
    ├── format_words.c
    ├── editor_words.c
    ├── dictionary_manipulation_words.c
    ├── system_words.c
    └── starforth_words.c

include/
├── vm.h                    # Core VM types and functions
├── vm_asm_opt.h            # x86_64 inline assembly
├── vm_asm_opt_arm64.h      # ARM64 inline assembly
├── io.h                    # I/O system interface
├── log.h                   # Logging macros
└── word_registry.h         # Word registration API
```

### Word Registration System

All words are registered through a central registry:

```c
void register_words(VM *vm) {
    register_stack_words(vm);
    register_arithmetic_words(vm);
    register_control_words(vm);
    register_defining_words(vm);
    register_memory_words(vm);
    // ... 14 more modules
}
```

**Benefits:**

- Centralized initialization
- Easy to enable/disable word sets
- Clean module boundaries
- No circular dependencies

---

## Platform Integration

### L4Re/StarshipOS Integration

StarForth is designed for **L4Re** (L4 Runtime Environment) on StarshipOS:

**Key Adaptations:**

1. **No libc dependency:**
    - All I/O through L4Re IPC
    - Custom memory management
    - Platform-specific `io.c`

2. **Capability-based addressing:**
    - VM addresses map to L4Re capabilities
    - Dataspace objects for memory regions
    - IPC for inter-VM communication

3. **Multi-VM architecture:**
    - Each StarForth instance as separate L4Re task
    - IPC-based message passing between VMs
    - Shared memory via dataspace capabilities

**Build Integration:**

```makefile
l4re:
	@echo "Building for L4Re/StarshipOS..."
	$(MAKE) MINIMAL=1 CFLAGS="$(BASE_CFLAGS) -DSTARFORTH_L4RE=1 \
	        -nostdlib -ffreestanding" \
	        PLATFORM_SRC=src/platform/starforth_l4re.c
```

### Raspberry Pi 4 Cross-Compilation

StarForth supports ARM64 cross-compilation for Raspberry Pi 4:

```makefile
rpi4-cross:
	@echo "Cross-compiling for Raspberry Pi 4 (ARM64)..."
	$(MAKE) CC=aarch64-linux-gnu-gcc \
	        ARCH_FLAGS="-march=armv8-a+crc+simd -mtune=cortex-a72" \
	        ARCH_DEFINES="-DARCH_ARM64=1" \
	        CFLAGS="$(BASE_CFLAGS) -O3 -DUSE_ASM_OPT=1 -static"
```

**Features:**

- Cortex-A72 optimizations
- NEON SIMD support
- CRC32 hardware acceleration
- Static binary for easy deployment

---

## Performance Characteristics

### Benchmark Configuration

**Test System:**

- **CPU:** x86_64 with inline assembly optimizations
- **Compiler:** GCC with `-O3 -flto -march=native`
- **Test:** Tight loop with 1,000,000 iterations

**Benchmark Code:**

```forth
: BENCH  1000000 0 DO 1 2 + DROP LOOP ;
```

### Build Configurations

#### Regular Build

```bash
make clean && make
```

**Flags:**

- `-O2` optimization
- `USE_ASM_OPT=1` (inline assembly)
- LTO enabled
- Static linking

#### Fastest Build

```bash
make fastest
```

**Flags:**

- `-O3` optimization
- `USE_ASM_OPT=1` + `USE_DIRECT_THREADING=1`
- Aggressive inlining and loop unrolling
- Frame pointer omission
- PLT elimination

### Performance Comparison

| Configuration | Optimization Level | Assembly Opts | Direct Threading | Performance     |
|---------------|--------------------|---------------|------------------|-----------------|
| Debug         | `-O0`              | Disabled      | Disabled         | Baseline (100%) |
| Regular       | `-O2`              | Enabled       | Disabled         | ~300% faster    |
| Fast          | `-O3`              | Enabled       | Disabled         | ~350% faster    |
| Fastest       | `-O3`              | Enabled       | Enabled          | ~400% faster    |

### Memory Usage

| Component       | Size      | Notes                     |
|-----------------|-----------|---------------------------|
| VM structure    | ~20 KB    | Includes stacks and state |
| Dictionary area | 2 MB      | Pre-allocated for words   |
| User block area | 3 MB      | 3072 blocks of 1KB each   |
| **Total**       | **~5 MB** | Fixed at compile time     |

### Stack Depth

```c
#define STACK_SIZE 1024  // cells (8 KB each stack)
```

- **Data stack:** 1024 cells (8192 bytes)
- **Return stack:** 1024 cells (8192 bytes)
- **Control-flow stack:** 1024 entries
- **Loop stack:** 1024 entries
- **Leave stack:** 1024 entries

**Maximum nesting:**

- 1024 nested function calls
- 1024 nested loops
- 1024 nested IF statements

---

## Future Optimizations

### Planned Improvements

1. **Hash Table Dictionary**
    - O(1) lookup vs. O(n) linear search
    - 256-bucket hash with chaining
    - Maintains entropy tracking

2. **JIT Compilation**
    - Compile high-entropy words to native code
    - Platform-specific code generators (x86_64, ARM64)
    - Threshold: entropy > 10,000

3. **Subroutine Threading**
    - Hybrid indirect/direct threading
    - Hot words use direct threading
    - Cold words remain indirect

4. **SIMD Optimizations**
    - AVX2 for x86_64 bulk operations
    - NEON for ARM64 vector processing
    - Block copy/compare acceleration

5. **Cache-Aware Dictionary Layout**
    - Frequently used words grouped together
    - Reduce cache misses in dictionary search
    - Profile-guided optimization

---

## Design Rationale

### Why No Dynamic Allocation?

1. **Predictability:** Fixed memory usage for embedded systems
2. **L4Re compatibility:** Pre-allocated dataspaces
3. **Safety:** No memory leaks or fragmentation
4. **Performance:** No `malloc` overhead

### Why 5MB Memory?

- **Dictionary:** 2MB for ~10,000 words
- **Blocks:** 3MB = 3072 blocks for user data
- **Sweet spot:** Fits in L1/L2 cache on modern CPUs
- **Scalable:** Adjustable via `VM_MEMORY_SIZE`

### Why Linked-List Dictionary?

- **FORTH-79 standard:** Expected behavior
- **Simplicity:** Easy to implement and debug
- **Dynamic:** Words can be added/removed at runtime
- **Shadowing:** Allows word redefinition
- **Future-proof:** Can add hash table later without breaking API

### Why Separate Control-Flow Stacks?

- **Hygiene:** IF/ELSE/THEN independent of DO/LOOP
- **LEAVE support:** Clean compilation without stack pollution
- **Error detection:** Unmatched control structures caught at compile time
- **Debugging:** Clear separation of concerns

---

## Security Considerations

### Memory Safety

1. **Bounds checking:** All memory access validated via `vm_addr_ok()`
2. **Stack overflow protection:** Checks on every push operation
3. **No pointer exposure:** Virtual addresses, not C pointers
4. **Dictionary fence:** Protected system words cannot be forgotten

### Control Flow Integrity

1. **Epoch system:** Prevents stale control-flow frames
2. **Type checking:** CF stack entries tagged by type
3. **Nesting validation:** Mismatched IF/THEN caught at compile time
4. **Branch validation:** All branch targets validated

### Input Validation

1. **Buffer overflow protection:** Fixed-size input buffer
2. **Numeric overflow detection:** Range checks on conversions
3. **Word name limits:** `WORD_NAME_MAX = 31` characters
4. **TIB bounds checking:** `>IN` validated against buffer size

---

## Testing and Validation

StarForth includes a comprehensive test suite with **707 test cases** covering:

- Stack operations (52 tests)
- Arithmetic (89 tests)
- Control flow (73 tests)
- Defining words (41 tests)
- Memory operations (68 tests)
- Dictionary operations (55 tests)
- Block system (34 tests)
- Vocabulary system (28 tests)
- String operations (45 tests)
- I/O operations (37 tests)
- System integration (185 tests)

**Test Coverage:** ~93% of word implementations

**Test Framework:**

- In-process test runner
- Automatic test discovery
- Stack state verification
- Memory leak detection
- Performance regression testing

**Run tests:**

```bash
make test       # All tests
make test-fast  # Skip slow tests
```

---

## Conclusion

StarForth's architecture balances **performance**, **safety**, and **portability** while maintaining strict **FORTH-79
compliance**. The modular design with zero libc dependencies makes it ideal for embedded systems, microkernel
environments, and experimental OS development.

**Key Strengths:**

- ✅ **Fast:** Inline assembly + LTO + direct threading
- ✅ **Safe:** Bounds checking + virtual addresses
- ✅ **Portable:** ANSI C99 + platform-specific modules
- ✅ **Predictable:** Fixed memory + no dynamic allocation
- ✅ **Modular:** 19 word modules + clean interfaces
- ✅ **Standards-compliant:** Full FORTH-79 implementation

**Production Ready:** StarForth is suitable for:

- L4Re/StarshipOS microkernel systems
- Raspberry Pi 4 embedded applications
- Bare-metal firmware development
- Real-time control systems
- Educational FORTH implementations

---

**For more information:**

- [Quick Start Guide](../QUICKSTART.md)
- [L4Re Integration](L4RE_INTEGRATION.md)
- [x86_64 Optimizations](ASM_OPTIMIZATIONS.md)
- [ARM64 Optimizations](ARM64_OPTIMIZATIONS.md)
- [Testing Guide](../TESTING.md)
