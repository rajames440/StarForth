#!/usr/bin/env python3
"""
OPP #2 Regression Detection Analysis
Analyzes window width optimization results_run_01_2025_12_08 against OPP #1 baseline.
Detects 6 types of optimization regressions as per REGRESSION_DETECTION_FRAMEWORK.md
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

import csv
import sys
from collections import defaultdict
import statistics

# OPP #1 Baseline (from previous experiments)
OPP1_BASELINE_NS = 5947974  # Mean execution time from OPP #1

# Regression thresholds (from REGRESSION_DETECTION_FRAMEWORK.md)
REGRESSION_THRESHOLDS = {
    'time_increase': 5.0,        # >5% slower = FAIL
    'variance_increase': 2.0,     # >2x variance increase = WARN
    'p99_increase': 8.0,          # >8% p99 slower = WARN
    'heat_ratio_change': 3.0,     # >3% hot/stale ratio change = WARN
    'cache_hit_drop': 5.0,        # >5% cache hit decrease = WARN
}

def load_and_parse_csv(filepath):
    """Load OPP #2 results_run_01_2025_12_08 CSV and parse metrics"""
    results = defaultdict(list)

    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            config = row['configuration']

            # Parse key metrics (Q48.16 format: divide by 65536)
            metrics = {
                'vm_workload_duration_ns': int(row['vm_workload_duration_ns_q48']) / 65536,
                'cache_hit_percent': float(row['cache_hit_percent']),
                'context_accuracy_percent': float(row['context_accuracy_percent']),
                'prefetch_accuracy_percent': float(row['prefetch_accuracy_percent']),
                'hot_word_count': int(row['hot_word_count']),
                'stale_word_ratio': float(row['stale_word_ratio']),
            }

            results[config].append(metrics)

    return results

