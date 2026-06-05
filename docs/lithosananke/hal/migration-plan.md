# HAL Migration Plan

## Overview

This document provides a **step-by-step plan** for refactoring StarForth to use the Hardware Abstraction Layer (HAL). The migration preserves all existing functionality while preparing the codebase for LithosAnanke.

**Goal:** Zero functional regressions, all 936+ tests pass, 0% algorithmic variance maintained.

---

## Migration Strategy

### Principles

1. **Incremental refactoring** - One subsystem at a time
2. **Test after each step** - Build + test suite must pass
3. **Platform parity** - Linux and L4Re work throughout migration
4. **No big bang** - HAL interfaces defined first, then adopted gradually

### Phases

```
Phase 1: Define HAL Interfaces (headers only)
  ↓
Phase 2: Implement HAL for Linux (refactor existing code)
  ↓
Phase 3: Migrate VM Core to use HAL
  ↓
Phase 4: Migrate Physics Subsystems to use HAL
  ↓
Phase 5: Migrate REPL and Word Implementations
  ↓
Phase 6: Implement HAL for L4Re (optional validation)
  ↓
Phase 7: Validate Deterministic Behavior
```

---

## Phase 1: Define HAL Interfaces

**Duration:** 1-2 days
**Risk:** Low (no code changes, only headers)

### Tasks

1. **Create HAL header directory**
   ```bash
   mkdir -p include/hal
   ```

2. **Write HAL interface headers**
   - `include/hal/hal_time.h`
   - `include/hal/hal_interrupt.h`
   - `include/hal/hal_memory.h`
   - `include/hal/hal_console.h`
   - `include/hal/hal_cpu.h`
   - `include/hal/hal_panic.h`

   See `interfaces.md` for full specifications.

3. **Update Makefile to include HAL headers**
   ```makefile
   INCLUDES += -Iinclude/hal
   ```

4. **Compile check (headers only)**
   ```bash
   make clean && make PLATFORM=linux
   ```

   Should compile without errors (HAL functions not yet called).

### Success Criteria

✅ All HAL headers compile without errors
✅ No changes to VM code yet
✅ Documentation reviewed and approved

---

## Phase 2: Implement HAL for Linux

**Duration:** 3-5 days
**Risk:** Medium (refactoring existing platform code)

### Current Platform Code Locations

Audit existing platform-specific code:

```bash
# Find POSIX-specific calls
grep -r "clock_gettime\|pthread\|signal\|malloc\|printf" src/
```

**Expected locations:**
- Timing: `src/platform/linux/time.c` (already exists)
- Memory: Direct `malloc()` calls in VM code (needs refactoring)
- Console: Direct `printf()` calls in REPL (needs refactoring)

### Tasks

1. **Create Linux HAL directory**
   ```bash
   mkdir -p src/platform/linux/hal
   ```

2. **Implement Linux HAL subsystems**

   **2a. `hal_time.c`** - Refactor `src/platform/linux/time.c`
   ```c
   /* Before: */
   uint64_t platform_time_ns(void) {
       struct timespec ts;
       clock_gettime(CLOCK_MONOTONIC, &ts);
       return ...;
   }

   /* After: */
   uint64_t hal_time_now_ns(void) {
       struct timespec ts;
       clock_gettime(CLOCK_MONOTONIC, &ts);
       return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
   }
   ```

   **2b. `hal_memory.c`** - Wrap malloc/free
   ```c
   void *hal_mem_alloc(size_t size) {
       if (size == 0) return NULL;
       void *ptr = malloc(size);
       if (ptr) memset(ptr, 0, size);
       return ptr;
   }

   void hal_mem_free(void *ptr) {
       free(ptr);
   }
   ```

   **2c. `hal_console.c`** - Wrap stdio
   ```c
   void hal_console_putc(char c) {
       putchar(c);
       fflush(stdout);
   }

   int hal_console_getc(void) {
       return getchar();
   }
   ```

   **2d. `hal_interrupt.c`** - Signal-based interrupts
   See `platform-implementations.md` for full implementation.

   **2e. `hal_cpu.c`** - CPU info
   ```c
   unsigned int hal_cpu_id(void) {
       return 0;  /* Single-threaded */
   }

   void hal_cpu_relax(void) {
       sched_yield();
   }
   ```

   **2f. `hal_panic.c`** - Error handling
   ```c
   void hal_panic(const char *msg) {
       fprintf(stderr, "PANIC: %s\n", msg ? msg : "unknown");
       abort();
   }
   ```

