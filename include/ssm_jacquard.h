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

#ifndef SSM_JACQUARD_H
#define SSM_JACQUARD_H

#include <stdint.h>

/* ============================================================================
 * SSM L8: Jacquard Mode Selector (Data-Driven Architecture)
 * ============================================================================
 *
 * Based on 2^7 DoE with 300 reps (128 configurations, 38,400 total runs).
 * Top 5% analysis reveals optimal loop combinations for speed + stability.
 *
 * Architecture (Experimentally Validated):
 * - L1 (heat_tracking): DISABLED (harmful in 86% of top configs)
 * - L4 (pipelining_metrics): DISABLED (harmful in 100% of top configs)
 * - L7 (adaptive_heartrate): ALWAYS ON (beneficial in 71% of top configs)
 * - L2, L3, L5, L6: Runtime-controlled by L8 (workload-dependent)
 * - L8 (Jacquard): 4-bit selector (16 modes) controlling L2/L3/L5/L6
 *
 * L8 operates as a multi-dimensional classifier:
 *   L2 (window):      ON if entropy > 0.75 (diversity tracking)
 *   L3 (decay):       ON if temporal_decay > 0.5 (temporal locality)
 *   L5 (window_inf):  ON if cv > 0.15 (variance adaptation)
 *   L6 (decay_inf):   ON if cv > 0.15 AND temporal_decay > 0.3
 *
 * Top 5% Validated Modes:
 *   C4  (0100): L2=0, L3=1, L5=0, L6=0 - Temporal locality
 *   C7  (0111): L2=0, L3=1, L5=1, L6=1 - Full inference
 *   C9  (1001): L2=1, L3=0, L5=0, L6=1 - Diverse + decay_inf
 *   C11 (1011): L2=1, L3=0, L5=1, L6=1 - Diverse + inference
 *   C12 (1100): L2=1, L3=1, L5=0, L6=0 - Diverse + temporal
 */

/* ============================================================================
 * L8 Mode Definitions (4-bit: L2/L3/L5/L6)
 * ============================================================================
 */

typedef enum {
    /* Bits: L2 L3 L5 L6 */
    SSM_MODE_C0  = 0x0,  /* 0000: Minimal (stable/predictable workloads) */
    SSM_MODE_C1  = 0x1,  /* 0001: L6 only (decay inference) */
    SSM_MODE_C2  = 0x2,  /* 0010: L5 only (window inference) */
    SSM_MODE_C3  = 0x3,  /* 0011: L5+L6 (volatile workloads) */
    SSM_MODE_C4  = 0x4,  /* 0100: L3 only (temporal locality) ✅ TOP 5% */
    SSM_MODE_C5  = 0x5,  /* 0101: L3+L6 (temporal + decay_inf) */
    SSM_MODE_C6  = 0x6,  /* 0110: L3+L5 (temporal + window_inf) */
    SSM_MODE_C7  = 0x7,  /* 0111: L3+L5+L6 (full inference) ✅ TOP 5% */
    SSM_MODE_C8  = 0x8,  /* 1000: L2 only (high diversity) */
    SSM_MODE_C9  = 0x9,  /* 1001: L2+L6 (diverse + decay_inf) ✅ TOP 5% */
    SSM_MODE_C10 = 0xA,  /* 1010: L2+L5 (diverse + window_inf) */
    SSM_MODE_C11 = 0xB,  /* 1011: L2+L5+L6 (diverse + inference) ✅ TOP 5% */
    SSM_MODE_C12 = 0xC,  /* 1100: L2+L3 (diverse + temporal) ✅ TOP 5% */
    SSM_MODE_C13 = 0xD,  /* 1101: L2+L3+L6 (complex workload) */
    SSM_MODE_C14 = 0xE,  /* 1110: L2+L3+L5 (full adaptive, no decay_inf) */
    SSM_MODE_C15 = 0xF   /* 1111: L2+L3+L5+L6 (full adaptive, all on) */
} ssm_l8_mode_t;

/* ============================================================================
 * L8 Metrics (Input to Mode Selection)
 * ============================================================================
 */

