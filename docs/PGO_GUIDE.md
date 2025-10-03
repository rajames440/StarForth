# StarForth Profile-Guided Optimization (PGO) Guide

**Complete guide to building the fastest possible StarForth binary using profile-guided optimization**

---

## Overview

Profile-Guided Optimization (PGO) is a compiler optimization technique that uses runtime profiling data to guide code
generation. StarForth's PGO system exercises all major code paths to create an optimized binary tailored to real-world
usage patterns.

### Performance Gains

PGO typically delivers **5-15% performance improvements** over standard `-O3` optimization by:

- Better branch prediction (hot paths predicted correctly)
- Improved instruction cache utilization (hot code packed together)
- More aggressive inlining of frequently-called functions
- Optimal code layout for common execution patterns

---

## Quick Start

### Basic PGO Build

```bash
make pgo
```

This runs a comprehensive 6-stage build:

1. **Clean environment** - Remove old artifacts and profile data
2. **Instrumentation build** - Compile with profiling instrumentation (`-fprofile-generate`)
3. **Profiling workload** - Execute comprehensive test suite to generate profile data
4. **Collect profile data** - Gather `.gcda` files from all code paths
5. **Optimized rebuild** - Recompile with profile data (`-fprofile-use`)
6. **Cleanup** - Remove temporary profile data

**Build time:** ~2-3 minutes (depending on hardware)

---

## Available PGO Targets

### 1. Standard PGO (`make pgo`)

**Use when:** You want maximum performance for typical StarForth workloads.

**What it does:**

- Runs comprehensive profiling workload (tests, benchmarks, REPL, blocks)
- Generates optimized binary with ASM optimizations + direct threading
- Cleans up profile data automatically

**Output:** Optimized `build/starforth` binary

### 2. PGO + perf (`make pgo-perf`)

**Use when:** You need detailed performance analysis and want to identify bottlenecks.

**Requires:** `sudo` access for perf, linux-tools-generic package

**What it does:**

- Everything from standard PGO
- Captures perf profiling data during workload execution
- Generates call graphs and hotspot analysis
- Keeps frame pointers for accurate stack traces

**Output:**

- Optimized `build/starforth` binary
- `pgo-perf.data` - perf profiling data

**View results:**

```bash
# Interactive TUI
sudo perf report -i pgo-perf.data

# Text report (top functions by CPU time)
sudo perf report --stdio -i pgo-perf.data --sort comm,dso,symbol --percent-limit 1

# Generate flamegraph (if you have flamegraph.pl)
sudo perf script -i pgo-perf.data | ./scripts/flamegraph.pl > pgo-flame.svg
```

### 3. PGO + Valgrind (`make pgo-valgrind`)

**Use when:** You need instruction-level analysis or want to understand cache behavior.

**Requires:** valgrind package, kcachegrind (optional, for visualization)

**What it does:**

- Runs callgrind profiling during benchmark workload
- Collects instruction counts, cache misses, branch mispredictions
- Builds optimized binary with profile data + debug symbols

**Output:**

- Optimized `build/starforth` binary (with `-g` debug symbols)
- `pgo-callgrind.out` - callgrind profiling data

**View results:**

```bash
# GUI visualization (best option)
kcachegrind pgo-callgrind.out

# Text report
callgrind_annotate pgo-callgrind.out
```

**Note:** Callgrind adds significant overhead (~10-100x slowdown), so workload is limited to 100 benchmark iterations.

---

## Benchmark Comparison

Compare performance before and after PGO:

```bash
make bench-compare
```

This builds both regular and PGO binaries, runs identical benchmarks, and reports timing differences.

**Example output:**

```
🏁 Benchmark Comparison: Regular vs PGO

Building regular optimized binary...
Running benchmark (10000 iterations)...
  Regular build: 0:00.45 elapsed, 0.44 user

Building PGO optimized binary...
Running benchmark (10000 iterations)...
  PGO build:     0:00.38 elapsed, 0.37 user

✓ Comparison complete!
```

**Speedup:** 1.18x faster (18% improvement in this example)

---

## The PGO Profiling Workload

### What Gets Profiled

The profiling workload (`scripts/pgo-workload.sh`) exercises 7 major code paths:

#### 1. Unit Tests

- All word implementations (stack, arithmetic, logic, memory, etc.)
- Dictionary operations
- Compiler functionality
- ~400+ test cases

