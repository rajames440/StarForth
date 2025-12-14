# FORTH VM Pipelining & Speculative Execution Design

## Executive Summary

**Goal:** Reduce dictionary lookup latency by speculatively prefetching the next word while executing the current word.

**Physics Model Application:**
- Metrics: Word transition frequency (execution_heat[A→B])
- Decision: Speculatively prefetch word N+1 if P(N→N+1) > threshold
- Validation: Measure prefetch hit rate and latency savings
- Knobs: Threshold, speculation depth, confidence levels, misprediction cost

**Expected Impact:**
- Baseline (no pipelining): 31.543 ns cache hit, 56.237 ns bucket hit
- With pipelining: ~25-40% additional speedup possible (1.25-1.4× on top of cache)
- Cumulative with cache: 2.2-2.5× total (1.78× cache × 1.25-1.4× pipeline)

---

## 1. Conceptual Model

### Current Execution (No Pipelining)

```
Time:    T0      T1      T2      T3      T4
         ↓       ↓       ↓       ↓       ↓
Word A:  [L]     [E]     →
                         [L]     [E]     →
Word B:          (blocked)              [L]
                         [E]     →
                                 [L]     [E]
Word C:                          (blocked)

Legend: [L] = Lookup, [E] = Execute
→ = Transition latency

Problem: Word B lookup blocked until Word A executes
```

### With Pipelining

```
Time:    T0      T1      T2      T3      T4
         ↓       ↓       ↓       ↓       ↓
Word A:  [L]     [E]     →
                 [P]     [P]     →
Word B:  (prefetch)              [L]     [E]
                 [P]     [P]     →
Word C:  (prefetch)                     [L]

Legend: [L] = Lookup, [E] = Execute, [P] = Prefetch (parallel)
→ = Transition latency (now hidden by pipeline)

Benefit: Word B lookup latency is prefetched (hidden behind Word A execution)
```

### The Key Insight: Execution Heat Predicts Transitions

```
Observation:
  Word EXIT (heat=114) is almost always followed by:
    - Word LIT (heat=101)  [60% of the time]
    - Word CR  (heat=26)   [15% of the time]
    - Other   (heat=?)     [25% of the time]

Physics Model:
  transition_heat[EXIT→LIT] = 600 (60% of 1000 executions)
  transition_heat[EXIT→CR]  = 150 (15%)
  transition_heat[EXIT→?]   = 250 (25%)

Decision Rule:
  IF transition_heat[N→N+1] > THRESHOLD THEN
    prefetch_word(N+1)
  END
```

---

## 2. Metrics: Tracking Word Transitions

### 2.1 New Metrics to Collect

```c
typedef struct {
    // Existing (from execution_heat tracking):
    uint64_t execution_heat;              // Total executions of this word

    // NEW: Transition tracking
    uint64_t transition_heat[DICT_SIZE];  // How many times does each word follow this word?
    uint64_t total_transitions;           // Sum of all transitions from this word

    // NEW: Prediction accuracy
    uint64_t prefetch_attempts;           // How many times did we prefetch?
    uint64_t prefetch_hits;               // How many speculations were correct?
    uint64_t prefetch_misses;             // How many speculations were wrong?

    // NEW: Latency tracking
    uint64_t prefetch_latency_saved_q48;  // Q48.16 nanoseconds saved by prefetch
    uint64_t misprediction_cost_q48;      // Q48.16 nanoseconds lost by wrong speculation

} WordPhysicsMetrics;
```

### 2.2 Data Collection Points

**Where we collect transitions:**

```c
// In vm.c: execute_colon_word() inner loop
for (;;) {
    DictEntry *w = (DictEntry *)(uintptr_t)(*ip++);

    // Existing: increment execution heat
    w->execution_heat++;

    // NEW: track transition to next word
    if (next_word_ptr) {
        DictEntry *next = (DictEntry *)(uintptr_t)(*ip);
        if (next) {
            w->metrics.transition_heat[next->id]++;
            w->metrics.total_transitions++;
        }
    }

    // Execute word
    w->func(vm);
}
```

### 2.3 Transition Probability Calculation

