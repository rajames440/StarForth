<!-- Moved from docs/03-architecture/OVERVIEW.md to docs/working/architecture/03-architecture/OVERVIEW.md on 2026-06-16 (docs reorg Phase 2) -->
# StarForth Architecture Overview

**Status:** Complete
**Last Updated:** 2025-12-14
**Audience:** Developers, Architects, Contributors

## Executive Summary

StarForth is a FORTH-79 compliant virtual machine with a **physics-driven adaptive runtime**, formally proven to achieve **0% algorithmic variance** across experimental runs. The architecture consists of three layers (VM, HAL, Platform) and seven physics feedback loops that enable self-optimization while maintaining deterministic behavior.

**Key Innovation:** Physics-grounded metaphor (execution heat, rolling window of truth) that drives adaptive optimization without sacrificing reproducibility.

---

## System Architecture

### Three-Layer Model

```
┌──────────────────────────────────────────────────────────┐
│  Layer 1: VM Core + Physics Subsystems                   │
│  • FORTH-79 interpreter (vm.c)                           │
│  • Dictionary (words, compilation)                       │
│  • Stacks (data, return)                                 │
│  • Physics subsystems (heat, window, cache, pipelining)  │
│  • Heartbeat coordinator                                 │
│                                                           │
│  ↓ Platform-agnostic (calls HAL only)                    │
├──────────────────────────────────────────────────────────┤
│  Layer 2: Hardware Abstraction Layer (HAL)               │
│  • hal_time.h - Timing & timers                          │
│  • hal_interrupt.h - IRQ management                      │
│  • hal_memory.h - Memory allocation                      │
│  • hal_console.h - I/O                                   │
│  • hal_cpu.h - CPU control                               │
│                                                           │
│  ↓ Platform-specific implementations                     │
├──────────────────────────────────────────────────────────┤
│  Layer 3: Platform Implementations                       │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │ Linux        │ L4Re         │ StarKernel (planned) │ │
│  │ (POSIX)      │ (microkernel)│ (freestanding)       │ │
│  ├──────────────┼──────────────┼──────────────────────┤ │
│  │ clock_gettime│ L4Re::Clock  │ TSC + HPET + APIC    │ │
│  │ malloc/free  │ dataspaces   │ PMM + VMM + kmalloc  │ │
│  │ stdin/stdout │ L4Re console │ UART + framebuffer   │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

---

## Core Components

### 1. VM Core (`src/vm.c`)

**Responsibilities:**
- FORTH-79 interpreter loop
- Dictionary management
- Stack operations (data, return)
- Word execution
- Compilation (`:`, `;`)

**Key Data Structures:**
```c
typedef struct VM {
    vaddr_t data_stack[STACK_SIZE];
    vaddr_t return_stack[STACK_SIZE];
    vaddr_t dict_ptr;                   /* Dictionary pointer */
    vaddr_t here;                       /* Compilation pointer */
    DictEntry *latest;                  /* Most recent word */
    HeartbeatState heartbeat;           /* Timing coordinator */
    RollingWindowOfTruth *window;       /* Execution history */
    /* ... */
} VM;
```

**Execution Flow:**
1. Fetch next word from input stream
2. Search dictionary for word
3. If found: execute or compile (based on `WORD_IMMEDIATE`)
4. If not found: try to parse as number
5. Update execution heat (physics feedback)
6. Continue

### 2. Dictionary System

**Structure:**
- Linked list of `DictEntry` nodes
- Each entry contains: name, code pointer, flags, physics metadata
- Linear search with hot-words cache acceleration

**Dictionary Entry:**
```c
typedef struct DictEntry {
    char name[32];
    void (*code_ptr)(VM *vm);
    uint32_t flags;                     /* IMMEDIATE, HIDDEN, etc. */
    float execution_heat;               /* Physics: frequency tracking */
    PhysicsMetadata physics;            /* Window samples, decay state */
    TransitionMetrics *transitions;     /* Pipelining: successor prediction */
    struct DictEntry *next;             /* Linked list */
} DictEntry;
```

### 3. Memory Model

**Address Space:**
- `vaddr_t` (VM addresses) = byte offsets, not C pointers
- Dictionary: 0 → 2MB
- User blocks: block 2048+
- Heap: allocated via HAL

**Memory Access:**
```c
cell_t vm_load_cell(VM *vm, vaddr_t addr);  /* Safe load */
void vm_store_cell(VM *vm, vaddr_t addr, cell_t value);  /* Safe store */
```

---

## Physics Subsystems

The **physics-driven adaptive runtime** consists of six coordinated subsystems:

### 1. Execution Heat Model (`dictionary_heat_optimization.c`)

**Mechanism:** Words that execute frequently accumulate higher execution counts

**Implementation:**
```c
void track_execution_heat(DictEntry *entry) {
    entry->execution_heat += 1.0f;  /* Increment on each execution */
}
```

**Purpose:** Identifies frequently-executed words for optimization

**Feedback:** Positive (hot words get hotter via cache hits)

### 2. Rolling Window of Truth (`rolling_window_of_truth.c`)

**Metaphor:** Circular buffer captures execution history

**Implementation:**
```c
typedef struct RollingWindowOfTruth {
    ExecutionSample samples[WINDOW_SIZE];
    int head;               /* Write position */
    int count;              /* Number of valid samples */
    uint64_t timestamp_ns;  /* Last sample time */
} RollingWindowOfTruth;
```

**Purpose:** Deterministic seeding of physics metrics (ANOVA, Levene's test)

**Key Insight:** Fixed-size window ensures deterministic sample selection

### 3. Hot-Words Cache (`physics_hotwords_cache.c`)

**Mechanism:** Reorder dictionary based on execution heat

**Algorithm:**
1. Sort dictionary entries by `execution_heat`
2. Move hot words to front of linked list
3. Accelerates linear search (hot words found first)

**Performance Impact:** 10-30% speedup on realistic workloads

**Determinism:** Cache updates triggered by heat decay, not execution order

### 4. Pipelining Metrics (`physics_pipelining_metrics.c`)

**Mechanism:** Predict next word based on current word

**Implementation:**
```c
typedef struct TransitionMetrics {
    DictEntry *successor;      /* Most common next word */
    uint32_t transition_count; /* How many times seen */
} TransitionMetrics;
```

**Speculative Execution:** Prefetch successor word (future work)

**Determinism:** Transition counts updated deterministically

### 5. Inference Engine (`inference_engine.c`)

**Adaptive Parameters:**
- **Window width** - How many samples to analyze
- **Decay slope** - How fast heat decays

**Statistical Methods:**
- **ANOVA early-exit** - Stop when variance stabilizes
- **Levene's test** - Validate homogeneity of variance
- **Exponential regression** - Fit decay curve

**Key Property:** Inference uses deterministic algorithms (no randomness)

### 6. Heartbeat System (`src/heartbeat.c`)

**Purpose:** Centralized time-driven coordinator

**Responsibilities:**
- Trigger Loop #3 (heat decay) periodically
- Trigger Loop #5 (window width inference) periodically
- Coordinate all time-dependent operations

**Implementation:**
```c
void vm_tick(VM *vm) {
    if (!vm->heartbeat.enabled) return;

    vm->heartbeat.tick_count++;

    /* Loop #3: Heat decay */
    if (should_decay_heat(vm)) {
        decay_execution_heat(vm);
    }

    /* Loop #5: Window width inference */
    if (should_tune_window(vm)) {
        infer_window_width(vm);
    }
}
```

**Timing:** Periodic timer via HAL (`hal_timer_periodic()`)

---

## Seven Physics Feedback Loops

| Loop | Name | Type | Trigger | Effect |
|------|------|------|---------|--------|
| #1 | Execution Heat Tracking | Positive | Every word execution | Increments `execution_heat` |
| #2 | Rolling Window History | Neutral | Every execution | Captures sample in circular buffer |
| #3 | Linear Decay | Negative | Heartbeat tick | Decays `execution_heat` over time |
| #4 | Pipelining Metrics | Positive | Word→word transition | Increments `transition_count` |
| #5 | Window Width Inference | Adaptive | Heartbeat tick | Adjusts window size via Levene's test |
| #6 | Decay Slope Inference | Adaptive | Heartbeat tick | Adjusts decay rate via regression |
| #7 | Adaptive Heartrate | Adaptive | System load | Adjusts heartbeat frequency (future) |

**Determinism Proof:** All loops use deterministic algorithms; 0% algorithmic variance validated experimentally.

See `physics-engine/feedback-loops.md` for detailed analysis.

---

## Boot Sequence

### Hosted Platforms (Linux, L4Re)

```
main()
  ↓
