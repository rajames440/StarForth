# Optimization Regression Detection Framework

**Status:** DESIGN (for OPP #2 and beyond)
**Date:** 2025-11-19
**Purpose:** Ensure optimization ≠ performance, detect regressions early

---

## Core Principle

**OPTIMIZATION** = Best trade-off across ALL metrics
**PERFORMANCE** = Fastest execution time (one-dimensional)

These diverge when optimizations improve one metric while regressing others.

---

## Types of Optimization Regressions

### Regression Type 1: Metric Inversion
```
Optimization wins on PRIMARY metric (time)
But loses on SECONDARY metric (memory, variance, stability)
```

**Example:**
- Decay slope 0.2: 5.7% faster execution
- But: Cache size grows 15% due to extended retention
- Net: Not optimal (traded speed for memory)

**Detection:**
```python
if exec_time[new] < exec_time[old]:
    if cache_size[new] > cache_size[old] * 1.10:  # 10% regression
        return "REGRESSION: Speed gain canceled by memory cost"
```

### Regression Type 2: Variance Introduction
```
Average time improves
But execution variance increases
Unpredictability = Not optimal
```

**Example:**
- Window width 4096: 2% faster average
- But: Standard deviation increases from 1.2M ns to 2.8M ns
- Net: Not optimal (unpredictable = unreliable)

**Detection:**
```python
if exec_time_mean[new] < exec_time_mean[old]:
    if exec_time_std[new] > exec_time_std[old] * 1.25:  # 25% variance increase
        return "REGRESSION: Variance introduced"
```

### Regression Type 3: Tail Latency
```
Mean time improves
But p99 latency worsens
Real-world impact negative
```

**Example:**
- Cache threshold = 5: 3% faster on average
- But: p99 latency 50% worse (more cache thrashing)
- Net: Not optimal (unpredictable spikes)

**Detection:**
```python
if exec_time_p50[new] < exec_time_p50[old]:
    if exec_time_p99[new] > exec_time_p99[old] * 1.15:  # 15% worse
        return "REGRESSION: Tail latency degrades"
```

### Regression Type 4: Cascading Failure
```
Direct metric improves
But system-level behavior degrades
Example: Dictionary growth, memory fragmentation, resource leaks
```

**Example:**
- Decay slope 0.2: 5.7% faster
- But: Memory fragmentation increases over long runs
- After 1M instructions: GC overhead dominates
- Net: Not optimal (breaks for long-running processes)

**Detection:**
```python
def detect_cascading_failure(workload_size):
    small_run = measure(100k_instructions)
    large_run = measure(1M_instructions)

    if small_run[new] < small_run[old]:  # Wins on small workload
        if large_run[new] > large_run[old]:  # Loses on large workload
            return "REGRESSION: Cascading failure"
```

### Regression Type 5: Workload Sensitivity
```
Optimization wins on training workload
But loses on different workload pattern
Overfitting = Not optimal
```

**Example:**
- Decay slope optimized for 936 FORTH tests
- But: Performs poorly on long-running VM (heat decay is different)
- Net: Not optimal (only works for specific workload)

**Detection:**
```python
# Run on multiple workload patterns
workloads = [
    "936_forth_tests",    # Training workload
    "long_vm_session",    # Different pattern
    "random_word_sequence",  # Stress test
    "real_world_forth_code"  # User workload
]

for wl in workloads:
    result = measure_optimization(wl)
    if result[new] > result[old]:  # Regresses on any workload
        return f"REGRESSION: Fails on {wl}"
```

### Regression Type 6: Parameter Interaction
```
Parameter A wins independently
Parameter B wins independently
But A + B together regress
Interaction effect = Not optimal
```

**Example:**
- Decay slope 0.2 wins (OPP #1)
- Window width 512 wins (OPP #2)
- But together (0.2 + 512): Cache thrashing occurs
- Net: Not optimal (parameters interact negatively)

**Detection:**
```python
# Test all combinations, not just winners
for decay in [0.2, 0.33, 0.5, 0.7]:
    for window in [256, 512, 1024, 4096]:
        result = measure(decay, window)

        # Check if winning combo actually wins when combined
        if decay == 0.2 and window == 512:
            if result[new] > baseline:
                return "REGRESSION: Interaction effect"
```

---

## Multi-Metric Regression Detection Matrix

For each OPP experiment, create a matrix:

```
METRIC                  | OPP #1 | OPP #2 | OPP #3 | OPP #4 | OPP #5 | Notes
------------------------+--------+--------+--------+--------+--------+-------
Execution Time (mean)   | ✅ -5.7%| TBD    | TBD    | TBD    | TBD    | Primary
Execution Time (std)    | ✅ OK  | TBD    | TBD    | TBD    | TBD    | Watch for variance
Execution Time (p99)    | TBD    | TBD    | TBD    | TBD    | TBD    | Tail latency
Cache Hit Rate          | ✅ 25% | TBD    | TBD    | TBD    | TBD    | Must not regress
Dict Memory Size        | STABLE | TBD    | TBD    | TBD    | TBD    | Watch growth
Cache Size              | ✅ OK  | TBD    | TBD    | TBD    | TBD    | Cascading effect
Prefetch Accuracy       | ✅ 88% | TBD    | TBD    | TBD    | TBD    | Secondary
Heat Distribution       | TBD    | TBD    | TBD    | TBD    | TBD    | Diagnostics
Test Variance (across runs) | TBD | TBD  | TBD    | TBD    | TBD    | Determinism
Error Rate              | ✅ 0%  | TBD    | TBD    | TBD    | TBD    | Critical
Resource Leaks          | TBD    | TBD    | TBD    | TBD    | TBD    | Long-term
```

**Interpretation:**
- ✅ = No regression detected
- TBD = Need to measure
- RED = Regression detected (investigate)
- ORANGE = Threshold warning (borderline)

---

## Regression Detection Algorithm (Automated)

```python
class OptimizationRegression:
    """Detect regressions across all metrics"""

    THRESHOLDS = {
        "time_regression": 1.05,      # 5% slower = FAIL
        "variance_increase": 1.25,    # 25% more variance = WARN
        "p99_regression": 1.15,       # 15% worse p99 = WARN
        "cache_hit_drop": 0.95,       # 5% cache hit drop = FAIL
        "memory_growth": 1.10,        # 10% memory growth = WARN
    }

    def detect_regression(self, baseline, optimized, config_name):
        """
        Args:
            baseline: Metrics from previous optimization
            optimized: Metrics from new optimization
            config_name: Name of configuration (e.g., "DECAY_SLOPE_0.2")

        Returns:
            List of detected regressions with severity
        """
        regressions = []

        # Check primary metric
        if optimized["exec_time_mean"] > baseline["exec_time_mean"]:
            # Optimization didn't improve primary metric
            regressions.append({
                "severity": "CRITICAL",
                "metric": "execution_time",
                "reason": "Primary metric worse",
                "delta": (optimized["exec_time_mean"] / baseline["exec_time_mean"] - 1) * 100
            })

        # Check variance introduction
        var_ratio = optimized["exec_time_std"] / baseline["exec_time_std"]
        if var_ratio > self.THRESHOLDS["variance_increase"]:
            regressions.append({
                "severity": "WARNING",
                "metric": "variance",
                "reason": "Variance increased",
                "delta": (var_ratio - 1) * 100
            })

        # Check tail latency
        p99_ratio = optimized["exec_time_p99"] / baseline["exec_time_p99"]
        if p99_ratio > self.THRESHOLDS["p99_regression"]:
            regressions.append({
                "severity": "WARNING",
                "metric": "tail_latency",
                "reason": "P99 latency worse",
                "delta": (p99_ratio - 1) * 100
            })

        # Check cache hit rate
        cache_ratio = optimized["cache_hit_rate"] / baseline["cache_hit_rate"]
        if cache_ratio < 1.0 - 0.05:  # 5% drop
            regressions.append({
                "severity": "CRITICAL",
                "metric": "cache_hit_rate",
                "reason": "Cache hit rate dropped",
                "delta": (1 - cache_ratio) * 100
            })

        # Check memory growth
        mem_ratio = optimized["memory_usage"] / baseline["memory_usage"]
        if mem_ratio > self.THRESHOLDS["memory_growth"]:
            regressions.append({
                "severity": "WARNING",
                "metric": "memory_usage",
                "reason": "Memory usage grew",
                "delta": (mem_ratio - 1) * 100
            })

        return regressions

    def evaluate_optimization(self, regressions):
        """
        Decision rule: Is this optimization VALID?

        Returns:
            "ACCEPT": Safe to deploy
            "INVESTIGATE": Regressions found, needs analysis
            "REJECT": Critical regressions, don't deploy
        """
        critical_count = len([r for r in regressions if r["severity"] == "CRITICAL"])
        warning_count = len([r for r in regressions if r["severity"] == "WARNING"])

        if critical_count > 0:
            return "REJECT"
        elif warning_count > 2:
            return "INVESTIGATE"
        else:
            return "ACCEPT"
```

---

## OPP #2 Regression Detection Plan

### Primary Metric
- **Execution Time**: Must improve or stay same
  - Measure: mean, std, min, max, p50, p95, p99
  - Regression threshold: > 1.0x baseline (any increase = FAIL)

### Secondary Metrics (Must Not Regress)
- **Cache Hit Rate**: Must stay ≥ 25%
  - Regression threshold: < 95% of baseline

- **Dictionary Memory**: Must stay ≤ baseline
  - Regression threshold: > 10% growth

- **Variance**: Should decrease or stay same
  - Regression threshold: > 125% of baseline std

### Tertiary Metrics (Stability Checks)
- **Test Suite Stability**: All 789 tests must pass
  - Regression: Any flakes or failures

- **Heat Distribution**: Should be consistent
  - Regression: Mode/median changes > 10%

- **Prefetch Accuracy**: Should stay ≥ 88%
  - Regression: < 85% accuracy

### Diagnostic Metrics (Understanding)
- **Levene's Test Statistic**: Track window width stability
  - Lower = more stable variance (good)
  - Higher = variance differs (bad for that window size)

- **Slope Fit Quality**: Should be consistent
  - Regression: Fits degrade significantly

---

## OPP #2 Regression Report Template

```markdown
# OPP #2 Regression Analysis Report

## Summary
- Configuration Tested: ROLLING_WINDOW_SIZE = [256, 512, 1024, 4096]
- Baseline: Decay Slope 0.2 (from OPP #1)
- Date: [date]

## Primary Metric: Execution Time
| Window Size | Mean Time | Std Dev | Min | Max | p99 | vs Baseline |
|---|---|---|---|---|---|---|
| Baseline (OPP #1) | 5,947,974 | 1.2M | 4.5M | 12.8M | 8.2M | 0% |
| 256 | TBD | TBD | TBD | TBD | TBD | TBD |
| 512 | TBD | TBD | TBD | TBD | TBD | TBD |
| 1024 | TBD | TBD | TBD | TBD | TBD | TBD |
| 4096 | TBD | TBD | TBD | TBD | TBD | TBD |

**Regression Analysis:**
- [ ] Primary metric improved or maintained
- [ ] Variance increased? (watch out)
- [ ] Tail latency worse? (watch out)

## Secondary Metrics
| Window Size | Cache Hit % | Dict Mem | Prefetch % | Deterministic? |
|---|---|---|---|---|
| Baseline | 25.06% | Xkb | 88.4% | ✅ |
| 256 | TBD | TBD | TBD | TBD |
| 512 | TBD | TBD | TBD | TBD |
| 1024 | TBD | TBD | TBD | TBD |
| 4096 | TBD | TBD | TBD | TBD |

**Regression Detected:**
- [ ] Cache hit rate regressed?
- [ ] Memory usage exploded?
- [ ] Prefetch accuracy dropped?
- [ ] Non-deterministic behavior?

## Diagnostics (Levene's Test)
| Window Size | Levene Stat | # Chunks | Test Passed? | Notes |
|---|---|---|---|---|
| 256 | TBD | TBD | TBD | Should pass statistically |
| 512 | TBD | TBD | TBD | Should pass statistically |
| 1024 | TBD | TBD | TBD | Should pass statistically |
| 4096 | TBD | TBD | TBD | Should pass statistically |

## Parameter Interaction Effects
- [ ] Does window width 256 still work with decay 0.2?
- [ ] Does window width 4096 cause cache bloat?
- [ ] Is OPP #1 result still reproducible?

## Decision
**Regression Status:** [ ] ACCEPT [ ] INVESTIGATE [ ] REJECT

**Justification:**
[Explain why metric was chosen as optimal, not just fastest]

**Recommendation for OPP #3:**
[Lock which parameter, why, expected improvement]
```

---

## Going Forward: Regression Detection Checklist

For **EVERY optimization opportunity** (OPP #2-5):

### Pre-Experiment
- [ ] Define primary metric (execution time)
- [ ] Define secondary metrics (cache hit, memory, stability)
- [ ] Define regression thresholds (>5% = fail, >25% = warn)
- [ ] Identify potential interaction effects
- [ ] Plan measurement methodology (mean, std, p99, etc.)

### During Experiment
- [ ] Collect all metrics for all configurations
- [ ] Run multiple samples (≥30 per config)
- [ ] Measure variance explicitly
- [ ] Log heat distribution, prefetch stats, diagnostics

### Post-Experiment
- [ ] Run regression detection algorithm
- [ ] Identify all regressions (CRITICAL, WARNING, OK)
- [ ] Investigate borderline cases
- [ ] Check for parameter interactions
- [ ] Validate on different workloads (if possible)

### Before Locking Parameter
- [ ] No CRITICAL regressions
- [ ] Understand all WARNINGS
- [ ] Reproduce OPP #1 result with new settings
- [ ] Document trade-offs in CLAUDE.md
- [ ] Get sign-off that this is OPTIMAL, not just FAST

---

## Key Questions for Optimization Validation

Before accepting any optimization result:

1. **Is it OPTIMAL or just FASTEST?**
   - Does it win on all metrics, or just execution time?
   - Are there trade-offs?

2. **Is it STABLE?**
   - Does variance increase?
   - Is it deterministic?

3. **Is it GENERAL?**
   - Works for different workloads?
   - Or only for test suite?

4. **Does it INTERACT?**
   - With previous optimizations?
   - With system parameters?

5. **Is it MAINTAINABLE?**
   - Can we explain why it's optimal?
   - Can we document trade-offs?

**If you can answer YES to all 5 → It's a TRUE optimization.**
**If you answer NO to any → It's probably just a fast-path, not optimal.**

---

**Framework Status:** READY FOR OPP #2
**Next Step:** Implement regression detection in experiment runner
**Target:** Identify first optimization regression and document handling

