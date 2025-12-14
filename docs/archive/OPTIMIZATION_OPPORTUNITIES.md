# StarForth Optimization Opportunities
## Bang for Buck + Implementation Difficulty Analysis

**Date:** 2025-11-19
**Context:** A_B_C_FULL + HEARTBEAT_ON is now the locked-in baseline configuration. Following opportunities are ordered by **value-to-difficulty ratio** (highest impact, lowest cost first).

---

## Quick Assessment Matrix

| Rank | Opportunity | Bang for Buck | Difficulty | Ratio | Est. Impact |
|------|-------------|---------------|-----------|-------|------------|
| 1 | Adaptive decay slope inference | ⭐⭐⭐⭐⭐ (High) | ⭐⭐ (Low) | **5:2 = 2.5** | 8-15% perf |
| 2 | Variance-based window width tuning | ⭐⭐⭐⭐ (High) | ⭐⭐ (Low) | **4:2 = 2.0** | 6-12% perf |
| 3 | Binary search slope fitting validation | ⭐⭐⭐ (Medium-High) | ⭐⭐⭐ (Medium) | **3:3 = 1.0** | 2-5% accuracy |
| 4 | Rolling window sizing experiment | ⭐⭐⭐⭐ (High) | ⭐⭐⭐ (Medium) | **4:3 = 1.33** | 5-8% perf |
| 5 | Decay rate parameter tuning | ⭐⭐⭐ (Medium-High) | ⭐⭐ (Low) | **3:2 = 1.5** | 3-6% perf |
| 6 | Hotwords cache threshold optimization | ⭐⭐⭐ (Medium-High) | ⭐⭐⭐ (Medium) | **3:3 = 1.0** | 2-4% perf |
| 7 | Speculative prefetch ML model | ⭐⭐⭐⭐⭐ (Very High) | ⭐⭐⭐⭐⭐ (Very High) | **5:5 = 1.0** | 15-30% perf |
| 8 | Kernel-aware memory placement | ⭐⭐⭐⭐ (High) | ⭐⭐⭐⭐ (Hard) | **4:4 = 1.0** | 10-20% perf |
| 9 | Dictionary reorganization (heat-aware) | ⭐⭐⭐ (Medium-High) | ⭐⭐⭐⭐ (Hard) | **3:4 = 0.75** | 3-5% perf |
| 10 | SIMD vectorization | ⭐⭐ (Medium) | ⭐⭐⭐⭐⭐ (Very Hard) | **2:5 = 0.4** | 5-10% perf |

---

## Tier 1: High ROI, Low Effort (Recommended Next)

### 1. Adaptive Decay Slope Inference ⭐⭐⭐⭐⭐

**What it is:**
Use closed-form linear regression on execution heat trajectory to dynamically optimize `decay_slope` (currently static at 0.33).

**Why it matters:**
- Decay slope controls how fast "cold" words fade from the hot-words cache
- Current fixed value (0.33) is a guess
- Optimal slope depends on workload's word frequency distribution
- **Potential gain:** 8-15% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐ (Low-Medium)
- **Effort:** 4-6 hours
- **Files affected:** `src/inference_engine.c` (already designed in CLAUDE.md)
- **Testing:** Existing test suite validates without changes

**Why it's #1:**
- Already designed in CLAUDE.md (Adaptive Inference Engine Architecture)
- Q48.16 math library already complete
- Just needs empirical validation against different workload patterns
- DoE experiment can directly measure impact

**Next Step:** Create DoE with `DECAY_SLOPE` as a factor (e.g., 0.2, 0.33, 0.5, 0.7)

---

### 2. Variance-Based Window Width Tuning ⭐⭐⭐⭐

**What it is:**
Instead of fixed `ROLLING_WINDOW_SIZE=4096`, use variance inflection point to find optimal window automatically.

**Why it matters:**
- Window size controls how many executions we remember for adaptive decisions
- Currently static; should vary by workload
- Too small = noisy data; too large = stale data
- **Potential gain:** 6-12% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐ (Low-Medium)
- **Effort:** 3-5 hours
- **Files affected:** `src/inference_engine.c` (variance inflection detection)
- **Testing:** Requires new test module for window sizing

