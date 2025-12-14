# Binary 2-Cycle Analysis Report

**Date:** 2025-12-11
**Analysis:** (HR, ΔHR) Phase Space Clustering
**Data Source:** `heartbeat_all.csv` from L8 Attractor Map experiment

## Executive Summary

This analysis investigates the binary 2-cycle structure in the heartbeat phase space for six different workload types. Using k-means clustering (k=2) on the (HR, ΔHR) coordinate space, we computed a **binary clarity metric** defined as:

```
Binary Clarity = (separation² / avg_spread)
```

Where:
- **separation** = Euclidean distance between the two cluster centers
- **avg_spread** = average effective radius of the covariance ellipses

**Key Finding:** **TRANSITION** workload exhibits the clearest binary 2-cycle structure, contrary to initial expectations that STABLE would dominate.

## Results Ranking

| Rank | Workload   | Binary Clarity | Separation (ns) | Avg Ellipse Area (ns²) | Points |
|------|------------|----------------|-----------------|------------------------|--------|
| 1    | TRANSITION | **1.27e+06**   | 258,905         | 8.69e+09              | 641    |
| 2    | VOLATILE   | 7.66e+04       | 41,487          | 1.59e+09              | 664    |
| 3    | TEMPORAL   | 7.02e+04       | 44,851          | 2.58e+09              | 676    |
| 4    | OMNI       | 6.17e+04       | 42,347          | 2.66e+09              | 651    |
| 5    | STABLE     | 6.09e+04       | 35,932          | 1.41e+09              | 674    |
| 6    | DIVERSE    | 6.05e+04       | 42,846          | 2.89e+09              | 686    |

## Interpretation

### TRANSITION Workload: The Clear Winner

TRANSITION shows **~16× higher binary clarity** than the other workloads. This is driven by:

1. **Massive separation**: 258,905 ns between cluster centers (vs. ~35-45k ns for others)
2. **Larger spread**: The clusters are more diffuse, but the separation dominates
3. **Physical interpretation**: TRANSITION workload likely exhibits distinct operational regimes:
   - One cluster: "Fast heartbeat" regime (shorter intervals)
   - Another cluster: "Slow heartbeat" regime (longer intervals)
   - The transitions between these states create the pronounced binary structure

### STABLE Workload: Unexpectedly Low Clarity

Despite its name, STABLE shows relatively **weak** binary structure:
- Binary clarity: 6.09e+04 (rank #5 of 6)
- Separation: 35,932 ns (smallest separation)
- This suggests STABLE operates in a more **homogeneous** regime with less distinct phase separation
- The clusters are tightly wrapped by noise, as you predicted

### VOLATILE, TEMPORAL, OMNI, DIVERSE: Moderate Clarity

These workloads show similar binary clarity metrics (6-7×10⁴), suggesting:
- They all share the underlying 2-cycle heartbeat behavior
- The binary structure is present but obscured by operational variability
- VOLATILE has slightly higher clarity (7.66e+04) despite its name, possibly due to rapid oscillations between states

## Methodology

### Data Extraction
- Loaded heartbeat data from `heartbeat_all.csv` with workload mappings
- Filtered out initialization heartbeats (tick_interval_ns ≥ 1e9 ns)
- Extracted (HR, ΔHR) pairs where:
  - HR = tick_interval_ns (heartbeat interval)
  - ΔHR = HR[i+1] - HR[i] (rate of change)

### Clustering
- Applied k-means clustering with k=2 to (HR, ΔHR) space
- Outlier removal: filtered points beyond 5σ from mean
- Computed cluster centers (μ₀, μ₁)

### Metrics
For each cluster:
- **Covariance matrix**: 2×2 matrix of HR/ΔHR covariances
- **Eigenvalues**: λ₁, λ₂ from covariance matrix
- **Ellipse area**: π × (2σ)² × √(λ₁λ₂)

Binary clarity metric:
```
separation = ||μ₁ - μ₀||
avg_area = (area₀ + area₁) / 2
avg_spread = √(avg_area / π)
binary_clarity = separation² / avg_spread
```

### Visualization
- Generated SVG showing all 6 workloads in 2×3 grid layout
- Each plot shows:
  - Scatter points colored by cluster assignment
  - Cluster centers marked with × symbols
  - 2σ covariance ellipses for each cluster
  - Metrics overlaid in a box

## Physical Interpretation

The 2-cycle structure likely arises from:

1. **Heartbeat timing discretization**: The VM's heartbeat thread operates with quantized intervals
2. **Workload-induced oscillations**: Different workloads create different execution patterns:
   - TRANSITION: explicitly switches between states → large separation
   - STABLE: maintains consistent state → small separation, tight clusters
   - VOLATILE: rapid state changes → moderate separation, larger spread

3. **Loop #7 (Adaptive Heartrate)**: The VM's adaptive heartbeat system may create attractor points in phase space, leading to clustering

## Recommendations

1. **Further investigate TRANSITION workload**:
   - What specific state transitions create the 260k ns separation?
   - Are there exactly two distinct heartbeat regimes, or is k=2 an oversimplification?

2. **Compare with higher k values**:
   - Try k=3 or k=4 clustering to see if sub-structures emerge
   - Use silhouette scores or elbow method to find optimal k

3. **Time-series analysis**:
   - Plot HR(t) and ΔHR(t) for individual runs to see transition dynamics
   - Compute autocorrelation to detect periodic behavior

4. **Levene's test validation**:
   - The VM uses Levene's test for window width inference (Loop #5)
   - Compare cluster variance homogeneity across workloads

## Files Generated

- `binary_2cycle_analysis.svg` - 6-panel visualization (1800×1200 px)
- `analyze_2cycle.py` - Pure Python analysis script (no external dependencies)
- `binary_2cycle_analysis_report.md` - This report

## Conclusion

The binary clarity metric successfully quantifies the "binary 2-cycle exposure" across workloads:

> **"TRANSITION has the clearest exposed 2-cycle; the others share it but are more tightly wrapped by noise."**

However, the results flip the expectation: **TRANSITION** (not STABLE) shows the most distinct binary structure. This suggests that **state transitions themselves** create the clearest phase-space separation, while **stable operation** leads to more homogeneous (though still 2-clustered) behavior.

This is a valuable insight for understanding the physics-driven adaptive runtime: the system's response to workload transitions is more pronounced than its steady-state behavior.

---

*Analysis performed with pure Python implementation (no sklearn/numpy dependencies required).*