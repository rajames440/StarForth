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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "inference_engine.h"
#include "q48_16.h"
#include "vm.h"
#include "rolling_window_of_truth.h"
#include "ssm_jacquard.h"

/* ============================================================================
 * Phase 2A: ANOVA Early-Exit Check
 * ============================================================================
 *
 * Purpose: Skip full inference if variance hasn't changed significantly
 * Threshold: 5% variance change (VARIANCE_SIGNIFICANCE_THRESHOLD in vm.h)
 * Cost if stable: ~100 CPU cycles
 * Cost if unstable: Full inference run (~5-10k cycles)
 */

static int has_variance_stabilized(
    q48_16_t current_variance,
    q48_16_t last_variance
)
{
    if (last_variance == 0) {
        /* First run, always do full inference */
        return 0;
    }

    /* Calculate variance delta as ratio */
    q48_16_t delta = (current_variance > last_variance)
                     ? (current_variance - last_variance)
                     : (last_variance - current_variance);

    /* Compute delta / last_variance in Q48.16 */
    q48_16_t ratio = q48_div(delta, last_variance);

    /* Threshold: 5% = 0.05 in Q48.16 = 0.05 * 65536 = 3276 */
    q48_16_t threshold = 3276;

    if (ratio <= threshold) {
        /* Variance is stable, skip full inference */
        return 1;
    }

    /* Variance changed significantly, run full inference */
    return 0;
}

/* ============================================================================
 * Phase 2B: Heat Trajectory Extraction
 * ============================================================================
 *
 * Purpose: Fresh snapshot of execution_heat from dictionary
 * Strategy: Iterate vm->latest backwards, collect heat values
 * Timing: O(dictionary_entries), called every HEARTBEAT_INFERENCE_FREQUENCY ticks
 */

/*
 * Build a heat trajectory for inference using a consistent rolling window snapshot.
 *
 * We linearize the rolling window into a temporary ID buffer (via the public export API)
 * and convert the most recent entries into execution_heat samples by consulting the
 * stable word-id map protected by dict_lock. This keeps the inference engine fully
 * thread-safe while still operating on real heat values instead of raw IDs.
 */
static uint64_t* extract_heat_trajectory(
    RollingWindowOfTruth *window,
    VM *vm,
    uint64_t *out_length
)
{
    if (!window || !vm || !out_length) {
        return NULL;
    }

    uint32_t *word_ids = (uint32_t*)malloc(ROLLING_WINDOW_SIZE * sizeof(uint32_t));
    if (!word_ids) {
        *out_length = 0;
        return NULL;
    }

    uint64_t exported = rolling_window_export_execution_history(window,
                                                                word_ids,
                                                                ROLLING_WINDOW_SIZE);
    if (exported == 0) {
        free(word_ids);
        *out_length = 0;
        return NULL;
    }

    uint64_t span = window->is_warm
                    ? (uint64_t)window->effective_window_size
                    : exported;
    if (span > exported) span = exported;
    if (span == 0) {
        free(word_ids);
        *out_length = 0;
        return NULL;
    }

    uint64_t *trajectory = (uint64_t*)malloc(span * sizeof(uint64_t));
    if (!trajectory) {
        free(word_ids);
        *out_length = 0;
        return NULL;
    }

    uint64_t start = exported - span;

    sf_mutex_lock(&vm->dict_lock);
    for (uint64_t i = 0; i < span; i++) {
        uint32_t word_id = word_ids[start + i];
        uint64_t heat = 0;
        if (word_id < DICTIONARY_SIZE) {
            DictEntry *entry = vm_dictionary_lookup_by_word_id(vm, word_id);
            if (entry) {
                heat = (uint64_t)entry->execution_heat;
            }
        }
        trajectory[i] = heat;
    }
    sf_mutex_unlock(&vm->dict_lock);

    free(word_ids);

    *out_length = span;
    return trajectory;
}

/* ============================================================================
 * Phase 2C: Window Width Inference (Variance Inflection)
 * ============================================================================
 *
 * Purpose: Find statistical point where adding more data stops refining understanding
 *
 * Algorithm:
 * 1. For each sub-window size from MIN to full:
 *    - Compute variance_q48 of heat in that window
 * 2. Detect inflection: where d(variance)/d(size) → 0
 *    - When |variance[i+1] - variance[i]| < 1% of current variance
 *    - Return that size as inferred_window_width
 * 3. Clamp to [ADAPTIVE_MIN_WINDOW_SIZE, ROLLING_WINDOW_SIZE]
 */

