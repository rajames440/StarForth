# 2^7 Factorial Design of Experiments (DoE)

## Overview

Comprehensive Design of Experiments (DoE) testing all 128 combinations of StarForth's 7 adaptive feedback loops (L1-L7) to identify optimal configurations and interaction effects.

**Experimental Space**: 2^7 = 128 configurations
**Recommended Replicates**: 300 runs per configuration
**Total Runs**: 38,400 (128 × 300)
**Duration**: ~6-8 hours on modern AMD64 hardware
**Purpose**: Identify which feedback loops are beneficial, harmful, or workload-dependent

---

## Quick Start

### Running the Full Experiment

```bash
# Navigate to experiment directory
cd experiments/doe_2x7

# Generate run matrix (creates all 128 configurations)
./generate_run_matrix.sh

# Run full DoE with 300 replicates
./run_doe.sh 300

# Results saved to: results_300_reps_<USER>_<TIMESTAMP>/
```

### Running with a Specific Git Commit

To ensure reproducibility, checkout the commit just before the L8 integration:

```bash
# Checkout commit before L8 final integration
git checkout 161a3667

# Build StarForth
make clean && make

# Run DoE
cd experiments/doe_2x7
./run_doe.sh 300

# Return to latest version when done
git checkout l8-final-integration
```

**Recommended Commit Points**:
- `161a3667` - Last commit with experimental loop flags (DoE compatible)
- `24b8fcd0` - Earlier DoE commit with all 7 loops functional

### Quick Test Run

For testing the experimental framework without waiting 8 hours:

```bash
# Run with only 10 replicates (~15 minutes)
./run_doe.sh 10

# Or test just the top 5% configurations from previous run
./run_top5_runoff.sh 30
```

---

## Experimental Design

### Independent Variables (Factors)

Seven binary factors (L1-L7), each with two levels (0=disabled, 1=enabled):

