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

#include "ssm_jacquard.h"
#include <string.h>

/* ============================================================================
 * L8 Initialization
 * ============================================================================
 */

void ssm_l8_init(ssm_l8_state_t *state, ssm_l8_mode_t initial_mode)
{
    if (!state) return;

    state->current_mode = initial_mode;
    state->hysteresis_counter = 0;
    state->pending_mode = initial_mode;
}

/* ============================================================================
 * L8 Mode Selection Logic (Data-Driven from Top 5% DoE Analysis)
 * ============================================================================
 */

void ssm_l8_update(const ssm_l8_metrics_t *metrics, ssm_l8_state_t *state)
{
    if (!metrics || !state) return;

    /* Classify metrics into binary features */
    int entropy_high = (metrics->entropy >= SSM_ENTROPY_HIGH_THRESHOLD);
    int cv_high = (metrics->cv >= SSM_CV_HIGH_THRESHOLD);
    int temporal_high = (metrics->temporal_decay >= SSM_TEMPORAL_DECAY_THRESHOLD);
    int temporal_med = (metrics->temporal_decay >= SSM_TEMPORAL_DECAY_LOW_THRESHOLD);

    /* Determine target mode using 4-bit logic (L2 L3 L5 L6) */
    ssm_l8_mode_t target_mode = SSM_MODE_C0;  /* Default: minimal */

    /* L2 bit: Enable rolling window if high entropy (diversity) */
    int L2_bit = entropy_high ? 1 : 0;

    /* L3 bit: Enable linear decay if high temporal locality */
    int L3_bit = temporal_high ? 1 : 0;

    /* L5 bit: Enable window inference if high CV (volatility) */
    int L5_bit = cv_high ? 1 : 0;

    /* L6 bit: Enable decay inference if CV high AND moderate temporal */
    int L6_bit = (cv_high && temporal_med) ? 1 : 0;

    /* Combine bits into mode selector: L2 L3 L5 L6 */
    target_mode = (ssm_l8_mode_t)((L2_bit << 3) | (L3_bit << 2) | (L5_bit << 1) | L6_bit);

    /* Hysteresis: only change mode after SSM_HYSTERESIS_TICKS consecutive votes */
    if (target_mode == state->pending_mode) {
        /* Same target as last time, increment counter */
        state->hysteresis_counter++;

        if (state->hysteresis_counter >= SSM_HYSTERESIS_TICKS) {
            /* Threshold reached, commit mode change */
            if (target_mode != state->current_mode) {
                state->current_mode = target_mode;
            }
            state->hysteresis_counter = 0;  /* Reset for next change */
        }
    } else {
        /* Target changed, reset hysteresis */
        state->pending_mode = target_mode;
        state->hysteresis_counter = 1;
    }
}

/* ============================================================================
 * L8 Mode Application (Set L2/L3/L5/L6 Bits)
 * ============================================================================
 */

void ssm_apply_mode(const ssm_l8_state_t *state, ssm_config_t *config)
{
    if (!state || !config) return;

    /* Extract bits from mode (L2 L3 L5 L6) */
    int mode_val = (int)state->current_mode;

    config->L2_rolling_window   = (mode_val >> 3) & 1;  /* Bit 3 */
    config->L3_linear_decay     = (mode_val >> 2) & 1;  /* Bit 2 */
    config->L5_window_inference = (mode_val >> 1) & 1;  /* Bit 1 */
    config->L6_decay_inference  = (mode_val >> 0) & 1;  /* Bit 0 */
}

/* ============================================================================
 * L8 Utility Functions
 * ============================================================================
 */

const char* ssm_l8_mode_name(ssm_l8_mode_t mode)
{
    switch (mode) {
    case SSM_MODE_C0:  return "C0_MINIMAL";
    case SSM_MODE_C1:  return "C1_DECAY_INF";
    case SSM_MODE_C2:  return "C2_WINDOW_INF";
    case SSM_MODE_C3:  return "C3_VOLATILE";
    case SSM_MODE_C4:  return "C4_TEMPORAL";           /* ✅ TOP 5% */
    case SSM_MODE_C5:  return "C5_TEMPORAL_DECAY_INF";
    case SSM_MODE_C6:  return "C6_TEMPORAL_WINDOW_INF";
    case SSM_MODE_C7:  return "C7_FULL_INFERENCE";     /* ✅ TOP 5% */
    case SSM_MODE_C8:  return "C8_DIVERSE";
    case SSM_MODE_C9:  return "C9_DIVERSE_DECAY_INF";  /* ✅ TOP 5% */
    case SSM_MODE_C10: return "C10_DIVERSE_WINDOW_INF";
    case SSM_MODE_C11: return "C11_DIVERSE_INFERENCE"; /* ✅ TOP 5% */
    case SSM_MODE_C12: return "C12_DIVERSE_TEMPORAL";  /* ✅ TOP 5% */
    case SSM_MODE_C13: return "C13_COMPLEX";
    case SSM_MODE_C14: return "C14_FULL_ADAPTIVE_NO_DECAY_INF";
    case SSM_MODE_C15: return "C15_FULL_ADAPTIVE";
    default:           return "UNKNOWN";
    }
}