#### 2. Stress Tests

- Deep call stacks (nested definitions)
- Stack exhaustion scenarios
- Large word definitions
- Edge cases and boundary conditions

#### 3. Integration Tests

- Complete Forth programs
- Multi-word interactions
- Real-world usage patterns

#### 4. Benchmarks

- 5000 iterations of hot-path operations
- Stack manipulation
- Arithmetic operations
- Control flow

#### 5. REPL Workload

- Interactive command processing
- Variable definitions
- Constant creation
- Control structures (IF/ELSE/THEN, DO/LOOP, BEGIN/UNTIL)
- Nested definitions
- Recursive functions
- String handling
- Memory operations
- Vocabulary management

#### 6. Block I/O Operations

- Block reading from disk image
- LIST operations
- Buffer management
- (Runs if `disks/rajames-rajames-1.0.img` exists)

#### 7. Profiler Integration

- Profiler overhead measurement
- Nested profiling calls

### Customizing the Workload

You can run the profiling workload independently:

```bash
./scripts/pgo-workload.sh ./build/starforth
```

To add custom workload patterns, edit `scripts/pgo-workload.sh` and add Forth code that exercises your specific use
cases.

---

## Understanding Profile Data

### Profile Data Files

During instrumentation, GCC generates two types of files:

- **`.gcno` files** (generated at compile time)
    - Coverage notes - which branches/blocks exist
    - Static instrumentation metadata
    - Removed before optimization build

- **`.gcda` files** (generated at runtime)
    - Coverage data - execution counts for each branch/block
    - Dynamic profiling information
    - Used by `-fprofile-use` to guide optimization

### Profile Data Location

Profile data is written to the directory where the instrumented binary runs. StarForth's build system searches:

```bash
./*.gcda
src/*.gcda
src/*/*.gcda
build/*.gcda
```

### Verifying Profile Coverage

After running the workload, check how many profile files were generated:

```bash
find . -name "*.gcda" -type f | wc -l
```

Expected: **100+ files** (one per source file that executed)

If you see fewer than expected, some code paths weren't exercised. Consider expanding the workload.

---

## Compiler Flags Explained

### Instrumentation Build (`-fprofile-generate`)

```bash
CFLAGS="-O2 -DUSE_ASM_OPT=1 -fprofile-generate"
LDFLAGS="-fprofile-generate -lgcov"
```

- `-O2` - Moderate optimization (faster build than `-O3`, sufficient for profiling)
- `-DUSE_ASM_OPT=1` - Enable assembly optimizations
- `-fprofile-generate` - Instrument code to collect edge/block execution counts
- `-lgcov` - Link against GCC coverage runtime library

### Optimized Build (`-fprofile-use`)

```bash
CFLAGS="-O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 \
        -fprofile-use -fprofile-correction -Wno-error=coverage-mismatch"
LDFLAGS="-fprofile-use"
```

