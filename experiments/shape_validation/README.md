# Workload Shape Validation Experiment

## Overview

Tests how a single StarForth configuration responds to different workload patterns ("shapes"). This experiment validates that the adaptive runtime's behavior changes appropriately based on workload characteristics rather than being configuration-locked.

**Configuration Tested**: C37 (binary `0100101`)
- L2 (Rolling Window): **ON**
- L5 (Window Inference): **ON**
- L7 (Adaptive Heartrate): **ON**
- All other loops: OFF

**Workload Shapes**: 4 distinct execution patterns
**Recommended Replicates**: 100-300 runs per workload
**Total Runs**: 4 × N replicates
**Duration**: ~10-30 minutes depending on replicates
**Purpose**: Validate that L2/L5/L7 adapt to workload dynamics

---

## Quick Start

### Running the Experiment

```bash
# Navigate to experiment directory
cd experiments/shape_validation

# Run with 100 replicates (recommended for validation)
./run_shapes.sh 100

# Results saved to: shape_run_<TIMESTAMP>/
```

### Running with a Specific Git Commit

To ensure reproducibility, checkout a commit with experimental loop flags:

```bash
# Checkout commit before L8 final integration
git checkout 161a3667

# Build StarForth
make clean && make

# Run shape validation
cd experiments/shape_validation
./run_shapes.sh 100

# Return to latest version
git checkout l8-final-integration
```

**Recommended Commit Points**:
- `161a3667` - Last commit with experimental loop flags (shape validation compatible)
- `24b8fcd0` - Earlier commit with all 7 loops functional

### Quick Test Run

```bash
# Quick test with 30 replicates (~5 minutes)
./run_shapes.sh 30
```

---

## Experimental Design

### Configuration Under Test: C37 (0100101)

This configuration enables adaptive loops that should respond to workload characteristics:

| Loop | State | Rationale |
|------|-------|-----------|
| L1 (Heat Tracking) | OFF | Avoid cache thrashing overhead |
| L2 (Rolling Window) | **ON** | Tracks execution patterns |
| L3 (Linear Decay) | OFF | No heat dissipation |
| L4 (Pipelining) | OFF | Avoid prediction overhead |
| L5 (Window Inference) | **ON** | Adapts window size to workload |
| L6 (Decay Inference) | OFF | No decay to infer |
| L7 (Adaptive Heartrate) | **ON** | Adjusts tick frequency |

**Why C37?**: This configuration has the minimal set of adaptive loops (L2/L5/L7) that should exhibit workload-dependent behavior without excessive overhead.

### Workload Shapes

Four distinct execution patterns designed to trigger different adaptive responses:

#### 1. **Baseline** (`init.4th`)
Standard DoE benchmark workload.

**Characteristics**:
- Mixed arithmetic and control flow
- Moderate complexity
- Predictable pattern

**Expected Behavior**:
- L5: Moderate window size (~1024-2048)
- L7: Stable heartbeat (~1ms ticks)

#### 2. **Square Wave** (`init-1.4th`)
Alternating between simple and complex operations.

**Characteristics**:
```forth
: SIMPLE   1 2 + DROP ;          ( Low complexity )
: COMPLEX  100 0 DO I DUP * DROP LOOP ;  ( High complexity )

1000 0 DO
    I 2 MOD IF COMPLEX ELSE SIMPLE THEN
LOOP
```

**Expected Behavior**:
- L5: Larger window to capture alternation pattern
- L7: More frequent heartbeats during pattern transition

#### 3. **Triangle Wave** (`init-2.4th`)
Gradually increasing then decreasing complexity.

**Characteristics**:
```forth
: RAMP-UP
    100 0 DO
        I 0 DO J K + DROP LOOP
    LOOP ;

: RAMP-DOWN
    100 0 DO
        100 I - 0 DO J K + DROP LOOP
    LOOP ;

RAMP-UP RAMP-DOWN
```