```c
static double transition_probability(DictEntry *from, DictEntry *to) {
    if (from->metrics.total_transitions == 0) return 0.0;
    return (double)from->metrics.transition_heat[to->id] /
           (double)from->metrics.total_transitions;
}

// Q48.16 fixed-point version (no floating-point)
static int64_t transition_probability_q48(DictEntry *from, DictEntry *to) {
    if (from->metrics.total_transitions == 0) return 0;
    // Compute: (transition_heat[to] / total_transitions) × 2^16
    return ((int64_t)from->metrics.transition_heat[to->id] << 16) /
           (int64_t)from->metrics.total_transitions;
}
```

---

## 3. Pipelining Decision Logic

### 3.1 Prediction Decision Tree

```
FOR EACH word execution:

  1. Calculate next word's identity (static: from instruction pointer)

  2. Look up transition probability: P(current→next)

  3. Decision:
     IF P(current→next) > SPECULATION_THRESHOLD THEN
       prefetch_word(next)
       metrics.prefetch_attempts++
     ELSE
       // No prefetch (low confidence)
     END

  4. Execute current word

  5. At transition, check prefetch result:
     IF actual_next == predicted_next THEN
       metrics.prefetch_hits++
       // Lookup latency was hidden by pipeline
     ELSE
       metrics.prefetch_misses++
       // Had to do full lookup (misprediction cost)
     END
```

### 3.2 The Knobs: Tuning Parameters

**Knob 1: SPECULATION_THRESHOLD**
```c
#define SPECULATION_THRESHOLD_Q48 (0.50 * (1LL << 16))  // 50% confidence
```
- **Range:** 0.10 (10%) to 0.95 (95%)
- **Semantics:** "Speculate if we're at least X% confident that transition occurs"
- **Trade-off:**
  - Low (10%): More speculation, higher hit rate, but more mispredictions
  - High (95%): Conservative, low misprediction cost, but miss optimization opportunities
- **Sweet spot:** 50-70% (balance of coverage vs. accuracy)

**Knob 2: SPECULATION_DEPTH**
```c
#define SPECULATION_DEPTH 1  // How many words ahead to prefetch
```
- **Range:** 1 to 4 (deeper depths get expensive)
- **Semantics:** "Speculatively prefetch up to N words in advance"
- **Trade-off:**
  - Depth=1: Prefetch word N+1 while executing N (simple)
  - Depth=2: Prefetch both N+1 and N+2 (more aggressive)
  - Depth=3+: Diminishing returns (limited by IP advancement rate)
- **Sweet spot:** 1-2 (Depth 1 recommended for simplicity)

**Knob 3: MIN_SAMPLES_FOR_SPECULATION**
```c
#define MIN_SAMPLES_FOR_SPECULATION 10  // Need 10+ transitions before speculating
```
- **Range:** 1 to 100
- **Semantics:** "Don't speculate on transitions we've only seen N times"
- **Trade-off:**
  - Low (1): Speculate on even rare transitions
  - High (100): Only speculate on well-established patterns
- **Sweet spot:** 10-20 (avoid noise, still responsive to patterns)

**Knob 4: MISPREDICTION_COST_Q48**
```c
#define MISPREDICTION_COST_Q48 (25LL << 16)  // ~25 nanoseconds (estimate)
```
- **Range:** 0 to 100+ nanoseconds
- **Semantics:** "Estimated penalty for wrong speculation (in Q48.16 ns)"
- **Calculation:**
  - Cost = Time to discard wrong prefetch + time to do correct lookup
  - ~25 ns typical (speculative pipeline flush + re-lookup)
- **Sweet spot:** 20-30 nanoseconds

**Knob 5: MINIMUM_PREFETCH_ROI**
```c
#define MINIMUM_PREFETCH_ROI 1.10  // Only prefetch if we expect 1.1× or better ROI
```
- **Range:** 1.0 to 2.0
- **Semantics:** "Only speculate if expected benefit exceeds cost by X%"
- **Calculation:**
  - ROI = (avg_lookup_latency / speculation_overhead) × prefetch_hit_rate
  - If ROI > threshold, prefetch is worthwhile
- **Sweet spot:** 1.1-1.3 (10-30% expected improvement)

---

## 4. Implementation Strategy

### 4.1 Phase 1: Instrumentation (Data Collection)

**Goal:** Collect transition metrics without making predictions yet.

