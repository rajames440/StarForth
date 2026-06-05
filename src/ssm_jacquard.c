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
#include <stdlib.h>

/* ============================================================================
 * L8 Initialization
 * ============================================================================
 */

void ssm_l8_init(ssm_l8_state_t *state, ssm_l8_mode_t initial_mode)
{
    if (!state) return;

    state->current_mode       = initial_mode;
    state->hysteresis_counter = 0;
    state->pending_mode       = initial_mode;
    state->table              = NULL;  /* Caller calls ssm_l8_init_table() separately */
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

/* ============================================================================
 * Adaptive Table: Integer Helpers
 * ============================================================================
 */

/* Newton's method integer square root, converges in ≤ 6 iterations for uint32_t */
static uint32_t isqrt32(uint32_t n)
{
    uint32_t x, y;
    if (n == 0) return 0;
    x = n;
    y = (x + 1u) / 2u;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2u;
    }
    return x;
}

/* Newton's method integer square root for uint64_t */
static uint64_t isqrt64(uint64_t n)
{
    uint64_t x, y;
    if (n == 0) return 0;
    x = n;
    y = (x + 1u) / 2u;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2u;
    }
    return x;
}

/* Integer approximation: ln(n) in Q16 via floor(log2(n)) * ln(2) * 65536.
 * Accurate within a factor of e/1 ≈ 2.7 — sufficient for UCB balancing. */
static uint32_t ln_approx_q16(uint32_t n)
{
    uint32_t bits, tmp;
    if (n <= 1u) return 0u;
    bits = 0u;
    tmp = n >> 1u;
    while (tmp > 0u) { tmp >>= 1u; bits++; }
    /* ln(2) * 65536 = 0.693147 * 65536 ≈ 45426 */
    return bits * 45426u;
}

/* ============================================================================
 * Adaptive Table: DoE Prior Seeding
 * ============================================================================
 *
 * Maps the DoE top-5% 4-bit modes (L2/L3/L5/L6) to the 7-bit space
 * (L1=0, L4=0, L7=1 held fixed for the high-score variants).
 *
 * Bit layout: b6=L1, b5=L2, b4=L3, b3=L4, b2=L5, b1=L6, b0=L7
 *
 * Top-5% modes (4-bit → 7-bit with L1=0,L4=0,L7=1):
 *   C4  (L3 only):        0100 → 0 0 1 0 0 0 1 = 0x11 = 17
 *   C7  (L3+L5+L6):       0111 → 0 0 1 0 1 1 1 = 0x17 = 23
 *   C9  (L2+L6):          1001 → 0 1 0 0 0 1 1 = 0x23 = 35
 *   C11 (L2+L5+L6):       1011 → 0 1 0 0 1 1 1 = 0x27 = 39
 *   C12 (L2+L3):          1100 → 0 1 1 0 0 0 1 = 0x31 = 49
 */
static void ssm_l8_seed_from_doe(SsmConfigTable *table)
{
    /* Top-5% config indices in 7-bit space */
    static const uint8_t top5[] = { 17u, 23u, 35u, 39u, 49u };
    uint32_t c, r, i;

    for (c = 0u; c < 128u; c++) {
        uint8_t bits = (uint8_t)c;
        int has_l1 = (bits & SSM_CFG_L1) != 0;
        int has_l4 = (bits & SSM_CFG_L4) != 0;
        int is_top5 = 0;
        uint32_t score;

        for (i = 0u; i < 5u; i++) {
            if (bits == top5[i]) { is_top5 = 1; break; }
        }

        if (is_top5)              score = (SSM_SCORE_MAX * 80u) / 100u;
        else if (has_l1 && has_l4) score = (SSM_SCORE_MAX *  5u) / 100u;
        else if (has_l1 || has_l4) score = (SSM_SCORE_MAX * 10u) / 100u;
        else                       score = (SSM_SCORE_MAX * 40u) / 100u;

        table->entries[c].config_bits    = bits;
        table->entries[c].benefit_score  = score;
        table->entries[c].trial_count    = 0u;
        table->entries[c].prev_window    = 0u;
        table->entries[c].prev_slope     = 0u;
        table->entries[c]._pad[0]        = 0u;
        table->entries[c]._pad[1]        = 0u;
        table->entries[c]._pad[2]        = 0u;

        for (r = 0u; r < 8u; r++) {
            table->regime_scores[r][c] = score;
            table->regime_trials[r][c] = 0u;
        }
    }

    /* Start on C4-equivalent (config index 17: L3+L7 only) */
    table->current_config            = 17u;
    table->current_regime            = 0u;
    table->_pad[0]                   = 0u;
    table->_pad[1]                   = 0u;
    table->trial_tick_count          = 0u;
    table->inference_runs_this_trial = 0u;
    table->anova_exits_this_trial    = 0u;

    for (r = 0u; r < 8u; r++) {
        table->total_regime_trials[r] = 0u;
    }
}

