# L8 Jacquard: Adaptive 128-Config Heat-Ranked Selector

**Status:** Design proposal — awaiting review and approval before implementation  
**Author:** Claude Code session, June 2026  
**Affects:** `include/ssm_jacquard.h`, `src/ssm_jacquard.c`, `src/inference_engine.c`, `include/vm.h`  

---

## 1. Executive Summary

The current L8 Jacquard selector is a **static 4-bit threshold classifier**: three metric
thresholds produce a 4-bit pattern that maps to one of 16 fixed modes controlling L2/L3/L5/L6.
It has no memory, no learning, and no ability to discover that the DoE-derived thresholds may
not be optimal for a specific deployed workload.

This document proposes replacing it with a **heat-ranked 8×128 adaptive config table** —
the same structure used by the hotword cache applied to configuration selection. The 128
entries cover all 2^7 DoE configurations (L1–L7). Benefit scores are updated via negative
feedback from the coupled (window, slope) convergence signal that the inference engine already
computes. DoE priors seed the initial scores so there is no cold-start penalty.

**Estimated additional overhead:** ~3–4 ns per heartbeat tick (0.03–0.04% of the 10 µs tick
period). Memory: ~10 KB, fully L1-cached.

**Verdict:** Worthwhile, conditional on the reward signal design described in Section 5.
See Section 9 for the full benefit/cost analysis.

---

## 2. What Exists Today

### 2.1 The Seven Loops

| Loop | Name | Current State | Reason |
|------|------|--------------|--------|
| L1 | Execution Heat Tracking | Permanently OFF | Harmful in 86% of top DoE configs |
| L2 | Rolling Window History | L8-controlled | ON if entropy > 0.75 |
| L3 | Linear Decay | L8-controlled | ON if temporal_decay > 0.5 |
| L4 | Pipelining Metrics | Permanently OFF | Harmful in 100% of top DoE configs |
| L5 | Window Width Inference | L8-controlled | ON if CV > 0.15 |
| L6 | Decay Slope Inference | L8-controlled | ON if CV > 0.15 AND temporal_decay > 0.3 |
| L7 | Adaptive Heartrate | Permanently ON | Beneficial in 71% of top DoE configs |
| L8 | Jacquard Policy Engine | — | Drives L2/L3/L5/L6 |

### 2.2 Current L8 Logic (`ssm_l8_update`, 156 lines total)

```c
int L2_bit = (entropy      >= SSM_ENTROPY_HIGH_THRESHOLD);   // 0.75
int L3_bit = (temporal     >= SSM_TEMPORAL_DECAY_THRESHOLD); // 0.5
int L5_bit = (cv           >= SSM_CV_HIGH_THRESHOLD);        // 0.15
int L6_bit = (cv >= 0.15 && temporal >= SSM_TEMPORAL_DECAY_LOW_THRESHOLD);

target_mode = (L2<<3) | (L3<<2) | (L5<<1) | L6;
// hysteresis: commit only after SSM_HYSTERESIS_TICKS=5 consecutive votes
```

Four thresholds hardcoded from the DoE. No state beyond the hysteresis counter and current mode.

### 2.3 Top-5% DoE Validated Modes

| Mode | Bits (L2 L3 L5 L6) | Description |
|------|-------------------|-------------|
| C4  | 0 1 0 0 | Temporal locality only |
| C7  | 0 1 1 1 | Decay + both inferences |
| C9  | 1 0 0 1 | Rolling window + decay-slope inference |
| C11 | 1 0 1 1 | Rolling window + both inferences |
| C12 | 1 1 0 0 | Rolling window + decay, no inference |

These modes will seed the table with high initial benefit scores (see Section 6).

---

## 3. Signal Chain and Coupled Feedback Topology

Understanding the signal chain is prerequisite to understanding the reward signal.
The three variables — decay slope, window width, and heat trajectory — form a coupled
feedback system, not three independent loops.

