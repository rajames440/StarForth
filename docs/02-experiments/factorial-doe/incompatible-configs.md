# Handling Incompatible Configurations in Factorial DoE

## Overview

Your intuition was correct: **some loop combinations can't coexist**. Rather than failing the entire experiment, the updated script treats incompatibility as valuable data.

This document explains how the script detects and records incompatible configurations during the factorial experiment.

---

## What is an Incompatible Configuration?

An incompatible configuration is a combination of feedback loop toggles that:

1. **Fails to compile** - Makefile target produces build errors
2. **Causes StarForth to crash** - Binary exits abnormally (segfault, signal, etc.)
3. **Produces invalid output** - Test harness runs but produces no metrics

Examples (hypothetical):
- Loop 5 (window inference) without Loop 2 (rolling window) = no data to infer from
- Loop 6 (decay inference) without Loop 3 (linear decay) = no slope to measure
- Certain combinations trigger memory conflicts or uninitialized states

---

## How the Script Handles It

### Detection Strategy

**Build Phase:**
```
Configuration X attempted
  ├─ make TARGET=fastest ENABLE_LOOP_1=1 ...ENABLE_LOOP_6=0
  ├─ Check exit code
  ├─ If success: mark as "OK", proceed to testing
  └─ If failure: mark as "BUILD_FAILED", skip all runs for this config
```

**Execution Phase:**
```
Run configuration X, iteration N
  ├─ Execute: ./starforth --doe-experiment
  ├─ Check exit code
  ├─ Try to extract metrics from output
  ├─ If success: mark as "OK", record metrics
  └─ If failure: mark as "CRASH", record incompatibility
```

### CSV Recording

Incompatible configurations are recorded with status flags:

**Successful run (viable config):**
```
timestamp,configuration,run_number,build_status,run_status,
2025-11-19T10:30:45,1_1_0_1_0_0,5,OK,OK,
<35+ metrics>,1,1,0,1,0,0
```

**Build failure (incompatible):**
```
timestamp,configuration,run_number,build_status,run_status,
2025-11-19T10:35:20,0_1_1_1_1_0,1,BUILD_FAILED,<skipped>,
<empty>,0,1,1,1,1,0
```

**Runtime crash (incompatible):**
```
timestamp,configuration,run_number,build_status,run_status,
2025-11-19T10:40:15,1_0_0_1_1_1,3,OK,CRASH,
<empty>,1,0,0,1,1,1
```

### Experiment Flow

The experiment **continues** when incompatibility is detected:

```
Run 1: Config A (OK)           → Test completes, metrics recorded
Run 2: Config B (BUILD_FAILED) → Build fails, skip all runs for B, continue
Run 3: Config A (OK)           → Test completes, metrics recorded
Run 4: Config C (CRASH)        → Crashes, record as incompatible, continue
...
Run N: Config D (OK)           → Test completes, metrics recorded

SUMMARY:
  ✓ Successful runs: 847/1920
  ✗ Failed configs: 17 (incompatible combinations detected)
  ✓ Experiment completed despite incompatibilities
```

---

## CSV Schema

### Two New Columns

| Column | Values | Meaning |
|--------|--------|---------|
| `build_status` | OK, BUILD_FAILED | Did Makefile compilation succeed? |
| `run_status` | OK, CRASH | Did StarForth execute without crashing? |

### Analysis Filter Rules

```python
import pandas as pd
df = pd.read_csv('experiment_results.csv')

# Only viable configurations
viable = df[(df['build_status'] == 'OK') & (df['run_status'] == 'OK')]

# Incompatible configs (at compile time)
build_failures = df[df['build_status'] == 'BUILD_FAILED']

# Incompatible configs (at runtime)
runtime_crashes = df[df['run_status'] == 'CRASH']

# All incompatible
incompatible = df[(df['build_status'] != 'OK') | (df['run_status'] != 'OK')]
```

---

## Interpreting Results

### Experiment Summary

After completion, you'll see:

