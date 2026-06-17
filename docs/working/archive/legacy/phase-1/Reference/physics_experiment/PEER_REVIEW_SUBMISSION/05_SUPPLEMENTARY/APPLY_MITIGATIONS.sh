#!/bin/bash
#
#  StarForth — Steady-State Virtual Machine Runtime
#
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
# See the License for the specific language governing permissions and
# limitations under the License.
#
# StarForth — Steady-State Virtual Machine Runtime
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#

################################################################################
# APPLY MITIGATIONS FOR OS SCHEDULING NOISE
#
# This script applies the key mitigation strategies to reduce variance from
# 60-70% to expected <40% (target: 30% improvement).
#
# Run this BEFORE conducting the next experiment round.
################################################################################

set -e

echo "================================================================================"
echo "APPLYING MITIGATIONS FOR OS SCHEDULING NOISE"
echo "================================================================================"

# Step 1: Verify current CPU governor
echo ""
echo "Step 1/4: Checking current CPU frequency governor..."
echo "         Current settings:"
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo -n "  $(dirname $cpu): "
    cat "$cpu" 2>/dev/null || echo "N/A"
done

# Step 2: Set performance mode (requires sudo)
echo ""
echo "Step 2/4: Setting CPU governor to 'performance' mode..."
echo "         This disables dynamic frequency scaling for consistency."
echo ""
echo "  ⚠ This requires sudo. Running..."
echo "     sudo tee /sys/devices/system/cpu/*/cpufreq/scaling_governor <<<performance"
echo ""

if [ "$EUID" -eq 0 ]; then
    echo performance | tee /sys/devices/system/cpu/*/cpufreq/scaling_governor > /dev/null
    echo "  ✓ Performance mode set successfully"
else
    echo "  ⚠ Not running as root. To set performance mode, run:"
    echo "     sudo bash -c 'echo performance | tee /sys/devices/system/cpu/*/cpufreq/scaling_governor > /dev/null'"
fi

# Step 3: Display the command to run experiments with CPU pinning
echo ""
echo "Step 3/4: CPU Pinning Command Reference"
echo "         Use these commands to run benchmarks pinned to CPU core 0:"
echo ""
echo "  Single run (interactive):"
echo "    taskset -c 0 ./build/starforth -c '1 2 + . BYE'"
echo ""
echo "  Full 90-run experiment with CPU pinning:"
echo "    taskset -c 0 scripts/run_comprehensive_physics_experiment.sh results_with_mitigation"
echo ""
echo "  With nice priority (lower than default):"
echo "    taskset -c 0 nice -n -10 scripts/run_comprehensive_physics_experiment.sh results_with_mitigation"
echo ""

# Step 4: Display verification command
echo "Step 4/4: Verification"
echo "         After running the experiment, compare variance reduction with:"
echo ""
echo "  python3 physics_experiment/analyze_variance_sources.py"
echo ""
echo "  Expected improvement:"
echo "    Before: A_BASELINE CV = 69.58%"
echo "    After:  A_BASELINE CV = ~35-40% (target: -30-40% reduction)"
echo ""

echo "================================================================================"
echo "OPTIONAL ADVANCED MITIGATIONS"
echo "================================================================================"

echo ""
echo "If variance is still >40% after CPU pinning, try these:"
echo ""

echo "1. DISABLE SIMULTANEOUS MULTI-THREADING (SMT)"
echo "   This prevents hyper-thread contention on your CPU."
echo ""
echo "   Check if SMT is enabled:"
echo "     cat /sys/devices/system/cpu/cpu1/online"
echo "     # Should show: 1 (enabled) or 0 (disabled)"
echo ""
echo "   Disable SMT cores (on 8-core system with SMT):"
for i in 1 3 5 7; do
    echo "     echo 0 | sudo tee /sys/devices/system/cpu/cpu${i}/online > /dev/null"
done
echo ""

echo "2. REAL-TIME SCHEDULING (SCHED_FIFO)"
echo "   Runs benchmark with highest priority (99) using cooperative scheduling."
echo "   Requires root. May starve other processes!"
echo ""
echo "   Command:"
echo "     sudo chrt -f 99 taskset -c 0 scripts/run_comprehensive_physics_experiment.sh results_realtime"
echo ""

