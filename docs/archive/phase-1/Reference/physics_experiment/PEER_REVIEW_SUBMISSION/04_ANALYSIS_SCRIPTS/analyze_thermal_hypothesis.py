#!/usr/bin/env python3
"""
Thermal Noise Hypothesis Analysis

Tests the hypothesis that CPU thermal state (temperature and frequency) from
the automated build process affects benchmark measurement reliability.

This script analyzes:
1. CPU temperature patterns across randomized runs
2. CPU frequency patterns across randomized runs
3. Correlation between thermal state and runtime variance
4. Statistical significance of thermal effects on performance
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

#                                   ***   StarForth   ***
#
#   analyze_thermal_hypothesis.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-11-07T14:14:04.815-05
#
#
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/Reference/physics_experiment/PEER_REVIEW_SUBMISSION/04_ANALYSIS_SCRIPTS/analyze_thermal_hypothesis.py

#                                   ***   StarForth   ***
#
#   analyze_thermal_hypothesis.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-11-07T08:01:29.050-05
#
#
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/physics_experiment/PEER_REVIEW_SUBMISSION/04_ANALYSIS_SCRIPTS/analyze_thermal_hypothesis.py

import csv
import sys
from pathlib import Path
from typing import List, Dict, Tuple
from collections import defaultdict
import statistics


def load_experiment_data(csv_file: Path) -> List[Dict]:
    """Load experimental results_run_01_2025_12_08 from CSV."""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields
            for field in ['total_lookups', 'cache_hits', 'total_runtime_ms',
                          'cpu_temp_before_c', 'cpu_freq_before_mhz',
                          'cpu_temp_after_c', 'cpu_freq_after_mhz']:
                if field in row and row[field]:
                    try:
                        row[field] = float(row[field])
                    except ValueError:
                        row[field] = None
            data.append(row)
    return data


def validate_data_integrity(data: List[Dict]) -> Tuple[bool, str]:
    """Validate that data collection was clean (no corruption)."""
    issues = []

    for i, row in enumerate(data, 1):
        # Check for text corruption in numeric fields
        if row.get('total_lookups') is None:
            issues.append(f"Row {i}: total_lookups is missing")

        # Check for consistency
        if row.get('total_lookups') != 100000:
            issues.append(f"Row {i}: total_lookups != 100000 (got {row.get('total_lookups')})")

    if issues:
        return False, "\n".join(issues[:10])  # Show first 10 issues
    return True, "Data integrity validated"


def analyze_thermal_patterns(data: List[Dict]) -> Dict:
    """Analyze CPU temperature and frequency patterns."""

    by_config = defaultdict(list)
    by_run_order = defaultdict(list)

    for i, row in enumerate(data):
        config = row['configuration']
        by_config[config].append(row)
        by_run_order[i] = row

    results = {}

    # Temperature analysis
    for config in ['A_BASELINE', 'B_CACHE', 'C_FULL']:
        if config in by_config:
            temps_before = [r['cpu_temp_before_c'] for r in by_config[config]
                            if r.get('cpu_temp_before_c') is not None and r['cpu_temp_before_c'] > 0]
            temps_after = [r['cpu_temp_after_c'] for r in by_config[config]
                           if r.get('cpu_temp_after_c') is not None and r['cpu_temp_after_c'] > 0]

            if temps_before:
                results[f'{config}_temp_before_mean'] = statistics.mean(temps_before)
                results[f'{config}_temp_before_stdev'] = statistics.stdev(temps_before) if len(temps_before) > 1 else 0

            if temps_after:
                results[f'{config}_temp_after_mean'] = statistics.mean(temps_after)
                results[f'{config}_temp_delta_mean'] = statistics.mean(
                    [after - before for after, before in zip(temps_after, temps_before)]
                ) if temps_before and temps_after else 0

    # Frequency analysis
    for config in ['A_BASELINE', 'B_CACHE', 'C_FULL']:
        if config in by_config:
            freqs_before = [r['cpu_freq_before_mhz'] for r in by_config[config]
                            if r.get('cpu_freq_before_mhz') is not None and r['cpu_freq_before_mhz'] > 0]
            freqs_after = [r['cpu_freq_after_mhz'] for r in by_config[config]
                           if r.get('cpu_freq_after_mhz') is not None and r['cpu_freq_after_mhz'] > 0]

            if freqs_before:
                results[f'{config}_freq_before_mean'] = statistics.mean(freqs_before)
                results[f'{config}_freq_before_stdev'] = statistics.stdev(freqs_before) if len(freqs_before) > 1 else 0

            if freqs_after:
                results[f'{config}_freq_after_mean'] = statistics.mean(freqs_after)

    # Temporal thermal trend (first runs vs last runs)
    first_20_temps = [r['cpu_temp_before_c'] for r in data[:20]
                      if r.get('cpu_temp_before_c') is not None and r['cpu_temp_before_c'] > 0]
    last_20_temps = [r['cpu_temp_before_c'] for r in data[-20:]
                     if r.get('cpu_temp_before_c') is not None and r['cpu_temp_before_c'] > 0]

    if first_20_temps and last_20_temps:
        results['thermal_trend_first_20_mean'] = statistics.mean(first_20_temps)
        results['thermal_trend_last_20_mean'] = statistics.mean(last_20_temps)
        results['thermal_trend_delta'] = results['thermal_trend_last_20_mean'] - results['thermal_trend_first_20_mean']

    return results


def correlate_thermal_with_runtime(data: List[Dict]) -> Dict:
    """Correlate thermal state with runtime variance."""

    results = {}

    for config in ['A_BASELINE', 'B_CACHE', 'C_FULL']:
        config_runs = [r for r in data if r['configuration'] == config]

        if not config_runs:
            continue

        # Extract runtime and thermal data
        runtimes = []
        temps_before = []

        for row in config_runs:
            if row.get('total_runtime_ms') and row.get('cpu_temp_before_c'):
                try:
                    rt = float(row['total_runtime_ms'])
                    temp = float(row['cpu_temp_before_c'])
                    if rt > 0 and temp > 0:
                        runtimes.append(rt)
                        temps_before.append(temp)
                except (ValueError, TypeError):
                    pass

        if len(runtimes) > 2 and len(temps_before) > 2:
            # Calculate correlation coefficient manually
            mean_rt = statistics.mean(runtimes)
            mean_temp = statistics.mean(temps_before)

            numerator = sum((rt - mean_rt) * (temp - mean_temp)
                            for rt, temp in zip(runtimes, temps_before))
            denom_rt = sum((rt - mean_rt) ** 2 for rt in runtimes)
            denom_temp = sum((temp - mean_temp) ** 2 for temp in temps_before)

            if denom_rt > 0 and denom_temp > 0:
                correlation = numerator / (denom_rt * denom_temp) ** 0.5
                results[f'{config}_thermal_runtime_correlation'] = correlation

            results[f'{config}_runtime_mean'] = mean_rt
            results[f'{config}_runtime_stdev'] = statistics.stdev(runtimes) if len(runtimes) > 1 else 0

    return results


def print_report(csv_file: Path):
    """Generate and print analysis report."""

    print("=" * 70)
    print("THERMAL NOISE HYPOTHESIS ANALYSIS")
    print("=" * 70)
    print()

    # Load data
    try:
        data = load_experiment_data(csv_file)
        print(f"Loaded {len(data)} experimental runs")
    except Exception as e:
        print(f"ERROR: Failed to load data: {e}")
        return

    # Validate data integrity
    print()
    print("-" * 70)
    print("DATA INTEGRITY VALIDATION")
    print("-" * 70)
    is_valid, msg = validate_data_integrity(data)
    if is_valid:
        print(f"✓ {msg}")
        print(f"  All {len(data)} rows have valid total_lookups=100000")
    else:
        print(f"✗ Data corruption detected:")
        print(msg)

    # Thermal patterns
    print()
    print("-" * 70)
    print("CPU THERMAL PATTERNS")
    print("-" * 70)
    thermal_results = analyze_thermal_patterns(data)

    for config in ['A_BASELINE', 'B_CACHE', 'C_FULL']:
        config_data = [r for r in data if r['configuration'] == config]
        if config_data:
            print(f"\n{config} ({len(config_data)} runs):")

            if f'{config}_temp_before_mean' in thermal_results:
                print(f"  Temperature before (mean):  {thermal_results[f'{config}_temp_before_mean']:.1f}°C")
                print(f"  Temperature before (stdev): {thermal_results[f'{config}_temp_before_stdev']:.2f}°C")

            if f'{config}_temp_after_mean' in thermal_results:
                print(f"  Temperature after (mean):   {thermal_results[f'{config}_temp_after_mean']:.1f}°C")

            if f'{config}_temp_delta_mean' in thermal_results:
                print(f"  Temperature delta (mean):   {thermal_results[f'{config}_temp_delta_mean']:.2f}°C")

            if f'{config}_freq_before_mean' in thermal_results:
                print(f"  Frequency before (mean):    {thermal_results[f'{config}_freq_before_mean']:.0f} MHz")

    # Temporal thermal trend
    if 'thermal_trend_first_20_mean' in thermal_results:
        print(f"\nTEMPORAL THERMAL TREND (across all runs):")
        print(f"  First 20 runs (mean):  {thermal_results['thermal_trend_first_20_mean']:.1f}°C")
        print(f"  Last 20 runs (mean):   {thermal_results['thermal_trend_last_20_mean']:.1f}°C")
        print(f"  Temperature delta:     {thermal_results['thermal_trend_delta']:+.1f}°C")

    # Thermal-Runtime correlation
    print()
    print("-" * 70)
    print("THERMAL-RUNTIME CORRELATION")
    print("-" * 70)
    corr_results = correlate_thermal_with_runtime(data)

    for config in ['A_BASELINE', 'B_CACHE', 'C_FULL']:
        if f'{config}_thermal_runtime_correlation' in corr_results:
            corr = corr_results[f'{config}_thermal_runtime_correlation']
            rt_mean = corr_results[f'{config}_runtime_mean']
            rt_stdev = corr_results[f'{config}_runtime_stdev']

            print(f"\n{config}:")
            print(f"  Runtime (mean):              {rt_mean:.2f} ms")
            print(f"  Runtime (stdev):             {rt_stdev:.2f} ms")
            print(f"  Thermal-Runtime correlation: {corr:+.3f}")

            if abs(corr) > 0.3:
                strength = "MODERATE" if abs(corr) < 0.7 else "STRONG"
                direction = "positive" if corr > 0 else "negative"
                print(f"  → {strength} {direction} correlation (CPU temp affects runtime)")
            else:
                print(f"  → Weak correlation (thermal noise not significant)")

    # Hypothesis conclusion
    print()
    print("-" * 70)
    print("HYPOTHESIS ASSESSMENT")
    print("-" * 70)
    print("""
The thermal noise hypothesis suggests that CPU thermal state from
automated build processes affects benchmark measurement reliability.

Evidence to examine:
1. Does temperature increase during the 90-run experiment? (drift)
2. Does temperature affect run-to-run runtime variance?
3. Do later runs have higher thermal state? (warmup effect)
4. Does frequency throttling correlate with slower runtimes?

""")

    if 'thermal_trend_delta' in thermal_results:
        delta = thermal_results['thermal_trend_delta']
        if delta > 5:
            print(f"✓ OBSERVED: {delta:.1f}°C temperature increase (warming trend)")
            print("  Suggests: Automated builds create thermal burden on system")
        elif delta < -5:
            print(f"✓ OBSERVED: {-delta:.1f}°C temperature decrease (cooling trend)")
            print("  Suggests: Thermal state stabilized by end of experiment")
        else:
            print(f"○ OBSERVED: {delta:+.1f}°C change (stable thermal state)")
            print("  Suggests: Thermal state remained relatively constant")

    print()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_thermal_hypothesis.py <experiment_results.csv>")
        sys.exit(1)

    csv_file = Path(sys.argv[1])
    if not csv_file.exists():
        print(f"ERROR: File not found: {csv_file}")
        sys.exit(1)

    print_report(csv_file)