def compute_stats(values):
    """Compute statistics for a list of values"""
    sorted_vals = sorted(values)
    n = len(sorted_vals)

    return {
        'mean': statistics.mean(values),
        'stdev': statistics.stdev(values) if n > 1 else 0,
        'min': min(values),
        'max': max(values),
        'p50': sorted_vals[n // 2],
        'p95': sorted_vals[int(n * 0.95)],
        'p99': sorted_vals[int(n * 0.99)] if n >= 100 else sorted_vals[-1],
        'count': n,
    }

def detect_metric_inversion(config_stats):
    """REGRESSION TYPE 1: Metric inversion (better metric gets worse)"""
    regressions = []

    for config, stats_list in config_stats.items():
        for metric_name in ['cache_hit_percent', 'context_accuracy_percent', 'prefetch_accuracy_percent']:
            values = [m[metric_name] for m in stats_list]
            stat = compute_stats(values)

            # These metrics should not decrease significantly
            if stat['mean'] < 70.0:  # Arbitrary low threshold for quality metrics
                regressions.append({
                    'type': 'METRIC_INVERSION',
                    'config': config,
                    'metric': metric_name,
                    'severity': 'MINOR',
                    'mean': stat['mean'],
                    'detail': f"{metric_name} low at {stat['mean']:.2f}%"
                })

    return regressions

def detect_variance_introduction(config_stats):
    """REGRESSION TYPE 2: Variance introduction (variance significantly increases)"""
    regressions = []

    for config, stats_list in config_stats.items():
        exec_times = [m['vm_workload_duration_ns'] for m in stats_list]
        exec_stats = compute_stats(exec_times)
        cv = (exec_stats['stdev'] / exec_stats['mean']) * 100  # Coefficient of variation

        # High variance may indicate instability
        if cv > 20.0:  # >20% CV suggests high variability
            regressions.append({
                'type': 'VARIANCE_INTRODUCTION',
                'config': config,
                'metric': 'vm_workload_duration_ns',
                'severity': 'WARN',
                'cv': cv,
                'stdev': exec_stats['stdev'],
                'detail': f"High execution time variance (CV={cv:.2f}%)"
            })

    return regressions

def detect_tail_latency_increase(config_stats):
    """REGRESSION TYPE 3: Tail latency increase (p99 gets worse)"""
    regressions = []

    for config, stats_list in config_stats.items():
        exec_times = [m['vm_workload_duration_ns'] for m in stats_list]
        exec_stats = compute_stats(exec_times)

        # Calculate p99 regression vs baseline
        p99_delta_pct = ((exec_stats['p99'] - OPP1_BASELINE_NS) / OPP1_BASELINE_NS) * 100

        if p99_delta_pct > REGRESSION_THRESHOLDS['p99_increase']:
            regressions.append({
                'type': 'TAIL_LATENCY_INCREASE',
                'config': config,
                'metric': 'p99_latency_ns',
                'severity': 'WARN',
                'p99': exec_stats['p99'],
                'delta_pct': p99_delta_pct,
                'detail': f"P99 latency {p99_delta_pct:.2f}% above baseline"
            })

    return regressions

def detect_mean_time_increase(config_stats):
    """PRIMARY REGRESSION: Mean execution time increase"""
    regressions = []

    for config, stats_list in config_stats.items():
        exec_times = [m['vm_workload_duration_ns'] for m in stats_list]
        exec_stats = compute_stats(exec_times)

        # Calculate mean regression vs baseline
        time_delta_pct = ((exec_stats['mean'] - OPP1_BASELINE_NS) / OPP1_BASELINE_NS) * 100

        if time_delta_pct > REGRESSION_THRESHOLDS['time_increase']:
            regressions.append({
                'type': 'TIME_INCREASE',
                'config': config,
                'severity': 'FAIL',
                'mean': exec_stats['mean'],
                'delta_pct': time_delta_pct,
                'detail': f"Execution time {time_delta_pct:.2f}% above baseline (REGRESSION)"
            })
        elif time_delta_pct > -2.0:  # Marginal improvement or no improvement
            regressions.append({
                'type': 'MARGINAL_IMPROVEMENT',
                'config': config,
                'severity': 'INFO',
                'mean': exec_stats['mean'],
                'delta_pct': time_delta_pct,
                'detail': f"Marginal improvement: {time_delta_pct:.2f}% vs baseline"
            })

    return regressions

def detect_cache_hit_drop(config_stats):
    """REGRESSION TYPE 4: Cache hit rate significant drop"""
    regressions = []

    for config, stats_list in config_stats.items():
        cache_hits = [m['cache_hit_percent'] for m in stats_list]
        cache_stats = compute_stats(cache_hits)

        # If cache hit rate is notably low
        if cache_stats['mean'] < 80.0:
            regressions.append({
                'type': 'LOW_CACHE_HIT_RATE',
                'config': config,
                'severity': 'INFO',
                'mean_hit_rate': cache_stats['mean'],
                'detail': f"Cache hit rate {cache_stats['mean']:.2f}% (below 80% target)"
            })

    return regressions

def detect_parameter_interaction(configs_stats):
    """REGRESSION TYPE 6: Parameter interaction effects"""
    regressions = []

    # Check if improvement varies significantly across window sizes
    improvements = {}
    for config, stats_list in configs_stats.items():
        exec_times = [m['vm_workload_duration_ns'] for m in stats_list]
        exec_stats = compute_stats(exec_times)
        delta_pct = ((exec_stats['mean'] - OPP1_BASELINE_NS) / OPP1_BASELINE_NS) * 100
        improvements[config] = delta_pct

    if improvements:
        best = max(improvements.values())
        worst = min(improvements.values())
        spread = best - worst

        # High spread suggests parameter sensitivity
        if spread > 8.0:  # >8% spread is significant
            regressions.append({
                'type': 'PARAMETER_SENSITIVITY',
                'severity': 'WARN',
                'improvement_spread_pct': spread,
                'best_config': [k for k, v in improvements.items() if v == best][0],
                'worst_config': [k for k, v in improvements.items() if v == worst][0],
                'detail': f"High sensitivity to window size: {spread:.2f}% spread"
            })

    return regressions

def generate_report(results_csv):
    """Generate comprehensive regression report"""
    print("=" * 70)
    print("OPP #2 REGRESSION DETECTION ANALYSIS")
    print("=" * 70)
    print()

    # Load and parse results_run_01_2025_12_08
    config_stats = load_and_parse_csv(results_csv)

    print(f"OPP #1 Baseline (reference): {OPP1_BASELINE_NS:,.0f} ns")
    print()

    # Analyze each configuration
    print("CONFIGURATION ANALYSIS:")
    print("-" * 70)

    for config in sorted(config_stats.keys()):
        stats_list = config_stats[config]
        exec_times = [m['vm_workload_duration_ns'] for m in stats_list]
        exec_stats = compute_stats(exec_times)

        time_delta_pct = ((exec_stats['mean'] - OPP1_BASELINE_NS) / OPP1_BASELINE_NS) * 100
        cv = (exec_stats['stdev'] / exec_stats['mean']) * 100

        # Extract cache hit average
        cache_hits = [m['cache_hit_percent'] for m in stats_list]
        cache_stats = compute_stats(cache_hits)

        print(f"\n{config}:")
        print(f"  Execution Time:")
        print(f"    Mean:       {exec_stats['mean']:>12,.0f} ns")
        print(f"    Stdev:      {exec_stats['stdev']:>12,.0f} ns")
        print(f"    CV:         {cv:>12.2f}%")
        print(f"    Min:        {exec_stats['min']:>12,.0f} ns")
        print(f"    P50:        {exec_stats['p50']:>12,.0f} ns")
        print(f"    P95:        {exec_stats['p95']:>12,.0f} ns")
        print(f"    P99:        {exec_stats['p99']:>12,.0f} ns")
        print(f"    vs Baseline: {time_delta_pct:>11.2f}%")
        print(f"  Cache Hit Rate: {cache_stats['mean']:.2f}%")
        print(f"  Samples:    {exec_stats['count']} runs")

    print()
    print("=" * 70)
    print("REGRESSION DETECTION RESULTS:")
    print("=" * 70)

    # Run all regression detectors
    all_regressions = []
    all_regressions.extend(detect_mean_time_increase(config_stats))
    all_regressions.extend(detect_variance_introduction(config_stats))
    all_regressions.extend(detect_tail_latency_increase(config_stats))
    all_regressions.extend(detect_metric_inversion(config_stats))
    all_regressions.extend(detect_cache_hit_drop(config_stats))
    all_regressions.extend(detect_parameter_interaction(config_stats))

    # Group by severity
    fails = [r for r in all_regressions if r['severity'] == 'FAIL']
    warns = [r for r in all_regressions if r['severity'] == 'WARN']
    infos = [r for r in all_regressions if r['severity'] in ['INFO', 'MARGINAL_IMPROVEMENT']]

    if fails:
        print()
        print("FAILURES (Blocking):")
        for reg in fails:
            print(f"  ✗ [{reg['config']}] {reg['type']}")
            print(f"    {reg['detail']}")

    if warns:
        print()
        print("WARNINGS (Review Required):")
        for reg in warns:
            config = reg.get('config', 'SYSTEM')
            print(f"  ⚠ [{config}] {reg['type']}")
            print(f"    {reg['detail']}")

    if infos:
        print()
        print("INFORMATION:")
        for reg in infos:
            config = reg.get('config', 'SYSTEM')
            symbol = "✓" if "improvement" in reg['type'].lower() else "ℹ"
            print(f"  {symbol} [{config}] {reg['type']}")
            print(f"    {reg['detail']}")

    print()
    print("=" * 70)

    # Determine winner
    print("OPTIMIZATION DECISION:")
    print("-" * 70)

    config_improvements = {}
    for config, stats_list in config_stats.items():
        exec_times = [m['vm_workload_duration_ns'] for m in stats_list]
        exec_stats = compute_stats(exec_times)
        delta_pct = ((exec_stats['mean'] - OPP1_BASELINE_NS) / OPP1_BASELINE_NS) * 100
        config_improvements[config] = delta_pct

    best_config = min(config_improvements.items(), key=lambda x: x[1])

    if best_config[1] > 0:
        print(f"⚠ WARNING: All configurations show regression vs baseline!")
        print(f"Best result: {best_config[0]} with {best_config[1]:+.2f}%")
    else:
        print(f"✓ Best configuration: {best_config[0]}")
        print(f"  Improvement: {best_config[1]:.2f}% faster than OPP #1 baseline")
        print()
        print("Recommendation: Lock this configuration for OPP #3")

    print()
    print(f"Total regressions detected: {len(all_regressions)}")
    print(f"  - Failures: {len(fails)}")
    print(f"  - Warnings: {len(warns)}")
    print(f"  - Informational: {len(infos)}")
    print()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <experiment_results.csv>")
        sys.exit(1)

    csv_file = sys.argv[1]
    generate_report(csv_file)