```
═══════════════════════════════════════════════════════════
EXPERIMENT COMPLETE (with incompatibility detection)
═══════════════════════════════════════════════════════════
✓ Successful runs: 1847/1920
✓ Failed configurations (incompatible): 17

→ Incompatible configurations detected:
  ✗ 0_1_1_1_1_0 (BUILD_FAILED)
  ✗ 1_0_0_1_1_1 (CRASH)
  ✗ 0_1_0_1_1_1 (CRASH)
  ... (total 17)

→ These configs are marked in CSV with build_status or run_status
→ They represent configurations where loops cannot coexist without conflict
```

### What This Means

- **1847 successful runs** = viable, measurable configurations
- **17 incompatible** = design space constraints discovered
- **Experiment completed** = data collection succeeded despite incompatibilities
- **CSV includes both** = full picture of parameter space

---

## Analysis Examples

### Example 1: Summary by Viability

```python
import pandas as pd
df = pd.read_csv('experiment_results.csv')

# Count by status
viable = len(df[(df['build_status'] == 'OK') & (df['run_status'] == 'OK')])
build_fail = len(df[df['build_status'] == 'BUILD_FAILED'].drop_duplicates('configuration'))
runtime_crash = len(df[df['run_status'] == 'CRASH'].drop_duplicates('configuration'))

print(f"Viable configurations: {viable} runs")
print(f"Build failures: {build_fail} configurations")
print(f"Runtime crashes: {runtime_crash} configurations")
print(f"Total tested: {64} configurations, {64*30} runs")
print(f"Success rate: {viable}/{len(df)} = {100*viable/len(df):.1f}%")
```

Expected output:
```
Viable configurations: 1847 runs
Build failures: 5 configurations
Runtime crashes: 12 configurations
Total tested: 64 configurations, 1920 runs
Success rate: 1847/1920 = 96.2%
```

### Example 2: Identify Problematic Loop Combinations

```python
# Find configs that always crash
crashes = df[df['run_status'] == 'CRASH'].drop_duplicates('configuration')

print("Configurations that crash:")
for config in crashes['configuration']:
    parts = config.split('_')
    loops_on = [f"Loop{i+1}" for i, bit in enumerate(parts) if bit == '1']
    print(f"  {config}: {', '.join(loops_on) or 'None'}")
```

Expected output:
```
Configurations that crash:
  0_1_1_1_1_0: Loop2, Loop3, Loop4, Loop5
  1_0_0_1_1_1: Loop1, Loop4, Loop5, Loop6
  0_1_0_1_1_1: Loop2, Loop4, Loop5, Loop6
  ... etc
```

**Insight:** Loop 5 (window inference) appears in many crashes → suggests window inference has prerequisites or conflicts.

### Example 3: Feasibility Matrix

```python
# Which loop combinations are feasible?
df_viable = df[(df['build_status'] == 'OK') & (df['run_status'] == 'OK')].copy()

# For each config, mark as viable (1) or not (0)
feasible = {}
for config in df['configuration'].unique():
    is_viable = len(df_viable[df_viable['configuration'] == config]) > 0
    feasible[config] = 1 if is_viable else 0

# Show as binary matrix
print("Feasibility (1=viable, 0=incompatible):")
for i in range(64):
    config = f"{i%2}_{(i//2)%2}_{(i//4)%2}_{(i//8)%2}_{(i//16)%2}_{(i//32)%2}"
    print(f"{config}: {'✓' if feasible[config] else '✗'}")
```

Result: Feasibility map of 64 configurations for maintenance and design decisions.

---

## Maintenance Insights

### Why Some Combinations Are Incompatible

Common reasons:

1. **Missing Prerequisites:**
   - Loop 5 (window inference) needs Loop 2 (rolling window) data
   - Loop 6 (decay inference) needs Loop 3 (linear decay) to measure

2. **Memory/Resource Conflicts:**
   - Multiple loops allocating same memory region
   - Circular dependencies in initialization

3. **Semantic Contradictions:**
   - Loop X disables feature that Loop Y requires
   - Incompatible macro definitions

4. **Bugs in Specific Combinations:**
   - Race conditions appearing only with certain feature combinations
   - Uninitialized state when paths not taken

### Using This Information

**For Future Development:**
```
Findings: Loop 5 incompatible without Loop 2
  → Check: Does window inference code assume rolling window exists?
  → Action: Either add guard (if !LOOP_5 without LOOP_2) or document prerequisite
  → Test: Add compile-time or runtime check
```

