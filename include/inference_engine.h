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

#ifndef INFERENCE_ENGINE_H
#define INFERENCE_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#include "vm.h"          /* For RollingWindowOfTruth, VM structure */
#include "q48_16.h"      /* For Q48.16 fixed-point math */

/* ============================================================================
 * OpAmp Model: Inference Engine Architecture
 * ============================================================================
 *
 * The inference engine unifies all VM adaptive tuning into a single loop:
 *
 *   Rolling Window of Truth (execution_heat trajectory, metrics)
 *           ↓
 *   [Inference Engine]
 *           ├─ ANOVA Early-Exit: Check if variance stable
 *           ├─ Window Width Inference: Variance inflection detection
 *           ├─ Decay Slope Inference: Closed-form linear regression
 *           └─ Diagnostics: Fit quality, variance metrics
 *           ↓
 *   Tuning Outputs (adaptive_window_width, adaptive_decay_slope)
 *           ↓
 *   [Apply to VM]
 *           ↓
 *   [System Adapts]
 *           ↓
 *   [Feedback Loop Repeats]
 *
 * Key Design Principles:
 * - Q48.16 math only (no floating-point)
 * - ANOVA early-exit for efficiency (5% variance threshold)
 * - Closed-form algorithms (deterministic, fast)
 * - Observable diagnostics (RStudio dashboard feed)
 */

/* ============================================================================
 * Input Data Structure: Metrics snapshot + window context
 * ============================================================================
 */

typedef struct {
    VM *vm;                              /* Owning VM (for dictionary heat snapshots) */
    /* === Window Context === */
    RollingWindowOfTruth *window;         /* Pointer to window of truth */
    uint64_t trajectory_length;           /* Number of heat entries to analyze */

    /* === Current Metrics Snapshot === */
    uint64_t prefetch_hits;               /* Successful prefetch predictions */
    uint64_t prefetch_attempts;           /* Total prefetch attempts */
    uint64_t hot_word_count;              /* Words above execution_heat threshold */
    uint64_t stale_word_count;            /* Words with execution_heat in low range */
    uint64_t total_heat;                  /* Sum of all execution_heat values */
    uint32_t word_count;                  /* Total dictionary entries */

    /* === Baseline (For Trending) === */
    uint64_t last_total_heat;             /* Total heat from previous check */
    uint64_t last_stale_count;            /* Stale count from previous check */
} InferenceInputs;

/* ============================================================================
 * Output Data Structure: Tuning parameters + diagnostics
 * ============================================================================
 */

struct InferenceOutputs {
    /* === Tuning Outputs (Apply to VM) === */
    uint32_t adaptive_window_width;       /* Inferred optimal window size (from variance inflection) */
    uint64_t adaptive_decay_slope;        /* Inferred decay slope in Q48.16 (from regression) */

    /* === Diagnostics (RStudio Dashboard Feed) === */
    uint64_t window_variance_q48;         /* Variance of heat across window in Q48.16 */
    uint64_t slope_fit_quality_q48;       /* R² or residual metric in Q48.16 (0.0 to 1.0) */

    /* === Status Tracking === */
    uint32_t early_exited;                /* 1 if ANOVA check skipped full inference, 0 if full run */
    uint64_t last_check_tick;             /* Timestamp of last full inference (for monitoring) */
};

/* ============================================================================
 * Main API: Run Inference Engine
 * ============================================================================
 *
 * Single entry point that handles:
 * 1. ANOVA early-exit (check variance stability)
 * 2. Full inference (if variance changed significantly)
 * 3. Update tuning outputs
 *
 * Thread-Safety: Safe for synchronous calls from vm_tick()
 * (No shared state written; outputs struct is caller-allocated)
 */

