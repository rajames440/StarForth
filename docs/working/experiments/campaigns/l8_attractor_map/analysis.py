#!/usr/bin/env python3
"""
L8 Attractor Heartbeat Analysis
Publication-quality statistical analysis
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

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
import warnings
warnings.filterwarnings('ignore')

# Set style
sns.set_style("whitegrid")
plt.rcParams['figure.dpi'] = 150

# Create output directories
Path("analysis_output/charts").mkdir(parents=True, exist_ok=True)
Path("analysis_output/tables").mkdir(parents=True, exist_ok=True)
Path("analysis_output/report").mkdir(parents=True, exist_ok=True)

print("═══════════════════════════════════════════════════════════════")
print("    L8 Attractor Heartbeat Analysis")
print("═══════════════════════════════════════════════════════════════\n")

# 1. LOAD DATA
print("Loading datasets...")
results_dir = "results_20251209_145907"

heartbeat = pd.read_csv(f"{results_dir}/heartbeat_all.csv")
run_matrix = pd.read_csv(f"{results_dir}/l8_attractor_run_matrix.csv")
workload_map = pd.read_csv(f"{results_dir}/workload_mapping.csv")

print(f"  Heartbeat rows: {len(heartbeat)}")
print(f"  Runs: {len(run_matrix)}")
print(f"  Mapping entries: {len(workload_map)}")

# 2. MERGE DATA
print("\nMerging datasets...")

# Add row numbers (1-indexed to match workload_map)
heartbeat['row_num'] = range(1, len(heartbeat) + 1)

# Merge by checking if row_num is within [hb_start_row, hb_end_row]
merged_rows = []
for _, mapping in workload_map.iterrows():
    run_hb = heartbeat[
        (heartbeat['row_num'] >= mapping['hb_start_row']) &
        (heartbeat['row_num'] <= mapping['hb_end_row'])
    ].copy()
    
    run_hb['run_id'] = mapping['run_id']
    run_hb['init_script'] = mapping['init_script']
    run_hb['replicate'] = mapping['replicate']
    
    merged_rows.append(run_hb)

heartbeat_merged = pd.concat(merged_rows, ignore_index=True)
print(f"  Merged rows: {len(heartbeat_merged)}")

# 3. PER-RUN SUMMARY STATISTICS
print("\nComputing per-run summary statistics...")

per_run_stats = heartbeat_merged.groupby(['run_id', 'init_script', 'replicate']).agg(
    tick_count=('tick_number', 'count'),
    duration_ns=('elapsed_ns', lambda x: x.max() - x.min()),
    mean_interval_ns=('tick_interval_ns', 'mean'),
    sd_interval_ns=('tick_interval_ns', 'std'),
    mean_heat=('avg_word_heat', 'mean'),
    max_heat=('avg_word_heat', 'max'),
    mean_hot_words=('hot_word_count', 'mean'),
    max_hot_words=('hot_word_count', 'max'),
    mean_jitter_ns=('estimated_jitter_ns', 'mean'),
    sd_jitter_ns=('estimated_jitter_ns', 'std'),
    window_width_constant=('window_width', lambda x: len(x.unique()) == 1)
).reset_index()

per_run_stats['cv_interval'] = per_run_stats['sd_interval_ns'] / per_run_stats['mean_interval_ns']

# Save
per_run_stats.to_csv("analysis_output/tables/per_run_summary.csv", index=False)
print("  Saved: analysis_output/tables/per_run_summary.csv")

# 4. WORKLOAD-LEVEL SUMMARY
print("\nComputing workload-level statistics...")

workload_stats = per_run_stats.groupby('init_script').agg(
    n_runs=('run_id', 'count'),
    mean_tick_count=('tick_count', 'mean'),
    sd_tick_count=('tick_count', 'std'),
    mean_duration_ns=('duration_ns', 'mean'),
    sd_duration_ns=('duration_ns', 'std'),
    mean_heat=('mean_heat', 'mean'),
    sd_heat=('mean_heat', 'std'),
    mean_jitter=('mean_jitter_ns', 'mean'),
    sd_jitter=('mean_jitter_ns', 'std')
).reset_index()

workload_stats.to_csv("analysis_output/tables/workload_summary.csv", index=False)
print("  Saved: analysis_output/tables/workload_summary.csv\n")
print(workload_stats.to_string(index=False))

print("\n" + "="*67)
print("All summary statistics complete. Now generating charts...")
print("="*67 + "\n")

