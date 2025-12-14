# StarForth Metrics & Knobs: GAPS ONLY

## What's NOT Wired (Things to Add)

### DEAD CODE / NEVER CALLED

| What | Location | Status | Comment |
|------|----------|--------|---------|
| Bucket reordering | `physics_hotwords_cache.c:201-215` | Defined but NEVER CALLED | Bubble sort exists but no automatic trigger. Is this dead code or forgotten? |

---

### STACK METRICS (ZERO COLLECTION)

| Metric | File | Missing Since | Impact | Knob Needed |
|--------|------|---|---|---|
| Data stack peak depth | `stack_management.c` | Day 1 | Can't tell if workload needs larger stack | STACK_SIZE tuning blind |
| Return stack peak depth | `stack_management.c` | Day 1 | Can't measure colon definition nesting | Tail-call optimization impossible |
| Underflow attempts | `arithmetic_words.c:59-62` | Day 1 | Can't detect fragile code paths | Could optimize checks if zero underflows observed |
| Overflow attempts | `arithmetic_words.c` | Day 1 | Can't detect stack exhaustion | Early warning system missing |

---

### DICTIONARY METRICS (ZERO VISIBILITY)

| Metric | File | Missing Since | Impact | Why Matters |
|--------|------|---|---|---|
| Bucket load histogram | `vm.c: dict lookup` | Day 1 | First-char bucketing (26 buckets) may be unbalanced | Affects lookup latency, collision clustering |
| Dictionary growth rate | `memory_management.c` | Day 1 | No visibility into `here` pointer velocity | Can't predict "dictionary full" events |
| FORGET usage frequency | `dictionary_words.c` | Day 1 | No trace of word deletion patterns | Fragmentation unknown |
| Lookup failure rate | `vm.c` | Day 1 | How often is a word not found? | Error path frequency unknown |
| Word name length distribution | `vm.c` | Day 1 | Average name length unknown | Affects memory efficiency |

---

### BLOCK I/O METRICS (COMPLETELY DARK)

| Metric | File | Missing Since | Impact | Phase |
|--------|------|---|---|---|
| Block read frequency | `block_subsystem.c` | Day 1 | How often is BLOCK called? | DoE phase 1 |
| Block write frequency | `block_subsystem.c` | Day 1 | How often is UPDATE/FLUSH called? | DoE phase 1 |
| Writeback frequency | `block_subsystem.c:198-200` | Day 1 | When are dirty blocks flushed? | DoE phase 1 |
| Cache hit rate (disk) | `block_subsystem.c` | Day 1 | Blocks served from RAM cache % | DoE phase 1 |
| Dirty block ratio | `block_subsystem.c` | Day 1 | Blocks pending flush at time T | DoE phase 1 |
| I/O latency | `block_subsystem.c` | Day 1 | Time per read/write operation | DoE phase 2 |
| Fragmentation ratio | `block_subsystem.c` | Day 1 | Gaps in allocated blocks | DoE phase 2 |

---

### PER-WORD EXECUTION METRICS

| Metric | File | Missing Since | Impact | Use Case |
|--------|------|---|---|---|
| Per-word execution frequency | `vm.c: inner loop` | Day 1 | Which words are actually hot? | Profiling, hotspot detection |
| Per-word latency distribution | `vm.c` | Day 1 | Median/P99 time per word | Performance regression detection |
| Word category breakdown | `word_source/*.c` | Day 1 | Arithmetic vs. Stack vs. Control dominance | Workload characterization |
| Colon vs. native ratio | `vm.c` | Day 1 | User-defined word execution frequency | Code complexity analysis |

---

### HEAT DYNAMICS (PHASE 2 NOT ACTIVE)

| Metric | File | Status | Comment |
|--------|------|--------|---------|
| Heat decay trace | `physics_metadata.c: TODO` | Phase 2 placeholder | Need to measure if decay causes pattern loss |
| Heat percentile distribution | `physics_metadata.c` | Not updating dynamic | Stored but never dynamically recalculated |
| Heat concentration ratio | `physics_metadata.c` | Not tracked | (Top 10 words heat) / (total heat) |
| Heat velocity | `physics_metadata.c` | Not tracked | Rate of heat change per unit time |
| Heat plateau detection | `physics_metadata.c` | Not tracked | When does word heat level off? |