```
┌─────────────────────────────────────────────────────────────────┐
│                     COUPLED FEEDBACK SYSTEM                     │
│                                                                 │
│  decay_slope_q48                                                │
│      │                                                          │
│      ▼  (applied each heartbeat tick)                           │
│  DictEntry.execution_heat  ◄── decays at slope rate             │
│      │                                                          │
│      ▼  (word IDs recorded in execution order)                  │
│  RollingWindowOfTruth  ◄── width = effective_window_size        │
│      │                                                          │
│      ▼  Phase 2B: linearize → heat trajectory[]                 │
│  Inference Engine                                               │
│      │                                                          │
│      ├─► Phase 2A: variance stable? ──► early-exit (most ticks) │
│      │                                                          │
│      ├─► Phase 2C (L5): Levene's test                           │
│      │       └──► new effective_window_size ──────────────┐     │
│      │                                                    │     │
│      └─► Phase 2D (L6): log regression                   │     │
│              └──► new decay_slope_q48 ────────────────────┼──┐  │
│                                                           │  │  │
│  ┌────────────────────────────────────────────────────────┘  │  │
│  │  window feeds back into how much trajectory                │  │
│  │  Phase 2B extracts next time                              │  │
│  │                                                            │  │
│  └──► effective_window_size                                   │  │
│                                                               │  │
│  slope feeds back into heat distribution                      │  │
│  rolling window captures                                      │  │
│  └──── decay_slope_q48 ◄──────────────────────────────────────┘  │
│                                                                 │
│  Parallel: adaptive pass adjusts window based on diversity      │
│  (growth_rate_q48 vs threshold → shrink or grow)                │
└─────────────────────────────────────────────────────────────────┘
```

### 3.1 Why the Coupling Matters

- A steeper decay slope → words cool faster → rolling window sees a more homogeneous
  heat distribution → trajectory is less noisy → Levene's inflection shifts left
  (narrower window sufficient) → Phase 2C infers a narrower window.

- A narrower window → Phase 2B extracts a shorter trajectory → log regression in
  Phase 2D has fewer samples → slope estimate is noisier → less stable decay_slope_q48.

- They iterate toward a **joint attractor**: the window is exactly wide enough to give
  the slope estimator reliable data, and the slope is exactly steep enough that the window
  captures a stationary heat distribution. Neither is pushing the other any more.

This coupling is what makes the reward signal design non-trivial. See Section 5.

---

## 4. Proposed Architecture

### 4.1 Core Concept

Replace the 4-bit threshold classifier with a stratified heat-ranked table:

```
regime_scores[8][128]   — benefit score per (regime, config)
regime_trials[8][128]   — trial count per (regime, config)
```

The **8 regime bins** are the same 3-bit metric partition the current L8 already computes:

```c
regime = (entropy_high << 2) | (cv_high << 1) | temporal_high
```

Eight bins, one per combination of the three binary threshold tests. This is the same
partition as current L8 — we are not introducing new thresholds.

The **128 config entries** cover the full 2^7 DoE space: all combinations of L1–L7 as
independent binary variables. L1 and L4 are included with very low initial scores —
not disabled by fiat, but deprioritized by evidence.

### 4.2 Data Structures

```c
/* One entry in the config table */
typedef struct {
    uint8_t  config_bits;       /* 7-bit mask: b6=L1, b5=L2, b4=L3, b3=L4,
                                                b2=L5, b1=L6, b0=L7 */
    uint32_t benefit_score;     /* Heat analog — rises when stable, falls when not */
    uint32_t trial_count;       /* Total completed trials under this config */
    uint64_t last_active_ns;    /* Monotonic timestamp — supports score decay */
    uint32_t prev_window;       /* effective_window_size at end of last trial */
    uint64_t prev_slope;        /* decay_slope_q48 at end of last trial */
} SsmConfigEntry;               /* ~32 bytes per entry */

/* Full stratified table */
typedef struct {
    SsmConfigEntry entries[128];        /* Config metadata, static */
    uint32_t       regime_scores[8][128]; /* Benefit score per regime per config */
    uint32_t       regime_trials[8][128]; /* Trial count per regime per config */
    uint8_t        current_config;       /* Index of active config (0–127) */
    uint8_t        current_regime;       /* Regime bin at start of current trial */
    uint32_t       trial_tick_count;     /* Ticks elapsed in current trial */
    uint32_t       anova_exits_this_trial; /* Phase 2A early-exits accumulated */
    uint32_t       total_regime_trials[8]; /* Total trials per regime (for UCB ln term) */
} SsmConfigTable;
```

