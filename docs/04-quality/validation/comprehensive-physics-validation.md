/*
                                  ***   StarForth   ***

  COMPREHENSIVE PHYSICS ENGINE VALIDATION PROTOCOL

  Formal experimental design for validating the complete physics-driven
  optimization architecture across three independent configurations.

  Document Version: 1.1
  Date: 2025-11-06 (Updated with input parsing repair)
  Status: Ready for Execution (with documented workaround)
*/

---

# Comprehensive Physics Engine Validation Protocol

## Executive Summary

This protocol describes a rigorous, triple-blind experimental design to validate the StarForth physics engine's ability to gather metrics and make real-time optimization decisions across three cumulative feature configurations:

1. **Baseline**: Un-optimized FORTH VM (fastest build, no physics optimizations)
2. **Configuration A**: Cache + Word Lookup optimization (physics-driven frequency detection)
3. **Configuration B**: Cache + Word Lookup + Pipelining (multi-layer physics optimization)

Each configuration will be tested **30 times** (90 total runs) to establish statistical significance via Bayesian credible intervals and confidence bounds. Results will be recorded in CSV format for meta-analysis, visualization, and formal publication.

---

## Purpose

### Primary Objective

To empirically validate that the StarForth physics engine's frequency-driven optimization model produces statistically significant and reproducible performance improvements across multiple optimization layers, without manual tuning or external intervention.

### Secondary Objectives

1. **Establish Credible Intervals**: Compute 95% and 99% Bayesian credible intervals for all metrics
2. **Demonstrate Reproducibility**: Show that results are deterministic across 30 independent runs
3. **Quantify Incremental Impact**: Isolate the contribution of each optimization layer (cache, then pipelining)
4. **Validate Theory**: Confirm that observable metrics (execution frequency, word transitions) drive observable performance improvements
5. **Enable Publication**: Generate publication-ready empirical evidence for peer review

---

## Methodology

### Experimental Design

**Configuration Matrix** (3 configurations × 30 runs = 90 total experiments)

```
Configuration A (Baseline):
  - ENABLE_HOTWORDS_CACHE=0
  - ENABLE_PIPELINING=0
  - All other flags: fastest build profile
  - Runs: 30

Configuration B (Cache + Lookup):
  - ENABLE_HOTWORDS_CACHE=1
  - ENABLE_PIPELINING=0
  - All other flags: fastest build profile
  - Runs: 30

Configuration C (Cache + Pipelining):
  - ENABLE_HOTWORDS_CACHE=1
  - ENABLE_PIPELINING=1
  - All other flags: fastest build profile
  - Runs: 30
```

### Test Harness

**Workload**: BENCH-DICT-LOOKUP with 100,000 iterations per run
- Executes diverse FORTH word lookups (IF, DUP, DROP, +, -, @, !, etc.)
- Statistically valid sample size (exceeds 10,000 minimum for 95% confidence)
- Deterministic dictionary state (initialized identically each run)
- Captures latency in Q48.16 fixed-point (nanosecond precision)

**Benchmark Sequence** (PRE/RESET/POST with Warmup):

Each run executes a three-phase benchmark to ensure clean measurement:

```
PHASE 1 - PRE (Warmup):
  Execute 100,000 BENCH-DICT-LOOKUP iterations
  Purpose: Let cache populate, pipelining windows stabilize
  Metrics: NOT recorded (warmup only)

PHASE 2 - RESET (Clear Counters):
  Execute PHYSICS-RESET-STATS
  Execute PIPELINING-RESET-ALL
  Purpose: Clear all diagnostic counters for clean POST measurement

PHASE 3 - POST (Actual Measurement):
  Execute 100,000 BENCH-DICT-LOOKUP iterations
  Purpose: Measure performance with warm cache/pipelining state
  Metrics: RECORDED (all stats captured)
  Diagnostics: PHYSICS-CACHE-STATS, PIPELINING-STATS, ROLLING-WINDOW-STATS
```

**Rationale for Three-Phase Design**:
- Eliminates cold-start artifacts (cache empty, pipelining context unpopulated)
- Measures steady-state performance (realistic production scenario)
- Separates warmup effects from actual optimization benefits
- Ensures fair comparison: all configurations run warmup before measurement
- Reduces variance: stable cache state means lower variance across runs

### Measurement Protocol

**Per-Run Metrics** (recorded in CSV):

1. **Latency Statistics** (all in nanoseconds, Q48.16 fixed-point)
   - Cache hit latency (mean, min, max, stddev)
   - Bucket search latency (mean, min, max, stddev)
   - Context-based prediction latency (if pipelining enabled)

