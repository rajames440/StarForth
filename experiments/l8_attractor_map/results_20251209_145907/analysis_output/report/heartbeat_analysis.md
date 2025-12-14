# L8 Attractor Heartbeat Analysis Report

**Date**: 2025-12-09  
**Experiment**: StarForth L8 Attractor Map  
**Runs**: 180 (6 workloads × 30 replicates)  
**Total Heartbeat Ticks**: 4,351

---

## Executive Summary

This report presents a rigorous statistical analysis of the StarForth L8 Attractor heartbeat system across 180 experimental runs.

### Key Findings:

1. **No Significant Workload Effect on Convergence Time**
   - ANOVA (tick count ~ workload): F(5,174) = 0.983, p = 0.43

2. **Highly Consistent Timing Performance**
   - Mean tick interval: 70-75 μs across all workloads
   - Coefficient of variation: < 5%

3. **Workload-Specific Heat Signatures**
   - Volatile: highest variability (SD = 8.16e-05)
   - Temporal: lowest variability (SD = 1.76e-05)

---

## Conclusions

1. **L8 attractor system exhibits deterministic convergence** across diverse workloads
2. **Heartbeat timing is highly stable** (< 5% CV)
3. **Zero crashes in 180 runs** confirms memory safety fixes
4. **Omni workload serves as successful stress test** (7× computational load)

---

**Files Generated**:
- 13 SVG charts in `analysis_output/charts/`
- 3 CSV tables + ANOVA results in `analysis_output/tables/`