**Memory footprint:**

| Structure | Size |
|---|---|
| `SsmConfigEntry[128]` | 128 × 32 = 4,096 bytes |
| `regime_scores[8][128]` (uint32_t) | 4,096 bytes |
| `regime_trials[8][128]` (uint32_t) | 4,096 bytes |
| Trial state + totals | ~64 bytes |
| **Total** | **~12 KB** |

The entire table fits in L1 cache (32–64 KB typical). The access pattern is regime-local:
each trial-end touches 128 contiguous uint32_t values for the current regime, then stops.
This is the same cache-friendly pattern the hotword cache exploits.

### 4.3 `ssm_l8_state_t` Changes

Current `ssm_l8_state_t` has: `current_mode`, `hysteresis_counter`, `pending_mode`.

New `ssm_l8_state_t` adds a pointer to the config table (keeping the existing fields
for backward compatibility during transition, removing them once validated):

```c
typedef struct {
    ssm_l8_mode_t   current_mode;       /* Keep for compatibility */
    uint32_t        hysteresis_counter; /* Keep for compatibility */
    ssm_l8_mode_t   pending_mode;       /* Keep for compatibility */
    SsmConfigTable *table;              /* NULL = use legacy threshold mode */
} ssm_l8_state_t;
```

Gated behind `table != NULL` so the old path remains functional during development
and testing.

---

## 5. Reward Signal Design

### 5.1 The Circularity Problem

A naive reward signal based on correction magnitude within a trial will produce wrong results:

- A config with **L5+L6 enabled** always shows non-zero `Δwindow` and `Δslope` — because
  the inference engine is running and making corrections. Large corrections on the first trial
  after a config switch may reflect genuine convergence toward the attractor, not config failure.

- A config with **L5+L6 disabled** always shows zero `Δwindow` and `Δslope` — because
  nothing is running to update them. This would be incorrectly rewarded as "stable."

**The fix:** measure **joint convergence across consecutive trials**, not correction magnitude
within a trial. The system is converging when the corrections are getting *smaller* from one
trial to the next — not when they are zero in one trial.

### 5.2 Joint Error Signal

At the end of each trial, record the current (window, slope) pair as `(prev_window, prev_slope)`
in the `SsmConfigEntry`. At the end of the *next* trial under the same config and same regime,
compute joint error:

```
Δw = |window_trial_N+1 - window_trial_N| / window_trial_N
Δs = |slope_trial_N+1  - slope_trial_N|  / slope_trial_N

joint_error = sqrt(Δw² + Δs²) / sqrt(2)     ← normalized RMS in [0, 1]

stability   = 1.0 - clamp(joint_error, 0.0, 1.0)
```

When the window and slope have stopped moving relative to each other, `joint_error → 0`
and `stability → 1.0`. When they are still adjusting, `joint_error > 0` and
`stability < 1.0`.

### 5.3 ANOVA Rate as Secondary Signal

The ANOVA early-exit rate (Phase 2A) is a fast, cheap proxy for system stability:
it fires when variance hasn't changed enough to justify full inference. It is a necessary
but not sufficient condition for joint attractor convergence:

```
anova_stability = anova_exits_this_trial / trial_ticks     ∈ [0, 1]
```

### 5.4 Combined Stability Score

```
combined_stability = w_joint × stability + w_anova × anova_stability
```

Suggested weights: `w_joint = 0.75`, `w_anova = 0.25`.

The joint signal dominates because it directly measures coupled system behavior.
The ANOVA rate fills in information on ticks where inference didn't run.

### 5.5 Negative Feedback Reward Update

```
benefit_delta = GAIN × (combined_stability - 0.5) × 2.0

new_score = old_score × DECAY_FACTOR + benefit_delta
new_score = clamp(new_score, SCORE_MIN, SCORE_MAX)
```

- `combined_stability > 0.5`: the config is driving the system toward its attractor → positive
  delta → benefit_score rises.
