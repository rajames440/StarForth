/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

#ifndef VM_H
#define VM_H

/* Bare metal - no system headers, but we need FILE for heartbeat CSV export */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "platform_lock.h"

/* Forward declarations */
struct VM;
struct HotwordsCache_s; /* Physics hot-words cache (avoids circular include) */
typedef struct HotwordsCache_s HotwordsCache;
struct WordTransitionMetrics; /* Pipelining metrics (avoids circular include) */
typedef struct WordTransitionMetrics WordTransitionMetrics;
struct InferenceOutputs; /* Adaptive tuning inference engine outputs (avoids circular include) */
typedef struct InferenceOutputs InferenceOutputs;
struct HeartbeatWorker; /* Background heartbeat dispatcher */
typedef struct HeartbeatWorker HeartbeatWorker;

/* Bare metal type definitions */
typedef signed long cell_t;

/* ============================================================================
 * Rolling Window of Truth (Embedded to avoid circular includes)
 * ============================================================================
 *
 * Circular buffer capturing execution history for deterministic metric seeding.
 * Defined here to avoid include dependency issues.
 *
 * Knob #7: ROLLING_WINDOW_SIZE (tunable initial/conservative size)
 * - Default: 4096 (conservative capture of full pattern diversity at cold start)
 * - Usage: make ROLLING_WINDOW_SIZE=8192 (or 2048, 16384, etc. for different workloads)
 * - Starting large ensures statistical significance on cold systems
 * - System automatically shrinks during execution if diminishing returns detected
 * - Window is "warm" after ROLLING_WINDOW_SIZE executions
 *   (once warm, adaptive shrinking logic can reduce size if beneficial)
 * - Knobs control adaptive shrinking behavior (rate, bounds, thresholds)
 */

#ifndef ROLLING_WINDOW_SIZE
#define ROLLING_WINDOW_SIZE 4096
#endif

typedef struct
{
    uint32_t* execution_history; /* Circular buffer of word IDs */
    uint32_t* snapshot_buffers[2]; /* Double-buffer snapshots for readers */
    uint32_t window_pos; /* Current write position */
    uint64_t total_executions; /* Lifetime execution counter */
    int is_warm; /* 1 if window contains representative data */

    /* Adaptive window sizing: continuous self-tuning during execution */
    uint32_t effective_window_size; /* Current effective size (may shrink from initial) */
    uint64_t last_pattern_diversity; /* Pattern diversity at last check */
    uint64_t pattern_diversity_check_count; /* How many times we've checked diversity */
    volatile uint32_t snapshot_index; /* Published snapshot buffer */
    volatile uint32_t snapshot_pending; /* Writer signaled new data */
    uint32_t snapshot_window_pos[2];
    uint64_t snapshot_total_executions[2];
    uint32_t snapshot_effective_window_size[2];
    int snapshot_is_warm[2];
    uint32_t adaptive_check_accumulator; /* Execution counter toward next check */
    volatile uint32_t adaptive_pending;  /* Heartbeat should run adaptive shrink */
} RollingWindowOfTruth;


typedef void (*word_func_t)(struct VM* vm);

/* ===== VM address model (Phase 1 scaffolding) ============================
 * StarForth rule: addresses on the data stack are VM OFFSETS, not C pointers.
 * vaddr_t is a byte offset into the VM's virtual address space.
 * Implementations come in a later change; these are declarations only.
 */
typedef uint64_t vaddr_t;

/* Bounds check: nonzero if [addr, addr+len) is a valid VM range */
int vm_addr_ok(struct VM* vm, vaddr_t addr, size_t len);

/* Internal pointer materialization for subsystems (NOT for word sources) */
uint8_t* vm_ptr(struct VM* vm, vaddr_t addr);

/* Canonical memory accessors; all word code will funnel through these */
uint8_t vm_load_u8(struct VM* vm, vaddr_t addr);

void vm_store_u8(struct VM* vm, vaddr_t addr, uint8_t v);

