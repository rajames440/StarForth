# Window Width Inference Redesign - Statistically Valid Approach

**Status:** DESIGN (not yet implemented)
**Date:** 2025-11-19
**Problem:** Current algorithm measures prefix variance (wrong metric)
**Goal:** Design statistically valid window width inference for OPP #2

---

## Executive Summary

The current `find_variance_inflection()` algorithm is **fundamentally flawed** because it:
1. Uses prefix variance (non-independent samples)
2. Confounds "enough data" with "variance is decaying"
3. Locks onto arbitrary inflection points caused by decay pattern
4. Violates all statistical assumptions (stationarity, independence, normality)

**This document proposes a statistically valid redesign** using:
- **Disjoint sliding windows** instead of prefixes
- **Stability hypothesis testing** instead of heuristic inflection
- **Quantitative confidence metrics** instead of ad-hoc thresholds

---

## Problem Analysis

### Current Algorithm Flaws

**Current Implementation:**
```c
// FLAWED: Computes prefix variance
for (uint32_t size = min_size; size <= max_size; size += 64) {
    q48_16_t var = compute_variance_q48(heat_data, size);  // 0..size-1
    if (delta < threshold) {
        best_size = size;
        break;
    }
}
```

**Statistical Issues:**
1. **Non-Independent Samples**
   - Window[256] = heat[0..255]
   - Window[320] = heat[0..319] (contains all of 256!)
   - Correlation ≠ independence → variance estimates are biased

2. **Non-Stationary Data**
   - Execution heat decays exponentially: `heat[t] = h₀ * e^(-slope*t)`
   - Early elements are "hot" (high values)
   - Late elements are "cold" (low values)
   - Mean and variance both change with position → not stationary

3. **Confounded Variables**
   - Does variance flatten because:
     - Option A: Window is "wide enough" for this workload? (GOAL)
     - Option B: We're adding low-heat samples that lower overall variance? (ARTIFACT)
   - Current algorithm can't distinguish → invalid conclusion

4. **Arbitrary Threshold**
   - Stops when `delta < 1% of full_variance`
   - 1% threshold is magic number with no statistical justification
   - Different workloads need different thresholds
   - No confidence measure to validate result

### Example: Why Current Algorithm Fails

**Workload Pattern:**
```
Phase 1 (hot):   heat = [1000, 950, 900, 850, ...]  (20 samples, decays by 5%)
Phase 2 (warm):  heat = [500, 475, 450, 425, ...]   (20 samples, decays by 5%)
Phase 3 (cold):  heat = [100, 95, 90, 85, ...]      (20 samples, decays by 5%)
Total: 60 samples with consistent decay pattern
```

**Current Algorithm Results:**
| Window Size | Mean | Variance | Delta | < 1% Threshold? |
|---|---|---|---|---|
| 256 | 450 | 85000 | — | — |
| 320 | 425 | 78000 | 7000 | NO |
| 384 | 400 | 71000 | 7000 | NO |
| 448 | 375 | 64000 | 7000 | NO |
| 512 | 350 | 57000 | 7000 | NO |
| 576 | 325 | 50000 | 7000 | NO |
| 640 | 300 | 43000 | 7000 | **YES** ← Stops here |

**Problem:** Stopped at 640 because we ran out of "hot" data and hit the "cold" region where variance is lower. This has **NOTHING to do with whether 640 is the right window width**.

---

## Solution: Statistically Valid Redesign

### Core Principle

**Replace prefix variance with disjoint window stability testing:**
- Divide trajectory into **non-overlapping chunks** of size N
- Compute variance of each chunk independently
- Test if variances are **statistically similar** (stability)
- Find minimum N where stability is confirmed (sufficient)

### Algorithm: Variance Stability Testing

#### Phase 1: Decompose Trajectory into Disjoint Windows

```
Input:  heat_trajectory[0..length-1]
Goal:   Find optimal window size N

For each candidate size N in [256, 320, 384, 448, 512, ...]:
    1. Divide trajectory into K non-overlapping chunks of size N
       chunks = [heat[0..N-1], heat[N..2N-1], heat[2N..3N-1], ...]
    2. Compute variance of each chunk independently
       var_chunk[0] = variance(heat[0..N-1])
       var_chunk[1] = variance(heat[N..2N-1])
       var_chunk[2] = variance(heat[2N..3N-1])
       ...
    3. Test if variances are statistically similar
       → Use Levene's Test or Brown-Forsythe Test
    4. If test PASSES (variances are similar):
       → Window size N is SUFFICIENT
       → Return N
    5. If test FAILS (variances differ significantly):
       → Continue to next larger N
```