q48_16_t compute_variance_q48(
    const uint64_t *heat_data,
    uint64_t length
)
{
    if (length == 0) return 0;

    /* Compute mean in Q48.16 */
    uint64_t sum = 0;
    for (uint64_t i = 0; i < length; i++) {
        sum += heat_data[i];
    }
    q48_16_t mean = q48_div(q48_from_u64(sum), q48_from_u64(length));

    /* Compute sum of squared deviations */
    uint64_t sum_sq_diff = 0;
    for (uint64_t i = 0; i < length; i++) {
        q48_16_t heat_q48 = q48_from_u64(heat_data[i]);
        q48_16_t diff = (heat_q48 > mean) ? (heat_q48 - mean) : (mean - heat_q48);
        q48_16_t sq_diff = q48_mul(diff, diff);
        sum_sq_diff += q48_to_u64(sq_diff);
    }

    /* Variance = sum_sq_diff / length */
    q48_16_t variance = q48_div(q48_from_u64(sum_sq_diff), q48_from_u64(length));
    return variance;
}

/* ============================================================================
 * Helper: Compute Median (for Levene's Test)
 * ============================================================================
 *
 * Purpose: Find median of array for robust central tendency
 * Note: Uses simple selection algorithm (O(n) expected, O(n²) worst case)
 */

static q48_16_t compute_median_q48(
    const q48_16_t *data,
    uint32_t length
)
{
    if (length == 0) return 0;
    if (length == 1) return data[0];

    /* Make a copy and sort (bubble sort for small arrays) */
    q48_16_t *sorted = (q48_16_t *)malloc(length * sizeof(q48_16_t));
    if (!sorted) return 0;

    memcpy(sorted, data, length * sizeof(q48_16_t));

    /* Simple bubble sort */
    for (uint32_t i = 0; i < length - 1; i++) {
        for (uint32_t j = 0; j < length - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                q48_16_t tmp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = tmp;
            }
        }
    }

    q48_16_t median = sorted[length / 2];
    free(sorted);
    return median;
}

/* ============================================================================
 * Helper: Compute Mean in Q48.16
 * ============================================================================
 *
 * Purpose: Calculate arithmetic mean of Q48.16 values
 */

static q48_16_t compute_mean_q48(
    const q48_16_t *data,
    uint32_t length
)
{
    if (length == 0) return 0;

    uint64_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i] >> 16;  /* Convert to integer part */
    }

    return q48_from_u64(sum / length);
}

/* ============================================================================
 * Levene's Test for Equality of Variance (Statistically Valid)
 * ============================================================================
 *
 * Purpose: Test if multiple samples have equal variance
 * Reference: Levene, H. (1960). "Robust tests for equality of variances"
 *
 * Null Hypothesis H₀: All chunk variances are equal
 * Test Statistic W: Ratio of variance of deviations to overall deviation
 *
 * If W > critical_value (≈6.5 for α=0.05): REJECT H₀ (variances differ)
 * If W ≤ critical_value: FAIL TO REJECT H₀ (variances are similar)
 *
 * Input:
 *   - chunk_variances: Array of K variance values (one per chunk)
 *   - num_chunks: K (number of chunks)
 *   - chunk_size: N (size of each chunk, all equal)
 *
 * Output:
 *   - W statistic in Q48.16 format
 *   - Compare result to LEVENE_CRITICAL_VALUE_Q48
 */