- `combined_stability < 0.5`: the config is preventing convergence → negative delta →
  benefit_score falls.
- `combined_stability = 0.5`: neutral → no update.

`DECAY_FACTOR < 1.0` (suggest 0.99 per trial end) ensures scores decay over time,
preventing a config from holding the top position indefinitely based on past performance.

### 5.6 Minimum Trial Duration

Because the reward signal requires two consecutive trials under the same config to measure
`joint_error`, and because each trial must contain at least one full inference run to update
(window, slope), the minimum trial duration is:

```
min_trial_ticks = 2 × HEARTBEAT_INFERENCE_FREQUENCY = 2 × 1000 = 2,000 ticks = 20 ms
```

The first trial establishes `(prev_window, prev_slope)`. The second trial measures how much
they moved. The reward update fires at the end of the second trial. Subsequent trials under
the same config continue to update the reward.

A trial period of 2,000–5,000 ticks (20–50 ms) is suggested. Longer trials give more stable
reward estimates; shorter trials allow faster adaptation to workload regime changes. This is
a tunable parameter.

### 5.7 Special Case: L5 and L6 Both Disabled

When both L5 and L6 are disabled in the current config, `Δwindow` and `Δslope` from the
inference engine are always zero (the values are frozen). The joint error signal is
meaningless — both numerator and denominator are zero.

In this case, fall back to ANOVA rate alone:

```c
if (!L5_enabled && !L6_enabled) {
    combined_stability = anova_stability;
}
```

This correctly evaluates "stability without active inference" — does the system at least
not need correcting when left alone?

---

## 6. DoE Prior Seeding

The 38,400-run DoE provides empirical priors. These seed `regime_scores` at initialization
so the selector immediately gravitates toward known-good configs on first deployment.

### 6.1 Seeding Strategy

| Config category | Initial score | Reasoning |
|---|---|---|
| Top-5% configs (C4, C7, C9, C11, C12) in their 4-bit form | `SCORE_MAX × 0.80` | DoE-validated |
| Other configs with L1=0, L4=0 | `SCORE_MAX × 0.40` | Not validated but not harmful |
| Configs with L1=1 OR L4=1 | `SCORE_MAX × 0.10` | DoE found harmful on average |
| Configs with L1=1 AND L4=1 | `SCORE_MAX × 0.05` | Doubly harmful |

Note: the top-5% configs from the DoE are defined in 4-bit (L2/L3/L5/L6) space. For the
7-bit table, each top-5% 4-bit config maps to multiple 7-bit entries (L1=0, L4=0, L7=1 held
fixed). All such mappings receive the same high initial score.

### 6.2 Regime-Specific Seeding

The DoE ran across all regimes combined. If regime-specific DoE data is available in the
future, `regime_scores[r]` can be seeded independently per regime. For now, all 8 regimes
receive the same initial scores — the table will differentiate them over time through runtime
learning.

---

## 7. Config Selection: UCB Algorithm

### 7.1 Selection Score

At each trial end, the selector computes a UCB (Upper Confidence Bound) score for all 128
configs in the current regime and picks the highest:

```
ucb_score[r][c] = regime_scores[r][c]
                + UCB_K × sqrt(ln(total_regime_trials[r]) / regime_trials[r][c])
```

- `regime_scores[r][c]`: exploitation term — how well config C has performed in regime R
- `UCB_K × sqrt(...)`: exploration term — bonus for under-tried configs; shrinks as
  `regime_trials[r][c]` grows toward `total_regime_trials[r]`

**UCB_K** controls exploration aggressiveness. Suggest starting at `UCB_K = 0.1 × SCORE_MAX`.
Too large: wastes trials on known-bad configs. Too small: never explores.

### 7.2 Implementation Cost

- `ln(total_regime_trials[r])`: computed **once** per trial end, reused for all 128 entries
- Per config: 1 divide + 1 integer sqrt (~10 cycles) + 1 multiply + 1 add
- 128 configs × ~4 ops × 2 ns = **~1–2 µs per trial end**
- Amortized over 2,000-tick trial: **~0.5–1 ns per tick**