3. **Update Makefile**
   ```makefile
   ifeq ($(PLATFORM),linux)
       PLATFORM_SOURCES = \
           src/platform/linux/hal_time.c \
           src/platform/linux/hal_interrupt.c \
           src/platform/linux/hal_memory.c \
           src/platform/linux/hal_console.c \
           src/platform/linux/hal_cpu.c \
           src/platform/linux/hal_panic.c
   endif
   ```

4. **Build test (HAL code compiles)**
   ```bash
   make clean && make PLATFORM=linux
   ```

### Success Criteria

✅ Linux HAL compiles without errors
✅ Existing VM code still builds (not yet using HAL)
✅ No functional changes yet

---

## Phase 3: Migrate VM Core to use HAL

**Duration:** 2-3 days
**Risk:** High (core VM changes)

### Files to Modify

- `src/vm.c` - Core interpreter loop
- `src/vm_api.c` - External API
- `src/memory_management.c` - Dictionary allocator
- `include/vm.h` - VM struct

### Tasks

1. **Replace direct malloc/free in VM core**

   **Before:**
   ```c
   /* src/memory_management.c */
   void *mem = malloc(size);
   ```

   **After:**
   ```c
   #include "hal/hal_memory.h"

   void *mem = hal_mem_alloc(size);
   ```

2. **Replace timing calls in heartbeat**

   **Before:**
   ```c
   /* src/vm.c */
   #include <time.h>

   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   uint64_t now = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
   ```

   **After:**
   ```c
   #include "hal/hal_time.h"

   uint64_t now = hal_time_now_ns();
   ```

3. **Add HAL initialization to VM startup**

   **In `src/main.c`:**
   ```c
   int main(int argc, char **argv) {
       /* Initialize HAL subsystems */
       hal_time_init();
       hal_interrupt_init();
       hal_mem_init();
       hal_console_init();
       hal_cpu_init();

       /* Continue with existing VM initialization */
       VM *vm = vm_create();
       ...
   }
   ```

4. **Remove platform-specific #ifdefs**

   **Before:**
   ```c
   #ifdef PLATFORM_LINUX
       clock_gettime(CLOCK_MONOTONIC, &ts);
   #elif PLATFORM_L4RE
       l4re_kip_clock(kip);
   #endif
   ```

   **After:**
   ```c
   uint64_t now = hal_time_now_ns();  /* Works on all platforms */
   ```

5. **Build and test**
   ```bash
   make clean && make PLATFORM=linux
   make test
   ```

### Expected Failures

- **Compilation errors:** Missing `#include "hal/hal_*.h"`
- **Link errors:** HAL functions not implemented
- **Runtime errors:** HAL not initialized before VM

**Fix incrementally:** Add includes, implement missing HAL functions, ensure init order.

### Success Criteria

✅ VM builds using HAL interfaces
✅ All 936+ tests pass
✅ No platform-specific code in `src/vm.c`

---

## Phase 4: Migrate Physics Subsystems to use HAL

**Duration:** 2-3 days
**Risk:** Medium (determinism must be preserved)

### Files to Modify

- `src/dictionary_heat_optimization.c`
- `src/rolling_window_of_truth.c`
- `src/physics_hotwords_cache.c`
- `src/physics_pipelining_metrics.c`
- `src/inference_engine.c`
- `src/heartbeat.c`

### Tasks

1. **Audit timing calls in physics subsystems**
   ```bash
   grep -n "clock_gettime\|time\.h" src/*physics*.c src/heartbeat.c src/inference*.c
   ```

