# StarForth Multivariate Coupled Dynamics Experiment
## Phase 2: Golden Configuration Discovery via Stability Fingerprinting

**Status**: Design Phase
**Date**: 2025-11-20
**Paradigm**: Multivariate systems analysis (not factorial DOE)

---

## 1. Fundamental Insight

This is **not** a traditional DOE. We are measuring a **coupled dynamical system** where:

Every metric is **simultaneously**:
- A **signal** (measurable outcome)
- A **feedback variable** (influences future state)
- Part of a **field** (high-dimensional attractor landscape)

### The Coupling Web (Per Tick)
```
cache_hit%  ──┐
              ├──> bucket_load ──> heat ──> decay ──> window_width ──┐
bucket_hit% ─┘                                                       │
                                                                      ├──> prediction_accuracy
              ┌──────────────────────────────────────────────────────┘
              │
              ├──> cache_hit% (next tick feedback)
              │
              └──> heartbeat_interval (adaptive response to load)

load_intensity ──> word_access_pattern ──> heat_distribution ──> decay_effectiveness
                                                                      │
                                                                      └──> jitter_envelope
```

Each configuration produces a unique **trajectory** through metric-space. We don't measure "main effects." We measure **the shape of the trajectory**.

---

## 2. What We're Actually Measuring

### Per Tick (Milliseconds)
Each heartbeat tick (~1ms) captures the *current state* of the coupled system:

```c
typedef struct {
    /* === Temporal Anchor === */
    uint32_t tick_number;              /* Tick index within run */
    uint64_t tick_ns;                  /* Actual tick interval (ns) */
    uint64_t elapsed_ns;               /* Total elapsed since run start */

    /* === Metric Wave Functions === */
    uint32_t cache_hits_delta;         /* Cache hits this tick */
    uint32_t bucket_hits_delta;        /* Bucket hits this tick */
    double cache_hit_percent;          /* ψ_cache(t) */
    double bucket_hit_percent;         /* ψ_bucket(t) */

    /* === Feedback Signals === */
    uint64_t hot_word_count;           /* Words above heat threshold */
    double avg_word_heat;              /* Mean execution heat */
    uint32_t decay_estimate;           /* Inferred decay rate */
    uint32_t window_width;             /* Current window size */

    /* === Load Response === */
    uint32_t word_executions_delta;    /* Execution count this tick */
    uint32_t predictions_this_tick;    /* Speculative prefetch attempts */
    uint32_t prefetch_hits_delta;      /* Successful predictions */

    /* === Stability Measures === */
    double tick_jitter_ns;             /* Deviation from mean tick */
    double metric_stability_index;     /* Composite smoothness score */
} HeartbeatTickSnapshot;
```

### Per Run Summary Vector (After 100K+ ticks)
```c
typedef struct {
    /* === Convergence === */
    uint32_t convergence_time_ticks;   /* When steady-state reached */
    double cache_convergence_cv;       /* CV of cache% after convergence */
    double bucket_convergence_cv;      /* CV of bucket% after convergence */
    double decay_convergence_cv;       /* CV of decay estimate */
    double window_convergence_cv;      /* CV of window width */

    /* === Steady State === */
    double cache_hit_percent_steady;   /* Mean in steady state */
    double bucket_hit_percent_steady;  /* Mean in steady state */
    double avg_jitter_ns_steady;       /* Mean tick jitter (steady) */
    double hotword_ratio_steady;       /* Mean hot word count */

    /* === Dynamics === */
    double cache_rise_time_ticks;      /* Time to reach 95% of steady */
    double decay_rise_time_ticks;      /* Decay learning curve */
    double settling_time_ticks;        /* Total settle time */
    double overshoot_percent;          /* Peak overshoot if any */

    /* === Coupling Strength === */
    double load_to_heartbeat_corr;     /* correlation(load, tick_ns) */
    double load_to_cache_corr;         /* correlation(load, cache_hit%) */
    double heartbeat_to_cache_corr;    /* lag-aware correlation */
    double metric_correlation_matrix[7][7];  /* Full cross-correlation */

    /* === Spectral Properties === */
    double dominant_frequency_hz;      /* Primary oscillation frequency */
    double spectral_entropy;           /* Shannon entropy of spectrum */
    double dominant_eigenvalue;        /* Largest eigenvalue of cov matrix */

    /* === Stability Index === */
    double overall_stability_score;    /* 0-100: composite stability fingerprint */
    // Computed as:
    // (1/3)*jitter_score + (1/3)*convergence_score + (1/3)*coupling_score
} RunStabilityFingerprint;
```