| Loop | Factor Name | Description | Hypothesis |
|------|-------------|-------------|------------|
| L1 | `HEAT_TRACKING` | Execution frequency tracking | May cause cache thrashing |
| L2 | `ROLLING_WINDOW` | Execution history buffer | Enables pattern detection |
| L3 | `LINEAR_DECAY` | Heat dissipation over time | Prevents stale data accumulation |
| L4 | `PIPELINING_METRICS` | Word transition prediction | May add overhead |
| L5 | `WINDOW_INFERENCE` | Adaptive window sizing (Levene's test) | Optimizes L2 window size |
| L6 | `DECAY_INFERENCE` | Exponential regression on heat | Optimizes L3 decay slope |
| L7 | `ADAPTIVE_HEARTRATE` | Dynamic tick frequency | Reduces overhead when stable |

### Dependent Variables (Responses)

Primary metrics collected per run:

- `ns_per_word` - Mean execution time per FORTH word (primary performance metric)
- `cv` - Coefficient of variation (stability/predictability metric)
- `window_width` - Effective rolling window size (L2 output)
- `decay_slope_q48` - Heat decay rate in Q48.16 fixed-point (L3 output)
- `total_heat` - Cumulative execution heat across dictionary
- `hot_word_count` - Number of frequently-executed words
- `prefetch_hits` - Successful word transition predictions (L4 output)

### Configuration Encoding

Each configuration is encoded as a 7-bit binary number:

```
Bit Position: 6  5  4  3  2  1  0
Loop:         L7 L6 L5 L4 L3 L2 L1

Example: 0110001
         ↑↑↑↑↑↑↑
         ||||+++-- L1=1 (heat tracking ON)
         |||+----- L2=0 (rolling window OFF)
         ||+------ L3=0 (decay OFF)
         |+------- L4=0 (pipelining OFF)
         +-------- L5=1 (window inference ON)
          +------- L6=1 (decay inference ON)
           +------ L7=0 (adaptive heartrate OFF)

Config ID: C97 (binary 1100001 = decimal 97)
```

---

## Files and Scripts

### Core Scripts

#### `generate_run_matrix.sh`
Generates the run matrix file listing all 128 configurations.

**Usage**:
```bash
./generate_run_matrix.sh
```

**Output**: `run_matrix_<TIMESTAMP>.csv`

**Format**:
```csv
config_id,L1,L2,L3,L4,L5,L6,L7
0,0,0,0,0,0,0,0
1,1,0,0,0,0,0,0
2,0,1,0,0,0,0,0
...
127,1,1,1,1,1,1,1
```

#### `run_doe.sh`
Main experiment runner. Executes all 128 configurations with N replicates each.

**Usage**:
```bash
./run_doe.sh <NUM_REPLICATES> [--shuffle]
```

**Parameters**:
- `NUM_REPLICATES` - Number of repetitions per configuration (recommended: 300)
- `--shuffle` - Optional flag to randomize run order (reduces systematic bias)

**Example**:
```bash
# Standard run (300 reps, sequential order)
./run_doe.sh 300

# Randomized run order (reduces time-of-day effects)
./run_doe.sh 300 --shuffle
```

**Process**:
1. Creates timestamped results directory
2. Backs up current `conf/init.4th`
3. For each config × replicate:
   - Generates build flags (`-DENABLE_LOOP_X=0/1`)
   - Rebuilds StarForth with specific configuration
   - Runs benchmark workload via `--doe` flag
   - Captures metrics to CSV
4. Restores original init file

**Runtime**: ~20 seconds per run × 38,400 runs = ~213 hours sequential
**Actual**: ~6-8 hours with build caching and optimizations

#### `run_top5_runoff.sh`
Re-runs the elite configurations (top 5% from previous DoE) with higher precision.

**Usage**:
```bash
./run_top5_runoff.sh <NUM_REPLICATES>
```

**Purpose**: Validate top performers with more replicates to reduce noise

**Prerequisite**: Requires `optimal_configurations_ranked.csv` from previous DoE run

---

## Results Structure

Each DoE run creates a timestamped results directory:

```
results_<REPS>_<USER>_<TIMESTAMP>/
├── doe_results_<TIMESTAMP>.csv          # Raw experimental data
├── run_matrix_<TIMESTAMP>.csv           # Configuration matrix used
├── conditions_raw.txt                   # Ordered run list
├── conditions_randomized.txt            # Shuffled run list (if --shuffle used)
├── init_original.4th.backup             # Backup of original init file
│
├── all_128_configurations.csv           # Summary stats per config
├── optimal_configurations_ranked.csv    # Top performers ranked
├── top5pct_vs_baseline.csv             # Elite vs baseline comparison
├── interaction_anova_results.csv        # Statistical significance tests
│
├── doe_full_report.R                    # R analysis script
├── doe_full_report.html                 # Generated HTML report
│
└── [56 visualization plots]
    ├── box_L1_heat_tracking.png         # Per-loop distributions
    ├── facet_L1_heat_tracking.png       # Loop interaction effects
    ├── interaction_01_L1_x_L2.png       # Pairwise interactions (21 plots)
    ├── config_heatmap_128.png           # Performance heatmap
    ├── pareto_speed_stability.png       # Pareto frontier
    └── ...
```

### Key Output Files

#### `doe_results_<TIMESTAMP>.csv`
Raw experimental data with one row per run.

**Columns**:
```
run_id, config_id, rep, L1, L2, L3, L4, L5, L6, L7,
ns_per_word, cv, window_width, decay_slope_q48,
total_heat, hot_word_count, prefetch_hits, ...
```

#### `all_128_configurations.csv`
Summary statistics for each configuration (mean, median, SD, CV across replicates).

**Usage**: Identify best/worst configurations

#### `optimal_configurations_ranked.csv`
Top 5% configurations ranked by performance.

**Format**:
```csv
rank,config_id,binary,mean_ns_per_word,cv,loops_enabled
1,97,0110001,123.45,0.023,"L1,L5,L6"
2,7,0000111,124.12,0.025,"L3,L5,L6"
...
```

**Usage**: Feed into `run_top5_runoff.sh` for validation

---

## Analysis

### R Analysis Suite

The experiment auto-generates an R analysis script with comprehensive statistical tests:

```bash
cd results_300_reps_*/
Rscript doe_full_report.R
```

**Generated Outputs**:
- ANOVA tables (main effects + interactions)
- Effect size calculations (Cohen's d, η²)
- Interaction plots (all 21 pairwise combinations)
- Distribution plots (optimality curves)
- Pareto frontiers (speed vs stability)

**Requirements**:
- R ≥ 4.0
- Packages: `ggplot2`, `dplyr`, `tidyr`, `gridExtra`

**Installation**:
```r
install.packages(c("ggplot2", "dplyr", "tidyr", "gridExtra"))
```

### Key Findings (300 reps, Nov 26 2025)

**Loop Effectiveness** (Top 5% configurations):

| Loop | Effect | Top 5% Prevalence | Recommendation |
|------|--------|-------------------|----------------|
| L1 (Heat) | Harmful | 14% enable (86% disable) | **Disable by default** |
| L2 (Window) | Workload-dependent | 57% enable | L8-controlled |
| L3 (Decay) | Beneficial | 57% enable | L8-controlled |
| L4 (Pipeline) | Harmful | 0% enable (100% disable) | **Disable by default** |
| L5 (Window Inf) | Beneficial | 43% enable | L8-controlled |
| L6 (Decay Inf) | Workload-dependent | 57% enable | L8-controlled |
| L7 (Heartrate) | Beneficial | 71% enable | **Always-on** |

**Top 5 Configurations**:

| Rank | Config | Binary | Description | Performance |
|------|--------|--------|-------------|-------------|
| 1 | C97 (0110001) | L1+L5+L6 | Temporal+diverse | Best |
| 2 | C7 (0000111) | L3+L5+L6 | Full inference | Elite |
| 3 | C75 (0100011) | L2+L6 | Diverse+decay | Elite |
| 4 | C70 (0100110) | L2+L5+L6 | Diverse+inference | Elite |
| 5 | C1 (0000001) | L1 only | Minimal | Elite |

**Critical Insight**: No single configuration optimal across all workload types → justifies L8 dynamic mode selector

---

## Creating Your Own Experiment

### Method 1: Modify Existing DoE

1. **Checkout DoE-compatible commit**:
   ```bash
   git checkout 161a3667
   ```

2. **Edit loop configuration in Makefile**:
   ```make
   # Line ~229 in Makefile
   -DENABLE_LOOP_1_HEAT_TRACKING=0 \
   -DENABLE_LOOP_2_ROLLING_WINDOW=1 \
   # ... modify values to 0 or 1
   ```

3. **Run subset of configurations**:
   ```bash
   # Edit run_doe.sh to test specific configs
   configs_to_test="0 1 7 97 127"  # Baseline, L1-only, elite configs

   for config_id in $configs_to_test; do
       # Extract loop values from config_id
       # Build and run...
   done
   ```

### Method 2: Create Custom Workload

1. **Create custom init file**:
   ```bash
   cp conf/init-4.4th conf/init-myworkload.4th
   ```

2. **Edit workload** (FORTH code):
   ```forth
   Block 3001

   ( My Custom Workload )
   : MY-TEST
       1000 0 DO
           I DUP * DROP
       LOOP ;

   MY-TEST
   CR
   ```

3. **Run with custom init**:
   ```bash
   # Modify run_doe.sh line ~45:
   cp conf/init-myworkload.4th conf/init-4.4th
   ```

4. **Run experiment**:
   ```bash
   ./run_doe.sh 100  # 100 reps × 128 configs
   ```

### Method 3: Ablation Study (Single Loop)

Test the effect of one loop in isolation:

```bash
#!/bin/bash
# test_single_loop.sh - Ablation study for L3 (decay)

REPS=100
LOOP_TO_TEST=3

echo "Testing L3 (Linear Decay) ablation..."

# Test with loop OFF
make clean
make ENABLE_LOOP_${LOOP_TO_TEST}_LINEAR_DECAY=0
./build/amd64/standard/starforth --doe > results_L${LOOP_TO_TEST}_off.txt

# Test with loop ON
make clean
make ENABLE_LOOP_${LOOP_TO_TEST}_LINEAR_DECAY=1
./build/amd64/standard/starforth --doe > results_L${LOOP_TO_TEST}_on.txt

# Compare results_run_01_2025_12_08
echo "OFF: $(grep ns_per_word results_L${LOOP_TO_TEST}_off.txt)"
echo "ON:  $(grep ns_per_word results_L${LOOP_TO_TEST}_on.txt)"
```

---

## Troubleshooting

### Build Fails with "ENABLE_LOOP_X undefined"

**Problem**: You're on a commit after L8 integration (post-`l8-final-integration` tag)

**Solution**:
```bash
git checkout 161a3667  # Last commit with experimental flags
```

### Experiment Runs Out of Disk Space

**Problem**: 38,400 runs × build artifacts = large disk usage

**Solution**:
```bash
# Clean build artifacts between configs
# Edit run_doe.sh, add after each build:
make clean
```

### Results Show High Variance

**Problem**: Replicates too low (< 100)

**Solution**:
```bash
# Increase replicates (diminishing returns after 300)
./run_doe.sh 300
```

### R Analysis Script Fails

**Problem**: Missing R packages

**Solution**:
```r
# Install required packages
install.packages(c("ggplot2", "dplyr", "tidyr", "gridExtra", "RColorBrewer"))
```

---

## Best Practices

### 1. Randomize Run Order
Always use `--shuffle` flag to reduce systematic bias (time-of-day effects, thermal throttling):

```bash
./run_doe.sh 300 --shuffle
```

### 2. Monitor System Load
DoE runs are CPU-intensive. Ensure clean system state:

```bash
# Close other applications
# Disable CPU frequency scaling
sudo cpupower frequency-set --governor performance

# Monitor during run
watch -n 5 'grep MHz /proc/cpuinfo | head -1'
```

### 3. Backup Results Immediately
DoE runs take hours. Protect your data:

```bash
# Compress and backup results_run_01_2025_12_08
tar -czf doe_results_backup_$(date +%Y%m%d).tar.gz results_300_reps_*/
```

### 4. Document Your Hypotheses
Create an `EXPERIMENT_NOTES.txt` file:

```bash
cat > EXPERIMENT_NOTES.txt <<EOF
Date: $(date)
Commit: $(git rev-parse HEAD)
Hypothesis: L2+L3 synergy improves performance
Workload: Standard DoE benchmark (Block 3001)
Expected: Config 0110000 (L2+L3) ranks in top 10%
EOF
```

---

## References

- **DoE Methodology**: Montgomery, D. C. (2017). *Design and Analysis of Experiments*
- **Factorial Design**: Box, G. E. P., Hunter, W. G., & Hunter, J. S. (2005). *Statistics for Experimenters*
- **ANOVA**: Fisher, R. A. (1925). *Statistical Methods for Research Workers*

---

## See Also

- [`experiments/l8_validation/`](../l8_validation/README.md) - L8 Jacquard mode selector validation
- [`experiments/shape_validation/`](../shape_validation/README.md) - Benchmark shape validation
- [`docs/WORK_LOG_2025-11-26.md`](../../docs/WORK_LOG_2025-11-26.md) - Complete development log

---

**Last Updated**: November 26, 2025
**Experimental Commit**: `161a3667`
**Analysis Commit**: `4028cb92` (l8-final-integration)