echo "3. NUMA BINDING (Multi-socket systems only)"
echo "   Binds benchmark to single NUMA node to avoid memory latency variance."
echo ""
echo "   Check NUMA configuration:"
echo "     numactl -H"
echo ""
echo "   Run with NUMA binding:"
echo "     numactl --cpunodebind=0 --membind=0 --physcpubind=0 \\"
echo "       scripts/run_comprehensive_physics_experiment.sh results_numa"
echo ""

echo "4. MEASURE WITH PERF"
echo "   Collect detailed performance counter data to understand variance sources."
echo ""
echo "   Measure cache misses and stalls:"
echo "     perf stat -e cache-references,cache-misses,LLC-loads,LLC-load-misses,cycles,instructions \\"
echo "       taskset -c 0 ./build/starforth -c '1 2 + . BYE'"
echo ""
echo "   Or profile execution:"
echo "     perf record -e cycles,instructions -g --cgroup=\$(pidof starforth) \\"
echo "       taskset -c 0 ./build/starforth ..."
echo "     perf report"
echo ""

echo "================================================================================"
echo "STATISTICAL IMPROVEMENTS"
echo "================================================================================"

echo ""
echo "Even without system-level changes, improve statistics with trimmed means:"
echo ""
echo "  Python code to calculate trimmed mean:"
cat << 'EOF'

import statistics

runtimes = [4.5, 4.7, 5.2, 5.1, 4.8, 17.6]  # 17.6 is outlier from OS noise

# Method 1: Trimmed mean (remove bottom/top 10%)
sorted_rt = sorted(runtimes)
trim_amount = max(1, int(len(sorted_rt) * 0.1))
trimmed = sorted_rt[trim_amount:-trim_amount]
mean_trimmed = statistics.mean(trimmed)

# Method 2: Median + IQR (more robust)
median = statistics.median(runtimes)
q1, q3 = statistics.quantiles(runtimes, n=4)[1:3]
iqr = q3 - q1

print(f"Original mean: {statistics.mean(runtimes):.2f} ms (includes outliers)")
print(f"Trimmed mean:  {mean_trimmed:.2f} ms (more representative)")
print(f"Median:        {median:.2f} ms ± {iqr/2:.2f} ms (IQR/2)")
EOF
echo ""

echo "================================================================================"
echo "EXPECTED RESULTS TIMELINE"
echo "================================================================================"
echo ""
echo "Baseline (current):        A_BASELINE CV = 69.58%"
echo "After CPU pinning:         A_BASELINE CV = ~35-45% (estimate)"
echo "After CPU pinning + fix:   A_BASELINE CV = ~30-35% (with trimmed means)"
echo "With performance governor: Should see <30% CV"
echo "With SMT disabled:         Potential drop to ~20% CV"
echo "With SCHED_FIFO (optional):Potential drop to <15% CV (if running alone)"
echo ""

echo "================================================================================"
echo "QUICK START (TL;DR)"
echo "================================================================================"
echo ""
echo "1. Set performance mode:"
echo "   sudo bash -c 'echo performance | tee /sys/devices/system/cpu/*/cpufreq/scaling_governor > /dev/null'"
echo ""
echo "2. Run experiment with CPU pinning:"
echo "   taskset -c 0 scripts/run_comprehensive_physics_experiment.sh results_test1"
echo ""
echo "3. Analyze results:"
echo "   python3 physics_experiment/analyze_variance_sources.py"
echo ""
echo "4. Compare variance before/after"
echo ""

echo "================================================================================"
echo "ADDITIONAL RESOURCES"
echo "================================================================================"
echo ""
echo "Full analysis and background:"
echo "  See: physics_experiment/VARIANCE_ANALYSIS_SUMMARY.md"
echo ""
echo "Quick reference:"
echo "  See: physics_experiment/QUICK_REFERENCE.txt"
echo ""
echo "Raw data:"
echo "  See: physics_experiment/full_90_run_comprehensive/experiment_results.csv"
echo ""
echo "================================================================================"