cell_t vm_load_cell(struct VM* vm, vaddr_t addr); /* requires alignment */
void vm_store_cell(struct VM* vm, vaddr_t addr, cell_t v);

/* Explicit stack<->offset conversions (keep intent obvious) */
static inline vaddr_t VM_ADDR(cell_t c) { return (vaddr_t)(uint64_t)c; }
static inline cell_t CELL(vaddr_t a) { return (cell_t)(int64_t)a; }
/* ======================================================================== */

#define STACK_SIZE 1024
#define DICTIONARY_SIZE 1024
#define WORD_ID_INVALID UINT32_MAX
#define VM_MEMORY_SIZE (5 * 1024 * 1024)  /* 5 MB total VM memory */
#define INPUT_BUFFER_SIZE 256
#define WORD_NAME_MAX 31
#define COMPILE_BUFFER_SIZE 1024

/* Block system configuration */
#define BLOCK_SIZE 1024                                 /* 1KB per block */
#define MAX_BLOCKS (VM_MEMORY_SIZE / BLOCK_SIZE)        /* 5120 blocks from 5MB */

/* Memory layout constants */
#define DICTIONARY_BLOCKS 2048                          /* First 2048 blocks (2MB) for dictionary */
#define DICTIONARY_MEMORY_SIZE (DICTIONARY_BLOCKS * BLOCK_SIZE)
#define USER_BLOCKS_START DICTIONARY_BLOCKS             /* User blocks start at block 2048 */

/* Persistent log configuration */
#define LOG_LINE_MAX 64                                 /* Each log line is 64 bytes */
#define LOG_LINES_PER_BLOCK (BLOCK_SIZE / LOG_LINE_MAX) /* 16 lines per 1KB block */
#define LOG_BLOCKS_START 3072                           /* Log starts at block 3072 */
#define LOG_BLOCKS_END 5120                             /* Log ends at block 5120 (exclusive) */
#define LOG_BLOCKS (LOG_BLOCKS_END - LOG_BLOCKS_START)  /* 2048 blocks (2MB) for logs */
#define LOG_LAYER1_MAX_LINES (LOG_BLOCKS * LOG_LINES_PER_BLOCK) /* 32768 max lines */

/* Word flags */
#define WORD_IMMEDIATE  0x80    /* Word executes immediately even in compile mode */
#define WORD_HIDDEN     0x40    /* Word is hidden from dictionary searches */
#define WORD_SMUDGED    0x20    /* Word is smudged (being defined) - FORTH-79 */
#define WORD_COMPILED   0x10    /* Word is user-compiled (not built-in) */
#define WORD_PINNED     0x08    /* Word's execution heat is pinned (cannot decay to zero) */
#define WORD_FROZEN     0x04    /* Word is frozen: execution heat does not decay (Phase 2) */

/* ACL (Access Control List) defaults - stub implementation */
#define ACL_USER_DEFAULT 0x01   /* Default access: users can execute and compile */

/* Physics model constants - execution heat drives optimization decisions */
#define SPIN_IDLE        0      /* Particle spin state: idle */
#define CHARGE_NEUTRAL   0      /* Particle charge state: neutral (execution heat acts as charge) */

/* Physics state flags */
#define PHYSICS_STATE_IMMEDIATE 0x01
#define PHYSICS_STATE_PINNED    0x02
#define PHYSICS_STATE_HIDDEN    0x04
#define PHYSICS_STATE_COMPILED  0x08

/* Phase 2: Decay mechanism configuration (tunable via Makefile) */
#ifndef DECAY_RATE_PER_US_Q16
#define DECAY_RATE_PER_US_Q16 1ULL      /* Q48.16: 1/65536 heat/μs = ~15 heat/sec, half-life ~6-7 seconds for 100-heat word (adaptive baseline) */
#endif

#ifndef DECAY_MIN_INTERVAL
#define DECAY_MIN_INTERVAL 1000ULL      /* Min elapsed time before decay applies (1 μs) */
#endif