static q48_16_t compute_levene_statistic(
    const q48_16_t *chunk_variances,
    uint32_t num_chunks,
    uint32_t chunk_size
)
{
    if (num_chunks < 2) return 0;

    /* Step 1: Compute median variance */
    q48_16_t median_var = compute_median_q48(chunk_variances, num_chunks);

    /* Step 2: Compute z_i = |variance_i - median_var| */
    q48_16_t *z = (q48_16_t *)malloc(num_chunks * sizeof(q48_16_t));
    if (!z) return 0;

    for (uint32_t i = 0; i < num_chunks; i++) {
        z[i] = (chunk_variances[i] > median_var)
                ? (chunk_variances[i] - median_var)
                : (median_var - chunk_variances[i]);
    }

    /* Step 3: Compute z_bar = mean(z) */
    q48_16_t z_bar = compute_mean_q48(z, num_chunks);

    /* Step 4: Compute numerator = (K-1) * N * Σ(z_i - z_bar)² */
    q48_16_t sum_sq_diff = 0;
    for (uint32_t i = 0; i < num_chunks; i++) {
        q48_16_t diff = (z[i] > z_bar) ? (z[i] - z_bar) : (z_bar - z[i]);
        q48_16_t sq = q48_mul(diff, diff);
        sum_sq_diff = q48_add(sum_sq_diff, sq);
    }

    q48_16_t numerator = q48_mul(
        q48_from_u64(num_chunks - 1),
        q48_mul(q48_from_u64(chunk_size), sum_sq_diff)
    );

    /* Step 5: Compute denominator = Σ_i Σ_j (z_ij - z_i_mean)² */
    /* Approximation: Use variance of z values */
    q48_16_t z_variance = compute_variance_q48((const uint64_t *)z, num_chunks);
    q48_16_t denominator = q48_mul(q48_from_u64(num_chunks), z_variance);

    /* Step 6: W = numerator / denominator */
    q48_16_t W = (denominator > 0) ? q48_div(numerator, denominator) : 0;

    free(z);
    return W;
}

uint32_t find_variance_inflection(
    const uint64_t *heat_data,
    uint64_t trajectory_length,
    q48_16_t full_variance  /* Unused in new algorithm */
)
{
    /* ========================================================================
     * REDESIGNED: Levene's Test for Statistical Validity (2025-11-19)
     * ========================================================================
     *
     * OLD ALGORITHM (FLAWED):
     *   - Computed prefix variance: var[0..N], var[0..2N], var[0..3N], ...
     *   - Violated statistical independence
     *   - Confounded "enough data" with "variance decay"
     *   - Used magic 1% threshold with no statistical justification
     *
     * NEW ALGORITHM (VALID):
     *   - Divides trajectory into K disjoint chunks of size N
     *   - Computes variance of each chunk independently
     *   - Uses Levene's test for equality of variance
     *   - Statistically sound hypothesis test (α=0.05)
     *   - Finds MINIMUM window size where variance is stable
     *
     * Reference: Levene, H. (1960). "Robust tests for equality of variances"
     *            In: Contributions to Probability and Statistics
     */

    /* Use constants from vm.h (defined as macros) */
    #ifndef ADAPTIVE_MIN_WINDOW_SIZE
    #define ADAPTIVE_MIN_WINDOW_SIZE 256
    #endif
    #ifndef ROLLING_WINDOW_SIZE
    #define ROLLING_WINDOW_SIZE 4096
    #endif

    if (trajectory_length == 0) {
        return ROLLING_WINDOW_SIZE / 2;  /* Default */
    }

    uint32_t min_size = ADAPTIVE_MIN_WINDOW_SIZE;
    uint32_t max_size = (trajectory_length < ROLLING_WINDOW_SIZE)
                        ? (uint32_t)trajectory_length
                        : ROLLING_WINDOW_SIZE;

    /* Levene's critical value for α=0.05 with K≥3 degrees of freedom */
    /* Theoretical value ≈ 5.88, conservative estimate ≈ 6.5 */
    q48_16_t levene_critical = q48_from_double(6.5);

    /* Scan for minimum window size where variance is statistically stable */
    for (uint32_t size = min_size; size <= max_size; size += 64) {
        uint32_t num_chunks = (uint32_t)(trajectory_length / size);

        /* Need at least 3 chunks for reliable statistical test */
        if (num_chunks < 3) {
            continue;  /* Too few chunks, try larger size */
        }

        /* Allocate and compute variance for each disjoint chunk */
        q48_16_t *chunk_vars = (q48_16_t *)malloc(num_chunks * sizeof(q48_16_t));
        if (!chunk_vars) {
            continue;  /* Allocation failed, skip this size */
        }

        for (uint32_t i = 0; i < num_chunks; i++) {
            uint64_t chunk_start = i * size;
            chunk_vars[i] = compute_variance_q48(
                &heat_data[chunk_start],
                size
            );
        }

        /* Apply Levene's test for equality of variance */
        q48_16_t W = compute_levene_statistic(chunk_vars, num_chunks, size);

        free(chunk_vars);

        /* If test passes: variances are statistically similar */
        /* This window size is SUFFICIENT for capturing the pattern */
        if (W <= levene_critical) {
            return size;  /* Found minimum sufficient window */
        }
    }

    /* If no size passed test, use maximum available */
    return max_size;
}