Integer sqrt via Newton's method (4–5 iterations, convergence guaranteed for uint32_t):
no floating point, no libm dependency. Compatible with freestanding kernel builds.

### 7.3 Bootstrap: First Two Trials

The first trial under any (regime, config) pair has `regime_trials[r][c] = 0`,
making the UCB exploration term undefined (`ln/0`). Bootstrap rule:

```
if regime_trials[r][c] == 0:
    ucb_score[r][c] = SCORE_MAX  // treat unexplored as maximally interesting
```

This ensures every config gets at least one trial in each regime before the UCB formula
takes over — equivalent to the "try everything once" initialization of UCB1.

---

## 8. Re-Ranking and Selection Path

Mirrors `dict_adaptive_optimization_pass` / `dict_reorganize_buckets_by_heat` exactly.

### 8.1 Per-Trial-End Sequence

```
1. Compute regime bin for current metrics (same 3 comparisons as today)
2. Compute joint_error and anova_stability for completed trial
3. Update regime_scores[current_regime][current_config] with benefit_delta
4. Increment regime_trials[current_regime][current_config]
5. Apply score decay: regime_scores[r][c] *= DECAY_FACTOR
6. Compute ucb_score for all 128 configs in current_regime
7. Select next_config = argmax(ucb_score)
8. Apply next_config's 7-bit L1–L7 mask to ssm_config
9. Reset trial state: trial_tick_count=0, anova_exits_this_trial=0
10. Record current (window, slope) into entries[current_config].prev_*
```

### 8.2 Per-Tick Sequence (replaces current ssm_l8_update)

```
1. Increment trial_tick_count
2. If inference ran this tick: increment anova_exits_this_trial (if early-exited)
3. If trial_tick_count >= min_trial_ticks: run per-trial-end sequence (step 1-10 above)
```

---

## 9. Overhead Estimates

All figures assume 3 GHz CPU, 10 µs heartbeat tick, 1,000-tick inference frequency,
2,000-tick minimum trial period.

### 9.1 Per-Tick Additions

| Operation | Cost | Notes |
|---|---|---|
| Regime bin computation | 0 ns | Identical to today's 3 comparisons |
| `trial_tick_count++` | ~0.3 ns | 1 integer add |
| ANOVA exit accumulation | ~0.3 ns | 1 conditional add |
| Trial-end check | ~0.3 ns | 1 compare |
| **Per-tick total** | **~1 ns** | 0.01% of 10 µs tick |

### 9.2 Per-Trial-End Additions (every 2,000 ticks = 20 ms)

| Operation | Cost | Amortized/tick |
|---|---|---|
| Joint error computation (Δw, Δs, RMS) | ~50 ns | 0.025 ns |
| ANOVA rate computation | ~10 ns | 0.005 ns |
| Combined stability + benefit_delta | ~30 ns | 0.015 ns |
| Score update + decay (1 array write) | ~15 ns | 0.0075 ns |
| UCB over 128 entries (int sqrt ×128) | ~1,500 ns | 0.75 ns |
| Argmax selection | ~50 ns | 0.025 ns |
| **Per-trial-end total** | **~1,655 ns** | **~0.83 ns/tick** |

### 9.3 Memory Access

| | Size | Cache behavior |
|---|---|---|
| `regime_scores[r][*]` (128 uint32_t) | 512 bytes per regime | ~8 cache lines, stays hot |
| `SsmConfigEntry[128]` | 4,096 bytes | Fits in L1; accessed sequentially |
| Full table | ~12 KB | Fits in L1 (32–64 KB typical) |

### 9.4 Summary Against Existing Costs

| Component | ns/tick amortized | % of 10 µs tick |
|---|---|---|
| Existing `ssm_l8_update` (today) | ~4 ns | 0.04% |
| Existing inference engine (20–50 µs every 1,000 ticks) | 20–50 ns | 0.20–0.50% |
| **New per-tick accumulation** | **~1 ns** | **0.01%** |
| **New per-trial-end (amortized)** | **~0.83 ns** | **0.008%** |
| **New L8 total addition** | **~2 ns** | **0.02%** |
| **New L8 total (replacing old L8)** | **~2 ns** | **0.02%** |