#ifndef HEAT_CACHE_DEMOTION_THRESHOLD
#define HEAT_CACHE_DEMOTION_THRESHOLD 10  /* Demotion from hotwords cache threshold */
#endif

/* Heartbeat tuning frequencies (in ticks) */
#ifndef HEARTBEAT_CHECK_FREQUENCY
#define HEARTBEAT_CHECK_FREQUENCY 256  /* Call vm_tick() every 256 word executions */
#endif

#ifndef HEARTBEAT_INFERENCE_FREQUENCY
#define HEARTBEAT_INFERENCE_FREQUENCY 5000  /* Run unified inference engine every 5000 ticks */
#endif

#ifndef HEARTBEAT_WINDOW_TUNING_FREQUENCY
#define HEARTBEAT_WINDOW_TUNING_FREQUENCY 1000  /* Tune window every 1000 ticks */
#endif

#ifndef HEARTBEAT_SLOPE_VALIDATION_FREQUENCY
#define HEARTBEAT_SLOPE_VALIDATION_FREQUENCY 5000  /* Validate decay every 5000 ticks */
#endif

#ifndef HEARTBEAT_THREAD_ENABLED
#define HEARTBEAT_THREAD_ENABLED 0
#endif

#ifndef HEARTBEAT_TICK_NS
#define HEARTBEAT_TICK_NS 1000000ULL  /* 1 millisecond */
#endif


typedef struct
{
    uint64_t published_tick;        /* Tick counter when snapshot committed */
    uint64_t published_ns;          /* Host monotonic timestamp of publish */
    uint32_t window_width;          /* Effective rolling window size */
    uint64_t decay_slope_q48;       /* Active decay slope */
    uint64_t hot_word_count;        /* Words above heat threshold */
    uint64_t stale_word_count;      /* Words in cooling band */
    uint64_t total_heat;            /* Aggregate execution heat */
} HeartbeatSnapshot;

/* ===== Per-Tick Instrumentation (Phase 2: Multivariate Dynamics) ==========
 *
 * Lightweight circular buffer capture for multivariate systems analysis.
 * Captures 7 key metrics every heartbeat tick (~1ms) with negligible overhead.
 * Used to measure coupled dynamics, convergence, coupling strength, stability.
 */
typedef struct
{
    uint32_t tick_number;           /* Sequential tick counter */
    uint64_t elapsed_ns;            /* Total elapsed since run start */
    uint64_t tick_interval_ns;      /* Actual tick interval from prior tick */

    uint32_t cache_hits_delta;      /* Cache hits during this tick */
    uint32_t bucket_hits_delta;     /* Bucket hits during this tick */
    uint32_t word_executions_delta; /* Words executed during this tick */

    uint64_t hot_word_count;        /* Words above heat threshold */
    double avg_word_heat;           /* Mean execution heat (Q48.16 / 65536) */
    uint32_t window_width;          /* Current rolling window size */

    uint32_t predicted_label_hits;  /* Successful context predictions */
    double estimated_jitter_ns;     /* Deviation from nominal tick */

    uint8_t l8_mode;                /* L8 Jacquard mode selector state (0-15) */
} HeartbeatTickSnapshot;

#define HEARTBEAT_TICK_BUFFER_SIZE 100000  /* ~100K ticks = ~100 seconds at 1kHz */

/* Phase 1 physics metadata */
typedef struct
{
    uint16_t temperature_q8; /* Execution-heat-backed hotness scaled to Q8 */
    uint64_t last_active_ns; /* Monotonic timestamp of last execution */
    uint64_t last_decay_ns;  /* Timestamp of last decay application */
    uint32_t mass_bytes; /* Header + body footprint */
    uint32_t avg_latency_ns; /* Rolling average latency (PROFILE_DETAILED+) */
    uint8_t state_flags; /* Encoded execution traits (immediate, pinned, etc.) */
    uint8_t acl_hint; /* Reserved for governance-driven ACLs */
    uint16_t pubsub_mask; /* Reserved topic bitmap */
} DictPhysics;