```c
// In vm.c: modify inner interpreter to track transitions

void execute_colon_word(VM *vm, DictEntry *entry) {
    cell_t *code = entry->code;
    cell_t *ip = code;
    DictEntry *prev_word = NULL;

    for (;;) {
        DictEntry *w = (DictEntry *)(uintptr_t)(*ip++);
        if (!w) break;

        // Track transition from previous word
        if (prev_word && prev_word->metrics.transition_heat) {
            if (w->id < DICT_SIZE) {
                prev_word->metrics.transition_heat[w->id]++;
                prev_word->metrics.total_transitions++;
            }
        }

        w->execution_heat++;

        // Execute
        w->func(vm);
        if (vm->error) break;

        prev_word = w;
    }
}
```

**Measurement:** Let system run normally, collect transition data.

**Output:** Transition matrices showing word-to-word frequency.

### 4.2 Phase 2: Prediction Without Prefetching

**Goal:** Measure prediction accuracy before actually speculating.

```c
typedef struct {
    DictEntry *predicted_next;
    DictEntry *actual_next;
    int was_correct;
    uint64_t latency_saved_if_hit;
} SpeculationRecord;

// Dry run: predict but don't prefetch
void execute_with_prediction_logging(VM *vm, DictEntry *entry) {
    cell_t *code = entry->code;
    cell_t *ip = code;

    for (;;) {
        DictEntry *w = (DictEntry *)(uintptr_t)(*ip++);
        if (!w) break;

        // Look ahead at next word
        DictEntry *predicted = predict_next_word(w);

        // Get actual next word
        DictEntry *actual = (DictEntry *)(uintptr_t)(*ip);

        // Log prediction accuracy
        if (predicted && actual) {
            metrics.predictions++;
            if (predicted == actual) {
                metrics.correct_predictions++;
            }
        }

        // Execute normally (no prefetch)
        w->func(vm);
        if (vm->error) break;
    }
}

// Calculate prediction accuracy
double prediction_accuracy = (double)metrics.correct_predictions / metrics.predictions;
```

**Measurement:** Run benchmark, measure how often predictions would be correct.

**Output:** Prediction accuracy metrics (hint at potential ROI).

### 4.3 Phase 3: Actual Pipelining with Tuning

**Goal:** Implement speculative prefetch with adjustable knobs.

```c
typedef struct {
    DictEntry *prefetched_word;
    uint64_t prefetch_time_ns;
    int was_used;
} SpeculativeCache;

void execute_with_pipelining(VM *vm, DictEntry *entry) {
    cell_t *code = entry->code;
    cell_t *ip = code;
    SpeculativeCache spec_cache = {0};

    for (;;) {
        DictEntry *w = (DictEntry *)(uintptr_t)(*ip++);
        if (!w) break;

        // PREFETCH: Do speculation for NEXT iteration
        // (while this word executes)
        DictEntry *next = (DictEntry *)(uintptr_t)(*ip);
        if (next && should_speculate(w, next)) {
            uint64_t spec_start = sf_monotonic_ns();
            spec_cache.prefetched_word = next;
            spec_cache.prefetch_time_ns = spec_start;
            metrics.prefetch_attempts++;
        }

        // EXECUTE: Run current word
        w->execution_heat++;
        w->func(vm);
        if (vm->error) break;

        // VALIDATE: Check if prefetch was correct
        DictEntry *actual_next = (DictEntry *)(uintptr_t)(*ip);
        if (spec_cache.prefetched_word == actual_next) {
            // HIT: Prefetch was correct
            uint64_t actual_lookup_latency = sf_monotonic_ns() - spec_cache.prefetch_time_ns;
            metrics.prefetch_hits++;
            metrics.total_latency_saved_q48 += (actual_lookup_latency << 16);
        } else if (spec_cache.prefetched_word != NULL) {
            // MISS: Prefetch was wrong
            metrics.prefetch_misses++;
            metrics.total_misprediction_cost_q48 += MISPREDICTION_COST_Q48;
        }

        spec_cache = (SpeculativeCache){0};  // Clear for next iteration
    }
}

// Decision function
static int should_speculate(DictEntry *from, DictEntry *to) {
    if (from->metrics.transition_heat[to->id] < MIN_SAMPLES_FOR_SPECULATION) {
        return 0;  // Not enough data
    }

    int64_t prob_q48 = transition_probability_q48(from, to);

    if (prob_q48 < SPECULATION_THRESHOLD_Q48) {
        return 0;  // Not confident enough
    }

    // Calculate expected ROI
    int64_t expected_savings = (int64_t)to->physics.avg_latency_ns << 16;  // Q48.16
    int64_t expected_cost = MISPREDICTION_COST_Q48;

    // Hit rate estimate from history
    double hit_rate = (double)from->metrics.prefetch_hits /
                     (double)(from->metrics.prefetch_attempts + 1);

    int64_t expected_roi = (expected_savings * hit_rate) - expected_cost;

    if (expected_roi < MINIMUM_PREFETCH_ROI) {
        return 0;  // Not worth it
    }

    return 1;  // Speculate!
}
```