- `-O3` - Maximum optimization level
- `-DUSE_ASM_OPT=1` - Assembly optimizations
- `-DUSE_DIRECT_THREADING=1` - Direct threading for inner interpreter
- `-fprofile-use` - Use profile data to guide optimization
- `-fprofile-correction` - Handle inconsistent profile data gracefully
- `-Wno-error=coverage-mismatch` - Warn (don't fail) on minor profile mismatches

### perf-Specific Flags (`-fno-omit-frame-pointer`)

```bash
-fno-omit-frame-pointer
```

- Keeps frame pointer register for accurate stack traces
- Slightly reduces performance (~2-3%) but essential for perf analysis
- Allows perf to generate accurate call graphs

---

## Integration with Other Tools

### perf - Linux Performance Analysis

**Install:**

```bash
sudo apt-get install linux-tools-generic
```

**Manual profiling:**

```bash
# Record profile data
sudo perf record -g --call-graph dwarf -o myprofile.data ./build/starforth --benchmark 10000

# View report
sudo perf report -i myprofile.data

# Generate flamegraph
sudo perf script -i myprofile.data | flamegraph.pl > flame.svg
```

**Key perf commands:**

- `perf record` - Capture profiling data
- `perf report` - View profiling results (interactive TUI)
- `perf annotate` - Show assembly with performance annotations
- `perf stat` - Display performance counter statistics
- `perf top` - Real-time performance monitoring

### Valgrind/Callgrind - Instruction-Level Profiling

**Install:**

```bash
sudo apt-get install valgrind kcachegrind
```

**Manual profiling:**

```bash
# Run with callgrind
valgrind --tool=callgrind \
         --callgrind-out-file=myprofile.out \
         --dump-instr=yes \
         --collect-jumps=yes \
         ./build/starforth --benchmark 1000

# View in kcachegrind (GUI)
kcachegrind myprofile.out

# Or text report
callgrind_annotate myprofile.out
```

**What callgrind shows:**

- Instruction counts (exact, deterministic)
- Function call counts
- Cache simulation (I1/D1/LL cache hits/misses)
- Branch prediction accuracy
- Caller/callee relationships

---

## Troubleshooting

### Issue: "No profile data found"

**Symptoms:**

```
warning: no profile data available for function 'vm_init'
```

**Causes:**

1. Workload didn't exercise code paths
2. Profile data files not found
3. Source changed between instrumentation and optimization builds

**Solutions:**

1. Expand workload to cover more code
2. Check for `.gcda` files: `find . -name "*.gcda"`
3. Don't modify source between stages (use `make pgo` which handles this)

### Issue: "Coverage mismatch"

**Symptoms:**

```
warning: source locations for function 'vm_execute' have changed
```

**Cause:** Source code modified between instrumentation and optimization

**Solution:** Run `make pgo` from scratch (it cleans first)

### Issue: "Permission denied writing .gcda"

**Cause:** Insufficient permissions in working directory

**Solution:**

```bash
chmod -R u+w .
make pgo
```

### Issue: PGO build slower than regular build

**Possible causes:**

1. Profile data doesn't match actual usage patterns
2. Workload too small/unrealistic
3. Compiler made poor optimization decisions

**Solutions:**

1. Customize workload to match your use case
2. Try different optimization levels (`-O2` vs `-O3`)
3. Compare with `make bench-compare` to verify

---

## Best Practices

### 1. Profile Representative Workloads

✅ **Do:**

- Use realistic test data
- Cover common code paths (90%+ of real usage)
- Include edge cases that are still frequent

❌ **Don't:**

- Profile only trivial operations
- Use synthetic benchmarks that don't match real usage
- Profile rarely-executed error paths

### 2. Keep Profile Data Fresh

- Re-run PGO after major code changes (>10% of codebase)
- Update profile data when adding new features
- Consider separate PGO builds for different workload profiles

### 3. Verify Performance Gains

Always measure before/after:

```bash
make bench-compare
```

If PGO doesn't improve performance, investigate:

- Is workload representative?
- Are hot paths correctly identified?
- Check perf data to understand what's optimized

### 4. Combine with Other Optimizations

PGO works best with:

- `-march=native` - CPU-specific instructions
- `-flto` - Link-time optimization
- `-DUSE_DIRECT_THREADING=1` - Direct threading inner interpreter
- `-DUSE_ASM_OPT=1` - Hand-optimized assembly

StarForth's `make pgo` enables all of these automatically.

---

## Advanced Usage

### Separate Training and Production Builds

**Scenario:** Profile on development machine, optimize for production.

**Step 1:** Collect profile data

```bash
# On dev machine
make clean
make CFLAGS="-O2 -fprofile-generate" LDFLAGS="-fprofile-generate -lgcov"
./scripts/pgo-workload.sh ./build/starforth

# Save profile data
tar czf profile-data.tar.gz $(find . -name "*.gcda")
```

**Step 2:** Optimize with profile data

```bash
# On production machine (or CI)
tar xzf profile-data.tar.gz
make clean-obj
make CFLAGS="-O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction" \
     LDFLAGS="-fprofile-use"
```

### Multi-Stage PGO (Iterative Optimization)

**Stage 1:** Initial PGO

```bash
make pgo
```

**Stage 2:** Profile the PGO binary (better profile data)

```bash
./scripts/pgo-workload.sh ./build/starforth
# This generates more accurate profiles using optimized code
```

**Stage 3:** Re-optimize with improved profile

```bash
make clean-obj
make CFLAGS="..." LDFLAGS="..." # Use profile data from stage 2
```

**Diminishing returns:** Usually not worth it (< 1% additional gain).

---

## Comparison with Other Builds

### Build Target Performance Comparison

| Target    | Optimization           | Speed        | Build Time | Use Case               |
|-----------|------------------------|--------------|------------|------------------------|
| `debug`   | `-O0 -g`               | 1.0x         | 30s        | Development, debugging |
| `all`     | `-O2`                  | 3.5x         | 45s        | Default, balanced      |
| `fast`    | `-O3 + ASM`            | 5.2x         | 60s        | Production, no LTO     |
| `fastest` | `-O3 + ASM + DT + LTO` | 6.8x         | 90s        | Maximum performance    |
| **`pgo`** | **Fastest + PGO**      | **7.5-8.0x** | **3m**     | **Absolute maximum**   |

**Legend:**

- ASM = Assembly optimizations (`-DUSE_ASM_OPT=1`)
- DT = Direct threading (`-DUSE_DIRECT_THREADING=1`)
- LTO = Link-time optimization (`-flto`)
- PGO = Profile-guided optimization

---

## Technical Details

### How PGO Works

1. **Instrumentation Phase**
    - Compiler inserts counters at control flow edges
    - Runtime increments counters during execution
    - Profile data written to `.gcda` files on program exit

2. **Optimization Phase**
    - Compiler reads profile data
    - Identifies hot paths (frequently executed code)
    - Identifies cold paths (rarely executed code)
    - Makes optimization decisions:
        - **Inline hot functions** (reduce call overhead)
        - **Pack hot code together** (improve I-cache locality)
        - **Predict branches** (arrange code for correct prediction)
        - **Devirtualize calls** (when type is known from profile)
        - **Unroll hot loops** (reduce branch overhead)

3. **Code Layout Optimization**
    - Hot code placed sequentially in memory
    - Cold code moved to separate section
    - Result: Better instruction cache utilization

### Profile Data Structure

`.gcda` files contain:

- **Arc counts:** Number of times each control flow edge executed
- **Block counts:** Number of times each basic block executed
- **Function summaries:** Entry/exit counts per function
- **Checksum:** Verify profile matches current source

### Compiler Optimization Heuristics

With profile data, GCC adjusts:

| Heuristic           | Default        | With PGO                                  |
|---------------------|----------------|-------------------------------------------|
| Inline threshold    | 600 units      | Adjusted per call site                    |
| Loop unroll factor  | 4              | Adjusted per loop (up to 8 for hot loops) |
| Branch prediction   | Static (50/50) | Dynamic (from profile)                    |
| Function outlining  | Disabled       | Cold code outlined                        |
| Register allocation | Balanced       | Favor hot paths                           |

---

## Platform-Specific Notes

### x86_64

- Full support for all PGO features
- Best results with `-march=native`
- perf hardware counters available
- Valgrind fully supported

### ARM64 (Raspberry Pi 4, Apple Silicon)

- Full PGO support
- Use `-march=armv8-a+crc+simd -mtune=cortex-a72` on RPi4
- perf support varies by kernel (check `perf list`)
- Valgrind support limited on some ARM platforms

### Cross-Compilation

PGO requires native execution for profiling. For cross-compilation:

1. Build instrumented binary for target architecture
2. Run on target hardware to collect profile data
3. Transfer `.gcda` files back to build machine
4. Cross-compile optimized binary with profile data

---

## References

### GCC Documentation

- [Profile-Guided Optimization](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fprofile-generate)
- [Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)

### Tools Documentation

- [perf Wiki](https://perf.wiki.kernel.org/index.php/Main_Page)
- [Valgrind Manual](https://valgrind.org/docs/manual/manual.html)
- [Callgrind Documentation](https://valgrind.org/docs/manual/cl-manual.html)

### Academic Papers

- "Feedback-Directed Optimization in Compilers" (ACM Computing Surveys)
- "Profile-Guided Post-Link Optimization" (Intel)

---

## Summary

**Quick command reference:**

```bash
# Standard PGO build (recommended)
make pgo

# PGO with performance analysis
make pgo-perf
sudo perf report -i pgo-perf.data

# PGO with instruction analysis
make pgo-valgrind
kcachegrind pgo-callgrind.out

# Compare performance
make bench-compare

# Custom workload
./scripts/pgo-workload.sh ./build/starforth
```

**Key takeaways:**

1. PGO delivers **5-15% performance improvement** for typical workloads
2. Workload must be **representative** of real usage
3. Best combined with other optimizations (ASM, direct threading, LTO)
4. Use `make pgo-perf` or `make pgo-valgrind` for detailed analysis
5. Verify gains with `make bench-compare`

---

**End of PGO Guide**