/* ===== VM Heartbeat Architecture (Loop #3 & #5 Coordination) ============
 *
 * Centralized time-driven tuning dispatcher coordinating:
 * - Loop #3: Heat decay validation (slope_validator, replaced by inference engine)
 * - Loop #5: Context-aware window tuning (window_tuner, replaced by inference engine)
 * - Unified Inference Engine: ANOVA early-exit + window width + decay slope inference
 *
 * Can run synchronously (now) or in background thread (future).
 * Designed as plugin architecture for extensibility.
 */
typedef struct
{
    uint64_t tick_count;               /* Total heartbeat ticks since VM init */
    uint64_t last_inference_tick;      /* Last tick we ran full inference engine */
    uint32_t check_counter;            /* Counter to trigger vm_tick() every N executions */
    int      heartbeat_enabled;        /* 1 = heartbeat active, 0 = disabled */
    uint64_t tick_target_ns;           /* Wake-up cadence (ns) */
    volatile uint32_t snapshot_index;  /* Published snapshot slot */
    HeartbeatSnapshot snapshots[2];    /* Double-buffered read-only snapshots */
    HeartbeatWorker* worker;           /* Background thread context (opaque) */

    /* === DoE Observation Counters (2^7 Factorial) === */
    uint64_t inference_run_count;      /* Times inference engine was invoked */
    uint64_t early_exit_count;         /* ANOVA early-exits (variance stable) */
    uint64_t words_executed;           /* Total word executions */
    uint64_t dictionary_lookups;       /* Dictionary search operations */

    /* === Per-tick instrumentation (Phase 2: Multivariate Dynamics) === */
    HeartbeatTickSnapshot* tick_buffer;     /* Circular buffer of per-tick snapshots */
    uint32_t tick_buffer_size;              /* Allocated size (HEARTBEAT_TICK_BUFFER_SIZE) */
    uint64_t tick_buffer_write_index;       /* Current write position (wraps) */
    uint64_t tick_count_total;              /* Total ticks since run start (monotonic) */
    uint64_t run_start_ns;                  /* Monotonic time at run start */
    uint32_t tick_number_offset;            /* Tick counter for this run */

    /* === L8 Attractor Bucket Statistics === */
    double bucket_sum_K;                    /* Sum of K values across bucket */
    double bucket_sum_K_squared;            /* Sum of K^2 for variance calculation */
    double bucket_sum_window_variance;      /* Sum of window variance across bucket */
    double bucket_sum_heat_variance;        /* Sum of heat variance across bucket */
    uint64_t bucket_mode_transitions;       /* Count of L8 mode transitions in bucket */
    uint32_t bucket_collapse_flag;          /* Flag indicating bucket collapse event */
    uint32_t bucket_tick_count;             /* Number of ticks in current bucket */
} HeartbeatState;

/* ===== Pipelining Global Metrics (Loop #4 & #5 Feedback) ================
 *
 * Aggregated metrics for speculative prefetch accuracy tracking.
 * Used to guide window size tuning via binary chop (Loop #5).
 */
typedef struct
{
    uint64_t prefetch_attempts;        /* Total speculative prefetch calls */
    uint64_t prefetch_hits;            /* Successful hits (word was looked up next) */
    uint64_t window_tuning_checks;     /* How many times we've checked window size */
    uint32_t last_checked_window_size; /* Window size at last tuning check */
    double   last_checked_accuracy;    /* Prefetch accuracy at last check */
    uint32_t suggested_next_size;      /* What binary chop recommends trying */
} PipelineGlobalMetrics;

/* Dictionary entry - enhanced for FORTH-79 compatibility */
typedef struct DictEntry
{
    struct DictEntry* link; /* Previous word (linked list) */
    word_func_t func; /* Function pointer for execution */
    uint8_t flags; /* Word flags */
    uint8_t name_len; /* Length of name */
    cell_t execution_heat; /* Execution frequency counter - drives optimization decisions */
    uint8_t acl_default; /* Access control list default permissions - stub */
    uint32_t word_id; /* Stable dictionary identifier for transition tracking */
    DictPhysics physics; /* Physics properties for elementary particle model */
    WordTransitionMetrics* transition_metrics; /* Word-to-word transition tracking for pipelining */
    char name[]; /* Variable-length name + optional code */
} DictEntry;

