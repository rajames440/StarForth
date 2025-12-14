# Experiments

This directory contains experimental designs, execution guides, and results for physics-based experiments on the StarForth VM.

## Experiment Types

- **[factorial-doe/](factorial-doe/)** - Factorial design of experiments for configuration analysis
- **[heartbeat-doe/](heartbeat-doe/)** - Heartbeat subsystem performance experiments
- **[james-law/](james-law/)** - James Law window scaling validation experiments
- **[physics-optimization/](physics-optimization/)** - Physics engine optimization experiments

## Experiment Lifecycle

1. **Design** - Define hypothesis, factors, response variables
2. **Protocol** - Write execution guide (document methodology)
3. **Execute** - Run experiments, collect data
4. **Analyze** - Statistical analysis, validate results
5. **Document** - Write summary, move to `06-research/` if publishing

## Key Results

**Deterministic Behavior:** StarForth achieves **0% algorithmic variance** across 90 experimental runs, demonstrating formally proven deterministic behavior of the physics-driven adaptive runtime.

## Design of Experiments Methodology

StarForth uses rigorous experimental methodology:
- Factorial designs for multi-factor analysis
- Heartbeat-driven data collection
- Statistical validation (ANOVA, Levene's test)
- Reproducible experimental protocols

## See Also

- Design of Experiments schema: `../06-research/doe-metrics-schema.md`
- Results for publication: `../06-research/results-for-publication.md`
- Quality validation: `../04-quality/validation/`