---

### PIPELINING METRICS (PHASE 1 INSTRUMENTATION, NOT USED)

| Metric | File | Status | Phase 2 Use |
|--------|------|--------|---|
| Prediction accuracy by depth | `physics_pipelining_metrics.c` | Not measured | Binary chop TRANSITION_WINDOW_SIZE |
| Context transition frequency | `physics_pipelining_metrics.c` | Counted but not reported | Validate context patterns |
| Speculation ROI histogram | `physics_pipelining_metrics.c` | Not aggregated | Tune SPECULATION_THRESHOLD |
| Misprediction cost (actual) | `physics_pipelining_metrics.c` | Measured but not validated | Calibrate cost model |
| Pipeline stall reduction | `physics_pipelining_metrics.c` | Not measured | Throughput impact unknown |

---

### CACHE POLLUTION & CYCLES

| Metric | File | Missing Since | Impact |
|--------|------|---|---|
| Cache eviction/re-promotion cycles | `physics_hotwords_cache.c` | Day 1 | Are we thrashing? |
| False negatives (words evicted before promotion) | `physics_hotwords_cache.c` | Day 1 | Should promotion threshold be lower? |
| Cache eviction candidate value | `physics_hotwords_cache.c` | Day 1 | Was LRU the right choice? |
| Bucket scan depth (position of hit) | `physics_hotwords_cache.c` | Day 1 | Is bucket order actually helping? |
| Promotion wait time | `physics_hotwords_cache.c` | Day 1 | How long does word stay in bucket before cache? |

---

## Priority for "Twisting the Dragon's Tail"

### 🔴 **CRITICAL (Do First)**
1. **Stack peak depth** - 5 minutes to add, massive insight
2. **Block I/O metrics** - Writeback behavior unknown (could be performance killer)
3. **Dictionary bucket load** - Unbalanced bucketing = latency tail

### 🟠 **HIGH (Phase 1 DoE)**
4. **Per-word execution frequency** - Validation for cache/window decisions
5. **Heat decay trace** - Validate Phase 2 feature before enabling
6. **Block I/O hit rate** - Complete the I/O picture

### 🟡 **MEDIUM (Phase 2 DoE)**
7. **Pipelining accuracy by depth** - Input to window size binary chop
8. **Colon definition nesting depth** - Tail-call optimization opportunities
9. **Dictionary growth velocity** - Predict "full" events

### 🟢 **NICE-TO-HAVE (Phase 3)**
10. Heat concentration ratio
11. Word category breakdown
12. Speculation ROI histogram

---

## Implementation Effort Estimate

| Category | Files | LOC | Time | Priority |
|----------|-------|-----|------|----------|
| Stack metrics | stack_management.c, vm.h | ~30 | 30 min | 🔴 |
| Dictionary metrics | vm.c, memory_management.c, vm.h | ~50 | 45 min | 🔴 |
| Block I/O metrics | block_subsystem.c, vm.h | ~80 | 1 hr | 🔴 |
| Per-word metrics | vm.c, physics_metadata.c, vm.h | ~100 | 1.5 hr | 🟠 |
| Heat dynamics | physics_metadata.c, vm.h | ~60 | 1 hr | 🟠 |
| Pipelining accuracy | physics_pipelining_metrics.c, vm.h | ~40 | 45 min | 🟡 |

---

## Bucket Reordering Mystery

**File**: `physics_hotwords_cache.c:201-215`
**Code**: `hotwords_bucket_reorder()` - bubble sort by execution_heat
**Status**: Function defined but **NEVER CALLED** from lookup path
**Question**: Is this dead code or forgot to hook it?

**If enabled, metrics needed:**
- Reorder frequency
- Cost (bubble sort latency)
- Benefit (does reordering actually improve hit latency?)