2. **Replace timing calls**

   **Example: `src/rolling_window_of_truth.c`**
   ```c
   /* Before: */
   #include <time.h>
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   uint64_t timestamp = ...;

   /* After: */
   #include "hal/hal_time.h"
   uint64_t timestamp = hal_time_now_ns();
   ```

3. **Migrate heartbeat to HAL timer**

   **Before:** `src/heartbeat.c` uses pthread + sleep
   ```c
   void *heartbeat_thread(void *arg) {
       while (running) {
           usleep(period_us);
           vm_tick(vm);
       }
   }
   ```

   **After:** Use HAL periodic timer
   ```c
   static VM *heartbeat_vm = NULL;

   static void heartbeat_isr(void *ctx) {
       (void)ctx;
       if (heartbeat_vm) {
           vm_tick_isr(heartbeat_vm);  /* ISR-safe VM tick */
       }
   }

   void heartbeat_start(VM *vm, uint64_t rate_hz) {
       heartbeat_vm = vm;
       uint64_t period_ns = 1000000000ULL / rate_hz;
       hal_timer_periodic(period_ns, heartbeat_isr, NULL);
   }
   ```

   **Critical:** `vm_tick_isr()` must be ISR-safe:
   - No malloc/free
   - No blocking I/O
   - Only lock-free ring buffer updates

4. **Test deterministic behavior**
   ```bash
   make fastest PLATFORM=linux
   ./build/amd64/fastest/starforth --doe > results_after_hal.csv
   ```

   Compare with baseline (before HAL migration):
   ```bash
   diff results_before_hal.csv results_after_hal.csv
   ```

   **Expected:** Identical results (0% algorithmic variance).

### Success Criteria

✅ Physics subsystems use HAL for all timing
✅ Heartbeat runs via HAL periodic timer
✅ 0% algorithmic variance maintained
✅ All tests pass

---

## Phase 5: Migrate REPL and Word Implementations

**Duration:** 1-2 days
**Risk:** Low (mostly console I/O)

### Files to Modify

- `src/repl.c` - Read-eval-print loop
- `src/word_source/*_words.c` - Words that do I/O (emit, key, etc.)

### Tasks

1. **Replace stdio in REPL**

   **Before:** `src/repl.c`
   ```c
   #include <stdio.h>

   char c = getchar();
   printf("ok\n");
   ```

   **After:**
   ```c
   #include "hal/hal_console.h"

   char c = hal_console_getc();
   hal_console_puts("ok\n");
   ```

2. **Update I/O words**

   **`src/word_source/io_words.c`:**
   ```c
   /* EMIT ( c -- ) */
   static void word_emit(VM *vm) {
       int c = vm_pop(vm);
       hal_console_putc((char)c);
   }

   /* KEY ( -- c ) */
   static void word_key(VM *vm) {
       int c = hal_console_getc();
       vm_push(vm, c);
   }
   ```

3. **Test REPL interactively**
   ```bash
   ./build/amd64/standard/starforth
   > 1 2 + .
   3  ok
   > BYE
   ```

### Success Criteria

✅ REPL works identically to before
✅ All I/O words use HAL
✅ No direct stdio calls in VM code

---

## Phase 6: Implement HAL for L4Re (Optional)

**Duration:** 3-5 days
**Risk:** Low (validates HAL portability)

This phase is **optional** but highly recommended to validate that the HAL abstraction actually works across platforms.

### Tasks

1. **Create L4Re HAL directory**
   ```bash
   mkdir -p src/platform/l4re/hal
   ```

2. **Implement L4Re HAL subsystems**
   - `hal_time.c` → L4Re clock API
   - `hal_memory.c` → L4Re dataspaces
   - `hal_console.c` → L4Re console service
   - `hal_interrupt.c` → L4Re IRQ objects

   See L4Re documentation for API details.

3. **Build for L4Re**
   ```bash
   make PLATFORM=l4re
   ```

4. **Run tests on L4Re**
   ```bash
   make PLATFORM=l4re test
   ```

### Success Criteria

✅ L4Re HAL compiles
✅ VM runs on L4Re without source changes
✅ Tests pass on L4Re

