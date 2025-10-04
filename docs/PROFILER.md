# StarForth Profiler Guide

**Complete guide to using StarForth's built-in word execution profiler for performance analysis and optimization**

---

## Overview

StarForth includes a lightweight, zero-overhead profiler that tracks word execution frequency and provides data-driven
optimization recommendations. The profiler helps identify hot paths (frequently executed code) that are candidates for
inline assembly optimization.

### Key Features

- **Lightweight Frequency Tracking** - Minimal overhead call counting
- **Hot Word Analysis** - Identifies optimization candidates by execution percentage
- **Multiple Profiling Levels** - From basic frequency tracking to detailed timing
- **Optimization Recommendations** - Data-driven suggestions for assembly optimization
- **Integration with PGO** - Complements Profile-Guided Optimization workflow

---

## Quick Start

### Basic Profiling

```bash
./build/starforth --profile 1 --profile-report < script.fth
```

This will:

1. Enable PROFILE_BASIC level (frequency tracking only)
2. Execute your Forth script
3. Print a profiling report showing the most frequently called words

### Example Output

```
========================================
     StarForth Profiler Report
========================================

Global Statistics:
  Dictionary lookups:  0

Most Frequently Called Words:
Word                      Calls
----------------------------------------
.                           661
CR                          464
LIT                         199
HERE                        149
I                           114
(LOOP)                      105
EXIT                        102
DUP                          93
:                            74
(0BRANCH)                    68

========================================
```

---

## Profiling Levels

The profiler supports multiple detail levels, controlled by the `--profile` flag:

### Level 0: PROFILE_DISABLED

```bash
./build/starforth  # Default - no profiling
```

**Use when:** Maximum performance, no profiling overhead

### Level 1: PROFILE_BASIC (Recommended)

```bash
./build/starforth --profile 1 --profile-report
```

**Features:**

- Word execution frequency tracking
- Zero-overhead call counting
- Dictionary lookup counts
- Lightweight, minimal performance impact

**Use when:** You want to identify hot words for optimization

### Level 2: PROFILE_DETAILED

```bash
./build/starforth --profile 2 --profile-report
```

**Features:**

- Everything from PROFILE_BASIC
- Word execution timing (nanosecond precision)
- Average, min, max execution times
- Top words by total execution time

**Use when:** You need timing data in addition to frequency

**Performance impact:** ~5-10% overhead due to timing calls

### Level 3: PROFILE_VERBOSE

```bash
./build/starforth --profile 3 --profile-report
```

**Features:**

- Everything from PROFILE_DETAILED
- Stack operation counts
- Memory read/write tracking (bytes)
- Detailed memory access patterns

**Use when:** Deep performance analysis required

**Performance impact:** ~15-20% overhead

---

## Command-Line Flags

### `--profile [N]`

Enable profiler at specified level (0-3).

```bash
--profile 1    # Basic frequency tracking
--profile 2    # Detailed timing
--profile 3    # Full verbose profiling
```

### `--profile-report`

Print profiling report on exit.

```bash
./build/starforth --profile 1 --profile-report < script.fth
```

**Note:** Must be used with `--profile` to have any effect.

---

## Understanding the Report

### Global Statistics

```
Global Statistics:
  Dictionary lookups:  0
```

- **Dictionary lookups** - Number of word lookups performed (tracked at PROFILE_BASIC+)
- **Stack operations** - Stack pushes/pops (tracked at PROFILE_VERBOSE+)
- **Memory reads/writes** - Memory access patterns (tracked at PROFILE_VERBOSE+)

### Most Frequently Called Words

```
Word                      Calls
----------------------------------------
LIT                        1560
EXIT                        562
(LOOP)                      525
```

Shows top 15 words by execution count:

- **LIT** - Literal values pushed to stack
- **EXIT** - End of word definition (every : word ends with EXIT)
- **(LOOP)** - Loop runtime implementation
- **I** - Loop counter access