**For Optimization:**
```
Result: All configs with Loop 6 crash if Loop 1 is OFF
  → Issue: Decay inference needs heat tracking for slope calculation
  → Fix: Add LOOP_1_REQUIRED_BY_LOOP_6 guard in code
  → Verify: Re-run factorial (expect fewer crashes)
```

**For Production:**
```
Feasible subset: {0_0_0_0_0_0, 1_1_0_0_0_0, 1_1_1_0_0_0, 1_1_1_1_1_1}
  → Recommendation: Use 1_1_1_1_1_1 (all compatible, best performance)
  → Fallback: Use 1_1_0_0_0_0 (compatible, simpler)
  → Baseline: Use 0_0_0_0_0_0 (no feedback, minimal overhead)
```

---

## Next Steps

1. **Run the experiment** to completion (incompatibilities are discovered organically)
2. **Analyze the CSV** to identify which configs are viable
3. **Examine incompatible configs** to understand why they fail
4. **Document prerequisites** (which loops require others)
5. **Fix or guard** the code to eliminate incompatibilities
6. **Re-run** experiment to validate fixes

---

## Technical Details

### Build Failure Detection

```bash
if make TARGET=fastest ENABLE_LOOP_1=1 ... > build_log 2>&1; then
    # Build successful
    BUILD_STATUS="OK"
else
    # Build failed - mark incompatible
    BUILD_STATUS="BUILD_FAILED"
    # Skip all runs for this config
    continue
fi
```

### Runtime Crash Detection

```bash
if "${BINARY}" --doe-experiment > run_log 2>&1; then
    # Binary executed (exit code 0)
    if extract_csv_row(run_log) succeeds:
        RUN_STATUS="OK"
    else:
        RUN_STATUS="CRASH"  # Binary ran but no output
else
    # Binary crashed (non-zero exit)
    RUN_STATUS="CRASH"
fi
```

### CSV Recording

```bash
# Viable run
printf '%s,%s,%s,%s,%s,%s,%s\n' \
    "${timestamp}" "${config}" "${run_num}" \
    "OK" "OK"  \
    "${csv_row}" "${flags}" >> results_run_01_2025_12_08.csv

# Incompatible run
printf '%s,%s,%s,%s,%s,%s\n' \
    "${timestamp}" "${config}" "${run_num}" \
    "${build_status}" "${run_status}" \
    "${flags}" >> results_run_01_2025_12_08.csv
```

---

## Example: Real Incompatibility

**Scenario:** Loop 5 (window inference) without Loop 2 (rolling window)

```c
// Loop 5: Window Inference Code
void vm_tick_inference_engine(VM *vm) {
    // Try to infer window width from rolling window
    RollingWindowOfTruth *window = &vm->rolling_window;  // LOOP_2 data

    // BUG: If LOOP_2 is OFF, window is uninitialized!
    uint32_t window_pos = window->window_pos;  // CRASH: uninitialized read
}
```

**Symptom in CSV:**
```
Configuration 0_0_0_0_1_0 marked as CRASH
  Loop1=0, Loop2=0, Loop3=0, Loop4=0, Loop5=1, Loop6=0
  ↑                                   ↑
  Missing prerequisite             Problematic loop
```

**Fix:**
```c
// Add guard in code
#if ENABLE_LOOP_5_WINDOW_INFERENCE
    #if !ENABLE_LOOP_2_ROLLING_WINDOW
        #error "Loop 5 requires Loop 2: window inference needs rolling window data"
    #endif
#endif
```

**Result after fix:** Configuration 0_0_0_0_1_0 removed from viable set (prevented at compile time), reduces crash count.

---

## Summary

| Aspect | Approach |
|--------|----------|
| **Detection** | Build exit codes + runtime crash detection |
| **Recording** | New `build_status` and `run_status` columns |
| **Handling** | Skip incompatible config, continue experiment |
| **Impact** | Partial dataset (viable configs) + incompatibility map |
| **Analysis** | Filter by status, identify patterns |
| **Action** | Fix code, re-run, measure improvement |

This design enables discovering incompatibilities without sacrificing experimental continuity.

---

**Version:** 1.0
**Last Updated:** 2025-11-19
**Status:** Integrated into run_factorial_doe.sh