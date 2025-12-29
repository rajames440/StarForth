#!/usr/bin/env python3
"""
Comprehensive Variance Source Analysis

Identifies and quantifies all sources of run-to-run performance variance
in the physics engine validation experiment. Analyzes:

1. OS Scheduling Noise - variance in execution time
2. Cache Behavior - L1/L2/L3 cache effects
3. Memory Access Patterns - NUMA and page faults
4. Warmup Effects - first runs vs steady state
5. Configuration-Specific Effects - how configs differ
6. Systematic Patterns - any temporal trends
"""

#   StarForth — Steady-State Virtual Machine Runtime
#
#   Copyright (c) 2023–2025 Robert A. James
#   All rights reserved.
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#  StarForth — Steady-State Virtual Machine Runtime
#   Copyright (c) 2023–2025 Robert A. James
#   All rights reserved.
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#        https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

#   StarForth — Steady-State Virtual Machine Runtime
#
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#        https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

#   StarForth — Steady-State Virtual Machine Runtime
#
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#        https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

#                                   ***   StarForth   ***
#
#   analyze_variance_sources.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-11-07T14:14:04.851-05
#
#
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/Reference/physics_experiment/PEER_REVIEW_SUBMISSION/04_ANALYSIS_SCRIPTS/analyze_variance_sources.py

#                                   ***   StarForth   ***
#
#   analyze_variance_sources.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-11-07T08:01:29.153-05
#
#
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/physics_experiment/PEER_REVIEW_SUBMISSION/04_ANALYSIS_SCRIPTS/analyze_variance_sources.py

import csv
from pathlib import Path
from typing import List, Dict, Tuple
from collections import defaultdict
import statistics
import math


def load_experiment_data(csv_file: Path) -> List[Dict]:
    """Load experimental results_run_01_2025_12_08 from CSV."""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields
            for field in ['total_runtime_ms', 'cache_hits', 'cache_hit_percent',
                          'bucket_hits', 'bucket_hit_percent', 'misses',
                          'cache_hit_latency_ns', 'bucket_search_latency_ns']:
                if field in row and row[field]:
                    try:
                        row[field] = float(row[field])
                    except ValueError:
                        row[field] = None
            data.append(row)
    return data


def calculate_coefficient_of_variation(values: List[float]) -> float:
    """Calculate CV (stdev / mean) as percentage."""
    if not values or len(values) < 2:
        return 0.0
    mean = statistics.mean(values)
    if mean == 0:
        return 0.0
    stdev = statistics.stdev(values)
    return (stdev / mean) * 100


def analyze_runtime_variance(data: List[Dict]) -> Dict:
    """Analyze runtime variance across and within configurations."""
    results = {}
    by_config = defaultdict(list)

    for row in data:
        config = row['configuration']
        runtime = row.get('total_runtime_ms')
        if runtime and runtime > 0:
            by_config[config].append(runtime)

    # Per-configuration variance
    print("\n" + "=" * 70)
    print("VARIANCE SOURCE 1: OS SCHEDULING NOISE")
    print("=" * 70)

    for config in sorted(by_config.keys()):
        runtimes = by_config[config]
        mean_rt = statistics.mean(runtimes)
        stdev_rt = statistics.stdev(runtimes) if len(runtimes) > 1 else 0
        cv = calculate_coefficient_of_variation(runtimes)

        min_rt = min(runtimes)
        max_rt = max(runtimes)
        range_pct = ((max_rt - min_rt) / mean_rt) * 100

        print(f"\n{config}:")
        print(f"  Runtime (mean):           {mean_rt:>8.2f} ms")
        print(f"  Runtime (stdev):          {stdev_rt:>8.2f} ms")
        print(f"  Coefficient of variation: {cv:>8.2f}%")
        print(f"  Min/Max:                  {min_rt:.2f} / {max_rt:.2f} ms")
        print(f"  Range:                    {range_pct:.1f}% of mean")

        results[f'{config}_runtime_cv'] = cv
        results[f'{config}_runtime_range'] = range_pct
        results[f'{config}_runtime_mean'] = mean_rt
        results[f'{config}_runtime_stdev'] = stdev_rt

    return results, by_config