**Why it's #2:**
- Directly tied to inference engine design (already specified)
- Pairs naturally with decay slope tuning
- Measurable via statistical tests (variance reduction)
- Can be tested in factorial design (2×2 with decay slope)

**Next Step:** Implement `find_variance_inflection()` in inference engine, test with DoE

---

### 3. Decay Rate Parameter Tuning ⭐⭐⭐

**What it is:**
Factorial DoE testing different `DECAY_MIN_INTERVAL_NS` and `ADAPTIVE_SHRINK_RATE` values.

**Why it matters:**
- Controls how aggressively hot-words decay and how window shrinks
- Currently hardcoded (1000ns, 75)
- Different workloads may benefit from different rates
- **Potential gain:** 3-6% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐ (Low)
- **Effort:** 1-2 hours (pure DoE, no code changes needed)
- **Files affected:** None (Makefile parameters only)
- **Testing:** Existing metrics, just new experiments

**Why it's #3:**
- Requires ZERO code changes (just Makefile knobs)
- Can run immediately after current baselines
- Quick feedback loop (each run ~10 seconds)
- Natural 2×2 or 2×3 factorial design

**Next Step:** Update `run_doe.sh` to test decay parameters, run TST_DECAY_01 through TST_DECAY_04

---

## Tier 2: Good ROI, Medium Effort

### 4. Rolling Window Sizing Experiment ⭐⭐⭐⭐

**What it is:**
2^2 factorial DoE: Test `ROLLING_WINDOW_SIZE` values (2048, 4096, 8192) × `DECAY_SLOPE` (0.33, 0.5).

**Why it matters:**
- Window size is a key tuning knob
- Inference engine design depends on proper sizing
- Can reveal interaction effects between window and decay
- **Potential gain:** 5-8% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐ (Medium)
- **Effort:** 6-8 hours (includes inference engine integration)
- **Files affected:** `src/inference_engine.c`, `src/vm.c`
- **Testing:** Statistical validation of variance inflection detection

**Why it's mid-tier:**
- Builds on decay slope + window width inference
- Requires inference engine to be fully operational
- More complex experiment (larger design space)
- Needs statistical analysis beyond simple comparisons

**Prerequisites:**
- Complete items #1 and #2 first
- Ensure inference engine passes unit tests

**Next Step:** After decay slope + window width inference are working, run TST_WINDOW_DoE_01

---

### 5. Hotwords Cache Threshold Optimization ⭐⭐⭐

**What it is:**
Factorial DoE: Test `HOTWORDS_EXECUTION_HEAT_THRESHOLD` (default=10) with different values (5, 10, 20, 50).

**Why it matters:**
- Threshold determines when words are promoted to hot-words cache
- Too low = cache thrash; too high = miss optimization
- Current value (10) is arbitrary
- **Potential gain:** 2-4% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐ (Medium)
- **Effort:** 2-3 hours
- **Files affected:** Makefile only + `include/physics_hotwords_cache.h`
- **Testing:** Cache statistics validation

**Why it's mid-tier:**
- Simple parameter sweep (can be done as Makefile knob)
- Requires statistical analysis of cache hit rates
- May interact with decay slope (higher slope = more aggressive threshold needed)

**Next Step:** Can run in parallel with other optimization work

---

## Tier 3: High Impact, High Effort (Future)

### 6. Binary Search Slope Fitting Validation ⭐⭐⭐

**What it is:**
Compare closed-form linear regression (current plan) vs. binary search for finding optimal decay slope.

**Why it matters:**
- Validates accuracy of closed-form inference vs. iterative approach
- Determines which algorithm to use in production
- **Potential gain:** 2-5% accuracy improvement in slope estimation

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐ (Medium)
- **Effort:** 8-12 hours (implement binary search, compare algorithms)
- **Files affected:** `src/inference_engine.c`
- **Testing:** Numerical analysis, convergence validation

**Why it's later:**
- Optimization of an optimization (diminishing returns)
- Requires both algorithms implemented and benchmarked
- More of a research question than product improvement
- Nice-to-have, not critical path

**Next Step:** After closed-form inference is working, run comparative benchmarks

---

### 7. Speculative Prefetch ML Model ⭐⭐⭐⭐⭐