---

## 5. Measurement & Validation

### 5.1 Key Metrics to Track

```c
typedef struct {
    // Basic speculation metrics
    uint64_t total_speculations;         // Words we attempted to prefetch
    uint64_t successful_prefetches;      // Speculations that matched actual flow
    uint64_t failed_prefetches;          // Speculations that didn't match

    // Calculated metrics
    double prefetch_accuracy;             // successful / total
    double prefetch_hit_rate;             // successful / total (same as above)

    // Latency metrics (Q48.16 fixed-point)
    int64_t total_latency_saved_q48;     // Sum of latency hidden by pipeline
    int64_t total_misprediction_cost_q48; // Sum of costs from wrong speculation

    // ROI calculation
    int64_t net_benefit_q48;             // saved - cost
    double roi_factor;                   // (saved / cost)

    // Per-word metrics
    double accuracy_by_source[DICT_SIZE]; // Prefetch accuracy for each word

} PipelineMetrics;
```

### 5.2 Expected Results (Hypothesis)

**Scenario A: Typical FORTH Workload**
```
Hot word transitions:
  EXIT → LIT:      60% probability
  LIT → @:         55% probability
  @ → DROP:        40% probability

Prediction accuracy: ~60-70%
Misprediction cost: ~25 ns
Latency saved per hit: ~30-50 ns
Expected speedup: 1.25-1.40× (over cache baseline)
```

**Scenario B: Complex Control Flow (FORTH loops)**
```
More varied transitions (less predictable)
Prediction accuracy: ~35-45%
Misprediction cost: ~25 ns
Latency saved per hit: ~30-50 ns
Expected speedup: 1.05-1.15× (less benefit from branching)
```

**Scenario C: Worst Case (Random execution)**
```
No pattern in transitions (uniform random)
Prediction accuracy: ~5% (random guess)
Expected speedup: <1.0× (pipeline costs more than it saves)
Result: Pipelining disabled (ROI < threshold)
```

### 5.3 A/B Testing Framework

```bash
# Baseline: No pipelining
make clean && make ENABLE_PIPELINING=0 fastest
./build/amd64/fastest/starforth << 'EOF'
PHYSICS-RESET-STATS
100000 BENCH-DICT-LOOKUP
PHYSICS-PIPELINE-STATS
BYE
EOF

# With pipelining (default threshold = 50%)
make clean && make ENABLE_PIPELINING=1 fastest
./build/amd64/fastest/starforth << 'EOF'
PHYSICS-RESET-STATS
100000 BENCH-DICT-LOOKUP
PHYSICS-PIPELINE-STATS
BYE
EOF

# With aggressive pipelining (threshold = 30%)
make clean && make ENABLE_PIPELINING=1 SPECULATION_THRESHOLD=0.30 fastest
./build/amd64/fastest/starforth << 'EOF'
PHYSICS-RESET-STATS
100000 BENCH-DICT-LOOKUP
PHYSICS-PIPELINE-STATS
BYE
EOF

# With conservative pipelining (threshold = 80%)
make clean && make ENABLE_PIPELINING=1 SPECULATION_THRESHOLD=0.80 fastest
./build/amd64/fastest/starforth << 'EOF'
PHYSICS-RESET-STATS
100000 BENCH-DICT-LOOKUP
PHYSICS-PIPELINE-STATS
BYE
EOF
```

**Output:** Compare speedup factors across threshold settings.

---

## 6. The Knobs: Complete Reference

### Knob Summary Table