/* ============================================================================
 * Adaptive Table: Trial-End Reward + UCB Selection (static)
 * ============================================================================ */
static void ssm_l8_trial_end(SsmConfigTable *table, ssm_config_t *config,
                              uint32_t current_window, uint64_t current_slope)
{
    uint32_t r = (uint32_t)table->current_regime;
    uint32_t c = (uint32_t)table->current_config;
    SsmConfigEntry *entry = &table->entries[c];

    /* --- ANOVA stability (fraction of inference runs that early-exited) --- */
    uint32_t anova_stability;   /* Q16 in [0, 65536] */
    if (table->inference_runs_this_trial > 0u) {
        anova_stability = (table->anova_exits_this_trial * 65536u) /
                           table->inference_runs_this_trial;
    } else {
        anova_stability = 0u;
    }

    /* --- Joint convergence signal (across consecutive trials) --- */
    int l5_on = (entry->config_bits & SSM_CFG_L5) != 0;
    int l6_on = (entry->config_bits & SSM_CFG_L6) != 0;
    uint32_t combined_stability;  /* Q16 in [0, 65536] */

    if ((l5_on || l6_on) && entry->prev_window > 0u && entry->prev_slope > 0u) {
        /* Normalized relative deltas in Q16 */
        uint32_t dw_raw = (current_window > entry->prev_window)
                          ? (current_window - entry->prev_window)
                          : (entry->prev_window - current_window);
        uint64_t ds_raw = (current_slope > entry->prev_slope)
                          ? (current_slope - entry->prev_slope)
                          : (entry->prev_slope - current_slope);

        uint64_t dw_q16 = ((uint64_t)dw_raw * 65536u) / (uint64_t)entry->prev_window;
        uint64_t ds_q16 = (ds_raw * 65536u) / entry->prev_slope;
        if (dw_q16 > 65536u) dw_q16 = 65536u;
        if (ds_q16 > 65536u) ds_q16 = 65536u;

        /* joint_error = sqrt(dw² + ds²) / sqrt(2), normalised to [0, 65536] */
        /* sqrt(2) * 65536 ≈ 92682 */
        uint64_t sum_sq = dw_q16 * dw_q16 + ds_q16 * ds_q16;
        uint64_t joint_err = (isqrt64(sum_sq) * 65536u) / 92682u;
        if (joint_err > 65536u) joint_err = 65536u;

        uint32_t stability = (uint32_t)(65536u - (uint32_t)joint_err);

        /* Option A fixed weights: 0.75 joint + 0.25 ANOVA */
        combined_stability = (uint32_t)(
            ((uint64_t)SSM_JOINT_WEIGHT_Q16 * stability +
             (uint64_t)SSM_ANOVA_WEIGHT_Q16 * anova_stability) / 65536u
        );
    } else {
        /* First trial OR both L5+L6 disabled: ANOVA only (§5.7) */
        combined_stability = anova_stability;
    }

    /* --- Benefit delta: positive when combined > 0.5, negative when < 0.5 --- */
    /* neutral at combined = 32768 */
    int64_t delta = ((int64_t)combined_stability - 32768) *
                    (int64_t)SSM_REWARD_GAIN / 32768;

    /* --- Score update with exponential decay --- */
    uint32_t old_score = table->regime_scores[r][c];
    uint32_t decayed   = (uint32_t)(((uint64_t)old_score * SSM_SCORE_DECAY_FACTOR_Q16) / 65536u);
    int64_t  new_s     = (int64_t)decayed + delta;
    uint32_t new_score;
    if      (new_s < (int64_t)SSM_SCORE_MIN) new_score = SSM_SCORE_MIN;
    else if (new_s > (int64_t)SSM_SCORE_MAX) new_score = SSM_SCORE_MAX;
    else                                     new_score = (uint32_t)new_s;

    table->regime_scores[r][c] = new_score;
    table->regime_trials[r][c]++;
    if (table->total_regime_trials[r] < 0xFFFFFFFFu) table->total_regime_trials[r]++;
    entry->trial_count++;

    /* Record end-of-trial (window, slope) for next trial's reward */
    entry->prev_window = current_window;
    entry->prev_slope  = current_slope;

    /* --- UCB: select best config for next trial --- */
    {
        uint32_t total    = table->total_regime_trials[r];
        uint32_t ln_total = ln_approx_q16(total > 0u ? total : 1u);
        uint32_t best_c   = 0u;
        uint32_t best_ucb = 0u;
        uint32_t ci;

        for (ci = 0u; ci < 128u; ci++) {
            uint32_t nc  = table->regime_trials[r][ci];
            uint32_t ucb;

            if (nc == 0u) {
                ucb = SSM_SCORE_MAX;   /* Bootstrap: unexplored = maximally interesting */
            } else {
                /* bonus = UCB_K * sqrt(ln_total_q16 / nc) / 256  (result in score units) */
                uint32_t ratio     = ln_total / nc;
                uint32_t sqrt_r    = isqrt32(ratio);
                uint32_t bonus     = (uint32_t)(((uint64_t)SSM_UCB_K * sqrt_r) / 256u);
                uint32_t sc        = table->regime_scores[r][ci];
                ucb = sc + bonus;
                if (ucb > SSM_SCORE_MAX) ucb = SSM_SCORE_MAX;
            }

            if (ucb > best_ucb) { best_ucb = ucb; best_c = ci; }
        }

        table->current_config = (uint8_t)best_c;

        /* Apply new config bits to ssm_config_t */
        if (config != NULL) {
            uint8_t bits = table->entries[best_c].config_bits;
            config->L2_rolling_window   = (bits & SSM_CFG_L2) ? 1 : 0;
            config->L3_linear_decay     = (bits & SSM_CFG_L3) ? 1 : 0;
            config->L5_window_inference = (bits & SSM_CFG_L5) ? 1 : 0;
            config->L6_decay_inference  = (bits & SSM_CFG_L6) ? 1 : 0;
        }
    }

    /* Reset trial accumulators */
    table->trial_tick_count          = 0u;
    table->inference_runs_this_trial = 0u;
    table->anova_exits_this_trial    = 0u;
}