typedef struct {
    double entropy;                   /* Rolling-window entropy/diversity (0.0-1.0) */
    double cv;                        /* Coefficient of variation (short-term volatility) */
    double temporal_decay;            /* Temporal locality strength (0.0-1.0) */
    double stability_score;           /* Combined stability metric for hysteresis */
    int    inference_ran_this_tick;   /* 1 if inference engine ran this tick */
    int    inference_early_exited;    /* 1 if inference early-exited (ANOVA stable) */
} ssm_l8_metrics_t;

/* ============================================================================
 * SSM Configuration (L2/L3/L5/L6 Mode Bits)
 * ============================================================================
 */

typedef struct {
    int L2_rolling_window;            /* 1 = window tracking active, 0 = off */
    int L3_linear_decay;              /* 1 = decay active, 0 = off */
    int L5_window_inference;          /* 1 = window inference active, 0 = off */
    int L6_decay_inference;           /* 1 = decay inference active, 0 = off */
} ssm_config_t;

/* ============================================================================
 * L8 Configuration Thresholds (Data-Driven from DoE)
 * ============================================================================
 */

#ifndef SSM_ENTROPY_HIGH_THRESHOLD
#define SSM_ENTROPY_HIGH_THRESHOLD 0.75      /* Entropy > 0.75 → enable L2 (diversity) */
#endif

#ifndef SSM_CV_HIGH_THRESHOLD
#define SSM_CV_HIGH_THRESHOLD 0.15           /* CV > 0.15 → enable L5/L6 (inference) */
#endif

#ifndef SSM_TEMPORAL_DECAY_THRESHOLD
#define SSM_TEMPORAL_DECAY_THRESHOLD 0.5     /* Temporal > 0.5 → enable L3 (decay) */
#endif

#ifndef SSM_TEMPORAL_DECAY_LOW_THRESHOLD
#define SSM_TEMPORAL_DECAY_LOW_THRESHOLD 0.3 /* Temporal > 0.3 + CV high → enable L6 */
#endif

#ifndef SSM_HYSTERESIS_TICKS
#define SSM_HYSTERESIS_TICKS 5               /* Consecutive ticks before mode change */
#endif

/* ============================================================================
 * Adaptive Table Constants (L8 Heat-Ranked 128-Config Selector)
 * Trial duration is a relative-time promise in ticks, not wall-clock.
 * Half-life of benefit score ≈ 69 trials. Calibrate against observed
 * regime dwell time measured in trials, not seconds.
 * ============================================================================
 */

#ifndef SSM_MIN_TRIAL_TICKS
#define SSM_MIN_TRIAL_TICKS       2000u   /* 2 × HEARTBEAT_INFERENCE_FREQUENCY */
#endif

#ifndef SSM_SCORE_MAX
#define SSM_SCORE_MAX             65535u
#endif

#ifndef SSM_SCORE_MIN
#define SSM_SCORE_MIN             0u
#endif

/* DECAY_FACTOR 0.99 in Q16: floor(0.99 * 65536) = 64881 */
#ifndef SSM_SCORE_DECAY_FACTOR_Q16
#define SSM_SCORE_DECAY_FACTOR_Q16  64881u
#endif

/* UCB exploration coefficient: 10% of SCORE_MAX */
#ifndef SSM_UCB_K
#define SSM_UCB_K                 6554u
#endif

/* Reward gain: 25% of SCORE_MAX per trial */
#ifndef SSM_REWARD_GAIN
#define SSM_REWARD_GAIN           16384u
#endif

/* Combined-stability weights in Q16 (Option A fixed split, see design doc §5.4) */
/* w_joint = 0.75 = 49152/65536, w_anova = 0.25 = 16384/65536 */
#ifndef SSM_JOINT_WEIGHT_Q16
#define SSM_JOINT_WEIGHT_Q16      49152u
#endif

#ifndef SSM_ANOVA_WEIGHT_Q16
#define SSM_ANOVA_WEIGHT_Q16      16384u
#endif

/* ============================================================================
 * 7-bit Config Bit Positions (b6=L1, b5=L2, b4=L3, b3=L4, b2=L5, b1=L6, b0=L7)
 * ============================================================================
 */

#define SSM_CFG_L1  0x40u
#define SSM_CFG_L2  0x20u
#define SSM_CFG_L3  0x10u
#define SSM_CFG_L4  0x08u
#define SSM_CFG_L5  0x04u
#define SSM_CFG_L6  0x02u
#define SSM_CFG_L7  0x01u

/* ============================================================================
 * Adaptive Table Data Structures
 * ============================================================================
 */