The new L8 is marginally *cheaper* than the current one in per-tick terms
(today's `ssm_l8_update` runs every tick; the new one accumulates cheaply per tick and
runs the heavy logic only at trial end, amortized over 2,000 ticks).

The existing Levene's test `malloc` inside Phase 2C costs more per call than the entire
new L8 reward + UCB selection combined.

---

## 10. Files Changed

| File | Change | Lines added (est.) |
|---|---|---|
| `include/ssm_jacquard.h` | Add `SsmConfigEntry`, `SsmConfigTable`; extend `ssm_l8_state_t` | +80 |
| `src/ssm_jacquard.c` | Add `ssm_l8_init_table()`, `ssm_l8_seed_from_doe()`, `ssm_l8_update_trial()`, `ssm_l8_select_next_config()`, `ssm_rerank_ucb()`; keep old `ssm_l8_update` as legacy path | +250 |
| `src/inference_engine.c` | Add `window_delta_q48` and `slope_delta_q48` to `InferenceOutputs`; populate them (already computed internally, just not exposed) | +10 |
| `include/vm.h` (via `inference_engine.h`) | Two new fields on `InferenceOutputs` | +5 |
| `src/ssm_jacquard.c` | DoE prior seeding table (static const array of 128 seed scores) | +40 |

**No changes to:** `rolling_window_of_truth.c`, `heartbeat_export.c`, `physics_runtime.c`,
`framebuffer.c`, `vt100.c`, any kernel or boot code.

Total estimated addition: **~385 lines**, isolated entirely within the L8 and inference
engine files.

---

## 11. What Doesn't Change

- The five inference engine phases (2A–2D) are architecturally unchanged. Two small
  outputs are added (`window_delta_q48`, `slope_delta_q48`) — values already computed
  internally that are simply not currently surfaced.
- The `ssm_config_t` L1–L7 bit flags are unchanged. Everything downstream reads them
  the same way.
- The rolling window, heartbeat, heat tracking, hotword cache, dictionary optimization
  — all unchanged.
- The kernel build (freestanding) is unaffected. `SsmConfigTable` uses only `stdint.h`
  types, no libc allocation (table is statically sized), no floating point
  (all arithmetic in Q48.16 or integer).

---

## 12. Open Questions for Captain Bob

These require decisions before coding begins:

**Q1: Include L1 and L4 in the 7-bit space?**  
Current plan: yes, seeded at 5–10% of `SCORE_MAX`. The DoE found them harmful on average
but the table can discover exceptions if they exist. Risk: UCB exploration may occasionally
select them and hurt performance during that trial. Mitigation: low seed + score decay means
they stay near the bottom. Alternative: exclude them (use 5-bit space, 32 configs) and keep
L7 hardwired to ON (as today).

**Q2: Tunable constants — bake in or expose via `starforth_config.h`?**  
Proposed new constants:
- `SSM_MIN_TRIAL_TICKS` (suggest 2000)
- `SSM_SCORE_MAX` (suggest 65535 to match Q48.16 range intuition)
- `SSM_SCORE_DECAY_FACTOR` (suggest 0.99 per trial end ≈ Q48.16 64881)
- `SSM_UCB_K` (suggest `SSM_SCORE_MAX / 10`)
- `SSM_REWARD_GAIN` (suggest `SSM_SCORE_MAX / 4`)
- `SSM_JOINT_WEIGHT` (suggest 0.75)
- `SSM_ANOVA_WEIGHT` (suggest 0.25)

All of these map naturally alongside the existing `ROLLING_WINDOW_SIZE`, `SSM_HYSTERESIS_TICKS`
etc. in `include/starforth_config.h`.

**Q3: Regime-specific seeding now or later?**  
Current plan: all 8 regimes seeded identically from DoE priors. Regime-specific DoE
analysis (if the raw run data is available broken out by metric regime) would improve
cold-start behavior. Can be deferred — the table learns regime-specific scores at runtime.

**Q4: Legacy threshold mode as compile-time flag or runtime gate?**  
Suggested: runtime gate (`table != NULL` in `ssm_l8_state_t`). Allows A/B testing the
old vs new L8 without recompilation. Alternative: compile-time `#ifdef SSM_L8_ADAPTIVE`
to keep the kernel build lean.

