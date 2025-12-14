# StarForth Physics Engine - Comprehensive Validation Experiment

## Quick Start

Execute the complete 90-run validation experiment:

```bash
# Run with default output directory (current directory)
./scripts/run_comprehensive_physics_experiment.sh

# Run with custom output directory
./scripts/run_comprehensive_physics_experiment.sh ./physics_results

# Then analyze results_run_01_2025_12_08
python3 scripts/analyze_physics_experiment.py ./physics_results/experiment_results.csv --output results_run_01_2025_12_08.md
```

## Overview

This document describes how to execute and analyze the comprehensive physics engine validation experiment described in `COMPREHENSIVE_PHYSICS_ENGINE_VALIDATION_PROTOCOL.md`.

**Quick Facts:**
- **Total Runs:** 90 (30 per configuration)
- **Configurations:** 3 (Baseline, Cache, Full)
- **Workload:** 100,000 dictionary lookups per run
- **Estimated Time:** 2-3 hours
- **Build Time:** 5-10 minutes (3 separate builds)
- **Output:** CSV with all metrics, per-run logs, analysis report

## Prerequisites

### System Requirements

- **OS:** Linux (or compatible POSIX system)
- **Architecture:** x86_64 (amd64) or ARM64 (aarch64/arm64)
- **RAM:** 4+ GB
- **Disk Space:** 500 MB for builds, 50 MB for results
- **Build Tools:** GCC, Make
- **Python:** Python 3.6+ (for analysis, optional but recommended)

### Verify Build System

```bash
cd /home/rajames/CLionProjects/StarForth

# Check Makefile has required flags
grep -E "ENABLE_HOTWORDS_CACHE|ENABLE_PIPELINING" Makefile

# Verify build commands work
make --version
gcc --version
```

### Verify FORTH Commands

Before running the full experiment, verify the benchmark commands exist:

```bash
# Build and verify commands
make fastest
./build/amd64/fastest/starforth -c ": test 10 BENCH-DICT-LOOKUP BYE"
```

If commands are missing, check:
- `src/word_source/benchmark_words.c` - BENCH-DICT-LOOKUP implementation
- `src/word_source/physics_cache_words.c` - PHYSICS-CACHE-STATS implementation
- `src/word_source/physics_pipelining_words.c` - PIPELINING-STATS implementation
- `include/rolling_window_of_truth.h` - ROLLING-WINDOW-STATS implementation

## Step-by-Step Execution

### Step 1: Prepare Workspace

```bash
# Navigate to repository root
cd /home/rajames/CLionProjects/StarForth

# Create output directory
mkdir -p physics_results
cd physics_results

# Verify write permissions
touch test_file && rm test_file
```

### Step 2: Run the Experiment

```bash
# From physics_results/ or any directory, run:
../scripts/run_comprehensive_physics_experiment.sh .

# Or from repo root:
./scripts/run_comprehensive_physics_experiment.sh ./physics_results
```

**Expected Output:**

```
════════════════════════════════════════════════════════════
   ⚡ Building the Fastest Forth in the West! ⚡
════════════════════════════════════════════════════════════
  Target Architecture: x86_64 (amd64)
  Build Profile: fastest

>>> Building Configuration: A_BASELINE
→ make TARGET=fastest ENABLE_HOTWORDS_CACHE=0 ENABLE_PIPELINING=0
✓ Build completed for A_BASELINE
✓ Binary ready: build/amd64/fastest/starforth

>>> Run 1/30 for A_BASELINE...
✓ Run 1 completed (2.3s)
>>> Run 2/30 for A_BASELINE...
✓ Run 2 completed (2.4s)
...
✓ Configuration A_BASELINE complete (30 runs)
...
✓ All 90 experiments completed successfully!
✓ Results saved to: ./experiment_results.csv
✓ Total runtime: 145 minutes
✓ Run logs: ./run_logs/
```

### Step 3: Monitor Progress

While the experiment runs, you can monitor progress in another terminal:

```bash
# Watch the experiment_results.csv grow
watch -n 5 'wc -l experiment_results.csv'

# Check recent run logs
ls -lhrt run_logs/ | tail -10

# See how many runs of each config are done
for config in A_BASELINE B_CACHE C_FULL; do
  echo "$config: $(ls run_logs/${config}*.log 2>/dev/null | wc -l) runs"
done
```

### Step 4: Verify Results

```bash
# Check CSV was created
head -5 experiment_results.csv
wc -l experiment_results.csv  # Should be 91 (1 header + 90 runs)

# Check all runs completed
echo "Configuration A_BASELINE:"
grep "^.*,A_BASELINE," experiment_results.csv | wc -l  # Should be 30
echo "Configuration B_CACHE:"
grep "^.*,B_CACHE," experiment_results.csv | wc -l      # Should be 30
echo "Configuration C_FULL:"
grep "^.*,C_FULL," experiment_results.csv | wc -l       # Should be 30
```