| Knob | Purpose | Range | Default | Trade-off |
|------|---------|-------|---------|-----------|
| **SPECULATION_THRESHOLD** | Min confidence to speculate | 0.10–0.95 | 0.50 | Coverage vs. Accuracy |
| **SPECULATION_DEPTH** | Words ahead to prefetch | 1–4 | 1 | Aggressiveness vs. Complexity |
| **MIN_SAMPLES** | Min transitions before speculating | 1–100 | 10 | Responsiveness vs. Noise |
| **MISPREDICTION_COST** | Penalty for wrong spec (ns) | 0–100 | 25 | Conservative vs. Aggressive |
| **MINIMUM_ROI** | Min expected improvement | 1.0–2.0 | 1.10 | Selectivity vs. Coverage |

### Knob Tuning Strategy

**Step 1: Collect Baseline**
```
Run with ENABLE_PIPELINING=0
Measure: Baseline speedup, latency distribution
```

**Step 2: Identify Best Threshold**
```
Try: 0.30, 0.50, 0.70, 0.90
Measure: Prefetch accuracy, net speedup
Find: Threshold that maximizes speedup
```

**Step 3: Optimize Depth**
```
Try: SPECULATION_DEPTH=1,2,3
For best threshold from Step 2
Measure: Speedup vs. CPU overhead
```

**Step 4: Tune Sample Minimum**
```
Try: MIN_SAMPLES=5, 10, 20, 50
Measure: How it affects early-startup behavior
```

**Step 5: Calibrate Misprediction Cost**
```
Measure actual misprediction penalty: How long does a wrong spec take to recover?
Tune MISPREDICTION_COST_Q48 accordingly
```

---

## 7. Advanced Topics

### 7.1 Adaptive Thresholding

**Dynamic threshold adjustment based on workload:**

```c
// Adjust threshold based on recent success rate
void adapt_speculation_threshold(void) {
    double recent_hit_rate = calculate_rolling_hit_rate(last_1000_speculations);

    if (recent_hit_rate > 0.70) {
        // Speculations are working well, be more aggressive
        SPECULATION_THRESHOLD_Q48 = 0.40 * (1LL << 16);  // Lower threshold
    } else if (recent_hit_rate < 0.40) {
        // Speculations are failing, be more conservative
        SPECULATION_THRESHOLD_Q48 = 0.60 * (1LL << 16);  // Higher threshold
    }
}

// Call periodically (e.g., every 10K words)
if (total_executions % 10000 == 0) {
    adapt_speculation_threshold();
}
```

### 7.2 Hierarchical Speculation

**Different threshold for different word categories:**

```c
// Some words have more predictable successors
if (from->flags & WORD_LOOP_CONTROL) {
    threshold = 0.30;  // Loops have predictable patterns
} else if (from->flags & WORD_CONDITIONAL) {
    threshold = 0.70;  // Conditionals are less predictable
} else {
    threshold = 0.50;  // Default
}
```

### 7.3 Multi-Step Prediction

**Predict chains of words:**

```c
// Example: EXIT almost always followed by LIT, which is followed by @
if (should_speculate(EXIT, LIT) && should_speculate(LIT, @)) {
    prefetch(LIT);
    prefetch(@);  // Depth-2 prefetch
}
```

---

## 8. Comparison to CPU Pipelining

### 8.1 Analogies

| CPU Concept | FORTH VM Analogy |
|---|---|
| **Instruction fetch** | Dictionary lookup |
| **Decode** | Word dispatch |
| **Execute** | Word execution (func call) |
| **Branch prediction** | Transition prediction |
| **Speculative execution** | Prefetch next word |
| **Branch misprediction penalty** | Misprediction cost (~25 ns) |
| **Pipeline depth** | SPECULATION_DEPTH |

### 8.2 Key Difference

**CPUs predict branches (conditional jumps).** FORTH VMs predict *sequential transitions* (what comes after current word).

- **Easier prediction:** Sequential is more predictable than branches
- **Simpler recovery:** Wrong sequential guess = just do lookup, no flush needed
- **Lower cost:** Prefetch is cheap (just a lookup ahead of time)

---

## 9. Expected Cumulative Speedup

### Stacking Optimizations

```
Baseline (no optimization):                1.0×
+ Hot-words cache (Phase 1):               1.78×
+ Pipelining (Phase 2):                    2.2–2.5× (1.78 × 1.25–1.4)
+ Stack fusion (Phase 3):                  2.6–3.5×
+ Other optimizations (Phase 4+):          5–8×
```