**Expected Behavior**:
- L5: Window size tracks complexity ramp
- L7: Heartbeat slows during stable ramps, accelerates at inflection

#### 4. **Damped Sine** (`init-3.4th`)
Oscillating complexity that decreases over time.

**Characteristics**:
```forth
VARIABLE AMPLITUDE 1000 AMPLITUDE !

: OSCILLATE
    100 0 DO
        AMPLITUDE @ I MOD 0 DO
            J K + DROP
        LOOP
        AMPLITUDE @ 10 - AMPLITUDE !  ( Damping )
    LOOP ;

OSCILLATE
```

**Expected Behavior**:
- L5: Window shrinks as amplitude decreases
- L7: Heartbeat rate increases as workload stabilizes

---

## Files and Scripts

### `run_shapes.sh`
Main experiment runner that tests all 4 workload shapes with a fixed configuration.

**Usage**:
```bash
./run_shapes.sh <NUM_REPLICATES>
```

**Parameters**:
- `NUM_REPLICATES` - Number of runs per workload (recommended: 100-300)

**Example**:
```bash
# Standard run (100 reps × 4 workloads = 400 runs)
./run_shapes.sh 100

# High precision (300 reps × 4 = 1200 runs)
./run_shapes.sh 300

# Quick exploratory (30 reps × 4 = 120 runs)
./run_shapes.sh 30
```

**Process**:
1. Verifies all workload files exist in `conf/`
2. Builds StarForth with configuration C37 (0100101)
3. For each workload × replicate:
   - Copies workload file to `conf/init.4th`
   - Runs StarForth with `--doe` flag
   - Captures metrics to CSV
4. Restores original init file
5. Generates randomized run order for bias reduction

**Runtime**: ~5 seconds per run × (4 workloads × N reps) = ~33 minutes for N=100

---

## Results Structure

Each run creates a timestamped results directory:

```
shape_run_<TIMESTAMP>/
├── shape_results.csv             # Raw experimental data
├── conditions_raw.txt            # Ordered run list
├── conditions_randomized.txt     # Shuffled run order
└── init_original.4th.backup      # Backup of original init file
```

### `shape_results.csv`

**Columns**:
```csv
run_id, workload, workload_label, rep,
config_id, config_bits, L1, L2, L3, L4, L5, L6, L7,
ns_per_word, cv,
window_width, decay_slope_q48,
total_heat, hot_word_count,
prefetch_hits, prefetch_attempts
```

**Sample Data**:
```csv
1,init.4th,baseline,1,37,0100101,0,1,0,0,1,0,1,125.3,0.021,1024,...
2,init-1.4th,square_wave,1,37,0100101,0,1,0,0,1,0,1,132.7,0.034,2048,...
3,init-2.4th,triangle,1,37,0100101,0,1,0,0,1,0,1,128.1,0.027,1536,...
4,init-3.4th,damped_sine,1,37,0100101,0,1,0,0,1,0,1,126.4,0.019,512,...
```

---

## Analysis

### R Analysis

Basic statistical comparison across workload shapes:

```r
# Load data
library(ggplot2)
library(dplyr)

data <- read.csv("shape_results.csv")

# Summary statistics per workload
summary_stats <- data %>%
  group_by(workload_label) %>%
  summarise(
    mean_ns = mean(ns_per_word),
    sd_ns = sd(ns_per_word),
    mean_window = mean(window_width),
    sd_window = sd(window_width),
    mean_cv = mean(cv)
  )

print(summary_stats)

# ANOVA: Does workload shape affect performance?
anova_model <- aov(ns_per_word ~ workload_label, data=data)
summary(anova_model)

# Plot: Performance by workload
ggplot(data, aes(x=workload_label, y=ns_per_word)) +
  geom_boxplot() +
  labs(title="Performance Across Workload Shapes",
       x="Workload Shape", y="ns per word") +
  theme_minimal()
ggsave("performance_by_shape.png", width=8, height=6)

# Plot: Adaptive window width by workload
ggplot(data, aes(x=workload_label, y=window_width)) +
  geom_boxplot() +
  labs(title="L5 Window Adaptation Across Workload Shapes",
       x="Workload Shape", y="Window Width (L5)") +
  theme_minimal()
ggsave("window_adaptation_by_shape.png", width=8, height=6)
```