---

## Phase 7: Validate Deterministic Behavior

**Duration:** 1 day
**Risk:** High (final validation)

### Tasks

1. **Run DoE on Linux (HAL)**
   ```bash
   make fastest PLATFORM=linux
   for i in {1..10}; do
       ./build/amd64/fastest/starforth --doe > results_linux_$i.csv
   done
   ```

2. **Verify 0% variance across runs**
   ```bash
   # All CSV files should be identical
   md5sum results_linux_*.csv
   ```

3. **Compare with pre-HAL baseline**
   ```bash
   diff results_before_hal.csv results_linux_1.csv
   ```

   **Expected:** Identical (HAL added zero overhead).

4. **Run full test suite**
   ```bash
   make test PLATFORM=linux
   ```

   **Expected:** All 936+ tests pass.

5. **Benchmark performance**
   ```bash
   make bench PLATFORM=linux
   ```

   **Expected:** < 5% performance delta from pre-HAL baseline.

### Success Criteria

✅ 0% algorithmic variance maintained
✅ All tests pass
✅ < 5% performance regression
✅ HAL abstraction validated

---

## Rollback Strategy

If migration fails at any phase:

1. **Git branch strategy**
   ```bash
   git checkout -b hal-migration
   # Work on branch, test each phase
   git commit -m "Phase N complete"
   # If phase fails, revert:
   git reset --hard HEAD~1
   ```

2. **Incremental commits**
   - Commit after each phase passes tests
   - Never commit broken code
   - Each commit should build and pass tests

3. **Feature flag (if needed)**
   ```c
   #ifdef ENABLE_HAL
       uint64_t now = hal_time_now_ns();
   #else
       struct timespec ts;
       clock_gettime(CLOCK_MONOTONIC, &ts);
       uint64_t now = ...;
   #endif
   ```

   Enable HAL incrementally, fall back if issues arise.

---

## Post-Migration Cleanup

After successful migration:

1. **Remove old platform code**
   ```bash
   # If src/platform/linux/time.c was fully replaced by hal_time.c
   git rm src/platform/linux/time.c
   ```

2. **Update documentation**
   - `docs/CLAUDE.md` - Mention HAL architecture
   - `README.md` - Update build instructions
   - `docs/DEVELOPER.md` - Add HAL section

3. **Update .gitignore**
   ```
   # Build artifacts for all platforms
   build/linux/
   build/l4re/
   build/kernel/
   ```

---

## Timeline Estimate

| Phase | Duration | Cumulative |
|-------|----------|------------|
| 1. Define HAL Interfaces | 1-2 days | 1-2 days |
| 2. Implement Linux HAL | 3-5 days | 4-7 days |
| 3. Migrate VM Core | 2-3 days | 6-10 days |
| 4. Migrate Physics | 2-3 days | 8-13 days |
| 5. Migrate REPL | 1-2 days | 9-15 days |
| 6. L4Re HAL (optional) | 3-5 days | 12-20 days |
| 7. Validation | 1 day | 13-21 days |

**Total:** 2-4 weeks (depending on L4Re inclusion)

---

## Success Metrics

The HAL migration is successful if:

1. ✅ **All tests pass** - 936+ tests on Linux (and L4Re if implemented)
2. ✅ **0% variance** - DoE results identical pre/post migration
3. ✅ **No regressions** - Performance < 5% slower
4. ✅ **Zero platform code in VM** - No #ifdef PLATFORM_X in core
5. ✅ **Clean abstractions** - HAL interfaces well-documented
6. ✅ **Ready for LithosAnanke** - Platform layer can be replaced

---

## Next Steps

After successful HAL migration:

1. **Implement LithosAnanke platform** (`src/platform/kernel/`)
2. **UEFI boot loader** (`src/platform/kernel/boot/uefi_loader.c`)
3. **Boot to `ok` prompt** on QEMU/OVMF
4. **Validate physics on bare metal** (determinism on real hardware)

See `lithosananke-integration.md` for LithosAnanke-specific implementation details.