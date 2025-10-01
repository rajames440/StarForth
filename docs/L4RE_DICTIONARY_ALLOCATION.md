# Dictionary Allocation Strategy for L4Re

## Current Implementation

StarForth currently uses `malloc()` and `free()` from the host C library for dictionary entry allocation:

- **Location**: `src/dictionary_management.c:228`
- **Function**: `vm_create_word()` uses `malloc(total)` to allocate DictEntry structures
- **Cleanup**: `FORGET` uses `free(e)` to deallocate entries (line 554 in defining_words.c)

This works fine for hosted environments but creates a dependency on libc heap allocator.

## Why This Matters for L4Re/StarshipOS

### Issues with Current Approach:

1. **Heap Dependency**: Requires full malloc implementation in L4Re environment
2. **Non-Deterministic**: Heap fragmentation and unpredictable allocation behavior
3. **Separate Address Space**: Dictionary entries live outside VM memory model
4. **Hard to Introspect**: Can't dump entire VM state as single memory region

### Benefits of VM-Based Allocation:

1. **Zero External Dependencies**: No libc malloc needed
2. **Deterministic**: Simple bump allocator, predictable behavior
3. **Unified Address Space**: Everything in one 5MB VM memory region
4. **Easy Snapshots**: Can serialize entire VM state
5. **Better for Microkernel**: Aligns with L4Re dataspace model

## Recommended Approach for L4Re

### Strategy: Conditional Compilation

Add a compile-time flag to switch between malloc and VM-based allocation:

```c
// In dictionary_management.c
#ifdef STARFORTH_VM_DICT_ALLOC
    /* VM-based allocation (for L4Re/embedded) */
    DictEntry *entry = dict_alloc_from_vm(vm, total);
#else
    /* Standard malloc (for hosted environments) */
    DictEntry *entry = (DictEntry *) malloc(total);
#endif
```

### Memory Layout (VM-Based Mode):

```
vm->memory[0...VM_MEMORY_SIZE] (5MB):
┌──────────────────────────────────────────┐
│ Dictionary Headers (256KB)               │  ← dict_headers_used grows up
│ - DictEntry structures                   │
│ - Allocated via dict_alloc()             │
├──────────────────────────────────────────┤
│ User Data & Code (variable)              │  ← HERE grows up
│ - Compiled definitions                   │
│ - Variables, constants                   │
│ - String literals                        │
├──────────────────────────────────────────┤
│ Block Storage (remainder)                │
│ - 1KB blocks for Forth block system     │
└──────────────────────────────────────────┘
```

### Implementation Steps:

1. **Add Dictionary Allocator** (256KB bump allocator at `memory[0]`):

```c
static DictEntry *dict_alloc_from_vm(VM *vm, size_t total_bytes) {
    /* Align to cell_t boundary */
    size_t align = sizeof(cell_t);
    total_bytes = (total_bytes + align - 1) & ~(align - 1);

    /* Check space */
    if (vm->dict_headers_used + total_bytes > DICT_HEADERS_MAX) {
        vm->error = 1;
        return NULL;
    }

    /* Allocate from VM memory */
    DictEntry *entry = (DictEntry *)(vm->memory + vm->dict_headers_used);
    vm->dict_headers_used += total_bytes;
    return entry;
}
```

2. **Update FORGET** (no individual frees, just rewind):

```c
/* Instead of free(e) loop, rewind allocator */
size_t target_offset = (uint8_t*)target - vm->memory;
vm->dict_headers_used = target_offset;
```

3. **Adjust VM Initialization**:

```c
vm->dict_headers_used = 0;     /* Headers start at memory[0] */
vm->here = DICT_HEADERS_MAX;   /* User data starts at 256KB */
```

4. **No Changes to Fast-Lookup Cache**: Cache buckets still use malloc for the pointer arrays, but entries themselves
   are in VM memory.

### Testing Strategy:

```bash
# Test with standard malloc (current):
make clean && make test

# Test with VM-based allocation:
make clean
CFLAGS="-DSTARFORTH_VM_DICT_ALLOC" make test

# Test L4Re minimal build:
make clean
CFLAGS="-DSTARFORTH_MINIMAL -DSTARFORTH_VM_DICT_ALLOC" make rpi4-cross
```

## Current Status

**Decision**: Keep using `malloc()` for now, document the abstraction point clearly.

**Rationale**:

- Current implementation is stable and well-tested
- L4Re port can implement the switch when needed
- Abstraction layer is simple and well-defined
- No performance difference between approaches

**Action Items for L4Re Port**:

1. Define `STARFORTH_VM_DICT_ALLOC` in L4Re build
2. Implement `dict_alloc_from_vm()` function
3. Update `FORGET` to use rewind instead of free loop
4. Test with full test suite
5. Verify no memory leaks in microkernel environment

## Minimal Code Changes Needed

The switch to VM-based allocation requires changes to only 3 functions:

1. **`vm_create_word()`** - Use `dict_alloc_from_vm()` instead of `malloc()`
2. **`dictionary_word_forget()`** - Rewind `dict_headers_used` instead of `free()` loop
3. **`vm_init()`** - Initialize `dict_headers_used` and offset `HERE`

Add to `VM` struct in `include/vm.h`:

```c
size_t dict_headers_used;  /* Bytes used for dictionary headers (VM mode only) */
```

**Estimated effort**: 4 hours to implement + 2 hours testing = **6 hours total**

## References

- Current malloc usage: `src/dictionary_management.c:228`
- Current free usage: `src/word_source/defining_words.c:554`
- Fast-lookup cache: `src/dictionary_management.c:43-67` (unchanged)
- L4Re dataspace docs: `docs/L4RE_INTEGRATION.md`

---

**Author**: R. A. James
**Date**: 2025-10-01
**Status**: Documented, implementation deferred to L4Re port
