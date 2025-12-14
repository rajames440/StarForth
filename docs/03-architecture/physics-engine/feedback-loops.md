# StarForth Physics Feedback Loops

This document describes the 7 feedback loops in StarForth's physics-driven adaptive runtime, including their positive (accumulation) and negative (stabilization) feedback mechanisms.

## Overview

StarForth implements a physics-grounded self-adaptive runtime with 7 configurable feedback loops. Each loop can be toggled via Makefile flags (`ENABLE_LOOP_N_*`). The loops build cumulatively for Design of Experiments (DoE) measurement.

## Feedback Loop Reference

| Loop | Name | Positive Signal (Accumulation) | Negative Feedback (Stabilization) |
|------|------|-------------------------------|-----------------------------------|
| **#1** | **Execution Heat Tracking** | Word execution increments `execution_heat` counter | **Loop #3** applies time-based decay; `HEAT_CACHE_DEMOTION_THRESHOLD` demotes cold words from cache |
| **#2** | **Rolling Window History** | Circular buffer records every word execution (up to `ROLLING_WINDOW_SIZE`) | Oldest entries overwritten on wrap; **adaptive shrinking** reduces `effective_window_size` when pattern diversity growth < `ADAPTIVE_GROWTH_THRESHOLD` (1%) |
| **#3** | **Linear Decay** | Heat decay rate (`decay_slope_q48`) starts at 1/3 | Decay itself IS negative feedback—reduces heat over time at rate `DECAY_RATE_PER_US_Q16`; slope adjusts ±5% based on hot/stale word ratio trends |
| **#4** | **Pipelining Metrics** | Records word→word transitions in `transition_metrics`; builds context windows for prediction | Transition counts age out; **miss rate feedback**—if `prefetch_accuracy` drops, suggested window size changes via binary chop |
| **#5** | **Window Width Inference** | Expands `effective_window_size` when pattern diversity increases | **Levene's test** for variance stability (α=0.05, critical W≈6.5); if variance stable → shrink to minimum sufficient size; `ADAPTIVE_SHRINK_RATE=75` (shrinks to 75% per cycle) |
| **#6** | **Decay Slope Inference** | Extracts optimal decay rate via exponential regression on heat trajectory | **ANOVA early-exit**: if variance changed <5% since last check, skip inference (cached results); slope bounded by fit quality (`slope_fit_quality_q48`) |
| **#7** | **Adaptive Heartrate** | `tick_count` increments; monitors system responsiveness | If system stable (variance stabilized), inference runs less frequently (`HEARTBEAT_INFERENCE_FREQUENCY`); **inference cost vs benefit** tradeoff—high cost triggers less frequent ticks |

## Detailed Loop Descriptions

### Loop #1: Execution Heat Tracking

**Purpose:** Count word usage to identify hot paths.

**Positive feedback:**
- Every word execution increments `DictEntry.execution_heat`
- Hot words get promoted to `hotwords_cache` for O(1) lookup

**Negative feedback:**
- Loop #3 (Linear Decay) reduces heat over time
- Words falling below `HEAT_CACHE_DEMOTION_THRESHOLD=10` are demoted from cache
- `PHYSICS-RESET-STATS` word can reset all heat counters

**Makefile flag:** `ENABLE_LOOP_1_HEAT_TRACKING`

---

### Loop #2: Rolling Window History

**Purpose:** Capture execution sequence for deterministic metric seeding.

**Positive feedback:**
- Circular buffer (`execution_history`) records every word ID executed
- Window grows to `ROLLING_WINDOW_SIZE` (default 4096) before wrapping
- Becomes "warm" after 1024 executions (representative data)

**Negative feedback:**
- Oldest entries overwritten when buffer wraps
- Adaptive shrinking reduces `effective_window_size` when:
  - Pattern diversity growth rate < `ADAPTIVE_GROWTH_THRESHOLD` (1%)
  - Checked every `ADAPTIVE_CHECK_FREQUENCY` (256) executions
  - Shrinks by `ADAPTIVE_SHRINK_RATE` (75% retained per cycle)
  - Floor at `ADAPTIVE_MIN_WINDOW_SIZE` (256)

**Makefile flag:** `ENABLE_LOOP_2_ROLLING_WINDOW`

---

### Loop #3: Linear Decay

**Purpose:** Age words over time to prevent unbounded heat accumulation.

**Positive feedback:**
- Decay slope (`decay_slope_q48`) starts at 1/3 in Q48.16 fixed-point
- Slope can increase if too many words are stale

**Negative feedback (this loop IS primarily negative feedback):**
- Heat decays at rate `DECAY_RATE_PER_US_Q16` (1/65536 heat/μs)
- Half-life approximately 6-7 seconds for 100-heat word
- Slope adjusts ±5% based on hot/stale word ratio:
  - Too many hot words → increase decay slope
  - Too many stale words → decrease decay slope
- Minimum interval `DECAY_MIN_INTERVAL` (1μs) prevents over-decay

**Makefile flag:** `ENABLE_LOOP_3_LINEAR_DECAY`

---

### Loop #4: Pipelining Metrics

**Purpose:** Track word-to-word transitions for speculative prefetch.

**Positive feedback:**
- Records transitions in `WordTransitionMetrics` per word
- Builds context windows (`TRANSITION_WINDOW_SIZE`, default 2) for prediction
- `prefetch_attempts` and `prefetch_hits` track accuracy