/* One entry: per-config metadata used by the reward signal */
typedef struct {
    uint8_t  config_bits;    /* 7-bit DoE config (b6=L1..b0=L7) */
    uint8_t  _pad[3];
    uint32_t benefit_score;  /* Heat analog in [SSM_SCORE_MIN, SSM_SCORE_MAX] */
    uint32_t trial_count;    /* Total completed trials for this config */
    uint32_t prev_window;    /* effective_window_size at end of last trial (0 = none) */
    uint64_t prev_slope;     /* decay_slope_q48 at end of last trial (0 = none) */
} SsmConfigEntry;            /* 24 bytes */

/* Full stratified table: 8 regime bins × 128 configs */
typedef struct {
    SsmConfigEntry entries[128];           /* Per-config metadata      (3,072 bytes) */
    uint32_t       regime_scores[8][128];  /* Benefit score per (r,c)  (4,096 bytes) */
    uint32_t       regime_trials[8][128];  /* Trial count per (r,c)    (4,096 bytes) */
    uint8_t        current_config;         /* Active config index 0-127 */
    uint8_t        current_regime;         /* Regime bin at trial start */
    uint8_t        _pad[2];
    uint32_t       trial_tick_count;       /* Ticks in current trial (relative time) */
    uint32_t       inference_runs_this_trial;  /* Inference runs in current trial */
    uint32_t       anova_exits_this_trial;     /* Phase 2A early-exits this trial */
    uint32_t       total_regime_trials[8]; /* Total trials per regime (UCB ln term) */
} SsmConfigTable;                          /* ~11.3 KB, L1-cache resident */

/* ============================================================================
 * L8 State (extended with adaptive table pointer)
 * ============================================================================
 */

typedef struct {
    ssm_l8_mode_t   current_mode;       /* Mirrored from table for diagnostics */
    uint32_t        hysteresis_counter; /* Legacy threshold mode hysteresis */
    ssm_l8_mode_t   pending_mode;       /* Legacy threshold mode pending */
    SsmConfigTable *table;              /* NULL = legacy threshold; non-NULL = adaptive */
} ssm_l8_state_t;

/* ============================================================================
 * L8 API
 * ============================================================================
 */

/**
 * @brief Initialize L8 state (legacy fields only; call ssm_l8_init_table separately)
 */
void ssm_l8_init(ssm_l8_state_t *state, ssm_l8_mode_t initial_mode);

/**
 * @brief Allocate and seed the adaptive config table
 * Sets state->table; must be called after ssm_l8_init().
 * On allocation failure, state->table remains NULL (legacy mode is used).
 */
void ssm_l8_init_table(ssm_l8_state_t *state);

/**
 * @brief Free the adaptive config table
 */
void ssm_l8_free_table(ssm_l8_state_t *state);

/**
 * @brief Per-tick update for the adaptive table path
 *
 * Accumulates tick count and ANOVA exit stats. Runs trial-end reward
 * computation + UCB selection every SSM_MIN_TRIAL_TICKS ticks and
 * applies the selected config to ssm_config_t.
 *
 * @param state      L8 state (must have state->table != NULL)
 * @param metrics    Current runtime metrics including inference status
 * @param config     SSM config to update when a trial ends
 * @param current_window  VM's effective_window_size this tick
 * @param current_slope   VM's decay_slope_q48 this tick
 */
void ssm_l8_update_table(ssm_l8_state_t *state, const ssm_l8_metrics_t *metrics,
                         ssm_config_t *config,
                         uint32_t current_window, uint64_t current_slope);

/**
 * @brief Apply the current table config to ssm_config_t
 * Used on first tick to set the initial DoE-seeded config immediately.
 */
void ssm_apply_mode_from_table(const ssm_l8_state_t *state, ssm_config_t *config);

/**
 * @brief Update L8 state based on current metrics (legacy threshold path)
 */
void ssm_l8_update(const ssm_l8_metrics_t *metrics, ssm_l8_state_t *state);

/**
 * @brief Apply L8 mode to SSM configuration (legacy threshold path)
 */
void ssm_apply_mode(const ssm_l8_state_t *state, ssm_config_t *config);

/**
 * @brief Get human-readable mode name
 */
const char* ssm_l8_mode_name(ssm_l8_mode_t mode);

#endif /* SSM_JACQUARD_H */