**Interpretation:**

- High call counts indicate hot paths
- These words are prime candidates for assembly optimization
- Words with >5% of total executions should be optimized first

### Top Words by Execution Time (PROFILE_DETAILED)

```
Top Words by Execution Time:
Word                 Calls    Total (µs)     Avg (ns)     Max (ns)
--------------------------------------------------------------------------------
DUP                    500      1234.56        2469         12000
+                      500       987.32        1975          8500
```

- **Total (µs)** - Total time spent in this word (microseconds)
- **Avg (ns)** - Average execution time per call (nanoseconds)
- **Max (ns)** - Longest single execution (nanoseconds)

---

## Hot Word Analysis

The profiler includes a hot word analysis function `profiler_print_hotspots()` that provides optimization
recommendations.

### Optimization Priority Levels

| % of Total | Priority | Recommendation                                   |
|------------|----------|--------------------------------------------------|
| ≥ 5.0%     | ⚡ HIGH   | Inline assembly candidate - optimize immediately |
| ≥ 2.0%     | 🔥       | Consider assembly optimization                   |
| ≥ 1.0%     | 💡       | Monitor for optimization                         |
| ≥ 0.5%     | 📊       | Profile for PGO                                  |
| < 0.5%     | —        | Low priority                                     |

### Example Hot Word Report

```
========================================
     Hot Word Analysis & Optimization
========================================

Top 25 Most Frequently Called Words:
Word                      Calls    % Total  Optimization Suggestion
--------------------------------------------------------------------------------
LIT                        1560      32.5%  ⚡ HIGH PRIORITY: Inline assembly candidate
EXIT                        562      11.7%  ⚡ HIGH PRIORITY: Inline assembly candidate
(LOOP)                      525      10.9%  ⚡ HIGH PRIORITY: Inline assembly candidate
I                           500      10.4%  ⚡ HIGH PRIORITY: Inline assembly candidate
+                           500      10.4%  ⚡ HIGH PRIORITY: Inline assembly candidate
*                           500      10.4%  ⚡ HIGH PRIORITY: Inline assembly candidate
DROP                        500      10.4%  ⚡ HIGH PRIORITY: Inline assembly candidate

Summary:
  Total word executions: 4800
  Unique words tracked:  25
  Top 10 coverage:       95.3%
```

**Key Insights:**

- **Top 10 coverage: 95.3%** - Optimizing just 10 words affects 95% of executions
- Focus optimization efforts on words with ≥5% execution percentage
- Use PGO to guide compiler optimization for words with 0.5-5% coverage

---

## Integration with Development Workflow

### 1. Profile Your Workload

```bash
# Run your typical Forth program with profiling
./build/starforth --profile 1 --profile-report < my_program.fth
```

### 2. Identify Hot Words

Look for words with high call counts or high percentage of total executions.

### 3. Optimize Hot Words

**For words ≥5% execution:**

- Implement inline assembly version (see `docs/ASM_OPTIMIZATIONS.md`)
- Add to `src/word_source/*_words.c` with `USE_ASM_OPT` guards

**For words 0.5-5% execution:**

- Let PGO handle optimization: `make pgo`
- Compiler will inline and optimize based on profile data

### 4. Verify Performance Gains

```bash
# Benchmark before optimization
./build/starforth --benchmark 10000

# Implement optimization
# ...

# Rebuild and benchmark again
make clean && make
./build/starforth --benchmark 10000
```

---

## Profiling Best Practices

### ✅ Do

- **Profile realistic workloads** - Use actual programs, not toy examples
- **Run sufficient iterations** - Ensure statistical significance (1000+ word executions)
- **Profile at BASIC level first** - Minimal overhead, identifies hot words quickly
- **Use DETAILED level for timing** - When you need to know execution duration
- **Combine with PGO** - Profile → identify hot words → optimize → PGO build

### ❌ Don't