**Negative feedback:**
- Transition counts age out over time
- Miss rate feedback: if `prefetch_accuracy` drops, binary chop suggests different window size
- `PipelineGlobalMetrics.suggested_next_size` guides tuning

**Makefile flag:** `ENABLE_LOOP_4_PIPELINING_METRICS`

---

### Loop #5: Window Width Inference

**Purpose:** Find optimal rolling window size via statistical testing.

**Positive feedback:**
- `effective_window_size` can grow when pattern diversity increases
- Growth rate > threshold → window expands toward `ROLLING_WINDOW_SIZE`

**Negative feedback:**
- **Levene's test** for equality of variance (statistically valid):
  - Divides trajectory into K disjoint chunks
  - Computes variance of each chunk independently
  - Test statistic W compared to critical value (~6.5 for α=0.05)
  - If W ≤ critical: variances stable → found minimum sufficient window
- Shrinks to smallest size where variance is statistically stable
- Prevents over-capture of redundant pattern data

**Makefile flag:** `ENABLE_LOOP_5_WINDOW_INFERENCE`

---

### Loop #6: Decay Slope Inference

**Purpose:** Extract optimal decay rate via exponential regression.

**Positive feedback:**
- Linear regression on log-transformed heat trajectory
- Model: `ln(heat[t]) = ln(h0) - slope*t` (exponential decay)
- Closed-form solution extracts `adaptive_decay_slope`

**Negative feedback:**
- **ANOVA early-exit**: if variance changed <5% since last inference, skip computation
  - `has_variance_stabilized()` compares current vs cached variance
  - Saves ~5-10k CPU cycles when stable
- Slope bounded by fit quality (`slope_fit_quality_q48`)
- Invalid slopes (0 or >100) rejected by `inference_outputs_validate()`

**Makefile flag:** `ENABLE_LOOP_6_DECAY_INFERENCE`

---

### Loop #7: Adaptive Heartrate

**Purpose:** Dynamic tick frequency adjustment for system responsiveness.

**Positive feedback:**
- `tick_count` increments every heartbeat
- Heartbeat thread wakes at `HEARTBEAT_TICK_NS` (1ms default)
- Monitors hot word count, total heat, window width

**Negative feedback:**
- If system is stable (variance stabilized), inference runs less frequently
- `HEARTBEAT_INFERENCE_FREQUENCY` (5000 ticks) controls full inference cadence
- Cost vs benefit tradeoff: expensive inference → less frequent ticks
- Background thread (`HeartbeatWorker`) can be disabled via `HEARTBEAT_THREAD_ENABLED=0`

**Makefile flag:** `ENABLE_LOOP_7_ADAPTIVE_HEARTRATE`

---

## Key Negative Feedback Mechanisms Summary

1. **Heat Decay (Loop #3)** — The primary stabilizer: prevents unbounded heat accumulation
2. **Window Shrinking (Loop #2 + #5)** — Pattern diversity plateau triggers size reduction via Levene's test
3. **ANOVA Early-Exit (Loop #6)** — Variance stability (<5% change) skips expensive inference
4. **Cache Demotion (Loop #1)** — Words below `HEAT_CACHE_DEMOTION_THRESHOLD=10` exit hot-words cache
5. **Slope Direction Reversal (Loop #3)** — If hot/stale ratio inverts, decay slope adjusts ±5%

## Configuration

### Enabling/Disabling Loops

```bash
# Disable a specific loop
make ENABLE_LOOP_3_LINEAR_DECAY=0

# Baseline build (all loops off)
make ENABLE_LOOP_1_HEAT_TRACKING=0 \
     ENABLE_LOOP_2_ROLLING_WINDOW=0 \
     ENABLE_LOOP_3_LINEAR_DECAY=0 \
     ENABLE_LOOP_4_PIPELINING_METRICS=0 \
     ENABLE_LOOP_5_WINDOW_INFERENCE=0 \
     ENABLE_LOOP_6_DECAY_INFERENCE=0 \
     ENABLE_LOOP_7_ADAPTIVE_HEARTRATE=0
```

### Tuning Knobs

| Knob | Default | Description |
|------|---------|-------------|
| `ROLLING_WINDOW_SIZE` | 4096 | Initial/maximum window size |
| `ADAPTIVE_SHRINK_RATE` | 75 | % retained when shrinking (75 = discard 25%) |
| `ADAPTIVE_MIN_WINDOW_SIZE` | 256 | Floor for window shrinking |
| `ADAPTIVE_CHECK_FREQUENCY` | 256 | Executions between diversity checks |
| `ADAPTIVE_GROWTH_THRESHOLD` | 1 | Growth rate (%) that signals saturation |
| `DECAY_RATE_PER_US_Q16` | 1 | Heat decay per microsecond (Q16 fixed-point) |
| `HEARTBEAT_INFERENCE_FREQUENCY` | 5000 | Ticks between full inference runs |
| `TRANSITION_WINDOW_SIZE` | 2 | Context depth for pipelining prediction |

## Related Files

- `src/rolling_window_of_truth.c` — Loop #2 implementation
- `src/dictionary_heat_optimization.c` — Loop #1, #3 heat management
- `src/inference_engine.c` — Loop #5, #6 unified inference
- `src/physics_pipelining_metrics.c` — Loop #4 transition tracking
- `src/vm.c` — `vm_tick()` heartbeat dispatcher (Loop #7)
- `include/rolling_window_knobs.h` — Tuning knob definitions