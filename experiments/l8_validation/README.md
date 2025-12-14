# L8 Jacquard Mode Selector Validation Experiment

## Overview

Validates the L8 Jacquard mode selector by comparing dynamic adaptive mode switching against static optimal configurations across diverse workload types.

## Experimental Design

**Type:** 8×5 Factorial Design
**Independent Variables:**
- **Control Strategy** (8 levels): L8_ADAPTIVE + 7 static configs
- **Workload Type** (5 levels): STABLE, DIVERSE, VOLATILE, TEMPORAL, TRANSITION

**Dependent Variables:**
- Primary: `ns_per_word`, `cv` (coefficient of variation)
- Secondary: Mode distribution (L8 only), mode switches, convergence time

**Total Runs:** 8 strategies × 5 workloads × N reps

## Control Strategies

| Strategy | L1 | L2 | L3 | L4 | L5 | L6 | L7 | Description |
|----------|----|----|----|----|----|----|----|----|
| **L8_ADAPTIVE** | 0 | runtime | runtime | 0 | runtime | runtime | 1 | Dynamic mode switching |
| **C0_BASELINE** | 0 | 0 | 0 | 0 | 0 | 0 | 1 | Minimal (baseline) |
| **C4_TEMPORAL** | 0 | 0 | 1 | 0 | 0 | 0 | 1 | Decay only (DoE rank #6) |
| **C7_FULL_INF** | 0 | 0 | 1 | 0 | 1 | 1 | 1 | Full inference (DoE ranks #2,#3) |
| **C9_DIVERSE_DECAY** | 0 | 1 | 0 | 0 | 0 | 1 | 1 | Window + decay_inf (DoE rank #5) |
| **C11_DIVERSE_INF** | 0 | 1 | 0 | 0 | 1 | 1 | 1 | Window + inference (DoE rank #4) |
| **C12_DIVERSE_TEMPORAL** | 0 | 1 | 1 | 0 | 0 | 0 | 1 | Window + decay (DoE rank #1) |
| **ALL_ON** | 0 | 1 | 1 | 0 | 1 | 1 | 1 | Everything except L1/L4 |

## Workload Types

| Workload | Characteristics | Expected L8 Mode |
|----------|----------------|------------------|
| **STABLE** | Predictable, repetitive (Fibonacci, factorial) | C0 or C4 |
| **DIVERSE** | High entropy (mixed ops, string ops, stack churn) | C9, C11, C12 |
| **VOLATILE** | High CV (random branching, nested conditionals) | C1, C7 |
| **TEMPORAL** | Strong locality (nested loops, hot words) | C4, C12 |
| **TRANSITION** | Phase shifts (STABLE→DIVERSE→VOLATILE) | Adaptive |

## Hypotheses

**H1 (Performance):** L8_ADAPTIVE matches or exceeds best static config per workload (≤5% margin)

**H2 (Stability):** L8_ADAPTIVE shows lower overall CV than any single static config

**H3 (Adaptation):** L8 mode distribution correlates with workload characteristics

**H4 (Generalization):** L8_ADAPTIVE outperforms static configs on TRANSITION workload

## Usage

```bash
# Quick test (10 reps = 400 runs, ~8 minutes)
cd experiments/l8_validation
./run_l8_validation.sh 10

# Standard validation (50 reps = 2,000 runs, ~40 minutes)
./run_l8_validation.sh 50

# High precision (100 reps = 4,000 runs, ~80 minutes)
./run_l8_validation.sh 100
```

## Output

Results directory: `l8_validation_YYYYMMDD_HHMMSS/`
- `l8_validation_results.csv` - Raw experimental data
- `conditions_raw.txt` - Ordered conditions
- `conditions_randomized.txt` - Randomized run order
- `init_original.4th.backup` - Backup of original init file

## Analysis

```bash
# Run R analysis (after experiment completes)
cd experiments/l8_validation
Rscript analyze_l8.R l8_validation_YYYYMMDD_HHMMSS
```

Expected plots:
- ANOVA interaction plot (strategy × workload)
- L8 mode distribution per workload
- Pareto frontier (speed vs stability)
- Convergence curves (L8 mode switches)

## Data-Driven Design

This experiment is grounded in the 2^7 DoE results (300 reps, 38,400 runs):
- **L1/L4 disabled:** Harmful in 86%/100% of top 5% configs
- **L7 enabled:** Beneficial in 71% of top 5% configs
- **L2/L3/L5/L6 contextual:** Static optimal configs vary by workload
- **L8 hypothesis:** Dynamic switching should match workload-specific optimal configs

## Success Criteria

1. L8 ≤ 5% slower than best static per workload
2. L8 shows lowest CV across all workloads
3. L8 modes align with expected patterns
4. L8 dominates on TRANSITION workload