### Expected Results

If L2/L5/L7 are adapting correctly:

| Workload | Expected ns/word | Expected Window Width | Expected CV |
|----------|------------------|----------------------|-------------|
| Baseline | Moderate (~125) | Medium (~1024-2048) | Low (~0.02) |
| Square Wave | Higher (~130+) | Large (~2048+) | Higher (~0.03+) |
| Triangle | Moderate (~128) | Medium-large (~1536) | Medium (~0.025) |
| Damped Sine | Lower (~122) | Shrinking (2048→512) | Low (~0.018) |

**Key Validation**: Window width should vary significantly across shapes (ANOVA p < 0.05), indicating L5 is responding to workload dynamics.

---

## Creating Custom Workload Shapes

### Method 1: Modify Existing Shape

1. **Copy existing workload**:
   ```bash
   cp ../../conf/init-1.4th ../../conf/init-my-shape.4th
   ```

2. **Edit the FORTH code**:
   ```forth
   Block 3001

   ( My Custom Shape: Exponential Growth )
   VARIABLE ITER 1 ITER !

   : GROW-WORK
       100 0 DO
           ITER @ 0 DO
               J K + DROP
           LOOP
           ITER @ 2 * ITER !  ( Double work each iteration )
       LOOP ;

   GROW-WORK
   CR
   ```

3. **Add to experiment** (edit `run_shapes.sh`):
   ```bash
   WORKLOADS=(
       "init.4th"
       "init-1.4th"
       "init-2.4th"
       "init-3.4th"
       "init-my-shape.4th"  # ← Add here
   )

   WORKLOAD_LABELS=(
       "baseline"
       "square_wave"
       "triangle"
       "damped_sine"
       "exponential_growth"  # ← Add here
   )
   ```

4. **Run experiment**:
   ```bash
   ./run_shapes.sh 100  # Now tests 5 workloads
   ```

### Method 2: Create Shape from Scratch

**Template**:
```forth
Block 3001

( Workload Shape: <NAME> )
( Characteristics: <DESCRIPTION> )

: MAIN-WORKLOAD
    ( Your FORTH code here )
    1000 0 DO
        I DUP * DROP
    LOOP ;

MAIN-WORKLOAD
CR
```

**Save as**: `conf/init-<shape-name>.4th`

### Shape Design Guidelines

**For High Entropy** (to trigger large L5 window):
```forth
: HIGH-ENTROPY
    1000 0 DO
        I 10 MOD CASE
            0 OF ( do work A ) ENDOF
            1 OF ( do work B ) ENDOF
            2 OF ( do work C ) ENDOF
            ...  ( 10 different branches )
        ENDCASE
    LOOP ;
```

**For Temporal Locality** (to trigger small L5 window):
```forth
: TEMPORAL-LOCALITY
    ( Repeat same sequence 1000 times )
    1000 0 DO
        1 2 + 3 4 + * DROP  ( Same pattern every iteration )
    LOOP ;
```

**For Volatility** (high CV):
```forth
VARIABLE RNG 12345 RNG !

: RAND RNG @ 1103515245 * 12345 + DUP RNG ! ;

: VOLATILE
    1000 0 DO
        RAND 1000 MOD 0 DO  ( Random work per iteration )
            J K + DROP
        LOOP
    LOOP ;
```

---

## Validation Criteria

### Success Criteria

The experiment succeeds if:

1. **L5 (Window Inference) Adapts**:
   - Window width significantly different across shapes (ANOVA p < 0.05)
   - Square wave → larger window than baseline
   - Damped sine → smaller window than baseline

2. **Performance Varies**:
   - ns/word significantly different across shapes (expected)
   - CV varies with workload volatility