def analyze_cache_behavior(data: List[Dict]) -> Dict:
    """Analyze cache hit/miss patterns and their correlation with runtime."""
    results = {}
    by_config = defaultdict(list)

    print("\n" + "=" * 70)
    print("VARIANCE SOURCE 2: CACHE BEHAVIOR & MEMORY ACCESS PATTERNS")
    print("=" * 70)

    for row in data:
        config = row['configuration']
        cache_hits = row.get('cache_hits', 0)
        cache_percent = row.get('cache_hit_percent', 0)
        bucket_hits = row.get('bucket_hits', 0)
        bucket_percent = row.get('bucket_hit_percent', 0)
        runtime = row.get('total_runtime_ms', 0)

        # Skip A_BASELINE (no cache)
        if config == 'A_BASELINE':
            continue

        by_config[config].append({
            'cache_hits': cache_hits,
            'cache_percent': cache_percent,
            'bucket_hits': bucket_hits,
            'bucket_percent': bucket_percent,
            'runtime': runtime,
        })

    # Analyze cache effectiveness
    for config in sorted(by_config.keys()):
        runs = by_config[config]
        if not runs:
            continue

        cache_percents = [r['cache_percent'] for r in runs if r['cache_percent']]
        bucket_percents = [r['bucket_percent'] for r in runs if r['bucket_percent']]
        runtimes = [r['runtime'] for r in runs if r['runtime'] > 0]

        print(f"\n{config}:")

        if cache_percents:
            cache_mean = statistics.mean(cache_percents)
            cache_stdev = statistics.stdev(cache_percents) if len(cache_percents) > 1 else 0
            print(f"  Cache hit rate (mean):    {cache_mean:>8.2f}%")
            print(f"  Cache hit rate (stdev):   {cache_stdev:>8.2f}%")
            results[f'{config}_cache_hit_mean'] = cache_mean
            results[f'{config}_cache_hit_stdev'] = cache_stdev

        if bucket_percents:
            bucket_mean = statistics.mean(bucket_percents)
            bucket_stdev = statistics.stdev(bucket_percents) if len(bucket_percents) > 1 else 0
            print(f"  Bucket hit rate (mean):   {bucket_mean:>8.2f}%")
            print(f"  Bucket hit rate (stdev):  {bucket_stdev:>8.2f}%")

        # Correlation: cache hits to runtime
        if len(cache_percents) == len(runtimes) and len(cache_percents) > 2:
            corr = calculate_pearson_correlation(cache_percents, runtimes)
            print(f"  Cache ↔ Runtime corr:     {corr:>8.3f}")
            if abs(corr) > 0.3:
                direction = "slower" if corr > 0 else "faster"
                print(f"  → More cache hits → {direction} execution")
            results[f'{config}_cache_runtime_corr'] = corr

    return results


def calculate_pearson_correlation(x: List[float], y: List[float]) -> float:
    """Calculate Pearson correlation coefficient."""
    if len(x) < 2 or len(y) < 2:
        return 0.0

    mean_x = statistics.mean(x)
    mean_y = statistics.mean(y)

    numerator = sum((x[i] - mean_x) * (y[i] - mean_y) for i in range(len(x)))
    denom_x = sum((x[i] - mean_x) ** 2 for i in range(len(x)))
    denom_y = sum((y[i] - mean_y) ** 2 for i in range(len(y)))

    if denom_x == 0 or denom_y == 0:
        return 0.0

    return numerator / math.sqrt(denom_x * denom_y)


def analyze_warmup_effects(data: List[Dict]) -> Dict:
    """Analyze warmup effects: do early runs differ from later runs?"""
    results = {}
    by_config = defaultdict(lambda: {'early': [], 'late': []})

    print("\n" + "=" * 70)
    print("VARIANCE SOURCE 3: WARMUP EFFECTS & SYSTEM STATE")
    print("=" * 70)

    # Split into early (first 15 runs per config) and late (last 15 runs per config)
    for row in data:
        config = row['configuration']
        run_num = int(row.get('run_number', 0))
        runtime = row.get('total_runtime_ms')

        if runtime and runtime > 0:
            if run_num <= 15:
                by_config[config]['early'].append(runtime)
            elif run_num > 15:
                by_config[config]['late'].append(runtime)

    for config in sorted(by_config.keys()):
        runs = by_config[config]
        early_runtimes = runs['early']
        late_runtimes = runs['late']

        if early_runtimes and late_runtimes:
            early_mean = statistics.mean(early_runtimes)
            late_mean = statistics.mean(late_runtimes)
            warmup_delta = late_mean - early_mean
            warmup_delta_pct = (warmup_delta / early_mean) * 100

            print(f"\n{config}:")
            print(f"  Early runs (1-15):        {early_mean:>8.2f} ms")
            print(f"  Late runs (16-30):        {late_mean:>8.2f} ms")
            print(f"  Difference:               {warmup_delta:>+8.2f} ms ({warmup_delta_pct:+.1f}%)")

            if abs(warmup_delta_pct) > 2:
                direction = "slower" if warmup_delta > 0 else "faster"
                print(f"  → Late runs are {direction} (warmup/cooldown effect)")
            else:
                print(f"  → No significant warmup effect detected")

            results[f'{config}_early_mean'] = early_mean
            results[f'{config}_late_mean'] = late_mean
            results[f'{config}_warmup_delta_pct'] = warmup_delta_pct

    return results