hal_*_init()          /* Initialize HAL subsystems */
  ↓
vm_create()           /* Allocate VM struct */
  ↓
vm_init_dictionary()  /* Populate built-in words */
  ↓
vm_init_physics()     /* Initialize heat, window, cache */
  ↓
heartbeat_start()     /* Start periodic timer */
  ↓
repl()                /* Enter REPL loop */
  ↓
vm_destroy()          /* Clean shutdown */
```

### StarKernel (Planned)

```
UEFI Firmware
  ↓
BOOTX64.EFI (uefi_loader.c)
  ↓ Collect BootInfo (memory map, ACPI, framebuffer)
  ↓ ExitBootServices()
  ↓
kernel_main()
  ↓
hal_*_init()          /* Kernel HAL implementations */
  ↓
vm_create()           /* VM starts in kernel mode */
  ↓
repl()                /* Forth as kernel shell */
```

See `hal/starkernel-integration.md` for kernel boot details.

---

## Data Flow

### Word Execution Path

```
User Input ("1 2 + .")
  ↓
tokenize()            /* Split into words */
  ↓
For each word:
  ├─> search_dictionary()
  │     ├─> hot_words_cache (check front)
  │     └─> linear_search (fallback)
  ↓
  ├─> execute_word()
  │     ├─> track_execution_heat()
  │     ├─> record_transition()
  │     └─> update_rolling_window()
  ↓
  └─> Output ("3")