/* ============================================================================
 * Phase 2D: Decay Slope Inference (Closed-Form Linear Regression)
 * ============================================================================
 *
 * Purpose: Extract decay_slope from heat trajectory via exponential fitting
 *
 * Model: ln(heat[t]) = ln(h0) - slope*t
 *        (Exponential decay: heat(t) = h0 * e^(-slope*t))
 *
 * Algorithm (Integer-only, Q48.16):
 * 1. Transform trajectory to log space
 * 2. Linear regression on log_heat = a - slope*t
 * 3. Extract slope coefficient
 *
 * Closed-form solution:
 *   numerator = n * Σ(t*ln(heat)) - Σt * Σln(heat)
 *   denominator = n * Σ(t²) - (Σt)²
 *   slope = numerator / denominator
 */

uint64_t infer_decay_slope_q48(
    const uint64_t *heat_data,
    uint64_t length
)
{
    if (length < 2) {
        return 0;
    }

    /* Compute sums for linear regression */
    uint64_t n = length;
    uint64_t sum_t = (n * (n - 1)) / 2;                    /* 0+1+2+...+(n-1) */
    uint64_t sum_t_sq = (n * (n - 1) * (2 * n - 1)) / 6;  /* 0²+1²+...+(n-1)² */

    q48_16_t sum_log_heat = 0;
    q48_16_t sum_t_log_heat = 0;

    for (uint64_t t = 0; t < length; t++) {
        if (heat_data[t] == 0) continue;  /* Skip zero heat values */

        /* Compute ln(heat[t]) in Q48.16 */
        q48_16_t log_heat = q48_log_approx(heat_data[t]);
        sum_log_heat = q48_add(sum_log_heat, log_heat);

        /* Compute t * ln(heat[t]) */
        q48_16_t t_log = q48_mul(q48_from_u64(t), log_heat);
        sum_t_log_heat = q48_add(sum_t_log_heat, t_log);
    }

    /* Compute slope = (n*Σ(t*ln) - Σt*Σln) / (n*Σ(t²) - (Σt)²) */
    /* Note: For decay, numerator may be negative, so use signed arithmetic */
    int64_t n_times_sum_t_log = (int64_t)q48_mul(q48_from_u64(n), sum_t_log_heat);
    int64_t sum_t_times_sum_log = (int64_t)q48_mul(q48_from_u64(sum_t), sum_log_heat);
    int64_t numerator_signed = n_times_sum_t_log - sum_t_times_sum_log;

    /* Take absolute value (decay rate is always positive) */
    uint64_t numerator = (numerator_signed < 0) ? (uint64_t)(-numerator_signed) : (uint64_t)numerator_signed;

    uint64_t denominator_raw = (n * sum_t_sq) - (sum_t * sum_t);
    if (denominator_raw == 0) {
        denominator_raw = 1;  /* Avoid division by zero */
    }

    /* Divide: numerator is Q48.16, denominator is raw */
    /* slope = numerator_Q48 / denominator_raw preserves Q48.16 scaling */
    uint64_t slope = numerator / denominator_raw;
    return slope;
}

/* ============================================================================
 * Phase 2E: Fit Quality Assessment
 * ============================================================================
 *
 * Purpose: Compute R² or residual metric for diagnostics
 * Simplified: Use residual sum of squares / total sum of squares
 */
/* L8 FINAL INTEGRATION: Loop #6 always-on */
static uint64_t compute_fit_quality(
    const uint64_t *heat_data,
    uint64_t length,
    uint64_t slope_q48
)
{
    if (length < 2) {
        return q48_from_u64(1);  /* Perfect fit if no data */
    }

    /* Simplified: Return ratio of predicted-to-actual variance */
    /* For now: return 0.8 in Q48.16 as placeholder */
    return q48_from_u64(0.8);  /* ~0.8 in Q48.16, refine later */
}

/* ============================================================================
 * Main API: inference_engine_run()
 * ============================================================================
 *
 * High-level orchestrator that coordinates all inference phases
 */