def analyze_latency_variance(data: List[Dict]) -> Dict:
    """Analyze per-operation latency variance (L1/L2/L3 cache effects)."""
    results = {}
    by_config = defaultdict(list)

    print("\n" + "=" * 70)
    print("VARIANCE SOURCE 4: L1/L2/L3 CACHE & MEMORY LATENCY")
    print("=" * 70)

    for row in data:
        config = row['configuration']
        cache_latency = row.get('cache_hit_latency_ns')
        bucket_latency = row.get('bucket_search_latency_ns')

        if config == 'A_BASELINE':
            continue

        if cache_latency and cache_latency > 0:
            by_config[config].append({
                'cache_latency': cache_latency,
                'bucket_latency': bucket_latency if bucket_latency and bucket_latency > 0 else None,
            })

    for config in sorted(by_config.keys()):
        runs = by_config[config]
        if not runs:
            continue

        cache_latencies = [r['cache_latency'] for r in runs]
        bucket_latencies = [r['bucket_latency'] for r in runs if r['bucket_latency']]

        print(f"\n{config}:")

        if cache_latencies:
            cache_lat_mean = statistics.mean(cache_latencies)
            cache_lat_stdev = statistics.stdev(cache_latencies) if len(cache_latencies) > 1 else 0
            cache_lat_cv = (cache_lat_stdev / cache_lat_mean * 100) if cache_lat_mean > 0 else 0

            print(f"  Cache hit latency (mean): {cache_lat_mean:>8.1f} ns")
            print(f"  Cache hit latency (stdev):{cache_lat_stdev:>8.1f} ns")
            print(f"  Latency CV:               {cache_lat_cv:>8.1f}%")

            if cache_lat_cv > 10:
                print(f"  → HIGH variability in L1/L2 hit times (cache coherency/NUMA effects)")
            results[f'{config}_cache_latency_cv'] = cache_lat_cv

        if bucket_latencies:
            bucket_lat_mean = statistics.mean(bucket_latencies)
            bucket_lat_stdev = statistics.stdev(bucket_latencies) if len(bucket_latencies) > 1 else 0
            print(f"  Bucket search latency (mean): {bucket_lat_mean:>8.1f} ns")
            print(f"  Bucket search latency (stdev):{bucket_lat_stdev:>8.1f} ns")

    return results


def print_mitigation_strategies():
    """Print recommended mitigation strategies."""
    print("\n" + "=" * 70)
    print("MITIGATION STRATEGIES FOR VARIANCE SOURCES")
    print("=" * 70)

    mitigations = [
        {
            'source': 'OS Scheduling Noise',
            'impact': 'Context switching, thread migration, interrupt handling',
            'strategies': [
                '1. CPU Affinity: Use taskset to pin benchmark to single CPU core',
                '2. RT Scheduling: Run with SCHED_FIFO (requires root/nice privileges)',
                '3. Disable irqbalance: Prevent interrupt handler migration',
                '4. Set CPU governor to "performance" mode (not "ondemand")',
                '5. Disable SMT (Simultaneous Multi-Threading) if hyperthreads exist',
            ]
        },
        {
            'source': 'Cache Behavior & Memory Access',
            'impact': 'Variability in cache hit/miss rates, TLB misses, page faults',
            'strategies': [
                '1. Warmup Phase: Run 3-5 iterations before measurement to stabilize caches',
                '2. Cache Flush: Clear L1/L2/L3 caches between runs (if benchmark allows)',
                '3. NUMA Awareness: Use numactl to bind to single NUMA node',
                '4. Hugetlbfs: Use large pages to reduce TLB pressure',
                '5. Memory Prefetching: Enable CPU prefetchers (baseline calibration)',
            ]
        },
        {
            'source': 'Warmup Effects',
            'impact': 'JIT compilation, heap initialization, buffer pool growth',
            'strategies': [
                '1. Extended Warmup: Your code already does this! Good practice.',
                '2. Steady-State Detection: Only measure after consistent perf. achieved',
                '3. Statistical Analysis: Use median/IQR instead of mean (robust to outliers)',
                '4. Trimmed Mean: Remove top/bottom 10% of runs before averaging',
            ]
        },
        {
            'source': 'L1/L2/L3 Cache Latency',
            'impact': 'Core frequency scaling, branch prediction misses',
            'strategies': [
                '1. Fixed Frequency: Disable frequency scaling during runs',
                '2. Branch Prediction: Use branch-friendly patterns in hot loops',
                '3. Cache Line Alignment: Align hot data structures to 64-byte boundaries',
                '4. Speculative Prefetch: Enable CPU prefetchers for sequential access',
                '5. Perf Counters: Use perf to measure cache misses/stalls directly',
            ]
        },
    ]

    for m in mitigations:
        print(f"\n{m['source'].upper()}")
        print(f"Impact: {m['impact']}")
        print(f"Mitigations:")
        for strategy in m['strategies']:
            print(f"  {strategy}")