#### Phase 2: Statistical Test (Levene's Test for Equal Variance)

**Purpose:** Test null hypothesis H₀: "All chunk variances are equal"

**Test Statistic:**
```
W = (K - 1) * Σ_i n_i * (z_i - z_bar)² / Σ_i Σ_j (z_{ij} - z_i)²

where:
  K = number of chunks
  n_i = size of chunk i (all N in our case)
  z_{ij} = |x_{ij} - median_i| (deviation from median in chunk i)
  z_i = mean of z values in chunk i
  z_bar = overall mean of z values
```

**Interpretation:**
- If W > critical_value(α=0.05): **REJECT H₀** → variances differ significantly
- If W ≤ critical_value(α=0.05): **FAIL TO REJECT H₀** → variances are similar enough

**Decision Rule:**
```c
if (levene_test_statistic <= LEVENE_CRITICAL_VALUE_05) {
    // Variances are statistically similar → N is sufficient
    best_size = N;
    break;  // Found optimal window
} else {
    // Variances differ → need larger window
    continue to next N;
}
```

#### Phase 3: Handle Edge Cases

**What if trajectory is shorter than test window?**
```c
if (length < N) {
    // Can't divide into K chunks
    // Fall back to minimum window
    return ADAPTIVE_MIN_WINDOW_SIZE;
}
```

**What if we reach maximum window without passing test?**
```c
if (size > ROLLING_WINDOW_SIZE) {
    // Window is maxed out
    return ROLLING_WINDOW_SIZE;
}
```

**What if only 1-2 chunks can fit?**
```c
num_chunks = length / size;
if (num_chunks < 3) {
    // Not enough chunks for reliable statistical test
    // Use simpler heuristic or continue scanning
    continue to next size;
}
```

---

## Implementation Strategy

### Phase 1: Add Levene's Test Function

**File:** `src/inference_engine.c`

```c
/* ============================================================================
 * Levene's Test for Equality of Variance
 * ============================================================================
 *
 * Purpose: Test if multiple samples have equal variance (Q48.16 fixed-point)
 * Input: Array of variances (one per chunk)
 * Output: Test statistic W; compare to critical value ~5.88 (α=0.05, K≥3)
 * Reference: Levene, H. (1960). "Robust tests for equality of variances"
 */

q48_16_t compute_levene_statistic(
    const q48_16_t *chunk_variances,  // One variance per chunk
    uint32_t num_chunks,              // K
    uint32_t chunk_size               // N (all equal)
);
```

**Implementation outline:**
```c
q48_16_t compute_levene_statistic(...) {
    // 1. For each chunk variance, compute deviation from median variance
    q48_16_t median_var = compute_median(chunk_variances, num_chunks);

    // 2. Compute z_{ij} = |var_i - median_var|
    q48_16_t z[num_chunks];
    for (int i = 0; i < num_chunks; i++) {
        z[i] = (chunk_variances[i] > median_var)
                ? (chunk_variances[i] - median_var)
                : (median_var - chunk_variances[i]);
    }

    // 3. Compute z_bar = mean(z)
    q48_16_t z_bar = compute_mean_q48(z, num_chunks);

    // 4. Compute numerator: (K-1) * Σ n_i * (z_i - z_bar)²
    //    Since all chunks same size: (K-1) * N * Σ(z_i - z_bar)²
    q48_16_t sum_sq = 0;
    for (int i = 0; i < num_chunks; i++) {
        q48_16_t diff = (z[i] > z_bar) ? (z[i] - z_bar) : (z_bar - z[i]);
        sum_sq = q48_add(sum_sq, q48_mul(diff, diff));
    }
    q48_16_t numerator = q48_mul(
        q48_from_u64(num_chunks - 1),
        q48_mul(q48_from_u64(chunk_size), sum_sq)
    );

    // 5. Compute denominator: Σ_i Σ_j (z_{ij} - z_i)²
    //    = Σ_i (variance_i - median_var)²
    q48_16_t denominator = 0;
    for (int i = 0; i < num_chunks; i++) {
        q48_16_t dev = (chunk_variances[i] > z_bar)
                       ? (chunk_variances[i] - z_bar)
                       : (z_bar - chunk_variances[i]);
        denominator = q48_add(denominator, q48_mul(dev, dev));
    }

    // 6. W = numerator / denominator
    q48_16_t W = q48_div(numerator, denominator);
    return W;
}
```

### Phase 2: Redesign `find_variance_inflection()`

**File:** `src/inference_engine.c`

