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
 * L8 State
 * ============================================================================
 */

typedef struct {
    ssm_l8_mode_t current_mode;       /* Current operating mode (C0-C3) */
    uint32_t hysteresis_counter;      /* Counts consecutive ticks before mode change */
    ssm_l8_mode_t pending_mode;       /* Mode waiting for hysteresis confirmation */
} ssm_l8_state_t;

/* ============================================================================
 * L8 Metrics (Input to Mode Selection)
 * ============================================================================
 */

typedef struct {
    double entropy;                   /* Rolling-window entropy/diversity (0.0-1.0) */
    double cv;                        /* Coefficient of variation (short-term volatility) */
    double temporal_decay;            /* Temporal locality strength (0.0-1.0) */
    double stability_score;           /* Combined stability metric for hysteresis */
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
 * L8 API
 * ============================================================================
 */

/**
 * @brief Initialize L8 state
 *
 * @param state L8 state to initialize
 * @param initial_mode Starting mode (default: C0_CRUISE)
 */
void ssm_l8_init(ssm_l8_state_t *state, ssm_l8_mode_t initial_mode);

/**
 * @brief Update L8 state based on current metrics
 *
 * Uses rule-based classification to select mode based on entropy and CV.
 * Includes hysteresis to prevent rapid mode flapping.
 *
 * @param metrics Current runtime metrics (ns_ewma, cv, entropy)
 * @param state L8 state (updated in-place)
 */
void ssm_l8_update(const ssm_l8_metrics_t *metrics, ssm_l8_state_t *state);

/**
 * @brief Apply L8 mode to SSM configuration (set L3/L4 bits)
 *
 * @param state L8 state containing current mode
 * @param config SSM configuration to update (L3/L4 bits)
 */
void ssm_apply_mode(const ssm_l8_state_t *state, ssm_config_t *config);

/**
 * @brief Get human-readable mode name
 *
 * @param mode Mode to describe
 * @return Static string describing mode (e.g., "C0_CRUISE")
 */
const char* ssm_l8_mode_name(ssm_l8_mode_t mode);

#endif /* SSM_JACQUARD_H */