### Per Configuration Summary (After 150+ runs)
```c
typedef struct {
    /* === Between-Run Statistics === */
    double stability_score_mean;       /* Mean across runs */
    double stability_score_std;        /* StdDev (low = reproducible) */
    double stability_score_cv;         /* Coefficient of variation */

    /* === Convergence Reliability === */
    double convergence_time_mean_ticks;
    double convergence_time_std_ticks;
    uint32_t convergence_success_rate; /* % of runs that converge */

    /* === Coupling Stability === */
    double load_heartbeat_corr_mean;
    double load_heartbeat_corr_std;
    int coupling_is_stable;            /* 1 if corr reliable across runs */

    /* === Attractor Basin Depth === */
    double eigenmode_decay_rate;       /* How fast perturbations die */
    double basin_depth_estimate;       /* Resistance to noise */

    /* === Jitter Envelope === */
    double tick_jitter_mean_ns;
    double tick_jitter_std_ns;
    double tick_jitter_cv;             /* Coefficient of variation */
    int jitter_is_white;               /* 1 if noise is Gaussian white */

    /* === Noise Propagation === */
    double metric_noise_coupling;      /* How noise spreads across metrics */
    double entropy_growth_rate;        /* d(entropy)/dt */

    /* === Configuration Signature === */
    int config_id;                     /* 1_0_1_1_1_0 encoded */
    char config_name[32];              /* "1_0_1_1_1_0" */
    int loop_1_enabled, loop_2_enabled, ...;
} ConfigurationFingerprint;
```

---

## 3. Experimental Design

### Parameters
```
Configurations:     5 elite (from Stage 1)
Runs per config:    150-200 (statistically significant for correlation)
Ticks per run:      ~100,000 (to measure convergence + steady state)
Total heartbeat ticks: 5 × 150 × 100,000 = 75M ticks (~1.25 hours per config)
Total experiment:   ~6-8 hours wall-clock
```

### Test Matrix
- **Randomized run order** across all 750 runs (avoid temporal confounds)
- **Interleaved configs** to minimize thermal/memory state drift
- **Background tasks minimized** (dedicated CPU for heartbeat)

### Sampling Strategy
- **Every tick captured** (no subsampling—need time-series fidelity)
- **Per-run fingerprint computed** from time-series
- **Per-config fingerprint computed** from 150+ run fingerprints

---

## 4. Analysis Pipeline

### Phase 1: Per-Run Fingerprinting (Immediate)
For each run's 100K-tick time series:

1. **Detect convergence**: When ψ_cache(t) CV drops < 5%
2. **Compute steady-state metrics**: Mean, std, CV after convergence
3. **Extract dynamics**: Rise time, settling time, overshoot
4. **Measure coupling**:
   - correlation(load_intensity, heartbeat_ns) with lag search
   - correlation matrix of all 7 metrics
5. **Spectral analysis**: FFT of cache_hit% to find dominant modes
6. **Compute stability index**: Composite of jitter + convergence + coupling

### Phase 2: Per-Config Fingerprinting (After all runs)
For each config's 150+ fingerprints:

1. **Compute robustness**: std of stability_score across runs
2. **Assess convergence reliability**: % of runs with successful convergence
3. **Evaluate coupling strength**: mean(load↔heartbeat_corr), std
4. **Estimate attractor basin**: Eigenmode decay rate
5. **Characterize noise**: Jitter CV, entropy growth

### Phase 3: Golden Config Selection
Compare 5 configuration fingerprints on:

| Dimension | Weight | Ideal Behavior |
|-----------|--------|--|
| **Stability Score** | 30% | Highest mean, lowest std |
| **Convergence Reliability** | 25% | 100% convergence, low variability |
| **Coupling Strength** | 20% | Strong (0.8+) & stable load↔heartbeat correlation |
| **Attractor Basin Depth** | 15% | Fast eigenmode decay (resilient to noise) |
| **Jitter Smoothness** | 10% | Low CV, white noise (not colored) |

**Golden Config** = argmax(weighted_score) over 5 configs

---

## 5. CSV/Data Format

### Primary Output: Per-Tick Time Series
```csv
run_id,config,tick_number,tick_ns,elapsed_ns,cache_hits_delta,bucket_hits_delta,cache_hit_percent,bucket_hit_percent,hot_word_count,avg_word_heat,decay_estimate,window_width,word_executions_delta,predictions_this_tick,prefetch_hits_delta,tick_jitter_ns,metric_stability_index
1,1_0_1_1_1_0,1,1000000,1000000,45,23,89.2,91.5,8,234.5,0.33,4096,156,12,8,0,0.95
1,1_0_1_1_1_0,2,1001200,2001200,47,25,89.3,91.6,8,234.6,0.33,4096,158,11,9,1200,0.96
...
```
(100K rows per run × 750 runs = 75M rows total)