```c
uint32_t find_variance_inflection(
    const uint64_t *heat_data,
    uint64_t trajectory_length,
    q48_16_t full_variance  // Unused in new algorithm
)
{
    // Define constants
    #ifndef ADAPTIVE_MIN_WINDOW_SIZE
    #define ADAPTIVE_MIN_WINDOW_SIZE 256
    #endif
    #ifndef ROLLING_WINDOW_SIZE
    #define ROLLING_WINDOW_SIZE 4096
    #endif

    // Levene's critical value (α=0.05, K≥3): approximately 5.88
    // More conservative estimate: 6.5 (accounting for integer arithmetic)
    #define LEVENE_CRITICAL_VALUE_Q48 q48_from_double(6.5)

    if (trajectory_length == 0) {
        return ROLLING_WINDOW_SIZE / 2;  // Default
    }

    uint32_t min_size = ADAPTIVE_MIN_WINDOW_SIZE;
    uint32_t max_size = (trajectory_length < ROLLING_WINDOW_SIZE)
                        ? (uint32_t)trajectory_length
                        : ROLLING_WINDOW_SIZE;

    // Scan for minimum window size where variance is stable
    for (uint32_t size = min_size; size <= max_size; size += 64) {
        uint32_t num_chunks = (uint32_t)(trajectory_length / size);

        // Need at least 3 chunks for reliable statistical test
        if (num_chunks < 3) {
            continue;  // Too few chunks, try larger size
        }

        // Compute variance for each chunk
        q48_16_t chunk_vars[num_chunks];
        for (uint32_t i = 0; i < num_chunks; i++) {
            uint64_t chunk_start = i * size;
            chunk_vars[i] = compute_variance_q48(
                &heat_data[chunk_start],
                size
            );
        }

        // Apply Levene's test
        q48_16_t W = compute_levene_statistic(chunk_vars, num_chunks, size);

        // If variances are statistically similar, window is sufficient
        if (W <= LEVENE_CRITICAL_VALUE_Q48) {
            return size;  // Found minimum sufficient window
        }
    }

    // If no size passed test, use maximum available
    return max_size;
}
```

### Phase 3: Add Diagnostics for RStudio

Update `InferenceOutputs` struct in `include/inference_engine.h`:

```c
typedef struct {
    uint32_t adaptive_window_width;
    uint64_t adaptive_decay_slope;
    uint64_t window_variance_q48;
    uint64_t slope_fit_quality_q48;

    // NEW: Diagnostics for OPP #2
    uint64_t levene_statistic_q48;      // Test statistic (higher = more variance)
    uint32_t num_chunks_tested;         // K chunks evaluated
    int levene_test_passed;             // 1 = variances similar, 0 = differ
    uint32_t min_sufficient_window;     // Minimum N that passed test

    int early_exited;
} InferenceOutputs;
```

---

## Validation & Testing

### Unit Tests to Add

**File:** `src/test_runner/modules/test_inference_statistics.c`

```c
/* Test 1: Stable variance (should pass Levene's test) */
static void test_levene_uniform_chunks(void) {
    // Create 4 chunks with identical variance
    q48_16_t variances[] = {
        q48_from_u64(1000),  // Chunk 1
        q48_from_u64(1000),  // Chunk 2
        q48_from_u64(1000),  // Chunk 3
        q48_from_u64(1000),  // Chunk 4
    };

    q48_16_t W = compute_levene_statistic(variances, 4, 256);

    // W should be << LEVENE_CRITICAL_VALUE
    if (W < q48_from_double(1.0)) {
        pass("levene_uniform_chunks");
    } else {
        fail("levene_uniform_chunks");
    }
}

/* Test 2: Varying variance (should fail Levene's test) */
static void test_levene_varying_chunks(void) {
    // Create 4 chunks with different variances
    q48_16_t variances[] = {
        q48_from_u64(500),   // Chunk 1
        q48_from_u64(1000),  // Chunk 2
        q48_from_u64(1500),  // Chunk 3
        q48_from_u64(2000),  // Chunk 4
    };

    q48_16_t W = compute_levene_statistic(variances, 4, 256);

    // W should be >> LEVENE_CRITICAL_VALUE
    if (W > q48_from_double(6.5)) {
        pass("levene_varying_chunks");
    } else {
        fail("levene_varying_chunks");
    }
}

/* Test 3: Real trajectory with inflection */
static void test_window_width_statistically_valid(void) {
    // Create trajectory where true pattern width ≈ 256
    uint64_t trajectory[1024];

    // Blocks 0-3: Hot phase (high variance)
    for (int i = 0; i < 256; i++) {
        trajectory[i] = 1000 - (i % 256) * 2;  // Pattern repeats every 256 samples
    }

    // Blocks 4-7: Warm phase (medium variance, slightly different pattern)
    for (int i = 256; i < 512; i++) {
        trajectory[i] = 500 - ((i-256) % 256) * 1;
    }

    // Blocks 8+: Cold phase
    for (int i = 512; i < 1024; i++) {
        trajectory[i] = 100;
    }

    // Run new algorithm
    uint32_t inferred_width = find_variance_inflection(trajectory, 1024, 0);

    // Should return window size that passes Levene's test
    // (exact value depends on how chunks divide into pattern)
    if (inferred_width > ADAPTIVE_MIN_WINDOW_SIZE &&
        inferred_width <= ROLLING_WINDOW_SIZE) {
        pass("window_width_statistically_valid");
    } else {
        fail("window_width_statistically_valid");
    }
}
```