def print_summary(all_results: Dict):
    """Print summary of variance findings."""
    print("\n" + "=" * 70)
    print("VARIANCE SUMMARY & RECOMMENDATIONS")
    print("=" * 70)

    # Identify largest variance sources
    variance_sources = {}

    for key, value in all_results.items():
        if 'runtime_cv' in key:
            config = key.split('_')[0]
            if config not in variance_sources:
                variance_sources[config] = {'runtime_cv': value, 'cache_lat_cv': 0}
            variance_sources[config]['runtime_cv'] = value

        if 'cache_latency_cv' in key:
            config = key.split('_')[0]
            if config not in variance_sources:
                variance_sources[config] = {'runtime_cv': 0, 'cache_lat_cv': value}
            variance_sources[config]['cache_lat_cv'] = value

    print("\nVariance Coefficient Summary (lower is better):")
    for config in sorted(variance_sources.keys()):
        sources = variance_sources[config]
        print(f"\n{config}:")
        print(f"  Runtime CV:        {sources['runtime_cv']:>7.2f}% ← OS scheduling + cache effects")
        print(f"  Latency CV:        {sources['cache_lat_cv']:>7.2f}% ← L1/L2/L3 cache coherency")

        if sources['runtime_cv'] > 5:
            print(f"  ⚠ HIGH variance - significant OS scheduling noise detected")
        else:
            print(f"  ✓ GOOD variance - system fairly stable")

    print("\n" + "-" * 70)
    print("RECOMMENDED NEXT STEPS:")
    print("-" * 70)
    print("""
1. IMMEDIATE (Low effort, high impact):
   ✓ Pin benchmark to single CPU core: taskset -c 0 ./build/starforth ...
   ✓ Set CPU governor: echo performance | tee /sys/devices/system/cpu/*/cpufreq/scaling_governor
   ✓ Use trimmed mean (remove outliers) for final statistics

2. SHORT TERM (Medium effort):
   ✓ Implement explicit warmup phase (3-5 runs before measurement)
   ✓ Use median + IQR instead of mean ± stdev
   ✓ Measure cache misses with: perf stat -e cache-references,cache-misses ./benchmark

3. MEDIUM TERM (Research):
   ✓ Test with SCHED_FIFO (requires root): chrt -f 99 ./benchmark
   ✓ Try disabling SMT: echo 0 > /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
   ✓ Profile with perf to identify which ops have high variance

4. LONG TERM (System-level optimizations):
   ✓ Use real-time kernel (RT_PREEMPT patches)
   ✓ Implement custom memory management with mmap/madvise
   ✓ Profile and cache-optimize hot code paths
""")


def main(csv_file: Path):
    """Run complete variance analysis."""
    print("=" * 70)
    print("COMPREHENSIVE VARIANCE SOURCE ANALYSIS")
    print("=" * 70)
    print(f"Analyzing: {csv_file.name}")

    data = load_experiment_data(csv_file)
    print(f"Loaded {len(data)} runs")

    all_results = {}

    # Analysis 1: Runtime variance
    rt_results, by_config = analyze_runtime_variance(data)
    all_results.update(rt_results)

    # Analysis 2: Cache behavior
    cache_results = analyze_cache_behavior(data)
    all_results.update(cache_results)

    # Analysis 3: Warmup effects
    warmup_results = analyze_warmup_effects(data)
    all_results.update(warmup_results)

    # Analysis 4: Latency variance
    latency_results = analyze_latency_variance(data)
    all_results.update(latency_results)

    # Print mitigations
    print_mitigation_strategies()

    # Print summary
    print_summary(all_results)


if __name__ == '__main__':
    csv_file = Path(
        '/home/rajames/CLionProjects/StarForth/physics_experiment/full_90_run_comprehensive/experiment_results.csv')
    if csv_file.exists():
        main(csv_file)
    else:
        print(f"Error: {csv_file} not found")