**Q5: How to validate correctness before running the full DoE?**  
Suggested validation sequence (hosted VM only):
1. `make -f Makefile.starkernel fbtest` — confirms no regression in framebuffer
2. `make test` — confirms all 936+ FORTH words still pass
3. Run `--doe` mode for 300 reps each with and without `SSM_L8_ADAPTIVE` defined;
   compare variance and mean execution time. The adaptive selector should produce equal
   or better variance at equal or lower mean time after a warm-up period of ~1000 trials.

---

## 13. Benefit / Cost Assessment

### 13.1 Concrete Expected Benefits

**1. Regime-specific optimality.**  
The DoE established which configs win *on average*. This table learns which config wins
*for this specific workload in this specific regime*. For FORTH programs with unusual heat
distributions (e.g., heavily recursive, deeply compositional, or with very stable vs very
bursty execution patterns), the optimal config per regime may differ from the DoE average.
The table discovers this without re-running the DoE.

**2. No manual tuning for new workloads.**  
Current L8: if the DoE thresholds are miscalibrated for a specific FORTH application, the
classifier is stuck in the wrong mode permanently. The adaptive table self-corrects within
a few hundred trial periods (a few seconds of runtime at 20ms trials).

**3. Joint convergence signal closes the feedback loop.**  
The inference engine currently produces (window, slope) outputs with no feedback about
whether those outputs are causing the system to converge. The reward signal adds this
feedback, making the overall system a genuine closed-loop controller rather than
open-loop inference.

**4. Architecturally consistent.**  
The hotword cache, bucket reorganization, and adaptive window already use heat-ranked
structures. L8 becoming heat-ranked completes the pattern — everything in the SSM is
adaptive and data-driven, not hardcoded.

### 13.2 Honest Caveats

**1. The 0% algorithmic variance result.**  
The DoE showed the system converges to the same attractor states from any of the 128
starting configurations. If the attractor is already well-characterized and robust, the
adaptive table is solving a problem that doesn't significantly affect final-state behavior
for most workloads. The benefit is primarily in *how quickly* the system reaches its
attractor, not *where* the attractor is.

**2. L1 and L4 almost certainly stay at the bottom.**  
The DoE evidence against them is overwhelming (86% and 100% harmful respectively). The
exploration of L1 and L4 slots in the 128-entry space is methodologically correct but
practically likely to produce no new findings. The useful learning is within the 16-config
subspace of {L2, L3, L5, L6} combinations, stratified by regime — which the current L8
already approximates with fixed thresholds.

**3. Reward signal parameter sensitivity.**  
The weights (`w_joint`, `w_anova`), gain, decay factor, UCB_K, and trial duration interact.
The DoE that validated the current system was designed to avoid exactly this kind of
parameter sensitivity. A second DoE over the adaptive L8's parameters may be needed to
find the optimal operating point — or the parameters can be set conservatively and left
fixed, accepting slightly suboptimal but stable behavior.

**4. Complexity increase is real.**  
`ssm_jacquard.c` is currently 156 clean lines. The addition is ~385 lines of nontrivial
logic in the heartbeat thread. Bugs here affect the adaptation rate, not correctness
(worst case: the selector sticks on a suboptimal config, not on an incorrect config),
but the surface area grows meaningfully.

### 13.3 Verdict

**Worth doing.** The overhead is negligible by any measure. The architectural coherence
with the existing hotword cache is a genuine advantage. The joint convergence reward signal
is a principled design that closes a feedback loop the current system leaves open.

The primary condition: **get the reward signal right before writing any other code**.
Specifically, resolve Q1 (L1/L4 inclusion), confirm the `window_delta` / `slope_delta`
output additions to `InferenceOutputs`, and agree on the `min_trial_ticks` value. 
Everything else is mechanical once those are fixed.

The primary risk is not overhead or correctness — it is parameter sensitivity in the
reward signal. Suggest conservative initial values (low GAIN, high DECAY, moderate UCB_K)
and validate against the existing DoE baseline before changing any parameters.

---

*End of design document. Awaiting review and approval.*