### Step 5: Analyze Results

```bash
# Generate markdown report
python3 scripts/analyze_physics_experiment.py experiment_results.csv --output analysis_report.md

# View report
cat analysis_report.md

# Or in markdown viewer if available
mdless analysis_report.md  # or pandoc, etc.
```

## Output Files

### experiment_results.csv

CSV file with one row per run (90 total rows). Schema:

| Column | Type | Description |
|--------|------|-------------|
| timestamp | ISO8601 | When run started |
| configuration | string | A_BASELINE, B_CACHE, or C_FULL |
| run_number | int | 1-30 |
| total_lookups | int | Iterations executed |
| cache_hits | int | Count of cache hits (if enabled) |
| cache_hit_percent | float | Hit rate percentage |
| bucket_hits | int | Count of bucket hits |
| bucket_hit_percent | float | Bucket hit rate |
| misses | int | Total misses |
| miss_percent | float | Miss rate |
| cache_hit_latency_ns | float | Mean cache hit time (ns) |
| cache_hit_stddev_ns | float | Stddev of cache hits (ns) |
| cache_hit_min_ns | float | Min cache hit latency (ns) |
| cache_hit_max_ns | float | Max cache hit latency (ns) |
| bucket_search_latency_ns | float | Mean bucket search time (ns) |
| bucket_search_stddev_ns | float | Stddev of bucket searches (ns) |
| bucket_search_min_ns | float | Min bucket search latency (ns) |
| bucket_search_max_ns | float | Max bucket search latency (ns) |
| context_predictions_total | int | Total predictions made (pipelining) |
| context_correct | int | Correct predictions (pipelining) |
| context_accuracy_percent | float | Prediction accuracy % |
| window_diversity_percent | float | Pattern diversity captured (%) |
| window_final_size_bytes | int | Final rolling window size (bytes) |
| total_runtime_ms | float | Total execution time (ms) |
| memory_allocated_bytes | int | Memory used (bytes) |
| speedup_vs_baseline | float | Speedup relative to baseline (computed in analysis) |
| ci_lower_95 | float | Lower 95% credible interval |
| ci_upper_95 | float | Upper 95% credible interval |

### run_logs/

Directory with one log file per run, named:
- `A_BASELINE_run_1.log` through `A_BASELINE_run_30.log`
- `B_CACHE_run_1.log` through `B_CACHE_run_30.log`
- `C_FULL_run_1.log` through `C_FULL_run_30.log`

Each log contains complete FORTH output including:
- Benchmark execution output
- PHYSICS-CACHE-STATS results
- PIPELINING-STATS results
- ROLLING-WINDOW-STATS results
- Any error messages

### experiment_summary.txt

Summary of experiment execution:
- Start/end timestamps
- Total runtime
- File paths
- Run counts

### analysis_report.md (generated)

Markdown report with:
- Executive summary
- Per-configuration statistics
- Comparative analysis (speedup factors)
- Credible intervals
- Success criteria validation
- Hypothesis testing results
- Methodology notes
- Conclusions

## Customization

### Run Fewer Tests for Validation

To test the infrastructure without running full 90 tests:

```bash
# Edit the script and change:
RUNS_PER_CONFIG=5   # Instead of 30

# Or modify directly in script:
./scripts/run_comprehensive_physics_experiment.sh

# Inside script, before RUNS_PER_CONFIG=30 line:
# Uncomment and set to 5 for testing, 30 for full
```

### Use Different Build Profile

Edit the script and change `BUILD_PROFILE="fastest"` to:
- `standard` - Standard optimized build
- `fast` - Fast build (no LTO)
- `turbo` - Turbo build (ASM only)

### Change Benchmark Iterations

Edit the script and change `BENCH_ITERATIONS=100000` to:
- `10000` - Quick validation test
- `100000` - Standard (recommended)
- `1000000` - Extended precision test

### Adjust Tuning Knobs

Edit the script and add to make command:

```bash
# Example: More conservative window shrinking
make TARGET=fastest ENABLE_HOTWORDS_CACHE=1 ENABLE_PIPELINING=1 \
    ADAPTIVE_SHRINK_RATE=90 ADAPTIVE_MIN_WINDOW_SIZE=512

# Or more aggressive:
make TARGET=fastest ENABLE_HOTWORDS_CACHE=1 ENABLE_PIPELINING=1 \
    ADAPTIVE_SHRINK_RATE=50 ADAPTIVE_MIN_WINDOW_SIZE=128
```