/**
 * @brief Run adaptive inference engine on window data
 *
 * High-level flow:
 * 1. ANOVA Check: Is variance delta < 5%?
 *    - YES: Return cached outputs immediately (cost: ~100 cycles)
 *    - NO: Continue to full inference
 *
 * 2. Extract trajectory: Fresh execution_heat snapshot from dictionary
 *
 * 3. Window Width Inference:
 *    - Compute variance across entire window
 *    - Detect inflection point (where d(variance)/d(size) → 0)
 *    - Return optimal window size
 *
 * 4. Decay Slope Inference:
 *    - Transform trajectory to log space (ln(heat[t]))
 *    - Fit linear regression: ln(heat) = a - slope*t
 *    - Extract slope coefficient in Q48.16
 *
 * 5. Diagnostics:
 *    - Compute fit quality (R² or residual)
 *    - Store variance metric for next early-exit check
 *
 * @param inputs Snapshot of window + metrics (caller-provided)
 * @param outputs Tuning parameters + diagnostics (caller-provided, will be updated)
 *
 * @return void (results_run_01_2025_12_08 in outputs struct)
 *
 * Preconditions:
 * - inputs != NULL
 * - outputs != NULL
 * - inputs->window != NULL
 * - inputs->window->is_warm == 1 (window has enough data)
 * - inputs->trajectory_length > 1
 *
 * Side Effects:
 * - Modifies outputs struct (caller must preserve if caching)
 * - Allocates temporary trajectory buffer (freed on return)
 */
void inference_engine_run(InferenceInputs *inputs, InferenceOutputs *outputs);

/* ============================================================================
 * Configuration Constants (In vm.h, but referenced here)
 * ============================================================================
 *
 * User-tunable via Makefile:
 *
 * #define HEARTBEAT_INFERENCE_FREQUENCY 5000
 *   - Ticks between full inference runs
 *   - Decreasing = faster response, more CPU
 *   - Increasing = slower response, less CPU
 *
 * #define VARIANCE_SIGNIFICANCE_THRESHOLD 5
 *   - Percentage (0-100) at which variance change triggers re-inference
 *   - Default: 5% (skip inference if variance changed < 5%)
 *   - Decreasing = more frequent re-inference (conservative)
 *   - Increasing = less frequent re-inference (aggressive)
 *
 * #define ADAPTIVE_MIN_WINDOW_SIZE 256
 *   - Minimum effective_window_size (never shrink below this)
 *   - Ensures minimum statistical validity
 *   - Increasing = higher overhead, more accuracy
 */

/* ============================================================================
 * Helper Functions (For Testing / Diagnostics)
 * ============================================================================
 */

/**
 * @brief Pretty-print inference outputs for logging
 *
 * Format: "window_width=XXX variance_q48=YYYY slope_q48=ZZZZ fit_quality=WWWW"
 *
 * @param outputs Inference outputs struct
 * @return Pointer to static string (valid until next call)
 */
const char* inference_outputs_to_string(const InferenceOutputs *outputs);

/**
 * @brief Validate inference outputs for sanity
 *
 * Checks:
 * - window_width is within [ADAPTIVE_MIN_WINDOW_SIZE, ROLLING_WINDOW_SIZE]
 * - slope_q48 is reasonable (between 0.001 and 100 in Q48.16)
 * - variance and fit_quality are between 0.0 and 1.0 in Q48.16
 *
 * @param outputs Inference outputs to validate
 * @return 1 if valid, 0 if suspicious
 */
int inference_outputs_validate(const InferenceOutputs *outputs);

/* ============================================================================
 * Internal Functions (Exposed for Unit Testing)
 * ============================================================================
 */

/**
 * @brief Compute variance of heat data in Q48.16 format
 *
 * @param heat_data Array of heat values
 * @param length Number of elements in array
 * @return Variance in Q48.16 format
 */
q48_16_t compute_variance_q48(const uint64_t *heat_data, uint64_t length);

/**
 * @brief Find variance inflection point for optimal window width
 *
 * @param heat_data Array of heat values
 * @param trajectory_length Total trajectory length
 * @param full_variance Variance of full trajectory
 * @return Optimal window width
 */
uint32_t find_variance_inflection(const uint64_t *heat_data, uint64_t trajectory_length, q48_16_t full_variance);

/**
 * @brief Infer decay slope from heat trajectory using linear regression
 *
 * @param heat_data Array of heat values
 * @param length Number of elements
 * @return Decay slope in Q48.16 format
 */
uint64_t infer_decay_slope_q48(const uint64_t *heat_data, uint64_t length);

#endif /* INFERENCE_ENGINE_H */