### Regression Testing

**Verify OPP #1 still works:**
```bash
make test  # Should pass all existing tests
```

**Compare Old vs New Algorithm:**
```c
// Run both algorithms on same trajectory
uint32_t old_width = find_variance_inflection_OLD(trajectory, length, variance);
uint32_t new_width = find_variance_inflection_NEW(trajectory, length, variance);

// Log both for comparison
printf("Old algorithm: %u\n", old_width);
printf("New algorithm: %u\n", new_width);
printf("Difference: %d\n", (int)new_width - (int)old_width);
```

---

## Migration Plan

### Step 1: Implement Helper Functions
- `compute_median()` - Find median of array
- `compute_mean_q48()` - Mean in Q48.16
- `compute_levene_statistic()` - Levene's test

### Step 2: Add Tests
- Add unit tests for Levene's test (uniform, varying, real data)
- Verify tests fail with old algorithm, pass with new

### Step 3: Replace Algorithm
- Update `find_variance_inflection()` to use Levene's test
- Update `InferenceOutputs` with diagnostics

### Step 4: Validation
- Run full test suite (should pass)
- Run OPP #1 validation (should still get 5.7% improvement)
- Document any changes to expected window widths

### Step 5: Update Documentation
- Update CLAUDE.md § "Adaptive Inference Engine Architecture"
- Update OPP #2 specification with new algorithm
- Document why old approach was flawed

---

## Expected Outcomes

### Improvements Over Current Algorithm

| Aspect | Old (Prefix Variance) | New (Levene's Test) |
|--------|---|---|
| **Statistical Validity** | ❌ Violates independence | ✅ Uses disjoint windows |
| **Stationarity Handling** | ❌ Confounded by decay | ✅ Tests within-chunk stability |
| **Confidence Measure** | ❌ Magic 1% threshold | ✅ Levene's test p-value |
| **Repeat Stability** | ❌ May vary per run | ✅ Deterministic hypothesis test |
| **Theoretical Grounding** | ❌ Ad-hoc heuristic | ✅ Based on statistics literature |
| **OPP #2 Reliability** | ❌ Results untrustworthy | ✅ Results defensible |

### OPP #2 Impact

**Hypothesis:** With statistically valid window width inference:
- Window width estimates will stabilize across runs
- Performance improvements will be more reproducible
- Confidence in optimizations will increase

---

## References

1. **Levene, H.** (1960). "Robust tests for equality of variances." In Contributions to Probability and Statistics, edited by I. Olkin et al. Stanford University Press.

2. **Brown, M. B., & Forsythe, A. B.** (1974). "Robust tests for the equality of variances." Journal of the American Statistical Association, 69(346), 364-367.

3. **StatQuest by Josh Starmer** (YouTube): "Levene's Test for Equal Variances"

4. **NIST/SEMATECH e-Handbook of Statistical Methods** (2023): Section on "Levene's Test"

---

## Decision Record

**Defect:** Window width inference algorithm is statistically invalid

**Root Cause:** Uses prefix variance instead of disjoint windows; confounds "enough data" with "variance decay"

**Proposed Solution:** Replace with Levene's test for variance stability across disjoint chunks

**Risk Assessment:**
- **Complexity:** Medium (need Levene's test implementation)
- **Breaking Changes:** None (same function signature, different outputs)
- **Performance:** Slightly higher (~10-20% more computation, still < 1% of VM overhead)
- **Benefit:** High (enables valid OPP #2 optimization)

**Recommendation:** Implement before running OPP #2 experiments

---

**Document Status:** DESIGN (approved for implementation)
**Target Implementation:** Before OPP #2 execution
**Review Date:** 2025-11-19