**What it is:**
Train a neural network on word transition sequences to predict next word with >88% accuracy (beat current 88.37%).

**Why it matters:**
- Current prefetch accuracy (752/851 = 88.37%) is deterministic, hard-coded patterns
- ML model could adapt to actual runtime patterns
- Could enable dynamic prefetch strategy
- **Potential gain:** 15-30% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐⭐⭐ (Very Hard)
- **Effort:** 40-80 hours (full ML pipeline: data collection, training, inference)
- **Files affected:** New ML inference module + word transition metrics collection
- **Testing:** Cross-validation, A/B testing in FORTH

**Why it's ambitious:**
- Requires data collection infrastructure (already have rolling window!)
- Inference latency must be < 1µs to be worth it
- Training overhead (offline, could do periodically)
- Integration into VM execution path is non-trivial

**Prerequisites:**
- Complete inference engine work (Tiers 1-2)
- Build robust word transition metrics collection
- Prove ML-guided prefetch is faster than current approach

**Future:** Post-MVP optimization, not critical for tuning baseline

---

### 8. Kernel-Aware Memory Placement ⭐⭐⭐⭐

**What it is:**
Use Linux kernel hints (NUMA, page migration) to place hot dictionary words near CPU cores executing them.

**Why it matters:**
- Current VM uses unified memory layout
- NUMA systems have variable latency (local ~100ns, remote ~300ns)
- Hot words could be pinned to NUMA nodes close to execution
- **Potential gain:** 10-20% performance improvement on NUMA systems

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐⭐ (Hard)
- **Effort:** 30-50 hours (NUMA topology, page pinning, migration policy)
- **Files affected:** `src/platform/linux/numa_*.c` (new module), `src/memory_management.c`
- **Testing:** NUMA benchmark harness, multi-socket validation