/* ============================================================================
 * Adaptive Table: Public API
 * ============================================================================
 */

void ssm_l8_init_table(ssm_l8_state_t *state)
{
    SsmConfigTable *table;
    if (!state) return;

    table = (SsmConfigTable *)malloc(sizeof(SsmConfigTable));
    if (!table) {
        state->table = NULL;   /* Fall back to legacy mode */
        return;
    }
    memset(table, 0, sizeof(SsmConfigTable));
    ssm_l8_seed_from_doe(table);
    state->table = table;
}

void ssm_l8_free_table(ssm_l8_state_t *state)
{
    if (!state || !state->table) return;
    free(state->table);
    state->table = NULL;
}

void ssm_l8_update_table(ssm_l8_state_t *state, const ssm_l8_metrics_t *metrics,
                         ssm_config_t *config,
                         uint32_t current_window, uint64_t current_slope)
{
    SsmConfigTable *table;
    uint8_t regime;

    if (!state || !state->table || !metrics) return;
    table = state->table;

    /* Current regime (same 3 comparisons as legacy L8) */
    regime = (uint8_t)(
        ((metrics->entropy       >= SSM_ENTROPY_HIGH_THRESHOLD)    ? 4u : 0u) |
        ((metrics->cv            >= SSM_CV_HIGH_THRESHOLD)          ? 2u : 0u) |
        ((metrics->temporal_decay >= SSM_TEMPORAL_DECAY_THRESHOLD)  ? 1u : 0u)
    );

    table->trial_tick_count++;

    if (metrics->inference_ran_this_tick) {
        table->inference_runs_this_trial++;
        if (metrics->inference_early_exited) {
            table->anova_exits_this_trial++;
        }
    }

    if (table->trial_tick_count >= SSM_MIN_TRIAL_TICKS) {
        table->current_regime = regime;
        ssm_l8_trial_end(table, config, current_window, current_slope);

        /* Mirror L2/L3/L5/L6 bits into the legacy mode field for diagnostics */
        {
            uint8_t bits = table->entries[table->current_config].config_bits;
            state->current_mode = (ssm_l8_mode_t)(
                (((bits & SSM_CFG_L2) ? 1u : 0u) << 3) |
                (((bits & SSM_CFG_L3) ? 1u : 0u) << 2) |
                (((bits & SSM_CFG_L5) ? 1u : 0u) << 1) |
                (((bits & SSM_CFG_L6) ? 1u : 0u) << 0)
            );
        }
    }
}

void ssm_apply_mode_from_table(const ssm_l8_state_t *state, ssm_config_t *config)
{
    const SsmConfigTable *table;
    uint8_t bits;

    if (!state || !state->table || !config) return;
    table = state->table;
    bits  = table->entries[table->current_config].config_bits;

    config->L2_rolling_window   = (bits & SSM_CFG_L2) ? 1 : 0;
    config->L3_linear_decay     = (bits & SSM_CFG_L3) ? 1 : 0;
    config->L5_window_inference = (bits & SSM_CFG_L5) ? 1 : 0;
    config->L6_decay_inference  = (bits & SSM_CFG_L6) ? 1 : 0;
}