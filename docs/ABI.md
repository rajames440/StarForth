# StarForth Application Binary Interface (ABI)

## Overview

This document defines the Application Binary Interface (ABI) for StarForth, the high-performance FORTH-79 virtual
machine. The ABI specifies how components interact at the binary level, including memory layout, calling conventions,
data structures, and system interfaces.

**Version:** 1.1.0
**Date:** October 2025
**Architecture Support:** x86_64, ARM64

---

## Table of Contents

1. [Data Types and Sizes](#data-types-and-sizes)
2. [Memory Layout](#memory-layout)
3. [Stack Management](#stack-management)
4. [Dictionary Structure](#dictionary-structure)
5. [Calling Conventions](#calling-conventions)
6. [Register Usage](#register-usage)
7. [VM State Structure](#vm-state-structure)
8. [Block Storage Interface](#block-storage-interface)
9. [Extension Interface](#extension-interface)
10. [Platform-Specific Details](#platform-specific-details)

---

## 1. Data Types and Sizes

### Core Types

```c
typedef intptr_t cell_t;        // Native pointer-sized integer
typedef uintptr_t ucell_t;      // Unsigned cell
typedef uint8_t byte_t;         // Byte type
typedef char* address_t;        // Memory address
```

### Type Sizes by Architecture

| Type        | x86_64  | ARM64   | Description                      |
|-------------|---------|---------|----------------------------------|
| `cell_t`    | 8 bytes | 8 bytes | Stack cell, matches pointer size |
| `ucell_t`   | 8 bytes | 8 bytes | Unsigned cell                    |
| `byte_t`    | 1 byte  | 1 byte  | Byte operations                  |
| `address_t` | 8 bytes | 8 bytes | Memory addresses                 |

### Alignment Requirements

- **x86_64**: 8-byte alignment for stack cells
- **ARM64**: 8-byte alignment for stack cells (strict alignment required)
- **Dictionary entries**: 8-byte aligned on both architectures
- **Block buffers**: 1024-byte alignment recommended for performance

---

## 2. Memory Layout

### VM Memory Map

```
┌──────────────────────────────────┐  High Memory
│      Return Stack (8KB)          │  ← RSP
├──────────────────────────────────┤
│      Data Stack (8KB)            │  ← SP
├──────────────────────────────────┤
│      Dictionary Space (64KB)     │  ← DP, LATEST
├──────────────────────────────────┤
│      Block Buffers (Dynamic)     │
├──────────────────────────────────┤
│      VM State Structure          │
└──────────────────────────────────┘  Low Memory
```

### Memory Regions

| Region       | Default Size      | Configurable | Growth   |
|--------------|-------------------|--------------|----------|
| Data Stack   | 1024 cells (8KB)  | Yes          | Downward |
| Return Stack | 1024 cells (8KB)  | Yes          | Downward |
| Dictionary   | 8192 cells (64KB) | Yes          | Upward   |
| Block Cache  | 16 blocks × 1KB   | Yes          | Dynamic  |

---

## 3. Stack Management

### Data Stack (SP)

- **Direction**: Grows downward (pre-decrement push)
- **Cell Size**: 8 bytes (64-bit)
- **Depth**: 1024 cells maximum
- **Alignment**: 8-byte aligned

**Push Operation:**

```c
*--vm->sp = value;
```

**Pop Operation:**

```c
value = *vm->sp++;
```

**Top of Stack Access:**

```c
top = *vm->sp;        // TOS
second = *(vm->sp+1);  // NOS (Next On Stack)
```

### Return Stack (RSP)

- **Direction**: Grows downward (pre-decrement push)
- **Cell Size**: 8 bytes (64-bit)
- **Depth**: 1024 cells maximum
- **Usage**: Return addresses, loop indices, temporary storage

**Push Operation:**

```c
*--vm->rsp = value;
```

**Pop Operation:**

```c
value = *vm->rsp++;
```

### Stack Pointer Initialization

```c
vm->sp = vm->stack + STACK_SIZE;        // Top of data stack
vm->rsp = vm->return_stack + RSTACK_SIZE; // Top of return stack
```

---

## 4. Dictionary Structure

### Dictionary Entry Layout

Each dictionary entry (word definition) follows this structure:

```c
typedef struct DictionaryEntry {
    struct DictionaryEntry* link;  // +0:  Pointer to previous entry (8 bytes)
    uint8_t flags;                 // +8:  Flags byte (1 byte)
    uint8_t name_length;           // +9:  Name length (1 byte)
    char name[MAX_NAME_LEN];       // +10: Name string (variable, null-terminated)
    // Padding to 8-byte boundary
    void* code_field;              // Execution token (function pointer)
    cell_t data[];                 // Parameter field (variable length)
} DictionaryEntry;
```

### Dictionary Entry Fields

| Offset   | Size     | Field         | Description                          |
|----------|----------|---------------|--------------------------------------|
| 0        | 8 bytes  | `link`        | Pointer to previous dictionary entry |
| 8        | 1 byte   | `flags`       | Word flags (IMMEDIATE, HIDDEN, etc.) |
| 9        | 1 byte   | `name_length` | Length of word name (0-31)           |
| 10       | Variable | `name`        | Word name string (null-terminated)   |
| Aligned  | 8 bytes  | `code_field`  | Execution token (XT)                 |
| Variable | Variable | `data`        | Parameter field for compiled code    |

### Dictionary Flags

```c
#define FLAG_IMMEDIATE  0x80  // Execute immediately during compilation
#define FLAG_HIDDEN     0x40  // Hidden from dictionary searches
#define FLAG_COMPILE    0x20  // Compile-only word
#define FLAG_INLINE     0x10  // Inline optimization hint
```

### Dictionary Pointers

- **LATEST**: Points to most recently defined word
- **DP (Dictionary Pointer)**: Points to next free dictionary space
- **HERE**: Synonym for DP in traditional FORTH

---

## 5. Calling Conventions

### Native Code Interface

StarForth words are implemented as C functions with this signature:

```c
typedef void (*forth_word_fn)(VM* vm);
```

### Calling Convention

- **Parameter Passing**: Via VM structure pointer (`VM* vm`)
- **Stack Access**: Through `vm->sp` and `vm->rsp`
- **Return**: Functions return `void`, results on stack
- **Stack Effect**: Functions must maintain stack discipline

### Example Word Implementation

```c
// DUP ( n -- n n )
void forth_DUP(VM* vm) {
    cell_t top = *vm->sp;
    *--vm->sp = top;
}

// + ( n1 n2 -- n3 )
void forth_ADD(VM* vm) {
    cell_t n2 = *vm->sp++;
    cell_t n1 = *vm->sp++;
    *--vm->sp = n1 + n2;
}
```

### Error Handling

Functions should check for stack underflow/overflow:

```c
if (vm->sp >= vm->stack + STACK_SIZE) {
    vm_error(vm, "Stack underflow");
    return;
}
```

---

## 6. Register Usage

### x86_64 Architecture

When assembly optimizations are enabled (`USE_ASM_OPT=1`):

| Register | Purpose               | Preserved? | Notes                         |
|----------|-----------------------|------------|-------------------------------|
| RAX      | Scratch, return value | No         | Temporary calculations        |
| RBX      | Saved                 | Yes        | Can be used for locals        |
| RCX      | Scratch               | No         | Loop counter                  |
| RDX      | Scratch               | No         | Temporary                     |
| RSI      | Source pointer        | No         | String operations             |
| RDI      | `VM* vm` pointer      | Yes        | First argument (System V ABI) |
| RSP      | C stack pointer       | Yes        | Must preserve                 |
| RBP      | Frame pointer         | Yes        | Must preserve                 |
| R8-R15   | Available             | Varies     | Follow System V ABI           |

**Stack Pointers in Assembly:**

- Use `[rdi + offsetof(VM, sp)]` for data stack pointer
- Use `[rdi + offsetof(VM, rsp)]` for return stack pointer

### ARM64 Architecture

When assembly optimizations are enabled:

| Register | Purpose            | Preserved? | Notes                    |
|----------|--------------------|------------|--------------------------|
| X0       | `VM* vm` pointer   | No         | First argument (AAPCS64) |
| X1-X7    | Scratch            | No         | Temporary values         |
| X8       | Indirect result    | No         | Struct return            |
| X9-X15   | Scratch            | No         | Temporary                |
| X16-X17  | Intra-call scratch | No         | Linker usage             |
| X19-X28  | Callee-saved       | Yes        | Preserved across calls   |
| X29      | Frame pointer (FP) | Yes        | Must preserve            |
| X30      | Link register (LR) | Yes        | Return address           |
| SP       | Stack pointer      | Yes        | Must preserve            |

**Stack Pointers in Assembly:**

- Use `ldr x1, [x0, #offsetof(VM, sp)]` to load data stack pointer
- Use `ldr x2, [x0, #offsetof(VM, rsp)]` to load return stack pointer

---

## 7. VM State Structure

### Core VM Structure

```c
typedef struct VM {
    // Stack pointers
    cell_t* sp;              // Data stack pointer
    cell_t* rsp;             // Return stack pointer

    // Stack arrays
    cell_t stack[STACK_SIZE];         // Data stack (8KB)
    cell_t return_stack[RSTACK_SIZE]; // Return stack (8KB)

    // Dictionary management
    cell_t dictionary[DICT_SIZE];     // Dictionary space (64KB)
    cell_t* dp;                        // Dictionary pointer (HERE)
    DictionaryEntry* latest;           // Latest word

    // Input/Output
    char* input_buffer;                // Current input buffer
    size_t input_length;               // Input buffer length
    size_t input_position;             // Current parse position

    // Interpreter state
    int state;                         // 0=interpret, 1=compile
    cell_t base;                       // Numeric base (2-36)

    // Block storage
    BlockSystem* block_system;         // Block I/O subsystem

    // Error handling
    int error_code;                    // Last error code
    char error_msg[256];               // Error message buffer

    // Profiling and instrumentation
    ProfileData* profile;              // Profiling data (if enabled)

    // Platform-specific extensions
    void* platform_data;               // Platform-specific state
} VM;
```

### VM Structure Offsets (x86_64)

| Field          | Offset | Size  | Notes                |
|----------------|--------|-------|----------------------|
| `sp`           | 0      | 8     | Data stack pointer   |
| `rsp`          | 8      | 8     | Return stack pointer |
| `stack`        | 16     | 8192  | Data stack array     |
| `return_stack` | 8208   | 8192  | Return stack array   |
| `dictionary`   | 16400  | 65536 | Dictionary space     |
| `dp`           | 81936  | 8     | Dictionary pointer   |
| `latest`       | 81944  | 8     | Latest word pointer  |

*(Offsets may vary with compilation options)*

---

## 8. Block Storage Interface

### Block Structure

```c
#define BLOCK_SIZE 1024  // Standard FORTH block size

typedef struct Block {
    uint8_t data[BLOCK_SIZE];
    uint32_t block_number;
    uint32_t flags;
    time_t modified_time;
} Block;
```

### Block Flags

```c
#define BLOCK_FLAG_DIRTY    0x01  // Block modified, needs writing
#define BLOCK_FLAG_LOADED   0x02  // Block in cache
#define BLOCK_FLAG_LOCKED   0x04  // Block locked in cache
```

### Block System Interface

```c
typedef struct BlockSystem {
    Block* (*get_block)(uint32_t block_num);
    void (*update_block)(Block* block);
    void (*flush_block)(Block* block);
    void (*flush_all)(void);
} BlockSystem;
```

### Block Word ABI

**BLOCK ( n -- addr )**

- Input: Block number `n` on stack
- Output: Address of 1024-byte block buffer
- Side effects: May allocate buffer, load from disk

**UPDATE ( -- )**

- Marks most recently accessed block as dirty
- No stack effect

**FLUSH ( -- )**

- Writes all dirty blocks to persistent storage
- No stack effect

---

## 9. Extension Interface

### Adding Custom Words

To add custom native words to the VM:

```c
// 1. Define the word function
void forth_CUSTOM(VM* vm) {
    // Implementation
}

// 2. Register with VM during initialization
vm_register_word(vm, "CUSTOM", forth_CUSTOM, 0);
```

### Word Registration Function

```c
void vm_register_word(
    VM* vm,
    const char* name,
    forth_word_fn function,
    uint8_t flags
);
```

### Extension Module Structure

```c
typedef struct ExtensionModule {
    const char* name;
    void (*init)(VM* vm);
    void (*cleanup)(VM* vm);
    const WordDefinition* words;
    size_t word_count;
} ExtensionModule;
```

---

## 10. Platform-Specific Details

### x86_64 Specifics

- **Alignment**: Natural alignment (8 bytes for cells)
- **Endianness**: Little-endian
- **Assembly Syntax**: Intel syntax (`-masm=intel`)
- **Optimizations**: SSE2+ vector operations available
- **Cache Line**: Typically 64 bytes

### ARM64 Specifics

- **Alignment**: Strict 8-byte alignment enforced
- **Endianness**: Little-endian (bi-endian capable)
- **NEON**: SIMD vector operations available
- **Cache Line**: Typically 64 bytes
- **Memory Barriers**: Required for multi-threaded code

### Minimal/Embedded Platforms

When building with `MINIMAL=1`:

- No standard library dependencies
- Custom `memcpy`, `memset` implementations
- Reduced memory footprint
- Optional features disabled

### L4Re Microkernel

When building for L4Re (`L4RE_TARGET=1`):

- Uses L4Re allocation APIs
- Dataspaces for dictionary and stacks
- IPC-based block storage
- No POSIX dependencies

---

## Compatibility Notes

### Binary Compatibility

- **Word size**: Must match architecture (64-bit only)
- **Dictionary format**: Portable across same-architecture systems
- **Block files**: Binary compatible across all platforms
- **Serialization**: Native byte order (not network byte order)

### Source Compatibility

- **ANSI C99**: All code uses C99 standard
- **POSIX**: Optional, not required for minimal builds
- **GCC/Clang**: Primary compilers, tested regularly

---

## Appendix: ABI Version History

### Version 1.1.0 (Current)

- Added ARM64 architecture support
- Enhanced profiling instrumentation
- Block versioning system
- Strict pointer mode option

### Version 1.0.0

- Initial x86_64 ABI definition
- Basic stack and dictionary layout
- Core word set implementation

---

## References

1. **FORTH-79 Standard**: Draft Proposed American National Standard for FORTH-79
2. **System V ABI**: AMD64 Architecture Processor Supplement
3. **AAPCS64**: Procedure Call Standard for the ARM 64-bit Architecture
4. **StarForth Documentation**: Complete reference in `/docs/`

---

*This ABI specification is maintained as part of the StarForth project and is released into the public domain under CC0
1.0.*