**Why it's ambitious:**
- Only helps on multi-socket systems (RPi4, small servers won't see benefit)
- Requires careful page pinning to avoid fragmentation
- Platform-specific (L4Re won't support same approach)
- Maintenance burden for multi-platform support

**Prerequisites:**
- Complete baseline tuning (Tiers 1-2)
- Identify NUMA systems as target (if applicable)
- Profile-guided data showing memory pressure

**Future:** Platform-specific optimization, post-MVP

---

### 9. Dictionary Reorganization (Heat-Aware) ⭐⭐⭐

**What it is:**
Reorder dictionary entries by execution heat to improve cache locality.

**Why it matters:**
- Currently dictionary is laid out by definition order (arbitrary)
- Hot words could be grouped in memory to improve L1/L2 cache hits
- **Potential gain:** 3-5% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐⭐ (Hard)
- **Effort:** 20-30 hours (reorganization algorithm, validation, testing)
- **Files affected:** `src/memory_management.c`, `src/dictionary_management.c`, serialization
- **Testing:** Memory layout validation, cache performance analysis

**Why it's later:**
- Complex to implement without breaking compatibility
- Requires careful serialization/deserialization of dictionary
- Benefits diminish if inference engine is already optimizing cache
- Risky to existing stability

**Prerequisites:**
- Complete hotwords cache tuning
- Prove inference engine is stable
- Measure memory pressure before reorganizing

**Future:** Advanced optimization, after baseline is solid

---

## Tier 4: Experimental/Research

### 10. SIMD Vectorization ⭐⭐

**What it is:**
Use AVX2/AVX-512 vector instructions for bulk word execution, batch processing, or memory operations.

**Why it matters:**
- Modern CPUs have unused vector capacity
- Could enable batch word processing (e.g., 4 parallel words)
- **Potential gain:** 5-10% performance improvement

**Implementation Cost:**
- **Difficulty:** ⭐⭐⭐⭐⭐ (Very Hard)
- **Effort:** 60-100+ hours
- **Files affected:** Core execution loop in `src/vm.c`, new SIMD module
- **Testing:** Correctness validation (SIMD bugs are subtle), cross-platform

**Why it's last:**
- FORTH's nature (sequential data stack) doesn't parallelize well
- Word dispatch is hard to vectorize (data dependencies)
- Gains are speculative (may not vectorize at all)
- Maintenance burden (SIMD code is fragile, platform-specific)
- Diminishing returns after other optimizations

**Prerequisites:**
- All other optimizations complete
- Proof that word dispatch is the actual bottleneck
- Acceptance that maintenance cost is worth 5-10% gain

**Future:** Research project, not MVP priority

---

## Recommended Sequencing

### Phase 1: Foundation (Weeks 1-2)
```
1. Run factorial DoE on decay rate (TST_DECAY_01-04)      [2-4 runs × 30 samples]
   └─ Measure impact of DECAY_MIN_INTERVAL_NS, SHRINK_RATE
   └─ Expected: 3-6% improvement, identify optimal values

2. Complete adaptive decay slope inference (code)           [4-6 hours]
   └─ Implement closed-form regression in inference engine
   └─ Unit test against synthetic trajectories
```

### Phase 2: Validation (Weeks 2-3)
```
3. Complete variance-based window tuning (code)             [3-5 hours]
   └─ Implement variance inflection detection
   └─ Unit test with synthetic variance patterns

4. Run 2×2 factorial DoE: decay_slope × window_size        [8-16 runs × 30 samples]
   (TST_WINDOW_DoE_01-04)
   └─ Expected: 8-15% improvement over baseline
   └─ Identify optimal strategy
```

### Phase 3: Refinement (Week 4)
```
5. Hotwords threshold DoE (optional)                        [4-8 runs × 30 samples]
   (TST_THRESHOLD_01-04)
   └─ Sweep cache threshold, identify sweet spot

6. Binary search validation (research)                      [8-12 hours]
   └─ Compare closed-form vs iterative approaches
   └─ Document numerical stability findings
```

### Phase 4+: Advanced (Future)
```
7-10. ML models, NUMA optimization, vectorization          [Deferred to Phase 2 product]
```

---

## Decision Framework: Which to Do First?

**If you want quick wins:** Start with #1 + #3 (decay slope inference + parameter tuning)
- Effort: ~10 hours code + 2-3 days experiments
- Expected gain: 8-15% over baseline
- Validates inference engine approach

**If you want systematic optimization:** Do Tier 1 + Tier 2 in order
- Effort: ~30 hours code + 5-7 days experiments
- Expected gain: 15-25% over baseline
- Comprehensive tuning of all major knobs

**If you want research:** Pick one deep dive (binary search, ML, NUMA)
- Effort: 40-100 hours
- Expected gain: Varies widely, high risk/high reward
- Publishable results

---

## Open Questions

1. **Should we start with parameter sweeps (no code) or inference implementation (code)?**
   - Parameter sweeps are faster feedback (days vs weeks)
   - Inference engine is better long-term (automatic adaptation)
   - Recommendation: **Do parameter sweeps first, then inference**

2. **Is 88.37% prefetch accuracy "good enough" or should we chase higher?**
   - Current value is deterministic, fixed by test workload
   - ML approaches could improve to ~92-95%, but with latency risk
   - Recommendation: **Validate parameter tuning first, then consider ML**

3. **Should we optimize for TST_03 (deterministic test suite) or real-world workloads?**
   - Test suite gives clean, reproducible metrics
   - Real workloads would show variance in pipelining
   - Recommendation: **Keep test suite for validation, plan real-workload experiments for Phase 2**

---

## Summary Table

| Rank | Item | Impact | Effort | ROI | Status |
|------|------|--------|--------|-----|--------|
| 1 | Decay slope inference | High | Low | 2.5 | Design ready, code needed |
| 2 | Window width tuning | High | Low | 2.0 | Design ready, code needed |
| 3 | Decay rate DoE | Med-High | Very Low | 1.5 | Ready to run now |
| 4 | Window sizing DoE | High | Medium | 1.33 | Ready after #1-2 complete |
| 5 | Cache threshold DoE | Med-High | Medium | 1.0 | Ready to run now |
| 6 | Binary search validation | Medium | Medium | 1.0 | Research phase |
| 7 | ML prefetch model | Very High | Very High | 1.0 | Future product phase |
| 8 | NUMA memory placement | High | High | 1.0 | Platform-specific |
| 9 | Dictionary reorganization | Medium-High | High | 0.75 | Advanced optimization |
| 10 | SIMD vectorization | Medium | Very High | 0.4 | Research/experimental |
