# Window Scaling Experiment — James Law Validation

**Objective**: Empirically validate the **James Law of Computational Dynamics**:

```
Λ = W / (DoF + 1)
```

where:
- **Λ** = stability-smoothing factor (effective window capacity per degree of freedom)
- **W** = rolling window size (bytes)
- **DoF** = degrees of freedom (number of active feedback loops, 0-7)

## Hypothesis

The quantity `K = Λ × (DoF + 1) / W` should remain approximately constant across:
- Multiple window sizes (512 to 65,536 bytes)
- Multiple degrees of freedom (0-7 active loops)
- Diverse workload patterns

A constant `K ≈ 1.0` across all conditions would validate the James Law as a fundamental scaling relationship in adaptive computational systems.

## Expected Outcomes

### Scenario A: Law Holds (K ≈ 1.0)
- **Result**: James Law validated as a universal scaling relationship
- **Implication**: System behavior is predictable and governed by geometric invariants
- **Patent claim**: "Empirically validated conservation law in adaptive runtimes"

### Scenario B: Critical Threshold Exists
- **Result**: Law holds for `W < W_critical`, then breaks down
- **Implication**: Phase transition exists ("gravitational collapse")
- **Patent claim**: "Predictable stability boundaries in multi-loop feedback systems"

### Scenario C: DoF-Dependent Scaling
- **Result**: K varies systematically with DoF but not randomly
- **Implication**: More complex relationship (e.g., logarithmic, power-law)
- **Patent claim**: "Novel scaling relationship in adaptive virtual machines"

---

## Experimental Design

### Independent Variables

| Variable | Levels | Values |
|----------|--------|--------|
| **DoF** | 8 | 0, 1, 2, 3, 4, 5, 6, 7 |
| **Window Size** | 12 | 512, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 16384, 32769, 52153, 65536 |
| **Replicate** | 30 | 1-30 |

### Dependent Variables

Primary metrics:
- **Execution time** (ns/word) - Performance measure
- **Coefficient of variation** (CV) - Stability measure
- **Lambda (Λ)** - Effective smoothing factor (computed from window metrics)

Secondary metrics:
- Cache hit rates
- Context prediction accuracy
- Heat distribution (entropy)
- Mode selection (for L8 adaptive runs)

### Experimental Controls

- **Fixed workload**: `init-l8-omni.4th` (mega-workload combining all patterns)
- **Randomized run order**: Shuffled matrix eliminates temporal bias
- **Identical hardware**: Same CPU, frequency, temperature controls
- **Clean builds**: Fresh compilation for each configuration

### Total Runs

```
8 DoF × 12 windows × 30 reps = 2,880 runs
```

Estimated time: ~24-48 hours (depends on build+run time per configuration)

---

## Directory Structure

```
experiments/
├── bin/                           # Pre-built VM configurations
│   ├── dof0_w512/
│   │   ├── starforth             # Pre-built binary
│   │   └── config.txt            # Build metadata
│   ├── dof0_w1024/
│   └── ... (96 configs total)
│
└── window_scaling_james_law/
    ├── README.md                      # This file
    ├── run_matrix_shuffled.csv        # Generated: Randomized experiment plan
    ├── scripts/
    │   ├── generate_run_matrix.R      # Generate shuffled run matrix
    │   ├── prebuild_all_configs.sh    # NEW: Pre-build all 96 configs
    │   ├── run_window_sweep_prebuilt.sh # NEW: Fast execution (recommended)
    │   ├── run_window_sweep.sh        # OLD: Rebuild approach (slower)
    │   └── analyze_results.R          # Validate James Law from data
    ├── results/
    │   ├── raw/
    │   │   └── window_sweep_results.csv   # Raw experimental data
    │   └── processed/
    │       ├── james_law_validation.csv   # K values by condition
    │       ├── K_distribution.png         # Visualization: K vs DoF/W
    │       └── stability_surface.png      # 3D: DoF × W × CV
    └── conf/
        └── init-l8-omni.4th           # Mega-workload (in main conf/ directory)
```