### Secondary Output: Per-Run Summary
```csv
run_id,config,convergence_time_ticks,cache_convergence_cv,bucket_convergence_cv,cache_hit_percent_steady,...,overall_stability_score
1,1_0_1_1_1_0,5234,0.032,0.028,...,82.4
2,1_0_1_1_1_0,5156,0.031,0.029,...,83.1
...
```

### Tertiary Output: Per-Config Summary
```csv
config,stability_score_mean,stability_score_std,convergence_time_mean_ticks,...,golden_rank
1_0_1_1_1_0,82.7,2.1,5200,...,1
1_0_1_1_1_1,79.3,4.5,6100,...,3
...
```

---

## 6. Implementation Roadmap

### Step 1: Heartbeat Instrumentation
- Extend `HeartbeatWorker` with circular buffer for per-tick snapshots
- Capture 7 key metrics every tick (minimal overhead)
- Stream snapshots to file or in-memory ring buffer

### Step 2: Run Fingerprinting
- Implement per-run analysis: convergence detection, CV computation, coupling measurement
- Generate `RunStabilityFingerprint` struct for each run
- Write R/Python code to compute eigenvalues, spectral properties

### Step 3: Config Fingerprinting
- Aggregate 150+ run fingerprints per config
- Compute robustness statistics
- Generate `ConfigurationFingerprint` for each of 5 configs

### Step 4: Golden Config Decision
- Weight and score 5 config fingerprints
- Identify winner by composite stability index
- Generate report with reasoning

### Step 5: Visualization & Reporting
- Time-series plots per run (cache%, bucket%, tick_ns over time)
- Correlation heatmaps (per config, per run)
- PCA projection of runs into first 3 principal components
- Stability score distributions (violin plots)
- Golden config justification report

---

## 7. Interpretation Guide

### What Each Metric Means

| Metric | Signal | Interpretation |
|--------|--------|---|
| **cache_hit%** | Feedback loop #1 effectiveness | Higher = better prediction |
| **bucket_hit%** | Secondary lookup efficiency | Validates hash function |
| **hot_word_count** | Decay loop output | More = hotter words concentrated |
| **avg_word_heat** | Decay state | Slopes indicate decay strength |
| **tick_ns variation (CV)** | Heartbeat stability | Lower CV = steadier timing |
| **convergence_time** | Inference speed | Faster = learns pattern quicker |
| **load↔heartbeat_corr** | Adaptive responsiveness | 1.0 = perfect coupling, 0 = deaf |
| **dominant_eigenvalue** | Attractor basin strength | Larger = deeper basin, more stable |
| **overall_stability_score** | Composite quality | 0-100, captures all dimensions |

### Interpreting Configuration Fingerprints

A "good" config fingerprint has:
- ✅ High stability_score_mean (>80)
- ✅ Low stability_score_std (<3)
confidence high = reproducible
- ✅ 100% convergence_success_rate (all runs stabilize)
- ✅ 0.8+ load_heartbeat_corr (tight coupling to load)
- ✅ Low tick_jitter_cv (<0.2, <20% variation)
- ✅ Large dominant_eigenvalue (deep attractor)

A "poor" config fingerprint has:
- ❌ Low stability_score_mean (<75)
- ❌ High stability_score_std (>5)
unstable = irreproducible
- ❌ <80% convergence_success_rate (some runs diverge)
- ❌ <0.6 load_heartbeat_corr (loose/noisy coupling)
- ❌ High tick_jitter_cv (>0.35, >35% variation)
- ❌ Small dominant_eigenvalue (shallow attractor, noise-prone)

---

## 8. Next Steps

1. **Instrument heartbeat loop** to capture per-tick snapshots
2. **Implement RunStabilityFingerprint** computation (convergence detection, correlations)
3. **Build fingerprint aggregation** for ConfigurationFingerprint
4. **Write R analysis suite**:
   - Per-run time-series visualization
   - Correlation heatmaps
   - PCA decomposition
   - Stability score violin plots
   - Golden config selection report
5. **Execute experiment** with 150-200 runs per config (5 × 150+ = 750 total)
6. **Analyze results** and select golden config

---

## References

- **Coupled dynamical systems**: Kuramoto model, coupled oscillators
- **Stability analysis**: Lyapunov exponents, attractor basin depth
- **Time-series analysis**: Spectral entropy, eigenmode decomposition
- **Systems biology**: Feedback loops, homeostasis under load
- **Control theory**: Load-to-output coupling, response time, settling time

---

**Captain, this is the right framework. Ready to build the harness.**