/* VM modes */
typedef enum
{
    MODE_INTERPRET = 0,
    MODE_COMPILE = 1
} vm_mode_t;

/**
 * @struct VM
 * @brief Main virtual machine state container
 *
 * Contains all state information for a StarForth VM instance including stacks,
 * dictionary, memory, and execution state.
 */
typedef struct VM
{
    /** @name Stack Management
     * @{
     */
    cell_t data_stack[STACK_SIZE]; /**< Parameter stack storage */
    cell_t return_stack[STACK_SIZE]; /**< Return stack storage */
    int dsp; /**< Data stack pointer */
    int rsp; /**< Return stack pointer */
    int exit_colon; /**< Exit flag for colon definitions */
    int abort_requested; /**< ABORT flag for immediate termination */
    /** @} */

    /** @name Dictionary Management 
     * @{
     */
    uint8_t* memory; /**< Unified VM memory buffer */
    size_t here; /**< Next free memory location (byte offset) */
    DictEntry* latest; /**< Most recent word */
    DictEntry* word_id_map[DICTIONARY_SIZE]; /**< Stable ID → entry map for speculation */
    uint32_t recycled_word_ids[DICTIONARY_SIZE]; /**< Reusable ID stack for FORGET */
    uint32_t recycled_word_id_count; /**< Depth of recycled stack */
    uint32_t next_word_id; /**< Next fresh ID when recycle stack empty */

    /* Dictionary protection fence: words at/older than this are protected from FORGET */
    DictEntry* dict_fence_latest;
    size_t dict_fence_here;
    sf_mutex_t dict_lock;            /**< Protects dictionary structural mutations */

    /* Input system */
    char input_buffer[INPUT_BUFFER_SIZE];
    size_t input_length;
    size_t input_pos;

    /* Compiler state */
    vm_mode_t mode;
    DictEntry* compiling_word; /* Word being compiled */

    /* Compilation support */
    char current_word_name[WORD_NAME_MAX + 1];
    cell_t* compile_buffer;
    size_t compile_pos;
    size_t compile_size;

    /* FORTH-79 Dictionary manipulation support */
    cell_t state_var; /* STATE variable (0 interpret, -1 compile) */

    /* Execution bookkeeping */
    DictEntry* current_executing_entry;
    cell_t* ip; /* direct-threaded instruction pointer (points into memory in CELLS) */

    /* VM state */
    int error;
    int halted;

    /* Numeric base (Forth BASE). Default 10. */
    cell_t base;

    /* TIB stuff */
    unsigned char* tib_buf; /* pointer to TIB buffer (host alloc for now) */
    size_t tib_cap; /* capacity in bytes */
    cell_t* in_var; /* >IN (legacy; will migrate to VM addr) */
    cell_t* span_var; /* SPAN (legacy; will migrate to VM addr) */

    /* Block system (VM-backed variables / addresses) */
    vaddr_t scr_addr;
    vaddr_t state_addr; /* VM cell holding STATE (0=interp, -1=compile) */
    vaddr_t base_addr; /* VM cell: numeric base (2..36), default 10 */

    /** @name Physics Hot-Words Cache
     * @{
     */
    HotwordsCache* hotwords_cache; /**< Frequency-driven dictionary acceleration cache */
    /** @} */

    /** @name Rolling Window of Truth (Deterministic Metrics)
     * @{
     */
    RollingWindowOfTruth rolling_window; /**< Execution history for reproducible seeding */
    /** @} */

    /** @name Phase 2: Heat-Aware Dictionary Optimization
     * @{
     */
    cell_t heat_threshold_25th;     /**< 25th percentile heat (bucket search priority) */
    cell_t heat_threshold_50th;     /**< 50th percentile heat */
    cell_t heat_threshold_75th;     /**< 75th percentile heat */
    uint64_t last_bucket_reorg_ns;  /**< When we last reorganized buckets */
    int lookup_strategy;            /**< 0=naive, 1=heat-aware, 2=inference-reorg */
    /** @} */

    /** @name VM Heartbeat (Loop #3 & #5 Coordination)
     * @{
     */
    HeartbeatState heartbeat;       /**< Centralized time-driven tuning dispatcher */
    uint32_t heartbeat_decay_cursor_id; /**< Continuation cursor for background decay sweeps */
    /** @} */

    /** @name Pipelining Global Metrics (Loop #4 & #5 Feedback)
     * @{
     */
    PipelineGlobalMetrics pipeline_metrics; /**< Aggregated prefetch accuracy tracking */
    /** @} */

    /** @name Physics Loop #3: Adaptive Heat Decay Tuning
     * @{
     */
    uint64_t decay_slope_q48;           /**< Current decay rate (Q48.16 fixed-point, starts at 2:1 = 131072) */
    uint64_t last_decay_check_ns;       /**< Timestamp of last decay effectiveness measurement */
    uint64_t total_heat_at_last_check;  /**< Total dictionary heat at last check (for slope inference) */
    uint64_t hot_word_count_at_check;   /**< Hot words observed at last inference */
    uint64_t stale_word_count_at_check; /**< Stale words at last check (for trend analysis) */
    uint32_t word_count_at_check;       /**< Total word count at last check (for ratio calculation) */
    int decay_slope_direction;          /**< -1 = decrease slope, 0 = stable, +1 = increase slope (inference) */
    sf_mutex_t tuning_lock;             /**< Guards shared tuning knobs (slope, snapshots) */
    /** @} */

    /** @name Unified Inference Engine (Phase 2: Replacing Loops #3 & #5)
     * @{
     */
    InferenceOutputs* last_inference_outputs;  /**< Cached inference outputs (for ANOVA early-exit and tuning application) */
    /** @} */

    /** @name SSM L8: Jacquard Mode Selector (Steady State Machine)
     * @{
     */
    void* ssm_l8_state;     /**< Opaque pointer to ssm_l8_state_t (avoid circular include) */
    void* ssm_config;       /**< Opaque pointer to ssm_config_t (L3/L4 mode bits) */
    /** @} */
} VM;