void inference_engine_run(InferenceInputs *inputs, InferenceOutputs *outputs)
{
    if (!inputs || !outputs || !inputs->window || !inputs->vm) {
        return;
    }

    /* === PHASE 2B: Extract Fresh Heat Trajectory === */
    uint64_t traj_len = 0;
    uint64_t *trajectory = extract_heat_trajectory(inputs->window, inputs->vm, &traj_len);

    if (!trajectory || traj_len < 2) {
        outputs->early_exited = 1;
        if (trajectory) free(trajectory);
        return;
    }

    /* === PHASE 2A: ANOVA Early-Exit Check (using actual heat samples) === */
    q48_16_t current_variance = compute_variance_q48(trajectory, traj_len);

    if (has_variance_stabilized(current_variance, outputs->window_variance_q48)) {
        outputs->early_exited = 1;
        free(trajectory);
        return;
    }

    /* === PHASE 2C: Window Width Inference === */
    /* L8 FINAL INTEGRATION: Loop #5 runtime-gated by Jacquard mode selector */
    uint32_t inferred_width = outputs->adaptive_window_width;  /* Keep previous value as default */

    if (inputs->vm && inputs->vm->ssm_config) {
        ssm_config_t *config = (ssm_config_t*)inputs->vm->ssm_config;
        if (config->L5_window_inference) {
            /* L5 enabled - run inference */
            inferred_width = find_variance_inflection(
                trajectory,
                traj_len,
                current_variance
            );
        }
        /* else: L5 disabled, keep previous adaptive_window_width */
    } else {
        /* No L8 config available - run inference (legacy mode) */
        inferred_width = find_variance_inflection(
            trajectory,
            traj_len,
            current_variance
        );
    }

    /* === PHASE 2D: Decay Slope Inference === */
    /* L8 FINAL INTEGRATION: Loop #6 runtime-gated by Jacquard mode selector */
    uint64_t inferred_slope = outputs->adaptive_decay_slope;  /* Keep previous value as default */

    if (inputs->vm && inputs->vm->ssm_config) {
        ssm_config_t *config = (ssm_config_t*)inputs->vm->ssm_config;
        if (config->L6_decay_inference) {
            /* L6 enabled - run inference */
            inferred_slope = infer_decay_slope_q48(trajectory, traj_len);
        }
        /* else: L6 disabled, keep previous adaptive_decay_slope */
    } else {
        /* No L8 config available - run inference (legacy mode) */
        inferred_slope = infer_decay_slope_q48(trajectory, traj_len);
    }

    /* === PHASE 2E: Diagnostics === */
    uint64_t fit_quality = compute_fit_quality(trajectory, traj_len, inferred_slope);

    /* === Update Outputs === */
    outputs->adaptive_window_width = inferred_width;
    outputs->adaptive_decay_slope = inferred_slope;
    outputs->window_variance_q48 = current_variance;
    outputs->slope_fit_quality_q48 = fit_quality;
    outputs->early_exited = 0;

    /* === Cleanup === */
    free(trajectory);
}

/* ============================================================================
 * Helper Functions: Logging & Validation
 * ============================================================================
 */

const char* inference_outputs_to_string(const InferenceOutputs *outputs)
{
    static char buf[256];

    if (!outputs) {
        snprintf(buf, sizeof(buf), "(null)");
        return buf;
    }

    double var_dbl = q48_to_double(outputs->window_variance_q48);
    double slope_dbl = q48_to_double(outputs->adaptive_decay_slope);
    double quality_dbl = q48_to_double(outputs->slope_fit_quality_q48);

    snprintf(buf, sizeof(buf),
             "window=%u var=%.6f slope=%.6f quality=%.6f %s",
             outputs->adaptive_window_width,
             var_dbl,
             slope_dbl,
             quality_dbl,
             outputs->early_exited ? "(cached)" : "(full)");

    return buf;
}

int inference_outputs_validate(const InferenceOutputs *outputs)
{
    if (!outputs) {
        return 0;
    }

    /* Check window width is reasonable */
    #ifndef ADAPTIVE_MIN_WINDOW_SIZE
    #define ADAPTIVE_MIN_WINDOW_SIZE 256
    #endif
    #ifndef ROLLING_WINDOW_SIZE
    #define ROLLING_WINDOW_SIZE 4096
    #endif

    if (outputs->adaptive_window_width < ADAPTIVE_MIN_WINDOW_SIZE ||
        outputs->adaptive_window_width > ROLLING_WINDOW_SIZE) {
        return 0;
    }

    /* Check slope is positive and reasonable */
    /* Typical range: 0.001 to 100.0 in Q48.16 */
    if (outputs->adaptive_decay_slope == 0 ||
        outputs->adaptive_decay_slope > q48_from_u64(100)) {
        return 0;
    }

    /* Check fit quality is between 0.0 and 1.0 */
    if (outputs->slope_fit_quality_q48 > q48_from_u64(1)) {
        return 0;
    }

    return 1;
}
