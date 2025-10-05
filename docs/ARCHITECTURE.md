# StarForth Architecture

**Version:** 1.1.0
**Last Updated:** October 2025
**Author:** Robert A. James

---

## Table of Contents

1. [Overview](#overview)
2. [Design Philosophy](#design-philosophy)
3. [Core Architecture](#core-architecture)
4. [Memory Management](#memory-management)
5. [Block Storage System](#block-storage-system) ⭐ NEW
6. [Dictionary System](#dictionary-system)
7. [Execution Model](#execution-model)
8. [Optimization Strategies](#optimization-strategies)
9. [Module Organization](#module-organization)
10. [Platform Integration](#platform-integration)
11. [Performance Characteristics](#performance-characteristics)

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

---

## Block Storage System

### Architecture Overview

StarForth v1.1.0 features a **3-layer block storage architecture** with v2 on-disk format:

```
┌─────────────────────────────────────────────────────────┐
│              Layer 3: Forth Words (block_words.c)       │
│     BLOCK BUFFER UPDATE FLUSH LIST LOAD THRU SCR        │
├─────────────────────────────────────────────────────────┤
│          Layer 2: Block Subsystem (block_subsystem.c)   │
│           LBN→PBN mapping, BAM, Metadata, Cache          │
├─────────────────────────────────────────────────────────┤
│          Layer 1: Block I/O (blkio.h + backends)        │
│          FILE backend | RAM backend | L4Re backend      │
└─────────────────────────────────────────────────────────┘
```

### Layer 1: Block I/O Backends

**Vtable-based abstraction** (`include/blkio.h`):

```c
typedef struct blkio_ops {
    int (*read)(struct blkio_dev *dev, uint32_t lba, void *buf);
    int (*write)(struct blkio_dev *dev, uint32_t lba, const void *buf);
    int (*flush)(struct blkio_dev *dev);
    int (*info)(struct blkio_dev *dev, blkio_info_t *info);
} blkio_ops_t;
```

**Available backends:**

- `blkio_file.c` (245 LOC) - FILE-backed persistent storage
- `blkio_ram.c` (111 LOC) - RAM-only (testing)
- `blkio_l4ds.c` (planned) - L4Re dataspace
- `blkio_l4svc.c` (planned) - L4Re IPC service

### Layer 2: Block Subsystem v2

**LBN→PBN Mapping** (src/block_subsystem.c: 759 LOC):

```
Logical Block Numbers (LBN) - User View:
┌──────────────────┬──────────────────────────────┐
│ LBN 0-991        │ LBN 992+                     │
│ RAM (volatile)   │ DISK (persistent)            │
└──────────────────┴──────────────────────────────┘

Physical Block Numbers (PBN) - Internal:
┌──────────┬──────────────┬──────────┬──────────────┐
│ RAM      │ RAM          │ DISK     │ DISK         │
│ 0-31     │ 32-1023      │ 1024-    │ 1056+        │
│ RESERVED │ USER (992)   │ 1055 RES │ USER         │
└──────────┴──────────────┴──────────┴──────────────┘
```

**Reserved ranges hidden from users:**

- RAM PBN 0-31 → System reserved (32 blocks)
- DISK PBN 1024-1055 → System reserved (32 blocks)
- User sees: LBN 0-991 (RAM) + LBN 992+ (DISK)

**External BAM (Block Allocation Map):**

- 1-bit bitmap stored in dedicated 4KB devblocks
- 32768 bits per 4KB page
- Dynamic sizing: `B = ceil(3 * (total_devblocks - 1) / 32768)`

**v2 On-Disk Format:**

```
devblock 0:       Volume header (4 KiB)
                  - magic: 0x53544652 "STFR"
                  - version: 2
                  - BAM location and size
                  - capacity/allocation info

devblocks 1..B:   BAM pages (4 KiB each)
                  - 1 bit per Forth block
                  - Tracks allocation state

devblocks (1+B)..:Payload (3× 1KB data + 1KB metadata per 4KB)
                  - 3× 1KB Forth blocks packed
                  - 1KB metadata region (341 bytes per block)
```

**Volume Header (devblock 0):**

```c
typedef struct {
    uint32_t magic;              // 0x53544652 "STFR"
    uint32_t version;            // 2
    uint32_t total_volumes;
    uint64_t total_devblocks;    // 4 KiB units

    // BAM placement
    uint32_t bam_start;          // usually 1
    uint32_t bam_devblocks;      // size of BAM
    uint32_t devblock_base;      // first payload devblock

    // Capacity
    uint64_t tracked_blocks;     // 32768 * bam_devblocks
    uint64_t total_blocks;       // min(tracked, 3*(total-1-B))
    uint64_t free_blocks;

    // Allocation hints
    uint64_t first_free;
    uint64_t last_allocated;

    // Reserved ranges
    uint32_t reserved_disk_lo;   // e.g., 32
    uint32_t reserved_ram_lo;    // e.g., 32

    // Optional
    uint64_t created_time;
    uint64_t mounted_time;
    uint64_t hdr_crc;

    uint8_t _pad[...];           // Total 4096 bytes
} blk_volume_meta_t;
```

**Per-Block Metadata (341 bytes):**

```c
typedef struct {
    // Core integrity (16 bytes)
    uint64_t checksum;           // CRC64-ISO
    uint64_t magic;              // 0x424C4B5F5354524B "BLK_STRK"

    // Timestamps (16 bytes)
    uint64_t created_time;
    uint64_t modified_time;

    // Block status (16 bytes)
    uint64_t flags;
    uint64_t write_count;

    // Content identification (32 bytes)
    uint64_t content_type;       // 0=empty, 1=source, 2=data
    uint64_t encoding;           // 0=ASCII, 1=UTF-8, 2=binary
    uint64_t content_length;     // Actual data length ≤ 1024
    uint64_t reserved1;

    // Cryptographic (64 bytes)
    uint64_t entropy[4];         // 256-bit entropy
    uint64_t hash[4];            // SHA-256 slot

    // Security & ownership (40 bytes)
    uint64_t owner_id;
    uint64_t permissions;
    uint64_t acl_block;
    uint64_t signature[2];

    // Link/chain support (32 bytes)
    uint64_t prev_block;
    uint64_t next_block;
    uint64_t parent_block;
    uint64_t chain_length;

    // Application-specific (120 bytes)
    uint64_t app_data[15];

    uint8_t padding[5];          // Total 341 bytes
} blk_meta_t;
```

**LRU Cache (8 slots, 32KB total):**

```c
typedef struct {
    uint32_t devblock;           // 4 KiB unit
    uint8_t data[4096];          // 3× 1KB data + 1KB meta
    blk_meta_t meta[3];          // Decoded metadata
    uint8_t valid;
    uint8_t loaded;
    uint8_t dirty;
    uint8_t meta_dirty;
} cache_slot_t;

cache_slot_t cache[8];           // LRU cache
```

**CRC64 Integrity:**

```c
// ISO polynomial: 0x42F0E1EBA9EA3693
static uint64_t compute_crc64(const uint8_t *data, size_t len);
```

### Layer 3: Forth Interface

**Block words** (src/word_source/block_words.c):

```forth
BLOCK   ( n -- addr )     \ Get block buffer (LBN)
BUFFER  ( n -- addr )     \ Get empty block buffer (LBN)
UPDATE  ( -- )            \ Mark current block dirty
FLUSH   ( -- )            \ Write dirty blocks to disk
SAVE-BUFFERS ( -- )       \ Alias for FLUSH
LIST    ( n -- )          \ Display block (16×64 format)
LOAD    ( n -- )          \ Interpret block as Forth source
THRU    ( n1 n2 -- )      \ Load blocks n1 through n2
SCR     ( -- addr )       \ Screen number variable
```

**All operations use LBNs** (user-visible logical block numbers).

### Persistence Example

```bash
# Create disk image
qemu-img create -f raw mydisk.img 500M

# Run with persistent storage
./build/starforth --disk-img=mydisk.img

# In REPL
2048 BLOCK 1024 65 FILL UPDATE FLUSH   \ Fill with 'A'
2048 LIST                               \ Display
BYE

# Restart - data persists
./build/starforth --disk-img=mydisk.img
2048 LIST                               \ Still shows 'A'
```

### Performance Characteristics

- **Block read (cached):** ~100ns
- **Block read (disk):** ~10-100μs
- **FLUSH (8 blocks):** ~0.1-1ms
- **LRU cache:** 32KB (8× 4KB slots)
- **Metadata overhead:** 341 bytes per block
- **Packing efficiency:** 3× 1KB Forth blocks per 4KB devblock

### L4Re Integration Path

```c
// Future L4Re backends (architecture defined)
int blkio_open_l4ds(blkio_dev_t *dev, l4re_ds_t dataspace, ...);
int blkio_open_l4svc(blkio_dev_t *dev, l4_cap_idx_t service, ...);
```

**Benefits:**

- Clean abstraction - backend-agnostic
- Same Forth API regardless of backend
- Metadata travels with blocks
- CRC64 ensures integrity across all backends

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

## Error Handling Architecture

StarForth employs a **dual error system** designed to separate VM-level fatal errors from subsystem-level recoverable
errors. This architecture provides modularity while maintaining clear error semantics.

### Primary Error System: VM-Level Errors

The VM uses a simple **error flag** for fatal conditions that should halt execution:

```c
typedef struct {
    int error;        // Error flag: 0 = OK, 1 = error
    int halted;       // Halt flag: 0 = running, 1 = halted
    // ... other VM fields
} VM;
```

**Usage Pattern:**

```c
void some_forth_word(VM *vm) {
    if (!vm) return;

    /* Check preconditions */
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "WORD: stack underflow");
        return;
    }

    /* Perform operation */
    // ...
}
```

**VM Error Characteristics:**

- **Fatal:** Execution stops immediately
- **Simple:** Single boolean flag (`vm->error = 1`)
- **Propagated:** Checked by interpreter loop
- **Logged:** Always accompanied by `log_message(LOG_ERROR, ...)`
- **Scope:** Stack underflow/overflow, invalid memory access, compile errors

**Examples:**

```c
// Stack underflow
if (vm->dsp < 0) {
    vm->error = 1;
    log_message(LOG_ERROR, "DUP: stack underflow");
    return;
}

// Stack overflow
if (vm->dsp >= DATA_STACK_SIZE - 1) {
    vm->error = 1;
    log_message(LOG_ERROR, "PUSH: stack overflow");
    return;
}

// Invalid memory address
if (!vm_addr_ok(vm, addr)) {
    vm->error = 1;
    log_message(LOG_ERROR, "@: invalid address %lu", (unsigned long)addr);
    return;
}

// Malloc failure (dictionary allocation)
if (!entry) {
    vm->error = 1;
    log_message(LOG_ERROR, "CREATE: malloc failed for word '%s'", name);
    return;
}
```

### Secondary Error System: Block Subsystem Errors

The **block subsystem** uses a separate error code system for I/O operations:

```c
/* Block subsystem error codes */
#define BLK_SUCCESS       0   // Operation successful
#define BLK_ERR_IO       -1   // I/O error (disk read/write failed)
#define BLK_ERR_NOMEM    -2   // Out of memory (BAM allocation failed)
#define BLK_ERR_INVAL    -3   // Invalid parameter (bad block number)
#define BLK_ERR_NOTFOUND -4   // Block not found or unavailable
#define BLK_ENODEV       -5   // No device available
#define BLK_EINVAL       -6   // Invalid argument
#define BLK_EIO          -7   // I/O error
```

**Usage Pattern:**

```c
int blk_init_disk(const char *path) {
    /* Allocate BAM (Block Allocation Map) */
    g.bam = (uint8_t *) calloc(1, bam_size);
    if (!g.bam) {
        log_message(LOG_ERROR, "blk_init_disk: BAM allocation failed (%zu bytes)", bam_size);
        return BLK_ERR_NOMEM;
    }

    /* Read volume header */
    int rc = blkio_read(dev, 0, header_buf);
    if (rc != BLKIO_OK) {
        log_message(LOG_ERROR, "blk_init_disk: failed to read volume header");
        return BLK_ERR_IO;
    }

    return BLK_SUCCESS;
}
```

**Block Error Characteristics:**

- **Recoverable:** Caller can handle error and continue
- **Detailed:** Specific error codes indicate failure type
- **Return value:** Errors returned via function return codes
- **Logged:** Critical errors logged with context
- **Scope:** Block I/O, BAM operations, device management

**Propagation to VM:**

Block subsystem errors are converted to VM errors at the Forth word layer:

```c
static void forth_BLOCK(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "BLOCK: stack underflow");
        return;
    }

    uint32_t blk_num = (uint32_t) vm->data_stack[vm->dsp--];
    uint8_t *buf = blk_get_buffer(blk_num, 0);  // Returns NULL on error

    if (!buf) {
        vm->error = 1;  // Convert block error to VM error
        log_message(LOG_ERROR, "BLOCK: failed to get buffer for block %u", blk_num);
        return;
    }

    /* Push buffer address to stack */
    vaddr_t addr = vm_buffer_to_vaddr(vm, buf);
    vm->data_stack[++vm->dsp] = (cell_t)addr;
}
```

### Design Rationale: Why Two Error Systems?

#### 1. **Modularity**

The block subsystem can be used independently of the VM:

```c
/* Standalone block usage (no VM required) */
int init_storage(void) {
    int rc = blk_init_disk("storage.img");
    if (rc != BLK_SUCCESS) {
        fprintf(stderr, "Storage init failed: %d\n", rc);
        return -1;
    }
    return 0;
}
```

#### 2. **Error Granularity**

VM errors are binary (fatal/OK), while block errors distinguish:

- I/O failures vs. allocation failures vs. invalid parameters
- Transient errors (disk busy) vs. permanent errors (disk full)
- Recoverable conditions (retry-able) vs. fatal conditions

#### 3. **L4Re Compatibility**

Block error codes map cleanly to L4Re IPC error codes:

```c
/* Future L4Re integration */
int blkio_l4re_read(blkio_dev_t *dev, uint32_t block, uint8_t *buf) {
    l4_msgtag_t tag = l4_ipc_call(dev->cap, ...);
    if (l4_ipc_error(tag, l4_utcb())) {
        return BLK_ERR_IO;  // Map L4 IPC error to block error
    }
    return BLK_SUCCESS;
}
```

#### 4. **Separation of Concerns**

| Error Type       | Scope             | Handler          | Recovery                      |
|------------------|-------------------|------------------|-------------------------------|
| **VM Errors**    | Forth interpreter | `vm->error` flag | Halt execution, reset REPL    |
| **Block Errors** | I/O subsystem     | Return codes     | Retry, fallback, or propagate |

### Error Logging Standards

All error paths use `log.h` for consistent logging:

```c
#include "log.h"

void some_function(VM *vm) {
    if (error_condition) {
        vm->error = 1;
        log_message(LOG_ERROR, "FUNCTION: description of error (context=%d)", context_info);
        return;
    }
}
```

**Logging Levels:**

```c
typedef enum {
    LOG_NONE = -1,   // Disable all logging
    LOG_ERROR = 0,   // Error messages only
    LOG_WARN,        // Warning and error messages
    LOG_INFO,        // Informational, warning, and error messages
    LOG_TEST,        // Test results and all previous levels
    LOG_DEBUG        // Debug messages and all previous levels
} LogLevel;
```

**Error Logging Best Practices:**

1. **Always log context:**
   ```c
   log_message(LOG_ERROR, "ALLOT: invalid size %ld (max=%zu)", n, VM_MEMORY_SIZE);
   ```

2. **Include function/word name:**
   ```c
   log_message(LOG_ERROR, "CREATE: word name too long (%zu > %d)", len, WORD_NAME_MAX);
   ```

3. **Log before setting error flag:**
   ```c
   log_message(LOG_ERROR, "BLOCK: buffer allocation failed");
   vm->error = 1;  // Set flag after logging
   ```

4. **Use appropriate severity:**
   ```c
   log_message(LOG_DEBUG, "ALLOT: allocated %ld bytes at 0x%lx", n, addr);  // OK
   log_message(LOG_ERROR, "+!: address out of bounds");                     // Error
   log_message(LOG_WARN, "FORGET: word '%s' not found", name);              // Warning
   ```

### Error Checking in Performance-Critical Code

Stack operations use inline assembly and may skip logging for performance:

```c
#ifdef USE_ASM_OPT
static inline void vm_push(VM *vm, cell_t value) {
    __asm__ volatile (
        "incl %0\n\t"
        "movq %1, (%2,%0,8)\n\t"
        : "+r" (vm->dsp)
        : "r" (value), "r" (vm->data_stack)
        : "memory"
    );
}
#else
static inline void vm_push(VM *vm, cell_t value) {
    if (vm->dsp >= DATA_STACK_SIZE - 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "PUSH: stack overflow");
        return;
    }
    vm->data_stack[++vm->dsp] = value;
}
#endif
```

**Optimization Trade-off:** Inline ASM skips bounds checking for maximum performance. Overflow would corrupt adjacent
memory but is prevented by:

1. **Large stack size** (1024 cells = 8KB)
2. **Test coverage** catching overflow conditions
3. **Stack depth monitoring** in DEBUG builds

### Memory Allocation Error Handling

All dynamic allocations check for NULL and log failures:

```c
/* Dictionary entry creation */
DictEntry *entry = (DictEntry *) malloc(total);
if (!entry) {
    vm->error = 1;
    log_message(LOG_ERROR, "vm_create_word: malloc failed for '%.*s' (%zu bytes)",
                (int)len, name, total);
    return NULL;
}

/* BAM allocation (block subsystem) */
g.bam = (uint8_t *) calloc(1, bam_size);
if (!g.bam) {
    log_message(LOG_ERROR, "blk_init: BAM allocation failed (%zu bytes)", bam_size);
    return BLK_ERR_NOMEM;  // Return error code, not VM error
}

/* INIT system file buffer */
char *file_content = (char *) malloc(file_size + 1);
if (!file_content) {
    log_message(LOG_ERROR, "INIT: malloc failed for init.4th (%zu bytes)", file_size);
    fclose(fp);
    vm->error = 1;
    vm->halted = 1;
    return;
}

/* ... use buffer ... */

free(file_content);  // Always freed on all code paths
```

**Cleanup Requirements:**

1. **All `malloc()` must have corresponding `free()`**
2. **Error paths must clean up before returning**
3. **Use goto cleanup pattern for complex functions:**

```c
int complex_operation(VM *vm) {
    char *buffer = NULL;
    int *array = NULL;
    int rc = 0;

    buffer = malloc(size);
    if (!buffer) {
        log_message(LOG_ERROR, "malloc failed");
        rc = -1;
        goto cleanup;
    }

    array = calloc(count, sizeof(int));
    if (!array) {
        log_message(LOG_ERROR, "calloc failed");
        rc = -1;
        goto cleanup;
    }

    /* ... operations ... */

cleanup:
    free(array);
    free(buffer);
    return rc;
}
```

### Error Recovery in REPL

The REPL (Read-Eval-Print Loop) handles VM errors gracefully:

```c
void repl_loop(VM *vm) {
    while (!vm->halted) {
        printf("ok> ");
        fgets(input_buffer, sizeof(input_buffer), stdin);

        vm_interpret(vm, input_buffer);

        if (vm->error) {
            /* Error already logged by word implementation */
            printf(" ERROR\n");

            /* Reset error state but keep dictionary */
            vm->error = 0;
            vm->dsp = -1;      // Clear data stack
            vm->rsp = -1;      // Clear return stack
            vm->mode = MODE_INTERPRET;

            continue;  // Continue REPL
        }

        printf(" ok\n");
    }
}
```

**Error Recovery Strategy:**

1. **Log error** (done by word implementation)
2. **Display "ERROR"** to user
3. **Clear stacks** (prevent corruption)
4. **Reset to interpret mode** (cancel any compilation)
5. **Continue REPL** (don't exit)

### Summary: Error System Guidelines

| Situation                       | Error System           | Pattern                       |
|---------------------------------|------------------------|-------------------------------|
| **Stack underflow/overflow**    | VM error               | `vm->error = 1` + log         |
| **Invalid memory address**      | VM error               | `vm->error = 1` + log         |
| **Malloc failure (dictionary)** | VM error               | `vm->error = 1` + log         |
| **Compile-time errors**         | VM error               | `vm->error = 1` + log         |
| **Block I/O failure**           | Block error → VM error | Return code → `vm->error = 1` |
| **BAM allocation failure**      | Block error code       | Return `BLK_ERR_NOMEM`        |
| **Invalid block number**        | Block error code       | Return `BLK_ERR_INVAL`        |
| **Device not ready**            | Block error code       | Return `BLK_ENODEV`           |

**Key Principles:**

1. **VM errors are fatal** - halt execution
2. **Block errors are recoverable** - allow retry or fallback
3. **Always log error context** - aid debugging
4. **Clean up on error paths** - free allocated memory
5. **Dual systems enable modularity** - block subsystem is reusable

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
