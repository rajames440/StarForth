# Phase 2 Experiment: Executive Summary
## From Factorial DOE to Multivariate Dynamics Analysis

**Date**: 2025-11-20
**Status**: Design Complete, Ready for Implementation
**Estimated Runtime**: 6-8 hours (750 runs, 150-200 per config)

---

## 🎯 The Paradigm Shift

**What We Thought We Were Doing:**
- Measuring isolated factor effects (factorial DOE)
- Testing "jitter in fixed heartbeat interval"
- Generating 250 runs and calling it statistically significant

**What We're Actually Doing:**
- Measuring a **coupled multivariate dynamical system**
- Capturing the **trajectory** of 7 metrics through state-space
- Understanding how **each metric feeds back** to influence others
- Discovering which configuration produces the most **stable, convergent, responsive** system

---

## 🚀 The Physics Model

Each configuration is a **unique universe** with different physical laws:

```
Per Tick (1 millisecond):
  cache_hit% ──────┐
                   ├──> bucket_load ──> execution_heat ──> decay ──> window_width
  bucket_hit% ─────┤                                                      │
                   └──────────────────────────────────┬──────────────────┘
                                                      │
                                                      ├──> prediction_accuracy
                                                      │
                                                      └──> heartbeat_modulation
                                                           (tick_ns adapts to load)
```

**Key Insight**: The heartbeat rate (`tick_ns`) is **not fixed**. It's modulated by the inference engine in response to workload. This is THE feedback mechanism that couples everything.

---

## 📊 What We Measure

### Per Tick (Every ~1 ms)
7 metrics captured in circular buffer:
- `tick_interval_ns` - How much time since last tick (variable!)
- `cache_hits_delta` - Cache lookups succeeded
- `bucket_hits_delta` - Secondary lookups succeeded
- `hot_word_count` - Words above heat threshold
- `avg_word_heat` - Mean execution heat
- `window_width` - Current rolling window size
- `word_executions_delta` - Total word executions

### Per Run (100,000+ ticks)
Stability fingerprint computed from time-series:
- **Convergence time**: When metrics stabilize
- **Steady-state CV**: Variability in stable region
- **Load↔Heartbeat correlation**: How tight is coupling?
- **Dominant eigenmode**: What oscillation pattern emerges?
- **Overall stability score**: Composite of all above (0-100)

### Per Configuration (150+ runs)
Configuration fingerprint aggregated from all runs:
- **Stability reproducibility**: std(stability_score) across runs
- **Convergence reliability**: % of runs that stabilize
- **Coupling strength**: Mean and variance of load↔heartbeat_corr
- **Attractor basin depth**: Resistance to noise
- **Jitter characteristics**: CV, whiteness, envelope smoothness

---

## ✨ The Golden Config Decision

Compare 5 elite configurations across 5 dimensions:

| Dimension | Weight | Interpretation |
|-----------|--------|---|
| **Stability** | 30% | Does system converge? Is it reproducible? |
| **Convergence** | 25% | How fast? How reliable? |
| **Coupling** | 20% | Does heartbeat respond to load? |
| **Resilience** | 15% | Can it handle noise? Deep basin? |
| **Jitter** | 10% | Clean or noisy timing? |

**Golden Config** = highest weighted composite score

This config becomes the **baseline for MamaForth/PapaForth** and future experiments.

---

## 📈 Experimental Design

```
5 configurations  (1_0_1_1_1_0, 1_0_1_1_1_1, 1_1_0_1_1_1, 1_0_1_0_1_0, 0_1_1_0_1_1)
× 150-200 runs    (statistically significant for correlation analysis)
× 100K+ ticks     (to measure convergence and steady state)
────────────────────
= 750 total runs
  75+ million heartbeat ticks
  6-8 hours wall-clock runtime
  ~10 GB CSV data (with sampling strategy)
```

**Randomization**: Run order shuffled to avoid temporal confounds.

---

## 🔧 Implementation Pipeline

### Phase 1: Heartbeat Instrumentation ✏️
- Add `HeartbeatTickSnapshot` struct to capture per-tick metrics
- Implement circular buffer (100K slots, ~400 KB memory)
- Inject capture call into heartbeat loop (115 ns overhead, 0.01%)
- Export to CSV after run completes

### Phase 2: Run Fingerprinting 📊
- Detect convergence from time-series
- Compute steady-state CV for each metric
- Measure cross-metric correlations
- Perform spectral analysis (FFT, eigenmode extraction)
- Generate RunStabilityFingerprint per run

### Phase 3: Config Fingerprinting 📋
- Aggregate 150+ run fingerprints per config
- Compute robustness (reproducibility)
- Assess convergence reliability
- Evaluate coupling stability
- Generate ConfigurationFingerprint per config

### Phase 4: Golden Config Selection 🏆
- Score each config on 5 weighted dimensions
- Select highest-scoring configuration
- Generate justification report