3. **Stability Maintained**:
   - CV remains < 10% for all shapes
   - No mode flapping or instability

### Interpretation

**If window width doesn't vary**:
- L5 may not be working correctly
- Workload shapes may be too similar
- Increase replicate count for more statistical power

**If performance doesn't vary**:
- Workloads may have similar computational cost
- Design more extreme workload differences

---

## Testing Different Configurations

To test other configurations, edit `run_shapes.sh` lines 20-32:

```bash
# Example: Test C12 (0110001) instead
CONFIG_BITS="0110001"
CONFIG_DECIMAL=97

L1=1
L2=1
L3=0
L4=0
L5=0
L6=0
L7=1
```

Then run:
```bash
./run_shapes.sh 100
```

**Useful Configurations to Test**:
- `0000000` (C0): Baseline, no loops → should NOT adapt
- `0100101` (C37): L2+L5+L7 → should adapt (default)
- `0110001` (C97): L1+L5+L6 → DoE top config
- `1111110` (C126): All loops except L1 → maximum adaptation

---

## Troubleshooting

### Workload File Not Found

**Problem**: `ERROR: Workload file not found: conf/init-X.4th`

**Solution**:
```bash
# Verify files exist
ls -la ../../conf/init*.4th

# Create missing file
cp ../../conf/init-4.4th ../../conf/init-missing.4th
```

### Build Fails with "ENABLE_LOOP_X undefined"

**Problem**: You're on a commit after L8 integration

**Solution**:
```bash
git checkout 161a3667  # Last commit with experimental flags
make clean && make
```

### Results Show No Adaptation

**Problem**: All window widths are identical across shapes

**Possible Causes**:
1. L5 is disabled (check config bits)
2. Workload shapes are too similar
3. Not enough replicates to detect differences

**Solutions**:
```bash
# Increase replicates
./run_shapes.sh 300

# Test more extreme shapes
# Edit workload files to exaggerate differences
```

### High Variance in Results

**Problem**: Large standard deviation, noisy data

**Solutions**:
```bash
# Increase replicates
./run_shapes.sh 300

# Ensure clean system state
sudo cpupower frequency-set --governor performance
pkill -9 chrome firefox  # Close other apps
```

---

## Best Practices

### 1. Verify Workload Files Before Running

```bash
# Check all workload files exist
for wfile in init-4.4th init-1.4th init-2.4th init-3.4th; do
    if [ -f "../../conf/$wfile" ]; then
        echo "✓ $wfile"
    else
        echo "✗ $wfile MISSING"
    fi
done
```

### 2. Monitor Adaptive Metrics

Add logging to see L5/L7 decisions:

```bash
# Run with verbose logging
export STARFORTH_LOG_LEVEL=DEBUG
./run_shapes.sh 10

# Check logs for window adjustments
grep "INFERENCE\[window\]" shape_run_*/vm_output.tmp
```

### 3. Compare Against Non-Adaptive Config

Run experiment twice - once with C37 (adaptive), once with C0 (non-adaptive):

```bash
# Run with C37 (adaptive)
./run_shapes.sh 100
mv shape_run_* shape_run_adaptive/

# Edit run_shapes.sh: CONFIG_BITS="0000000", L1-L7 all = 0
# Run with C0 (non-adaptive)
./run_shapes.sh 100
mv shape_run_* shape_run_nonadaptive/

# Compare: window width should vary only in adaptive run
```

---

## See Also

- [`experiments/doe_2x7/`](../doe_2x7/README.md) - Full 2^7 factorial DoE
- [`experiments/l8_validation/`](../l8_validation/README.md) - L8 Jacquard validation
- [`docs/WORK_LOG_2025-11-26.md`](../../docs/WORK_LOG_2025-11-26.md) - Development log

---

**Last Updated**: November 26, 2025
**Experimental Commit**: `161a3667`
**Configuration Tested**: C37 (0100101)
