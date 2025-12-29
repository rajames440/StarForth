#!/usr/bin/env python3
"""Quick analysis of window_scaling heartbeat data"""

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

import os
import glob
import csv
from collections import defaultdict
import statistics

# Find all heartbeat CSV files
hb_dir = "results_run_one/raw/hb"
files = sorted(glob.glob(os.path.join(hb_dir, "run-*.csv")))

print(f"Found {len(files)} heartbeat files\n")

# Collect per-run summary
runs = []
window_sizes = defaultdict(list)
k_values = defaultdict(list)

for filepath in files:
    run_id = os.path.basename(filepath).replace("run-", "").replace(".csv", "")

    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

        if not rows:
            continue

        # Get W_max from first row
        w_max = int(rows[0]['window_width'])

        # Collect tick intervals for frequency calculation
        intervals = [float(row['tick_interval_ns']) for row in rows[1:] if float(row['tick_interval_ns']) < 1e9]

        if not intervals:
            continue

        # Calculate frequency (1/mean_interval in Hz)
        mean_interval_ns = statistics.mean(intervals)
        freq_hz = 1e9 / mean_interval_ns if mean_interval_ns > 0 else 0

        # Get K statistics
        k_vals = [float(row['K_approx']) for row in rows if float(row['K_approx']) > 0]
        mean_k = statistics.mean(k_vals) if k_vals else 0

        # Store
        window_sizes[w_max].append(freq_hz)
        k_values[w_max].append(mean_k)

        runs.append({
            'run_id': run_id,
            'w_max': w_max,
            'freq_hz': freq_hz,
            'mean_k': mean_k,
            'ticks': len(rows)
        })

# Summary by window size
print("="*70)
print("FREQUENCY (ω₀) BY WINDOW SIZE")
print("="*70)
print(f"{'W_max':<10} {'Runs':<6} {'Mean ω₀ (Hz)':<15} {'Std Dev':<10} {'CV (%)':<8}")
print("-"*70)

for w in sorted(window_sizes.keys()):
    freqs = window_sizes[w]
    if len(freqs) > 1:
        mean_f = statistics.mean(freqs)
        std_f = statistics.stdev(freqs)
        cv = (std_f / mean_f * 100) if mean_f > 0 else 0
        print(f"{w:<10} {len(freqs):<6} {mean_f:<15.3f} {std_f:<10.3f} {cv:<8.2f}")

print("\n")
print("="*70)
print("JAMES LAW K STATISTIC BY WINDOW SIZE")
print("="*70)
print(f"{'W_max':<10} {'Runs':<6} {'Mean K':<12} {'Std Dev':<10} {'|K-1|':<8}")
print("-"*70)

for w in sorted(k_values.keys()):
    k_vals = k_values[w]
    if len(k_vals) > 1:
        mean_k = statistics.mean(k_vals)
        std_k = statistics.stdev(k_vals)
        dev_from_1 = abs(mean_k - 1.0)
        print(f"{w:<10} {len(k_vals):<6} {mean_k:<12.6f} {std_k:<10.6f} {dev_from_1:<8.6f}")

print("\n")
print("="*70)
print("KEY FINDINGS")
print("="*70)

# Calculate overall stats
all_freqs = [r['freq_hz'] for r in runs]
all_k = [r['mean_k'] for r in runs]

if all_freqs:
    overall_freq = statistics.mean(all_freqs)
    overall_freq_std = statistics.stdev(all_freqs) if len(all_freqs) > 1 else 0
    overall_freq_cv = (overall_freq_std / overall_freq * 100) if overall_freq > 0 else 0

    print(f"1. Overall ω₀: {overall_freq:.3f} ± {overall_freq_std:.3f} Hz (CV={overall_freq_cv:.2f}%)")

if all_k:
    overall_k = statistics.mean(all_k)
    overall_k_std = statistics.stdev(all_k) if len(all_k) > 1 else 0

    print(f"2. Overall K: {overall_k:.6f} ± {overall_k_std:.6f} (Deviation from 1.0: {abs(overall_k-1.0):.6f})")

# Frequency stability across window sizes
freq_means = [statistics.mean(window_sizes[w]) for w in sorted(window_sizes.keys()) if len(window_sizes[w]) > 1]
if len(freq_means) > 1:
    freq_cv_across_windows = (statistics.stdev(freq_means) / statistics.mean(freq_means) * 100)
    print(f"3. Frequency CV across window sizes: {freq_cv_across_windows:.2f}%")
    if freq_cv_across_windows < 5:
        print("   ✓ HYPOTHESIS SUPPORTED: ω₀ is invariant across W_max")
    else:
        print("   ✗ HYPOTHESIS REJECTED: ω₀ varies significantly with W_max")

# K stability
k_means = [statistics.mean(k_values[w]) for w in sorted(k_values.keys()) if len(k_values[w]) > 1]
if k_means:
    k_deviation = statistics.mean([abs(k-1.0) for k in k_means])
    print(f"4. Mean K deviation from 1.0: {k_deviation:.6f}")
    if k_deviation < 0.1:
        print("   ✓ JAMES LAW VALIDATED: K ≈ 1.0 across conditions")
    else:
        print("   ✗ JAMES LAW NOT VALIDATED: K deviates significantly from 1.0")

print("\n")
print(f"Total runs analyzed: {len(runs)}")
print(f"Window sizes tested: {sorted(window_sizes.keys())}")
print("="*70)