/** @name Core VM Functions
 * @{
 */
/**
 * @brief Initialize a new VM instance
 * @param vm Pointer to VM structure to initialize
 */
void vm_init(VM* vm);

/**
 * @brief Interpret a string of Forth code
 * @param vm VM instance to use
 * @param input String containing Forth code to interpret
 */
void vm_interpret(VM* vm, const char* input);

/**
 * @brief Start the VM's read-eval-print loop
 * @param vm VM instance to run
 * @param script_mode If 1, suppress prompts and "ok" output (for piped input)
 */
void vm_repl(VM* vm, int script_mode);

/** @} */

/* Stack operations */
void vm_push(VM* vm, cell_t value);

cell_t vm_pop(VM* vm);

void vm_rpush(VM* vm, cell_t value);

cell_t vm_rpop(VM* vm);

/* Dictionary operations */
DictEntry* vm_find_word(VM* vm, const char* name, size_t len);

DictEntry* vm_create_word(VM* vm, const char* name, size_t len, word_func_t func);

void vm_make_immediate(VM* vm);

void vm_hide_word(VM* vm);

void vm_smudge_word(VM* vm); /* Added for FORTH-79 SMUDGE */
void vm_pin_execution_heat(VM* vm); /* Pin execution heat (prevent decay) */
void vm_unpin_execution_heat(VM* vm); /* Unpin execution heat (allow decay) */

/* Enhanced dictionary search functions */
DictEntry* vm_dictionary_find_by_func(VM* vm, word_func_t func);

DictEntry* vm_dictionary_find_latest_by_func(VM* vm, word_func_t func);

DictEntry* vm_dictionary_lookup_by_word_id(VM* vm, uint32_t word_id);

