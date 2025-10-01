# StarForth Doxygen Documentation Style Guide

This guide explains how to add Doxygen (Javadoc-style) documentation to StarForth source files.

## Overview

StarForth uses Doxygen to automatically generate API documentation from specially-formatted comments in source code. The
documentation is generated in multiple formats:

- **HTML** - Interactive web-based documentation
- **PDF** - Comprehensive reference manual
- **AsciiDoc** - Lightweight markup for technical docs
- **Markdown** - GitHub-friendly format
- **Man Pages** - Traditional Unix man pages

## Quick Start

Generate documentation:

```bash
make docs          # All formats
make docs-html     # HTML only (fast)
make docs-open     # Generate and open in browser
```

## Documentation Comment Syntax

### File Documentation

Every header (.h) and source (.c) file should start with a `@file` block:

```c
/**
 * @file vm.h
 * @brief StarForth Virtual Machine Core API
 *
 * @details
 * Detailed description of what this file contains and its purpose.
 * Can span multiple paragraphs and include code examples.
 *
 * @author R. A. James (rajames)
 * @date 2025-08-15
 * @version 1.0.0
 * @copyright CC0-1.0 (Public Domain)
 */
```

### Function Documentation

Document functions with `@brief`, `@param`, `@return`, and optional `@details`:

```c
/**
 * @brief Initialize the StarForth virtual machine
 *
 * @details
 * Allocates VM memory, initializes stacks, sets up the dictionary,
 * and registers the FORTH-79 standard word set. After calling this
 * function, the VM is ready to interpret Forth code.
 *
 * Memory layout after initialization:
 * - Data stack: Empty (DSP = -1)
 * - Return stack: Empty (RSP = -1)
 * - Dictionary: Contains FORTH-79 core words
 * - HERE: Points to first free cell
 *
 * @param vm Pointer to uninitialized VM structure
 *
 * @pre vm must point to valid memory
 * @post vm->memory is allocated (VM_MEMORY_SIZE bytes)
 * @post vm->error is 0 on success, 1 on failure
 *
 * @note If initialization fails, vm->error will be set and
 *       vm->memory will be NULL. Always check vm->error after calling.
 *
 * @warning Do not use the VM if vm->error is non-zero after init
 *
 * @see vm_cleanup()
 * @see vm_interpret()
 *
 * @par Example:
 * @code
 * VM vm;
 * vm_init(&vm);
 * if (vm.error) {
 *     fprintf(stderr, "VM initialization failed\n");
 *     return 1;
 * }
 * vm_interpret(&vm, "42 . CR");
 * vm_cleanup(&vm);
 * @endcode
 */
void vm_init(VM *vm);
```

### Structure/Typedef Documentation

Document types with `@brief`, `@details`, and member documentation:

```c
/**
 * @typedef cell_t
 * @brief Forth cell type (signed 64-bit integer)
 *
 * The fundamental data unit in Forth. All stack operations work with cells.
 *
 * @note Size is platform-dependent: sizeof(signed long)
 * @warning Typically 64-bit but may be 32-bit on some platforms
 */
typedef signed long cell_t;

/**
 * @struct VM
 * @brief StarForth Virtual Machine state structure
 *
 * @details
 * Contains all VM state including stacks, memory, dictionary, and
 * compilation/interpretation state. One VM instance per Forth system.
 *
 * ## Memory Management
 * - VM owns vm.memory (allocated in vm_init)
 * - Dictionary entries are malloc'd separately
 * - User must call vm_cleanup() to free resources
 *
 * ## Thread Safety
 * VM is **not thread-safe**. Use one VM per thread or external locking.
 */
typedef struct VM {
    /** @brief Data stack (1024 cells) */
    cell_t data_stack[STACK_SIZE];

    /** @brief Return stack (1024 cells) */
    cell_t return_stack[STACK_SIZE];

    /**
     * @brief Data stack pointer (index of top element)
     *
     * @details
     * - Valid range: -1 (empty) to STACK_SIZE-1 (full)
     * - dsp == -1 means stack is empty
     * - dsp == STACK_SIZE-1 means stack is full
     */
    int dsp;

    /**
     * @brief Return stack pointer
     * @see dsp
     */
    int rsp;

    /**
     * @brief VM memory buffer (VM_MEMORY_SIZE bytes)
     * @note Allocated by vm_init(), freed by vm_cleanup()
     */
    uint8_t *memory;

    /** @brief Next free memory location (byte offset) */
    size_t here;

    /**
     * @brief Most recently defined word
     * @details Forms a linked list via DictEntry->link
     */
    DictEntry *latest;

    /** @brief Error flag (0=ok, 1=error) */
    int error;

    /** @brief Halted flag (0=running, 1=halted) */
    int halted;
} VM;
```