- **Don't profile with LOG_DEBUG** - Logging overhead skews results
- **Don't optimize prematurely** - Profile first, then optimize hot paths only
- **Don't ignore top 10 coverage** - If <80%, workload may not be representative
- **Don't profile error paths** - Focus on normal execution, not exceptional cases

---

## Profiler Implementation Details

### Architecture

The profiler tracks word execution in two places:

1. **Outer Interpreter** (`vm_interpret_word()` in `src/vm.c:492`)
    - Tracks words executed from REPL or scripts
    - Direct word execution (not compiled)

2. **Inner Interpreter** (`execute_colon_word()` in `src/vm.c:426`)
    - Tracks words executed from compiled definitions
    - Threaded code execution

### Data Structure

```c
typedef struct {
    const DictEntry *entry;    // Dictionary entry pointer
    uint64_t call_count;       // Number of executions
    uint64_t total_time_ns;    // Total time (DETAILED+)
    uint64_t min_time_ns;      // Minimum time (DETAILED+)
    uint64_t max_time_ns;      // Maximum time (DETAILED+)
} WordStats;
```

### Frequency Tracking Function

```c
void profiler_word_count(const DictEntry *entry) {
    if (!profiler_state.enabled || !entry) return;

    WordStats *stats = get_word_stats(entry);
    if (stats) {
        stats->call_count++;
    }
}
```

**Zero-overhead design:**

- No timing calls (just counter increment)
- O(1) lookup for existing entries
- O(n) only on first call per word
- Capacity: 256 unique words (expandable)

---

## Advanced Usage

### Profiling Specific Code Sections

```forth
\ Profile a specific algorithm
: BENCHMARK-SECTION
  \ ... your code here ...
;

\ Run multiple times for statistical significance
: RUN-PROFILE
  1000 0 DO BENCHMARK-SECTION LOOP
;

RUN-PROFILE
BYE
```

```bash
./build/starforth --profile 1 --profile-report < profile_test.fth
```

### Comparing Different Implementations

**Version A:**

```bash
./build/starforth --profile 1 --profile-report < version_a.fth > profile_a.txt
```

**Version B:**

```bash
./build/starforth --profile 1 --profile-report < version_b.fth > profile_b.txt
```

Compare call counts and execution patterns to determine which implementation is more efficient.

### Integration with CI/CD

```bash
#!/bin/bash
# profile-ci.sh - Automated profiling in CI

./build/starforth --profile 1 --profile-report < test_suite.fth > profile_report.txt

# Extract top 10 hot words
grep -A 10 "Most Frequently Called" profile_report.txt > hot_words.txt

# Check if any new hot words need optimization
# (add logic to compare with baseline)
```

---

## Troubleshooting

### Issue: "No profiling data available"

**Cause:** Profiler not enabled or report flag missing

**Solution:**

```bash
./build/starforth --profile 1 --profile-report  # Enable both flags
```

### Issue: Report shows 0 calls for all words

**Cause:** No Forth code executed (BYE called immediately)

**Solution:** Ensure your script executes Forth words before BYE

### Issue: Profiler overhead too high

**Cause:** Using PROFILE_DETAILED or PROFILE_VERBOSE

**Solution:** Use PROFILE_BASIC (--profile 1) for minimal overhead

### Issue: Missing words in report

**Cause:** Profiler capacity limit reached (256 unique words)

**Solution:** Profiler tracks first 256 unique words only. Primitive words always tracked.

---

## Example Workflows

### Workflow 1: Optimize a Recursive Function

```bash
# 1. Profile the recursive function
cat > fib_test.fth <<'EOF'
: FIB ( n -- fib )
  DUP 2 < IF DROP 1 EXIT THEN
  DUP 1 - FIB
  SWAP 2 - FIB
  +
;

10 FIB .
BYE
EOF

./build/starforth --profile 1 --profile-report < fib_test.fth

# 2. Identify hot words (DUP, <, DROP, -, +, SWAP)
# 3. Implement assembly optimizations for top words
# 4. Rebuild and verify performance gain
```