2. **Cache Performance** (if enabled)
   - Total lookups executed
   - Cache hits (count and percentage)
   - Bucket hits (count and percentage)
   - Cache misses (count and percentage)
   - Promotions (words elevated to hot-words cache)
   - Evictions (words removed via LRU)

3. **Pipelining Metrics** (if enabled)
   - Total transitions recorded
   - Correct predictions (count and percentage)
   - Incorrect predictions (count and percentage)
   - Context window diversity (unique patterns observed)
   - Transition accuracy by context depth

4. **System Metrics**
   - Total execution time (milliseconds)
   - Memory allocated (bytes)
   - Pattern diversity saturation (%)
   - Rolling window final effective size

### Statistical Analysis

**Per Configuration** (across 30 runs):

1. **Descriptive Statistics**
   - Mean, median, mode
   - Standard deviation, variance
   - Min, max, range

2. **Bayesian Inference** (Q48.16 fixed-point arithmetic)
   - Posterior distribution estimates
   - 95% credible intervals (2.5th to 97.5th percentile)
   - 99% credible intervals (0.5th to 99.5th percentile)
   - Highest density regions (HDI)

3. **Comparative Analysis**
   - Speedup: Configuration B vs A (cache impact)
   - Speedup: Configuration C vs B (pipelining impact)
   - Speedup: Configuration C vs A (total impact)
   - Effect sizes (Cohen's d, or Bayesian equivalent)

4. **Reproducibility Metrics**
   - Coefficient of variation (CV = stddev / mean)
   - Range consistency (max - min across 30 runs)
   - Stability index (how deterministic are results)

### Data Recording Format

**CSV Schema** (one row per run):

```
timestamp,configuration,run_number,
total_lookups,cache_hits,cache_hit_percent,
bucket_hits,bucket_hit_percent,misses,miss_percent,
cache_hit_latency_ns,cache_hit_stddev_ns,cache_hit_min_ns,cache_hit_max_ns,
bucket_search_latency_ns,bucket_search_stddev_ns,bucket_search_min_ns,bucket_search_max_ns,
context_predictions_total,context_correct,context_accuracy_percent,
window_diversity_percent,window_final_size_bytes,
total_runtime_ms,memory_allocated_bytes,
speedup_vs_baseline,ci_lower_95,ci_upper_95
```

### Build and Execution Protocol

**Randomized Test Matrix Design** (Eliminates Temporal Bias):

Instead of executing configurations sequentially (A×30, B×30, C×30), we use **randomized ordering** to distribute config switches throughout the experiment timeline:

```
Step 1: Generate Test Specifications
  - Create 90 test specs: 30 × A_BASELINE, 30 × B_CACHE, 30 × C_FULL
  - Each spec includes: config_name, cache_flag, pipeline_flag, run_number

Step 2: Randomize Execution Order
  - Shuffle test specs using random permutation (sort -R)
  - Results in random sequence: C, A, B, C, C, A, B, A, C, B, ... (not sequential)

Step 3: Execute with Build Caching
  - Read randomized test matrix line by line
  - For each test spec:
    * If config differs from current: clean and rebuild (ONCE per config)
    * If config same as current: reuse binary (NO rebuild, saves time)
    * Execute benchmark run and record metrics

Step 4: Track Progress
  - Report "Run X/90 - CONFIG_NAME #RUN_NUMBER"
  - Preserve run numbering for reproducibility (A_BASELINE_run_8, C_FULL_run_27, etc.)
```

**Benefits of Randomized Execution**:

1. **Eliminates Order Bias**: Early-run anomalies spread across all configs, not concentrated in Baseline
2. **Temporal Distribution**: Config switches distributed throughout timeline, prevents block effects
3. **Build Efficiency**: Only 3 rebuilds total (one per config), despite interleaved runs
4. **Reproducibility**: Test matrix saved to `test_matrix.txt` for auditing and replication
5. **Scientific Rigor**: Matches best practices from peer-reviewed experimental design literature

**For Each Configuration** (executed in randomized order, 30 runs total):

```bash
# When encountering first instance of configuration in randomized matrix:
make clean
make ARCH=amd64 TARGET=fastest ENABLE_HOTWORDS_CACHE=${CACHE_FLAG} ENABLE_PIPELINING=${PIPELINE_FLAG}

# For each run of that configuration (interleaved with other configs):
# NOTE: Commands must be separated by newlines for proper VM parsing
printf '%s\n' \
    "100000 BENCH-DICT-LOOKUP" \
    "PHYSICS-RESET-STATS PIPELINING-RESET-ALL" \
    "100000 BENCH-DICT-LOOKUP" \
    "PHYSICS-CACHE-STATS" \
    "PIPELINING-STATS" \
    "ROLLING-WINDOW-STATS" \
    "BYE" | ./build/amd64/fastest/starforth
# Parse output, extract metrics, append to CSV
```

**Implementation Note (Input Parsing Repair)**:
- Initially implemented with bash heredoc (`cat <<EOF ... EOF`), but discovered VM outer interpreter required explicit newlines between commands
- StarForth VM tokenizes input on whitespace and processes complete commands per line (matching FORTH-79 standard REPL behavior)
- **Fix Applied (2025-11-06)**: Switched from heredoc to `printf '%s\n'` to explicitly put each command on separate line
- This ensures proper command demarcation and prevents VM from parsing multiple commands as one continuous input
- **Future Improvement**: VM input parser could be enhanced to handle multi-line blocks (Issue TBD), but current workaround is production-ready

**Example Randomized Execution Sequence**:
```
Trial 1: B_CACHE #8    (build B, run #8)
Trial 2: A_BASELINE #11 (build A, run #11)
Trial 3: C_FULL #9     (build C, run #9)
Trial 4: A_BASELINE #5  (reuse A binary, run #5)
Trial 5: C_FULL #24    (reuse C binary, run #24)
Trial 6: B_CACHE #1    (reuse B binary, run #1)
Trial 7: C_FULL #14    (reuse C binary, run #14)
... (86 more trials in randomized order)
```

---

## Justification

### Why This Design?

**1. Triple-Blind Configuration Progression**
- Baseline establishes "no optimization" control
- Configuration A isolates cache/lookup impact
- Configuration B measures incremental pipelining benefit
- Each layer's contribution becomes mathematically isolatable

**2. Statistical Power (30 Runs Per Configuration)**
- Exceeds minimum sample size (n=30) for 95% confidence in Bayesian inference
- Sufficient to detect effect sizes as small as 5% with high power
- Permits reliable credible interval computation
- Detects variance/stability issues that single runs cannot expose

**3. Q48.16 Fixed-Point Arithmetic**
- No floating-point approximation errors
- Nanosecond-precision latency measurement
- Formal verification compatible (Isabelle/HOL proofs)
- Reproducible: same computation always yields same result

**4. Deterministic Workload**
- Identical dictionary state across all 90 runs
- Same lookup sequence (100,000 iterations)
- No external variability (OS load, thermal drift, etc.)
- Results are pure function of optimization layer effectiveness

**5. Observable, Traceable Metrics**
- Every optimization decision logged (cache promotes, predictions made, etc.)
- Physics model decisions visible: "This word promoted because entropy > 50"
- Can prove causality: "Speedup occurred because X % of lookups hit cache"

**6. Publication-Ready Design**
- Follows empirical methods from peer-reviewed systems papers
- Bayesian approach (modern gold standard for small-sample inference)
- Reproducible: any lab can rebuild and re-run
- All code open-source, deterministic, auditable

### Why These Metrics?

**Latency Measurements**
- Direct measure of optimization effectiveness
- Nanosecond precision needed to resolve cache vs bucket cost
- Variance shows whether optimization is stable or brittle

**Cache/Pipeline Statistics**
- Prove that physics engine is making decisions (promotions, predictions)
- Show *why* speedup occurred (what % hit cache vs bucket)
- Enable root-cause analysis if results disappoint

**Pattern Diversity & Window Size**
- Validate rolling window self-tuning logic
- Show that system learned to shrink window appropriately
- Confirm no redundant data being captured

**Reproducibility Metrics**
- CV < 5% would indicate very stable physics model
- CV > 20% would suggest brittleness, need for investigation
- Range consistency shows whether outliers are concerning

### Why This Justifies Publishing?

1. **Rigor**: 90 runs × statistical inference = publication-grade evidence
2. **Reproducibility**: Deterministic system, can be re-run identically anywhere
3. **Novelty**: First empirical validation of multi-layer physics optimization in FORTH
4. **Causality**: Observable metrics directly explain performance improvements
5. **Scalability**: Method extends to future optimization layers (memory, scheduling, etc.)

---

## Success Criteria

### Minimum Standards for Publication

**Configuration A (Baseline)**
- ✅ All 30 runs complete without error
- ✅ 100,000 lookups per run executed successfully
- ✅ Metrics recorded in CSV format
- ✅ Mean latency computed with 95% credible interval

**Configuration B (Cache)**
- ✅ Cache hit rate > 20% (proves cache is functioning)
- ✅ Speedup > 1.1× vs Configuration A (statistically significant)
- ✅ 95% credible interval excludes 1.0 (rules out null hypothesis)
- ✅ Variance < 10% CV (reproducible, stable)

**Configuration C (Cache + Pipelining)**
- ✅ Prediction accuracy > 60% for context windows (better than random)
- ✅ Speedup > Configuration B (pipelining adds measurable benefit)
- ✅ Pattern diversity saturation > 90% (rolling window captured patterns)
- ✅ Window final size < initial size (self-tuning worked)

### Stretch Goals

- Speedup B vs A: 1.5–2.0× (repeat cache experiment)
- Speedup C vs B: 1.2–1.5× (pipelining incremental gain)
- Speedup C vs A: 1.8–3.0× (combined effect)
- CV across all metrics: < 5% (highly deterministic)

---

## Expected Outcomes & Deliverables

### Primary Deliverables

1. **experiment_results_comprehensive.csv** (90 rows, one per run)
   - Raw metrics from all configurations

2. **COMPREHENSIVE_PHYSICS_ENGINE_VALIDATION_RESULTS.md** (publication-ready report)
   - Summary statistics (means, CIs, Bayesian posteriors)
   - Comparative analysis (speedup, effect sizes)
   - Visualizations (latency distributions, CIs, speedup curves)
   - Interpretation and implications

3. **experiment_analysis.py** (analysis script)
   - Reads CSV, computes statistics
   - Generates publication-quality plots
   - Produces Bayesian credible intervals

### Secondary Deliverables

- Individual per-run logs (for audit trail, reproducibility)
- Build configurations used (for replication)
- Hardware/OS environment details (for contextualization)

---

## Timeline & Execution Schedule

**Phase 1: Setup (30 minutes)**
- Verify build system, test flags
- Create CSV output file with headers
- Prepare benchmarking script

**Phase 2: Baseline Runs (30–40 minutes)**
- Execute Configuration A × 30 runs
- Record metrics to CSV
- Quick sanity check (no crashes, reasonable latencies)

**Phase 3: Cache Runs (30–40 minutes)**
- Execute Configuration B × 30 runs
- Record metrics to CSV
- Verify cache is hitting (hit rate > 0%)

**Phase 4: Pipelining Runs (30–40 minutes)**
- Execute Configuration C × 30 runs
- Record metrics to CSV
- Verify predictions are occurring (accuracy > 0%)

**Phase 5: Analysis (20–30 minutes)**
- Compute statistics (means, stddevs, CIs)
- Generate visualizations
- Write results document

**Total Estimated Time: 2–3 hours**

---

## Assumptions & Constraints

### Assumptions

1. **Determinism**: Same input → same output (no OS variability, thermal drift, etc.)
2. **Dictionary Stability**: Dictionary unchanged across all 90 runs
3. **Build Reproducibility**: Same compile flags produce identical binaries
4. **Measurement Accuracy**: Q48.16 arithmetic captures latencies faithfully

### Constraints

- Must use "fastest" build profile (not production, not debug)
- Dictionary must be initialized identically each run
- No external load on system during experiments
- Single-threaded execution (no concurrency variability)

---

## Hypothesis

### Null Hypothesis (H₀)
The physics engine's optimization layers produce no statistically significant performance improvement:
- H₀: Speedup(B vs A) = 1.0
- H₀: Speedup(C vs B) = 1.0
- H₀: Speedup(C vs A) = 1.0

### Alternative Hypothesis (H₁)
The physics engine's optimization layers produce measurable performance improvements:
- H₁: Speedup(B vs A) > 1.1 (cache adds ≥10% improvement)
- H₁: Speedup(C vs B) > 1.05 (pipelining adds ≥5% improvement)
- H₁: Speedup(C vs A) > 1.15 (combined ≥15% improvement)

**Decision Rule**: If 95% credible interval excludes 1.0, reject H₀.

---

## Conclusion

This protocol provides a **rigorous, reproducible, publication-ready framework** for validating the StarForth physics engine across three cumulative optimization layers. By executing 90 runs with proper statistical analysis, we can produce peer-reviewed evidence that:

1. ✅ Observable metrics (execution frequency, word transitions) drive real performance gains
2. ✅ Optimization decisions are deterministic and repeatable
3. ✅ Physics model scales from single optimization (cache) to multi-layer architecture (cache + pipelining)
4. ✅ All improvements are measurable, traceable, and theoretically justified

The results will form the empirical foundation for StarForth's formal verification via Isabelle/HOL, demonstrating that performance claims are not only theoretically sound but empirically validated.

---

**Protocol Approved**: Ready to Execute
**Execution Start**: [TBD - User initiation]
**Expected Completion**: [TBD + 2-3 hours from start]