### Enum Documentation

```c
/**
 * @enum vm_mode_t
 * @brief VM compilation mode
 *
 * The VM operates in one of two modes:
 * - INTERPRET: Execute words immediately
 * - COMPILE: Add words to current definition
 */
typedef enum {
    MODE_INTERPRET = 0,  /**< Interpret mode (execute words) */
    MODE_COMPILE = 1     /**< Compile mode (build definitions) */
} vm_mode_t;
```

### Macro/Define Documentation

```c
/**
 * @def STACK_SIZE
 * @brief Maximum stack depth (1024 cells)
 *
 * Applies to both data stack and return stack. Stack overflow
 * detection is performed by vm_push() and vm_rpush().
 */
#define STACK_SIZE 1024

/**
 * @def VM_MEMORY_SIZE
 * @brief Total VM memory (5 MB)
 *
 * Layout:
 * - First 2 MB: Dictionary space
 * - Remaining 3 MB: User blocks (3072 blocks of 1KB each)
 */
#define VM_MEMORY_SIZE (5 * 1024 * 1024)
```

## Special Documentation Tags

### Grouping Related Functions

Use `@defgroup` and `@ingroup` to organize related functions:

```c
/**
 * @defgroup stack_ops Stack Operations
 * @brief Data stack manipulation words
 * @{
 */

/** @brief Push value onto data stack */
void vm_push(VM *vm, cell_t value);

/** @brief Pop value from data stack */
cell_t vm_pop(VM *vm);

/** @} */ // End of stack_ops group
```

### Cross-References

Use `@see` to link related functions:

```c
/**
 * @brief Duplicate top stack element
 * @see vm_drop()
 * @see vm_swap()
 */
void vm_dup(VM *vm);
```

### Code Examples

Use `@code` and `@endcode` for examples:

```c
/**
 * @par Example:
 * @code
 * VM vm;
 * vm_init(&vm);
 * vm_push(&vm, 42);
 * vm_push(&vm, 10);
 * cell_t result = vm_pop(&vm);  // result = 10
 * @endcode
 */
```

### Conditions and Warnings

```c
/**
 * @pre Stack must have at least 2 elements
 * @post Stack depth reduced by 1
 *
 * @note This is an optimized hot-path function
 * @warning Caller must check stack depth first
 *
 * @bug Known issue #42: Overflow not detected in debug builds
 * @todo Add SIMD optimization for x86_64
 * @deprecated Use vm_add_checked() instead
 */
```

## Documentation Quality Guidelines

### DO:

- ✅ Document ALL public API functions
- ✅ Include code examples for complex functions
- ✅ Explain pre-conditions and post-conditions
- ✅ Use `@warning` for dangerous operations
- ✅ Cross-reference related functions with `@see`
- ✅ Keep `@brief` to one line
- ✅ Put detailed explanations in `@details`

### DON'T:

- ❌ Document private static functions (unless complex)
- ❌ Repeat information already in function name
- ❌ Write obvious documentation (`@brief Get value` for `getValue()`)
- ❌ Use vague descriptions ("Does stuff with the stack")
- ❌ Forget to update docs when changing code

## Complete Example: Documented Header File

See `examples/doxygen_example.h` for a complete example showing all documentation styles.

## Checking Documentation

After adding documentation, check for warnings:

```bash
make docs-html
cat docs/api/doxygen_warnings.log
```

Common warnings to fix:

- Undocumented functions
- Missing `@param` for parameters
- Missing `@return` for non-void functions
- Invalid cross-references in `@see`

## Documentation Coverage

Check documentation coverage with:

```bash
make docs
# Look for "Documented: X/Y files" in output
```

Goal: 100% documentation coverage for:

- All public headers in `include/`
- All word source headers in `src/word_source/include/`
- Key implementation files in `src/`

## Viewing Generated Documentation

```bash
make docs-open      # Generate and open HTML docs
xdg-open docs/api/html/index.html  # Open existing HTML
man -l docs/api/man/vm_init.3      # View man page
evince docs/api/StarForth-API-Reference.pdf  # View PDF
```

## Integration with IDEs

### Visual Studio Code

Install "Doxygen Documentation Generator" extension to auto-generate documentation templates.

### CLion / IntelliJ

Built-in Doxygen support. Use `/**` and press Enter to generate template.

### Vim

Use DoxygenToolkit.vim plugin for documentation templates.

---

**Questions?** See the Doxygen manual: https://www.doxygen.nl/manual/

**Sniff-tested by Santino 🐕**
