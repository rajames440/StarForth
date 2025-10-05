# StarForth Word Development Guide

**Version:** 1.0.0
**Last Updated:** October 2025
**Audience:** C developers extending StarForth

---

## Table of Contents

1. [Overview](#overview)
2. [Word Implementation Basics](#word-implementation-basics)
3. [Step-by-Step: Creating a New Word](#step-by-step-creating-a-new-word)
4. [Creating a New Word Module](#creating-a-new-word-module)
5. [Naming Conventions](#naming-conventions)
6. [Testing Your Words](#testing-your-words)
7. [Common Patterns](#common-patterns)
8. [Error Handling](#error-handling)
9. [Performance Considerations](#performance-considerations)
10. [Complete Example](#complete-example)

---

## Overview

StarForth words are implemented as C functions that manipulate the VM state. This guide teaches you how to add new words
to StarForth, whether adding a single word to an existing module or creating an entirely new word category.

**Prerequisites:**

- Familiarity with C programming
- Understanding of FORTH stack operations
- Basic knowledge of StarForth architecture (see [ARCHITECTURE.md](ARCHITECTURE.md))

**What You'll Learn:**

- How to implement FORTH words in C
- How to register words with the VM
- How to write tests for your words
- Best practices for error handling and performance

---

## Word Implementation Basics

### Anatomy of a FORTH Word

Every FORTH word in StarForth is a C function with this signature:

```c
void word_function_name(VM *vm);
```

**Example - Simple Addition (+):**

```c
/**
 * @brief Implements FORTH word "+" that adds two numbers
 *
 * Stack effect: ( n1 n2 -- n3 )
 * Adds n1 and n2, pushing their sum n3 onto the stack.
 *
 * @param vm Pointer to the VM structure
 */
void arithmetic_word_plus(VM *vm) {
    /* 1. Check stack has enough values */
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "+: Stack underflow");
        vm->error = 1;
        return;
    }

    /* 2. Pop operands from stack */
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    /* 3. Perform operation */
    cell_t result = n1 + n2;

    /* 4. Push result */
    vm_push(vm, result);

    /* 5. Optional: Debug logging */
    log_message(LOG_DEBUG, "+: %ld + %ld = %ld", (long)n1, (long)n2, (long)result);
}
```

### Key Components

1. **Stack Validation:** Always check `vm->dsp` before popping
2. **Stack Operations:** Use `vm_pop()` and `vm_push()`
3. **Error Handling:** Set `vm->error = 1` on failure
4. **Logging:** Use `log_message()` for debugging
5. **Documentation:** Include Doxygen comments with stack effects

---

## Step-by-Step: Creating a New Word

Let's create a `SQUARE` word that squares a number: `( n -- n² )`

### Step 1: Choose the Right Module

Decide which existing module your word belongs to:

| Module               | Purpose            | Example Words                 |
|----------------------|--------------------|-------------------------------|
| `arithmetic_words.c` | Math operations    | `+`, `-`, `*`, `/`, `ABS`     |
| `stack_words.c`      | Stack manipulation | `DUP`, `DROP`, `SWAP`, `OVER` |
| `memory_words.c`     | Memory access      | `@`, `!`, `C@`, `C!`, `ALLOT` |
| `logical_words.c`    | Bitwise/logic      | `AND`, `OR`, `XOR`, `NOT`     |
| `string_words.c`     | String operations  | `TYPE`, `WORD`, `ACCEPT`      |
| `control_words.c`    | Control flow       | `IF`, `ELSE`, `LOOP`, `DO`    |

**SQUARE is arithmetic** → Add to `arithmetic_words.c`

### Step 2: Implement the Word Function

Add to `src/word_source/arithmetic_words.c`:

```c
/**
 * @brief Implements FORTH word "SQUARE" that squares a number
 *
 * Stack effect: ( n -- n² )
 * Squares the top stack value.
 *
 * @param vm Pointer to the VM structure
 */
static void arithmetic_word_square(VM *vm) {
    /* Check stack depth: need at least 1 value */
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "SQUARE: Stack underflow");
        vm->error = 1;
        return;
    }

    /* Pop value */
    cell_t n = vm_pop(vm);

    /* Calculate square */
    cell_t result = n * n;

    /* Push result */
    vm_push(vm, result);

    log_message(LOG_DEBUG, "SQUARE: %ld² = %ld", (long)n, (long)result);
}
```

**Note the `static` keyword:** Words are typically `static` unless exported in the header.

### Step 3: Add Forward Declaration (if needed)

If your word is called before it's defined, add forward declaration at top of file:

```c
/* Forward declarations */
static void arithmetic_word_square(VM *vm);
```

### Step 4: Register the Word

Find the `register_arithmetic_words()` function and add your word:

```c
void register_arithmetic_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 arithmetic words...");

    /* Existing words... */
    register_word(vm, "+", arithmetic_word_plus);
    register_word(vm, "-", arithmetic_word_minus);
    register_word(vm, "*", arithmetic_word_multiply);

    /* NEW: Your word */
    register_word(vm, "SQUARE", arithmetic_word_square);

    log_message(LOG_INFO, "Arithmetic words registered successfully");
}
```

### Step 5: Rebuild and Test

```bash
make clean && make

./build/starforth
```

```forth
StarForth> 5 SQUARE .
25  ok
StarForth> -3 SQUARE .
9  ok
```

**Done!** Your word is now part of StarForth.

---

## Creating a New Word Module

If you're adding a category of related words (e.g., trigonometry, bit manipulation, etc.), create a new module.

### Step 1: Create Source File

Create `src/word_source/mymodule_words.c`:

```c
/*
                                 ***   StarForth   ***
  mymodule_words.c - Custom Word Module
 Last modified - 10/5/25
  Copyright (c) 2025 Your Name - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
*/

#include "include/mymodule_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Module documentation */
/* MY-WORD1   ( n -- n² )     Square a number
 * MY-WORD2   ( n -- n³ )     Cube a number
 */

/**
 * @brief Squares a number
 * Stack effect: ( n -- n² )
 */
static void mymodule_word_square(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "MY-SQUARE: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n * n);
    log_message(LOG_DEBUG, "MY-SQUARE: %ld² = %ld", (long)n, (long)(n*n));
}

/**
 * @brief Cubes a number
 * Stack effect: ( n -- n³ )
 */
static void mymodule_word_cube(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "MY-CUBE: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n * n * n);
    log_message(LOG_DEBUG, "MY-CUBE: %ld³ = %ld", (long)n, (long)(n*n*n));
}

/**
 * @brief Registers all custom module words
 */
void register_mymodule_words(VM *vm) {
    log_message(LOG_INFO, "Registering custom module words...");

    register_word(vm, "MY-SQUARE", mymodule_word_square);
    register_word(vm, "MY-CUBE", mymodule_word_cube);

    log_message(LOG_INFO, "Custom module words registered");
}
```

### Step 2: Create Header File

Create `src/word_source/include/mymodule_words.h`:

```c
#ifndef STARFORTH_MYMODULE_WORDS_H
#define STARFORTH_MYMODULE_WORDS_H

#include "../../../include/vm.h"

/**
 * @brief Registers all custom module words with the VM
 * @param vm Pointer to VM instance
 */
void register_mymodule_words(VM *vm);

#endif /* STARFORTH_MYMODULE_WORDS_H */
```

### Step 3: Add to Word Registry

Edit `include/word_registry.h`:

```c
/* Add your module header */
#include "../src/word_source/include/mymodule_words.h"

/* Add function declaration */
void register_mymodule_words(VM *vm);
```

Edit `src/word_registry.c` to call your registration function:

```c
void register_forth79_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 word set...");

    /* Existing modules... */
    register_arithmetic_words(vm);
    register_stack_words(vm);

    /* NEW: Your module */
    register_mymodule_words(vm);

    log_message(LOG_INFO, "FORTH-79 word set registered successfully");
}
```

### Step 4: Update Build System

The build system should auto-detect `.c` files, but verify by checking the Makefile or running:

```bash
make clean && make
```

---

## Naming Conventions

### Function Names

**Pattern:** `<module>_word_<word_name>`

```c
arithmetic_word_plus       // + word in arithmetic module
stack_word_dup             // DUP word in stack module
mymodule_word_square       // SQUARE word in your module
```

**Why?** Prevents name collisions between modules.

### FORTH Word Names

**Standard FORTH:**

- Use uppercase: `DUP`, `SWAP`, `OVER`
- Hyphens for multi-word: `STAR-SLASH`, `/MOD`
- Special characters: `+`, `*`, `@`, `!`

**Custom Extensions:**

- Prefix with module name: `MY-SQUARE`, `MATH-SIN`
- Or use distinctive naming: `XSQUARE`, `XCUBE`

### File Names

- Source: `<module>_words.c`
- Header: `<module>_words.h`
- Test: `<module>_words_test.c`

---

## Testing Your Words

### Manual REPL Testing

```bash
./build/starforth
```

```forth
\ Test SQUARE word
5 SQUARE .          \ Should print: 25
-3 SQUARE .         \ Should print: 9
0 SQUARE .          \ Should print: 0

\ Test stack effects
10 SQUARE SQUARE .  \ Should print: 10000 (10² then result²)

\ Test error handling
SQUARE              \ Should error: stack underflow
```

### Automated Test Suite

**CRITICAL:** Tests execute FORTH code through `vm_interpret()` - this tests both the word AND the VM!

Create `src/test_runner/modules/mymodule_words_test.c`:

```c
#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Custom Module Test Suites */
static WordTestSuite mymodule_word_suites[] = {
    {
        "MY-SQUARE", {
            /* Test name, FORTH code, description, test type, expect_error, enabled */
            {"basic", "5 MY-SQUARE . CR", "Should print: 25", TEST_NORMAL, 0, 1},
            {"zero", "0 MY-SQUARE . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-3 MY-SQUARE . CR", "Should print: 9", TEST_NORMAL, 0, 1},
            {"large", "1000 MY-SQUARE . CR", "Should print: 1000000", TEST_NORMAL, 0, 1},
            {"empty_stack", "MY-SQUARE", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}  /* Terminator */
        },
        5  /* Test count */
    },

    {
        "MY-CUBE", {
            {"basic", "3 MY-CUBE . CR", "Should print: 27", TEST_NORMAL, 0, 1},
            {"zero", "0 MY-CUBE . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-2 MY-CUBE . CR", "Should print: -8", TEST_NORMAL, 0, 1},
            {"one", "1 MY-CUBE . CR", "Should print: 1", TEST_NORMAL, 0, 1},
            {"empty_stack", "MY-CUBE", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {NULL, {}, 0}  /* Suite terminator */
};

/**
 * @brief Runs all custom module word tests
 * @param vm VM instance with all words registered
 */
void test_mymodule_words(VM *vm) {
    log_message(LOG_TEST, "\n=== Testing Custom Module Words ===\n");

    /* Run all test suites - vm_interpret() executes the FORTH code */
    run_word_test_suites(vm, mymodule_word_suites);

    log_message(LOG_TEST, "\n=== Custom Module Tests Complete ===\n");
}
```

**Key Points:**

1. **Tests execute FORTH code:** `"5 MY-SQUARE . CR"` runs through the VM interpreter
2. **Tests the whole stack:** VM, interpreter, and your word all tested together
3. **Error cases test VM error handling:** `expect_error = 1` verifies VM catches errors
4. **Use `WordTestSuite` structure:** Consistent with all StarForth tests

Register your test in `src/test_runner/test_runner.c`:

```c
/* Add test function declaration */
extern void test_mymodule_words(VM *vm);

/* Call in main test runner - AFTER VM and words are initialized */
void run_all_tests(VM *vm) {
    /* ... existing tests ... */
    test_arithmetic_words(vm);
    test_stack_words(vm);

    /* Add your module test */
    test_mymodule_words(vm);
}
```

Run tests:

```bash
make test
# or
./build/starforth --run-tests
```

**What Gets Tested:**

- ✅ Word execution through VM interpreter (not direct C calls!)
- ✅ Stack manipulation correctness
- ✅ Error handling (underflow, overflow, etc.)
- ✅ Integration with other words (`.`, `CR`, etc.)
- ✅ Proper stack state after execution

---

## Common Patterns

### Pattern 1: Stack Manipulation (0 operands → 1 result)

```c
/* Example: HERE ( -- addr ) */
static void memory_word_here(VM *vm) {
    vaddr_t here_addr = (vaddr_t)vm->here;
    vm_push(vm, (cell_t)here_addr);
    log_message(LOG_DEBUG, "HERE: %lu", (unsigned long)here_addr);
}
```

### Pattern 2: Unary Operation (1 operand → 1 result)

```c
/* Example: ABS ( n -- |n| ) */
static void arithmetic_word_abs(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "ABS: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t result = (n < 0) ? -n : n;
    vm_push(vm, result);
    log_message(LOG_DEBUG, "ABS: |%ld| = %ld", (long)n, (long)result);
}
```

### Pattern 3: Binary Operation (2 operands → 1 result)

```c
/* Example: + ( n1 n2 -- n3 ) */
static void arithmetic_word_plus(VM *vm) {
    if (vm->dsp < 1) {  // Need at least 2 values
        log_message(LOG_ERROR, "+: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 + n2;
    vm_push(vm, result);
    log_message(LOG_DEBUG, "+: %ld + %ld = %ld", (long)n1, (long)n2, (long)result);
}
```

### Pattern 4: Memory Access (address → value)

```c
/* Example: @ ( addr -- value ) */
static void memory_word_fetch(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "@: Stack underflow");
        vm->error = 1;
        return;
    }

    vaddr_t addr = (vaddr_t)vm_pop(vm);

    /* CRITICAL: Bounds check */
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        log_message(LOG_ERROR, "@: Address 0x%lx out of bounds", (unsigned long)addr);
        vm->error = 1;
        return;
    }

    cell_t value = vm_load_cell(vm, addr);
    vm_push(vm, value);
    log_message(LOG_DEBUG, "@: [0x%lx] = %ld", (unsigned long)addr, (long)value);
}
```

### Pattern 5: Memory Store (value address → )

```c
/* Example: ! ( value addr -- ) */
static void memory_word_store(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "!: Stack underflow");
        vm->error = 1;
        return;
    }

    vaddr_t addr = (vaddr_t)vm_pop(vm);
    cell_t value = vm_pop(vm);

    /* CRITICAL: Bounds check */
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) {
        log_message(LOG_ERROR, "!: Address 0x%lx out of bounds", (unsigned long)addr);
        vm->error = 1;
        return;
    }

    vm_store_cell(vm, addr, value);
    log_message(LOG_DEBUG, "!: [0x%lx] := %ld", (unsigned long)addr, (long)value);
}
```

---

## Error Handling

### Always Check Stack Depth

```c
/* For word needing N stack values, check dsp >= N-1 */

/* 1 value needed: ( n -- ) */
if (vm->dsp < 0) { /* error */ }

/* 2 values needed: ( n1 n2 -- ) */
if (vm->dsp < 1) { /* error */ }

/* 3 values needed: ( n1 n2 n3 -- ) */
if (vm->dsp < 2) { /* error */ }
```

### Memory Access Pattern

```c
vaddr_t addr = (vaddr_t)vm_pop(vm);

if (!vm_addr_ok(vm, addr, size_needed)) {
    log_message(LOG_ERROR, "WORD: Invalid address 0x%lx", (unsigned long)addr);
    vm->error = 1;
    return;
}

/* Safe to access memory */
```

### Division by Zero

```c
static void arithmetic_word_divide(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "/: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t divisor = vm_pop(vm);

    /* Check for division by zero */
    if (divisor == 0) {
        log_message(LOG_ERROR, "/: Division by zero");
        vm->error = 1;
        vm_push(vm, 0);  // Push dummy value to maintain stack balance
        return;
    }

    cell_t dividend = vm_pop(vm);
    vm_push(vm, dividend / divisor);
}
```

### Resource Cleanup

```c
static void word_with_allocation(VM *vm) {
    char *buffer = malloc(size);
    if (!buffer) {
        log_message(LOG_ERROR, "WORD: malloc failed");
        vm->error = 1;
        return;
    }

    /* ... operations ... */

    if (error_condition) {
        free(buffer);  // ✅ Clean up before returning
        vm->error = 1;
        return;
    }

    free(buffer);  // ✅ Clean up on success
}
```

---

## Performance Considerations

### Use vm_pop/vm_push Directly

```c
/* FAST - Direct access */
cell_t n2 = vm_pop(vm);
cell_t n1 = vm_pop(vm);
vm_push(vm, n1 + n2);

/* SLOW - Unnecessary function calls */
cell_t n2 = get_stack_value(vm, 0);  // Don't do this
cell_t n1 = get_stack_value(vm, 1);
set_stack_value(vm, 0, n1 + n2);
```

### Minimize Logging in Hot Paths

```c
/* DEBUG logging is cheap when LOG_LEVEL < LOG_DEBUG */
log_message(LOG_DEBUG, "+: %ld + %ld = %ld", ...);  // OK

/* But avoid in tight loops */
for (int i = 0; i < 1000000; i++) {
    log_message(LOG_DEBUG, "Iteration %d", i);  // ❌ Slow!
}
```

### Inline Simple Operations

```c
/* Mark simple helpers as inline */
static inline cell_t abs_value(cell_t n) {
    return (n < 0) ? -n : n;
}
```

---

## Complete Example

Here's a complete, production-ready implementation of a new word module for **bitwise rotation** operations:

### File: `src/word_source/bitrot_words.c`

```c
/*
                                 ***   StarForth   ***
  bitrot_words.c - Bit Rotation Words
 Last modified - 10/5/25
  Copyright (c) 2025 (Your Name) - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
*/

#include "include/bitrot_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Bit Rotation Words:
 * ROL    ( n count -- n' )   Rotate left
 * ROR    ( n count -- n' )   Rotate right
 */

/**
 * @brief Implements FORTH word "ROL" (rotate left)
 *
 * Stack effect: ( n count -- n' )
 * Rotates n left by count bits.
 *
 * @param vm Pointer to the VM structure
 */
static void bitrot_word_rol(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "ROL: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t count = vm_pop(vm);
    cell_t n = vm_pop(vm);

    /* Normalize count to 0-63 range for 64-bit cell */
    count = count & 63;

    /* Rotate left: (n << count) | (n >> (64 - count)) */
    uint64_t un = (uint64_t)n;
    uint64_t result = (un << count) | (un >> (64 - count));

    vm_push(vm, (cell_t)result);
    log_message(LOG_DEBUG, "ROL: 0x%lx << %ld = 0x%lx",
                (unsigned long)n, (long)count, (unsigned long)result);
}

/**
 * @brief Implements FORTH word "ROR" (rotate right)
 *
 * Stack effect: ( n count -- n' )
 * Rotates n right by count bits.
 *
 * @param vm Pointer to the VM structure
 */
static void bitrot_word_ror(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "ROR: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t count = vm_pop(vm);
    cell_t n = vm_pop(vm);

    /* Normalize count */
    count = count & 63;

    /* Rotate right: (n >> count) | (n << (64 - count)) */
    uint64_t un = (uint64_t)n;
    uint64_t result = (un >> count) | (un << (64 - count));

    vm_push(vm, (cell_t)result);
    log_message(LOG_DEBUG, "ROR: 0x%lx >> %ld = 0x%lx",
                (unsigned long)n, (long)count, (unsigned long)result);
}

/**
 * @brief Registers all bit rotation words with the VM
 * @param vm Pointer to the VM structure
 */
void register_bitrot_words(VM *vm) {
    log_message(LOG_INFO, "Registering bit rotation words...");

    register_word(vm, "ROL", bitrot_word_rol);
    register_word(vm, "ROR", bitrot_word_ror);

    log_message(LOG_INFO, "Bit rotation words registered successfully");
}
```

### File: `src/word_source/include/bitrot_words.h`

```c
#ifndef STARFORTH_BITROT_WORDS_H
#define STARFORTH_BITROT_WORDS_H

#include "../../../include/vm.h"

/**
 * @brief Registers bit rotation words
 * @param vm Pointer to VM instance
 */
void register_bitrot_words(VM *vm);

#endif /* STARFORTH_BITROT_WORDS_H */
```

### Integration

Add to `src/word_registry.c`:

```c
#include "../src/word_source/include/bitrot_words.h"

void register_forth79_words(VM *vm) {
    /* ... existing registrations ... */
    register_bitrot_words(vm);
}
```

### Test

```forth
HEX
FF ROL 8 .    \ Rotate 0xFF left 8 bits -> FF00
FF00 ROR 8 .  \ Rotate 0xFF00 right 8 bits -> FF
DECIMAL
```

---

## Summary Checklist

When adding a new word:

- [ ] Choose appropriate module (or create new one)
- [ ] Implement word function with proper signature
- [ ] Add Doxygen comment with stack effect
- [ ] Check stack depth before popping
- [ ] Set `vm->error = 1` on errors
- [ ] Use `log_message()` for debugging
- [ ] Register word in `register_*_words()` function
- [ ] Write tests (manual REPL + automated)
- [ ] Follow naming conventions
- [ ] Bounds-check all memory access
- [ ] Free allocated resources on all paths
- [ ] Update documentation if needed

---

## Further Reading

- [ARCHITECTURE.md](ARCHITECTURE.md) - StarForth system architecture
- [TESTING.md](../TESTING.md) - Test framework guide
- [SECURITY.md](SECURITY.md) - Security best practices
- [DOXYGEN_STYLE_GUIDE.md](DOXYGEN_STYLE_GUIDE.md) - API documentation standards
- [FORTH-79 Standard](http://forth.sourceforge.net/standard/fst79/) - FORTH-79 specification

---

**Happy Word Development!** 🚀