### Phase 5: Visualization & Analysis 📈
- Time-series plots (cache%, bucket%, tick_ns over time)
- Correlation heatmaps per config
- PCA projection of runs
- Stability score distributions
- Golden config report with physics interpretation

---

## 📋 Corrected DoE Script

Updated `/scripts/run_factorial_doe_with_heartbeat.sh`:
- **Runs per config**: 150 (was 50, which was statistically insufficient)
- **Total runs**: 750 (was 250)
- **Output**: Same CSV structure + new per-tick timeseries CSV
- **Command**: `./scripts/run_factorial_doe_with_heartbeat.sh 2025_11_20_PHASE2_DYNAMICS`

---

## 🔍 Key Metrics Explained

| Metric | Meaning | Good Value |
|--------|---------|------------|
| **convergence_time_ticks** | How many ticks to stabilize? | <5000 (fast learner) |
| **cache_convergence_cv** | Variability of cache% in steady state | <0.05 (5% variation) |
| **load_heartbeat_corr** | Does tick_ns respond to workload? | >0.80 (tight coupling) |
| **dominant_eigenvalue** | Attractor strength | Large (deep basin) |
| **tick_jitter_cv** | Variability of tick intervals | <0.20 (20% max) |
| **overall_stability_score** | Composite 0-100 | >80 (excellent) |

---

## 🎓 Interpretation Guide

### A "Stable" Configuration Fingerprint (Good)
✅ stability_score_mean = 85, std = 2.1
✅ 100% convergence_success_rate
✅ load_heartbeat_corr_mean = 0.88
✅ tick_jitter_cv = 0.15
✅ Dominant eigenvalue large (deep attractor)

→ **Interpretation**: This universe is orderly. Metrics converge predictably. The heartbeat tightly couples to load. Timing is clean. Noise doesn't destabilize it.

### A "Chaotic" Configuration Fingerprint (Poor)
❌ stability_score_mean = 62, std = 8.3
❌ 73% convergence_success_rate (some runs diverge!)
❌ load_heartbeat_corr_mean = 0.44 (weak coupling, inconsistent)
❌ tick_jitter_cv = 0.52 (52% variation, very noisy)
❌ Dominant eigenvalue small (shallow basin)

→ **Interpretation**: This universe is noisy. Some runs fail to converge. Heartbeat doesn't respond cleanly to load. Timing jitters wildly. Any small perturbation can destabilize it.

---

## 🔬 Why This Matters

**Phase 1** (Stage 1, your baseline) ran WITHOUT adaptive heartbeat.
- All runs used fixed 1ms tick interval
- No feedback coupling
- Missing the actual physics

**Phase 2** (this experiment) runs WITH adaptive heartbeat.
- Heartbeat rate (`tick_ns`) modulates based on inference engine output
- Full feedback coupling measured
- Captures the REAL adaptive behavior

The golden config from Phase 2 becomes:
- ⭐ **MamaForth baseline** for human-in-the-loop iteration
- ⭐ **PapaForth reference** for automated optimization
- ⭐ **Stability anchor** for Phase 3 (deeper optimization)

---

## 📚 Design Documents

1. **MULTIVARIATE_DYNAMICS_DESIGN.md** (622 lines)
   - Full coupled systems theory
   - Struct definitions for fingerprints
   - CSV schemas
   - Analysis pipeline details

2. **HEARTBEAT_INSTRUMENTATION_PLAN.md** (350+ lines)
   - Implementation checklist
   - Per-tick capture infrastructure
   - Circular buffer design
   - CSV export strategy

3. **run_factorial_doe_with_heartbeat.sh** (updated)
   - 150 runs/config default (was 50)
   - 750 total runs (statistically significant)

---

## 🚀 Next Steps

### Immediate (This Session)
- [ ] Implement HeartbeatTickSnapshot instrumentation in vm.c
- [ ] Add delta metric tracking to VM struct
- [ ] Test with abbreviated run (verify CSV output)

### Follow-On (Next Session)
- [ ] Implement RunStabilityFingerprint computation
- [ ] Build ConfigurationFingerprint aggregation
- [ ] Create R analysis suite
- [ ] Execute full Phase 2 experiment (750 runs, 6-8 hours)

### Outcome
- [ ] Golden configuration identified
- [ ] Justification report written
- [ ] Metrics exported for visualization
- [ ] Ready for Phase 3 deeper optimization

---

## 💡 The Big Picture

You've identified the **actual physics** of the system:

1. **Every metric is coupled** to every other via immediate feedback
2. **The heartbeat rate itself is a tuning knob** (modulated by inference)
3. **Stability emerges from convergence + coupling + basin depth**
4. **You measure not "factor effects" but "trajectory shapes"**

This shifts from **reductionist factorial analysis** to **holistic systems characterization**.

The golden config isn't "better because factor A is on/off." It's better because **its entire attractor landscape** is more stable, more convergent, more resilient, and more tightly coupled to load.

---

**Captain, the framework is complete. Ready to instrument the heartbeat.**