### Why Pipelining is Worth Doing

1. **Orthogonal to cache** (can combine them)
2. **Leverages execution_heat data** (already collected)
3. **Simple mechanics** (just prefetch one word ahead)
4. **Tunable** (many knobs to optimize)
5. **Verifiable** (deterministic, no JIT needed)

---

## 10. Implementation Roadmap

### Phase 1: Instrumentation (1 week)
- [ ] Add transition_heat tracking to DictEntry
- [ ] Collect transition statistics during normal execution
- [ ] Print transition matrices for analysis
- **Deliverable:** Understanding of word-to-word patterns

### Phase 2: Prediction Analysis (1 week)
- [ ] Implement prediction-without-prefetch
- [ ] Measure prediction accuracy by threshold
- [ ] Find optimal threshold experimentally
- [ ] Estimate potential speedup
- **Deliverable:** A/B test showing prediction accuracy

### Phase 3: Implement Pipelining (2 weeks)
- [ ] Implement actual speculative prefetch
- [ ] Add should_speculate() decision function
- [ ] Collect prefetch metrics (hits, misses, ROI)
- [ ] Compare vs. baseline
- **Deliverable:** Measured speedup with default knobs

### Phase 4: Knob Tuning & Optimization (2 weeks)
- [ ] Systematic knob tuning experiments
- [ ] Adaptive thresholding
- [ ] Per-word threshold optimization
- [ ] Documentation of optimal knob values
- **Deliverable:** Performance-tuned pipelining system

### Phase 5: Integration & Testing (1 week)
- [ ] Integration with existing hot-words cache
- [ ] Full test suite (no regressions)
- [ ] Reproducibility validation
- [ ] Documentation for operators
- **Deliverable:** Production-ready pipelining

---

## 11. Risks & Mitigations

### Risk 1: Prediction Accuracy Too Low
**Symptom:** < 30% hit rate
**Mitigation:** Dynamic threshold adjustment, fallback to cache-only mode

### Risk 2: Misprediction Cost Too High
**Symptom:** Speedup < 1.0× (pipelining makes things slower)
**Mitigation:** Disable pipelining automatically when ROI < threshold

### Risk 3: Data Structure Overhead
**Symptom:** Memory usage explodes (transition_heat[] per word)
**Mitigation:** Use sparse matrix or hash-based approach, limit to top N transitions per word

### Risk 4: Complexity Explosion
**Symptom:** Too many knobs, hard to understand/tune
**Mitigation:** Start simple (SPECULATION_THRESHOLD only), add knobs incrementally

---

## 12. Success Criteria

✅ **Achievable Goals:**
- [ ] Prediction accuracy > 50% for hot words
- [ ] Prefetch hit rate > 40% overall
- [ ] Measured speedup 1.2–1.4× over cache
- [ ] Knobs documented and tunable
- [ ] Works with hot-words cache (cumulative 2.2–2.5×)
- [ ] No regressions on unoptimized code paths

⚠️ **Stretch Goals:**
- [ ] Adaptive thresholding improves over fixed threshold
- [ ] Depth-2 prefetch worthwhile in some workloads
- [ ] Per-word threshold optimization
- [ ] Formal verification of correctness

❌ **Non-Goals:**
- Exceed 2× speedup (unrealistic without JIT)
- Eliminate all misprediction cost
- Work perfectly on all possible FORTH programs

---

## Next Steps

1. **Start with Phase 1:** Instrument transition tracking
2. **Analyze patterns:** What words transition to what?
3. **Run prediction dry-run:** How accurate are predictions?
4. **Implement pipelining:** With default knobs
5. **Measure & iterate:** Tune knobs for best performance
6. **Document results:** Add to academic materials

---

## References

### Related Work
- CPU Branch Prediction (Yeh & Patt 1991)
- Speculative Execution (Sprangle & Wilkerson 2001)
- FORTH optimization (Bell Labs 1980s)

### Within StarForth
- `/docs/PHYSICS_HOTWORDS_CACHE_EXPERIMENT.md` - Cache baseline
- `/include/physics_metadata.h` - Metrics framework
- `/include/physics_hotwords_cache.h` - Cache implementation
- `/docs/PHYSICS_OPTIMIZATION_PROPOSALS.md` - Future opportunities