## Troubleshooting

### Build Fails

```bash
# Check compiler flags
make clean
make TARGET=fastest -n  # Dry run to see commands

# Rebuild with verbose output
make clean
make TARGET=fastest VERBOSE=1

# Check recent commits
git log --oneline -10
git diff HEAD
```

### Benchmark Commands Not Found

```bash
# Verify word implementations exist
grep -r "BENCH-DICT-LOOKUP" src/
grep -r "PHYSICS-CACHE-STATS" src/
grep -r "PIPELINING-STATS" src/
grep -r "ROLLING-WINDOW-STATS" src/

# Check they're registered
grep -E "BENCH-DICT-LOOKUP|PHYSICS-CACHE-STATS" include/word_registry.h
```

### Metrics Not Parsing

1. Check run log for actual output format:
   ```bash
   cat run_logs/A_BASELINE_run_1.log
   ```

2. Update regex patterns in `scripts/extract_benchmark_metrics.py` to match actual output

3. Test extraction manually:
   ```bash
   python3 scripts/extract_benchmark_metrics.py run_logs/A_BASELINE_run_1.log A_BASELINE 1
   ```

### CSV Analysis Fails

```bash
# Verify CSV format
head -1 experiment_results.csv

# Check row counts
wc -l experiment_results.csv

# Run analysis with debug
python3 scripts/analyze_physics_experiment.py experiment_results.csv --output report.md -v
```

### Out of Disk Space

```bash
# Check available space
df -h

# Clean old builds
make clean
rm -rf build/

# Or delete old results_run_01_2025_12_08
rm -rf physics_results_v1/
```

## Expected Success Criteria

The experiment is successful if:

### Configuration A (Baseline)
- ✓ All 30 runs complete without error
- ✓ 100,000 lookups per run executed
- ✓ Consistent execution times (low CV)
- ✓ CSV row for each run with metrics

### Configuration B (Cache)
- ✓ Cache hit rate > 20%
- ✓ Speedup > 1.1× vs Baseline (95% CI excludes 1.0)
- ✓ Coefficient of variation < 10% (stable)
- ✓ All 30 runs complete

### Configuration C (Full)
- ✓ Prediction accuracy > 60%
- ✓ Speedup > Configuration B
- ✓ Pattern diversity saturation > 90%
- ✓ Window final size < initial size
- ✓ All 30 runs complete

## Performance Tips

To optimize experiment runtime:

1. **Disable other services:**
   ```bash
   # Stop background processes to reduce system variability
   systemctl stop docker  # If applicable
   sudo killall updatedb  # Stop file indexing
   ```

2. **Use fastest build profile:**
   - Already default in script
   - Provides maximum determinism

3. **Set CPU governor to performance:**
   ```bash
   # If supported on your system
   echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
   ```

4. **Run in dedicated terminal:**
   - Avoid other terminal I/O during experiment
   - Minimize system load

## Analysis Tips

When reviewing results:

1. **Check Coefficient of Variation (CV):**
   - CV < 5% = Very stable system
   - CV 5-10% = Good stability
   - CV > 20% = Investigate variance sources

2. **Examine Credible Intervals:**
   - Narrow CI = Consistent results
   - Wide CI = High variance, may need more runs
   - CI excludes 1.0 = Statistically significant

3. **Validate Metrics:**
   - Cache hit % should increase in B and C
   - Prediction accuracy appears only in C
   - Latencies should decrease across configs

4. **Check for Outliers:**
   - Look at min/max in per-config stats
   - Large outliers may indicate system interruptions
   - Consider removing if documented (power state, GC, etc.)

## Publication

When publishing results:

1. **Report all three statistics:**
   - Point estimate (mean)
   - 95% credible interval
   - Sample size (n=30)

2. **Include methodology:**
   - Describe configurations exactly
   - List hardware (CPU, RAM, OS)
   - Note any special conditions

3. **Provide raw data:**
   - Include experiment_results.csv in supplementary materials
   - Include per-run logs for reproducibility
   - Make code available (StarForth is open source)

4. **Discuss limitations:**
   - Deterministic workload (may not reflect real usage)
   - Single system (generalizability)
   - 30 runs (statistical power)

## Further Reading

- `COMPREHENSIVE_PHYSICS_ENGINE_VALIDATION_PROTOCOL.md` - Detailed protocol
- `include/physics_hotwords_cache.h` - Cache implementation
- `include/physics_pipelining_metrics.h` - Pipelining metrics
- `include/rolling_window_of_truth.h` - Rolling window API
- `include/rolling_window_knobs.h` - Adaptive control knobs

---

**Last Updated:** November 2025
**Protocol Version:** 1.0
**Status:** Ready for Execution