void vm_dictionary_track_entry(VM* vm, DictEntry* entry);

void vm_dictionary_untrack_entry(VM* vm, DictEntry* entry);

cell_t* vm_dictionary_get_data_field(DictEntry* entry);

void vm_compile_word(VM* vm, DictEntry* entry);

/* Memory management */
void* vm_allot(VM* vm, size_t bytes);

void vm_align(VM* vm);

/* Input parsing */
int vm_parse_word(VM* vm, char* word, size_t max_len);

int vm_parse_number(VM* vm, const char* str, cell_t* value);

/* Compilation */
void vm_enter_compile_mode(VM* vm, const char* name, size_t len);

void vm_exit_compile_mode(VM* vm);

/* Colon word execution (exposed for SEE decompiler) */
void execute_colon_word(VM* vm);

void vm_compile_call(VM* vm, word_func_t func);

void vm_compile_literal(VM* vm, cell_t value);

void vm_compile_exit(VM* vm);

void vm_interpret_word(VM* vm, const char* word_str, size_t len);

/* Block system integration */
void* vm_get_block_addr(VM* vm, int block_num);

int vm_addr_to_block(VM* vm, void* addr);

/* Testing functions */
void vm_run_smoke_tests(VM* vm);

/* Add cleanup function declaration */
void vm_cleanup(VM* vm);

/* ===== VM Heartbeat (Time-Driven Tuning Dispatcher) =====================
 * Centralized coordination of Loop #3 and Loop #5 operations.
 * Called periodically from main execution loop.
 */
void vm_tick(VM* vm);                      /* Main heartbeat dispatcher */
void vm_tick_inference_engine(VM* vm);     /* Unified inference engine (Phase 2, replaces Loops #3 & #5) */
void vm_tick_window_tuner(VM* vm);         /* Loop #5: Context-aware window tuning (legacy, kept for compatibility) */
void vm_tick_slope_validator(VM* vm);      /* Loop #3: Heat decay validation (legacy, kept for compatibility) */
void vm_snapshot_read(const VM* vm, HeartbeatSnapshot* out_snapshot); /* Copy latest heartbeat snapshot */

/* Real-time heartbeat tick emission (Phase 2: Multivariate Dynamics) */
void heartbeat_capture_tick_snapshot(VM* vm, HeartbeatTickSnapshot* snapshot); /* Capture current VM metrics into tick snapshot */
void heartbeat_emit_tick_row(VM* vm, HeartbeatTickSnapshot* snapshot);         /* Emit tick snapshot as CSV row to stderr */

/* Start background heartbeat thread and/or enable heartbeat processing. */
void vm_heartbeat_start(VM* vm);

/* ===== Performance optimizations for release builds ==================== */
#ifdef STARFORTH_PERFORMANCE

/* Fast inline stack operations - skip bounds checking in performance builds */
static inline void vm_push_fast(VM* vm, cell_t value)
{
    vm->data_stack[++vm->dsp] = value;
}

static inline cell_t vm_pop_fast(VM* vm)
{
    return vm->data_stack[vm->dsp--];
}

static inline void vm_rpush_fast(VM* vm, cell_t value)
{
    vm->return_stack[++vm->rsp] = value;
}

static inline cell_t vm_rpop_fast(VM* vm)
{
    return vm->return_stack[vm->rsp--];
}

/* Performance macros - use fast paths in hot code */
#define VM_PUSH(vm, val) vm_push_fast(vm, val)
#define VM_POP(vm) vm_pop_fast(vm)
#define VM_RPUSH(vm, val) vm_rpush_fast(vm, val)
#define VM_RPOP(vm) vm_rpop_fast(vm)

/* Likely/unlikely branch prediction hints */
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

/* Safe versions with bounds checking for debug builds */
#define VM_PUSH(vm, val) vm_push(vm, val)
#define VM_POP(vm) vm_pop(vm)
#define VM_RPUSH(vm, val) vm_rpush(vm, val)
#define VM_RPOP(vm) vm_rpop(vm)
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#endif

#endif /* VM_H */