### Workflow 2: PGO-Guided Optimization

```bash
# 1. Build with profiling
make clean
make CFLAGS="-O2 -fprofile-generate" LDFLAGS="-fprofile-generate -lgcov"

# 2. Run profiler to identify hot Forth words
./build/starforth --profile 1 --profile-report --run-tests > forth_profile.txt

# 3. Run PGO workload
./scripts/pgo-workload.sh ./build/starforth

# 4. Build optimized binary with both profile types
make clean-obj
make CFLAGS="-O3 -DUSE_ASM_OPT=1 -fprofile-use" LDFLAGS="-fprofile-use"

# 5. Verify combined optimization
./build/starforth --benchmark 10000
```

---

## Integration with Other Tools

### With perf

```bash
# Combine StarForth profiler with Linux perf
sudo perf record -g ./build/starforth --profile 1 --run-tests
sudo perf report  # C-level profiling

# StarForth profiler shows Forth-level hot words
# perf shows C-level hot functions
```

### With Valgrind

```bash
# Get instruction counts alongside word frequency
valgrind --tool=callgrind ./build/starforth --profile 1 --benchmark 1000
kcachegrind callgrind.out.*
```

### With gprof

```bash
# Build with gprof
make CFLAGS="-pg -O2" LDFLAGS="-pg"

# Run with profiling
./build/starforth --profile 1 --run-tests

# Generate reports
gprof ./build/starforth gmon.out > gprof_report.txt
./build/starforth --profile 1 --profile-report --run-tests > forth_profile.txt
```

---

## API Reference

### C API

```c
// Initialize profiler
int profiler_init(ProfileLevel level);

// Shutdown profiler
void profiler_shutdown(void);

// Track word execution (lightweight)
void profiler_word_count(const DictEntry *entry);

// Track word execution with timing
void profiler_word_enter(const DictEntry *entry);
void profiler_word_exit(const DictEntry *entry);

// Generate reports
void profiler_generate_report(void);
void profiler_print_hotspots(void);

// Reset profiling data
void profiler_reset(void);
```

### Profiling Levels

```c
typedef enum {
    PROFILE_DISABLED = 0,  // No profiling
    PROFILE_BASIC = 1,     // Frequency tracking only
    PROFILE_DETAILED = 2,  // + timing data
    PROFILE_VERBOSE = 3,   // + stack/memory tracking
    PROFILE_FULL = 4       // Reserved for future use
} ProfileLevel;
```

---

## Performance Impact

| Level            | Overhead | Use Case                                     |
|------------------|----------|----------------------------------------------|
| PROFILE_DISABLED | 0%       | Production                                   |
| PROFILE_BASIC    | <1%      | Always-on profiling, hot word identification |
| PROFILE_DETAILED | 5-10%    | Detailed timing analysis                     |
| PROFILE_VERBOSE  | 15-20%   | Deep performance investigation               |

**Recommendation:** Use PROFILE_BASIC for continuous profiling, PROFILE_DETAILED for targeted optimization work.

---

## Summary

**Key Takeaways:**

1. **Start with `--profile 1 --profile-report`** - Minimal overhead, maximum insight
2. **Focus on top 10 words** - Usually 80-95% of execution time
3. **Optimize words with ≥5% coverage** - Best ROI for assembly optimization
4. **Use PGO for 0.5-5% words** - Let compiler optimize based on profile data
5. **Combine with perf/valgrind** - Multi-level profiling for complete picture

**Quick Reference:**

```bash
# Basic profiling
./build/starforth --profile 1 --profile-report < script.fth

# Detailed timing
./build/starforth --profile 2 --profile-report < script.fth

# Profile test suite
./build/starforth --profile 1 --profile-report --run-tests

# Profile with benchmarks
./build/starforth --profile 1 --profile-report --benchmark 10000
```

---

**End of Profiler Guide**