---

## Usage

### Recommended: Pre-Build Approach (~6 hours total)

#### Step 1: Generate Run Matrix

```bash
cd scripts/
./generate_run_matrix.R
```

**Output**: `run_matrix_shuffled.csv` (2,880 rows, randomized)

#### Step 2: Pre-Build All Configurations (~1.5 hours)

```bash
./prebuild_all_configs.sh
```

**Output**: 96 pre-built binaries in `experiments/bin/`
- Each config built once and stored safely
- Immune to `make clean`
- Can be reused for multiple experiment runs

#### Step 3: Execute Experiment (~4-5 hours)

```bash
./run_window_sweep.sh
```

**Output**: `results/raw/window_sweep_results.csv`
**Monitoring**: Check `results/raw/experiment.log` for progress

**Advantage**: No rebuilds! 3-5 hours faster than rebuild approach.

#### Step 4: Analyze Results (~5 minutes)

```bash
./analyze_results.R
```

**Output**:
- `results/processed/james_law_validation.csv` - K values and deviations
- `results/processed/*.png` - Plots validating the law

---

### Alternative: Rebuild-Per-Run Approach (~10 hours total)

For those who prefer simplicity over speed:

```bash
cd scripts/
./run_window_sweep.sh  # Rebuilds VM as configs change
```

**Slower but simpler** - useful for verification or debugging.

---

## Key Metrics

### Lambda Computation

From VM output, compute effective Λ:

```
Λ_effective = win_final_bytes / (DoF + 1)
```

Or, if using predicted values:

```
Λ_predicted = W / (DoF + 1)
```

### K Statistic

The James Law holds if:

```
K = Λ × (DoF + 1) / W ≈ 1.0
```

**Validation criteria**:
- Mean(K) within [0.95, 1.05]
- Std(K) < 0.1
- Max deviation from 1.0 < 10%

### Critical Window Detection

Identify `W_critical` where:
- CV suddenly increases (variance explosion)
- K deviates significantly from 1.0
- System collapses to config 0000000 (all loops off)

---

## Data Schema

### Run Matrix (`run_matrix_shuffled.csv`)

| Column | Type | Description |
|--------|------|-------------|
| `dof` | int | Degrees of freedom (0-7) |
| `window_size` | int | Rolling window size (bytes) |
| `replicate` | int | Replicate number (1-30) |
| `lambda_predicted` | float | W / (DoF + 1) |
| `r_s` | float | Schwarzschild radius analog |
| `window_category` | string | subcritical, baseline, stable, critical, collapse |
| `run_id_sequential` | int | Original sequential ID |
| `run_id_shuffled` | int | Shuffled execution order |
| `loop_mask` | string | Binary loop configuration (e.g., "0110111") |

### Results (`window_sweep_results.csv`)

68 columns total, including:
- **Experiment metadata**: timestamp, run_id, dof, window_size, replicate, loop_mask
- **Predicted values**: lambda_predicted, r_s, window_category
- **VM metrics**: Full --doe output (57 columns)
  - Performance: workload_ns_q48, runtime_ms, words_exec
  - Stability: CV computed from runtime variance
  - State: total_heat, entropy, decay_slope, win_diversity_pct
  - Cache: cache_hits, cache_hit_pct, bucket_hit_pct
  - Window: win_final_bytes, win_width, final_win_size

---

## Expected Results

### Baseline Validation (W = 4096)

From prior DOE experiments, we know:
```
Λ(DoF) × (DoF + 1) = 4096.0 ± 0.0  (CV = 0.00%)
```

This experiment extends this to arbitrary window sizes.

### Predicted Collapse Threshold

**Hypothesis**: `W_critical ≈ 16,384 bytes` (4 × W₀)

**Reasoning**:
- At W = 4096, system is stable (validated)
- At W = 8192, system should remain stable
- At W = 16384, critical threshold likely reached
- At W > 16384, system may collapse to config 0