```

### Physics Feedback Cycle

```
Word Execution
  ↓
Execution Heat += 1.0
  ↓
Rolling Window records sample
  ↓
Heartbeat Tick (periodic)
  ├─> Heat Decay (Loop #3)
  │     └─> Heat *= decay_factor
  ├─> Window Inference (Loop #5)
  │     └─> Levene's test → adjust window_width
  └─> (future) Decay Inference (Loop #6)
        └─> Regression → adjust decay_slope
  ↓
Hot-Words Cache Refresh
  ↓
(cycle repeats)
```

---

## Control Flow

### Compile Mode vs. Execute Mode

**Execute Mode:**
- Default state
- Words execute immediately when encountered

**Compile Mode:**
- Entered via `:` (colon)
- Words are compiled into dictionary (not executed)
- Exception: `WORD_IMMEDIATE` words execute even in compile mode

**Example:**
```forth
: SQUARE   ( n -- n² )   DUP * ;

\ During ":"
\   - "DUP" compiled (added to SQUARE's definition)
\   - "*" compiled
\ During ";"
\   - Exit compile mode
\   - SQUARE now in dictionary
```

### Interrupt Context (StarKernel)

**ISR-Safe Operations:**
- `heartbeat_tick_isr()` - ISR callback
- `vm_tick()` - Can run in ISR (no malloc/blocking I/O)
- Rolling window updates (lock-free)

**NOT ISR-Safe:**
- `hal_mem_alloc()` - Blocking
- `hal_console_getc()` - Blocking
- Dictionary compilation - Not reentrant

---

## Cross-Subsystem Interactions

### Heartbeat ↔ Physics

```
Heartbeat Timer (HAL)
  ↓ Fires periodic interrupt
heartbeat_isr()
  ↓ Calls
vm_tick()
  ↓ Coordinates
├─> dictionary_heat_decay()
├─> infer_window_width()
└─> (future) infer_decay_slope()
```

### Dictionary ↔ Cache

```
New Word Defined
  ↓
dictionary_add_word()
  ↓ Invalidates
hot_words_cache_invalidate()
  ↓ Triggers rebuild
hot_words_cache_rebuild()
  ↓ Sorts by
execution_heat
  ↓ Reorders
linked list
```

### Window ↔ Inference

```
Word Execution
  ↓
rolling_window_add_sample()
  ↓ Buffer fills
window_is_full()
  ↓ Triggers
infer_window_width()
  ↓ Analyzes via
levene_test()
  ↓ Adjusts
window->width
```

---

## Platform Abstraction

See [`hal/overview.md`](hal/overview.md) for comprehensive HAL documentation.

**Key Principle:** VM code never knows which platform it's on.

**Example:**
```c
/* VM code (platform-agnostic) */
uint64_t start = hal_time_now_ns();
execute_word(vm, word);
uint64_t elapsed = hal_time_now_ns() - start;

/* Linux HAL implementation */
uint64_t hal_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* Kernel HAL implementation */
uint64_t hal_time_now_ns(void) {
    return tsc_to_ns(rdtsc());  /* Direct hardware access */
}
```

---

## Roadmap: StarForth → StarKernel → StarshipOS

### Phase 1: StarForth (DONE)
✅ FORTH-79 compliant interpreter
✅ Physics-driven adaptive runtime
✅ 0% algorithmic variance proven
✅ 780+ tests passing
✅ Runs on Linux, L4Re

### Phase 2: HAL Migration (IN PROGRESS)
🔄 Define HAL interfaces
🔄 Refactor Linux platform to use HAL
🔄 Migrate VM core to HAL
🔄 Validate determinism preserved
📋 Implement L4Re HAL (optional)

See `hal/migration-plan.md` for detailed timeline.

### Phase 3: StarKernel (PLANNED)
📋 Implement kernel HAL (`platform/kernel/`)
📋 UEFI boot loader (`boot/uefi_loader.c`)
📋 Physical Memory Manager (PMM)
📋 Virtual Memory Manager (VMM)
📋 Kernel heap allocator (kmalloc)
📋 UART + framebuffer console
📋 TSC + HPET + APIC timer
📋 Boot to `ok` prompt on bare metal
📋 Validate physics on hardware

See `hal/starkernel-integration.md` for implementation details.

### Phase 4: StarshipOS (FUTURE)
🔮 Storage drivers (AHCI, NVMe)
🔮 Filesystem (FAT32, ext2, log-structured)
🔮 Networking (TCP/IP stack, VirtIO-net)
🔮 Process model (Forth tasks, scheduling)
🔮 Device model (block/net/char unification)
🔮 Security (capabilities, ACL, Forth-based access control)

---

## Performance Characteristics

### Microbenchmarks

| Operation | Cycles | Notes |
|-----------|--------|-------|
| `DUP` | ~5 | Stack manipulation (hot path) |
| `+` | ~8 | Arithmetic (optimized) |
| Dictionary search (hot) | ~20 | Cache hit |
| Dictionary search (cold) | ~200 | Linear search |
| Word call overhead | ~15 | Indirect call |
| Heat tracking | ~3 | Increment + branch |

### Macrobenchmarks

| Workload | Performance | Configuration |
|----------|-------------|---------------|
| Fibonacci (recursive) | 100M iterations/sec | `make fastest` |
| Ackermann(3,9) | 15s | `make fastest` |
| DoE full suite | 2.3s | 780+ tests, `make test` |

**Key Insight:** Physics overhead < 5% on average workloads.

---

## Debugging & Diagnostics

### Built-in Diagnostics

```forth
WORD-ENTROPY    \ Print execution heat statistics
.S              \ Print data stack
.R              \ Print return stack
WORDS           \ List all dictionary words
```

### Development Builds

```bash
make debug      # -g -O0, full symbols
make PROFILE=1  # -pg, gprof profiling
```

### DoE Mode

```bash
./starforth --doe    # Run experiments, output metrics
```

**Output:** Test results, execution heat, window metrics, determinism validation

---

## Key Invariants

1. **Determinism:** 0% algorithmic variance (formally validated)
2. **Platform Agnostic:** VM code has zero `#ifdef PLATFORM_*`
3. **Memory Safety:** All VM addresses via `vaddr_t`, bounds-checked with `STRICT_PTR=1`
4. **FORTH-79 Compliance:** All standard words implemented correctly
5. **Zero Warnings:** Builds clean with `-Wall -Werror`
6. **ANSI C99:** No GNU extensions, no C++ features

---

## Further Reading

### Architecture Details
- **[hal/](hal/)** - Hardware Abstraction Layer
- **[heartbeat-system/](heartbeat-system/)** - Centralized timing coordinator
- **[physics-engine/](physics-engine/)** - Physics subsystem details
- **[pipelining/](pipelining/)** - Speculative execution
- **[adaptive-systems/](adaptive-systems/)** - Window & decay inference

### Development
- **`../01-getting-started/`** - Quick start guide
- **`../CONTRIBUTING.md`** - Contribution guidelines
- **`../CLAUDE.md`** - Project overview for developers

### Research
- **`../02-experiments/`** - Experimental protocols
- **`../06-research/`** - Academic publications
- **`../ONTOLOGY.md`** - System ontology and taxonomy

---

## Glossary

| Term | Definition |
|------|------------|
| **vaddr_t** | VM address (byte offset, not C pointer) |
| **Execution Heat** | Frequency tracking metric for words |
| **Rolling Window** | Circular buffer of execution samples |
| **HAL** | Hardware Abstraction Layer |
| **Heartbeat** | Periodic timer coordinating physics loops |
| **DoE** | Design of Experiments (experimental methodology) |
| **POST** | Power-On Self Test (unit test organization) |
| **PMM** | Physical Memory Manager (kernel) |
| **VMM** | Virtual Memory Manager (kernel) |
| **TSC** | Time Stamp Counter (x86_64 high-res timer) |
| **HPET** | High Precision Event Timer |
| **APIC** | Advanced Programmable Interrupt Controller |

---

*This overview provides the "30,000-foot view" of StarForth's architecture. For detailed implementation, see subsystem documentation.*