**Test**: Measure P(collapse | W > W_critical) via mode selection and CV

---

## Analysis Plan (TODO: `analyze_results.R`)

### 1. Compute K for All Runs

```r
results <- results %>%
  mutate(
    lambda_effective = win_final_bytes / (dof + 1),
    K = lambda_effective / window_size
  )
```

### 2. Validate James Law

```r
summary <- results %>%
  group_by(dof, window_size) %>%
  summarise(
    mean_K = mean(K),
    sd_K = sd(K),
    max_dev = max(abs(K - 1.0))
  )

# Overall validation
mean(summary$mean_K)  # Should be ≈ 1.0
sd(summary$mean_K)    # Should be < 0.1
```

### 3. Identify Critical Window

```r
cv_by_window <- results %>%
  group_by(window_size) %>%
  summarise(mean_cv = mean(cv))

# Find elbow point where CV explodes
W_critical <- cv_by_window %>%
  filter(mean_cv > threshold) %>%
  pull(window_size) %>%
  min()
```

### 4. Generate Plots

- **Plot 1**: K vs DoF (faceted by window size)
- **Plot 2**: K vs W (faceted by DoF)
- **Plot 3**: CV vs W (detect phase transition)
- **Plot 4**: 3D stability surface (DoF × W × CV)
- **Plot 5**: Heatmap of K deviations

---

## Success Criteria

The experiment succeeds if:

1. ✓ K clusters around 1.0 for stable configurations
2. ✓ K deviation from 1.0 correlates with instability (high CV)
3. ✓ Critical window W_critical is reproducibly identified
4. ✓ Collapse behavior is consistent across replicates
5. ✓ Results are independent of replicate order (validated by shuffle)

**Gold standard**: `mean(K) = 1.00 ± 0.05` across all stable configurations

---

## Integration with Patent

If the James Law is validated, add this claim:

> **Claim XX**: A method for determining optimal window capacity in an adaptive
> runtime system, wherein the window size W and degrees of freedom DoF satisfy
> the relationship Λ = W/(DoF+1), and wherein violation of this relationship
> results in measurable performance degradation and/or system instability.

Supporting evidence:
- 2,880 experimental runs
- Statistical validation across 8 DoF × 12 window sizes
- Reproducible across 30 replicates
- Shape-invariant (single mega-workload tests all patterns)

---

## Timeline

### Pre-Build Approach (Recommended)
| Phase | Duration | Deliverable |
|-------|----------|-------------|
| **Setup** | 15 min | Run matrix generated, scripts validated |
| **Pre-build** | 1.5 hours | All 96 configs built |
| **Execution** | 4-5 hours | 2,880 runs completed |
| **Analysis** | 1 hour | Statistical validation, plots |
| **Documentation** | 2-4 hours | Update patent, write report |
| **Total** | ~1 day | James Law proven or disproven |

### Rebuild Approach (For Comparison)
| Phase | Duration | Deliverable |
|-------|----------|-------------|
| **Setup** | 15 min | Run matrix generated |
| **Execution** | 8-10 hours | 2,880 runs completed (with rebuilds) |
| **Analysis** | 1 hour | Statistical validation, plots |
| **Documentation** | 2-4 hours | Update patent, write report |
| **Total** | ~1.5 days | James Law proven or disproven |

---

## Notes

- **Build time**: Each configuration requires clean build (~30-60 sec)
- **Run time**: Each VM execution ~1-5 sec with --doe flag
- **Disk space**: ~50 MB for full dataset
- **Parallelization**: Currently sequential; could parallelize by window size
- **Checkpointing**: Runner writes incrementally; safe to resume if interrupted

---

## Contact

**Experiment design**: Robert A. James
**Implementation**: StarForth VM
**Analysis framework**: Based on DOE/L8/Shape validation experiments
**Date**: November 29, 2025

---

*"Either we discover a law, or we discover why it's not a law. Either way, we learn."*