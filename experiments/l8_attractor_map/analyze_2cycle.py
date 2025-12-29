#!/usr/bin/env python3
"""
Binary 2-cycle analysis for StarForth heartbeat data.
For each workload, fits two clusters in (HR, ΔHR) space and computes binary clarity metrics.
Pure Python implementation with manual SVG generation.
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

import csv
import math
import random

def mean(values):
    """Calculate mean of a list."""
    return sum(values) / len(values) if values else 0.0

def std(values):
    """Calculate standard deviation."""
    if len(values) < 2:
        return 0.0
    m = mean(values)
    variance = sum((x - m) ** 2 for x in values) / (len(values) - 1)
    return math.sqrt(variance)

def distance(p1, p2):
    """Euclidean distance between two 2D points."""
    return math.sqrt((p1[0] - p2[0])**2 + (p1[1] - p2[1])**2)

def kmeans_2clusters(points, max_iters=50):
    """Simple k-means clustering for k=2."""
    if len(points) < 2:
        return None, None

    # Initialize with two random points
    random.seed(42)
    centers = random.sample(points, 2)

    for iteration in range(max_iters):
        # Assign points to nearest center
        clusters = [[], []]
        for point in points:
            d0 = distance(point, centers[0])
            d1 = distance(point, centers[1])
            cluster_idx = 0 if d0 < d1 else 1
            clusters[cluster_idx].append(point)

        # Recompute centers
        new_centers = []
        for cluster in clusters:
            if cluster:
                center_x = mean([p[0] for p in cluster])
                center_y = mean([p[1] for p in cluster])
                new_centers.append((center_x, center_y))
            else:
                # Keep old center if cluster is empty
                new_centers.append(centers[len(new_centers)])

        # Check convergence
        if new_centers == centers:
            break
        centers = new_centers

    # Final assignment
    labels = []
    for point in points:
        d0 = distance(point, centers[0])
        d1 = distance(point, centers[1])
        labels.append(0 if d0 < d1 else 1)

    return labels, centers

def covariance_matrix(points):
    """Calculate 2x2 covariance matrix for 2D points."""
    if len(points) < 2:
        return [[0, 0], [0, 0]]

    xs = [p[0] for p in points]
    ys = [p[1] for p in points]

    mean_x = mean(xs)
    mean_y = mean(ys)

    n = len(points)
    cov_xx = sum((x - mean_x) ** 2 for x in xs) / (n - 1)
    cov_yy = sum((y - mean_y) ** 2 for y in ys) / (n - 1)
    cov_xy = sum((xs[i] - mean_x) * (ys[i] - mean_y) for i in range(n)) / (n - 1)

    return [[cov_xx, cov_xy], [cov_xy, cov_yy]]

def eigenvalues_2x2(cov):
    """Calculate eigenvalues of 2x2 covariance matrix."""
    a, b = cov[0][0], cov[0][1]
    c, d = cov[1][0], cov[1][1]

    trace = a + d
    det = a * d - b * c

    discriminant = trace**2 - 4 * det
    if discriminant < 0:
        discriminant = 0

    lambda1 = (trace + math.sqrt(discriminant)) / 2
    lambda2 = (trace - math.sqrt(discriminant)) / 2

    return max(lambda1, 0), max(lambda2, 0)

def load_data(heartbeat_file, mapping_file):
    """Load heartbeat data and workload mappings."""
    with open(heartbeat_file, 'r') as f:
        reader = csv.DictReader(f)
        heartbeat_data = list(reader)

    with open(mapping_file, 'r') as f:
        reader = csv.DictReader(f)
        mappings = list(reader)

    return heartbeat_data, mappings

def extract_workload_data(heartbeat_data, mappings, workload_name):
    """Extract HR and ΔHR for a specific workload across all replicates."""
    hrs = []
    delta_hrs = []

    for mapping in mappings:
        if workload_name in mapping['init_script']:
            start_row = int(mapping['hb_start_row']) - 1
            end_row = int(mapping['hb_end_row'])

            run_hrs = []
            for i in range(start_row, end_row):
                if i < len(heartbeat_data):
                    hr = float(heartbeat_data[i]['tick_interval_ns'])
                    # Skip initialization heartbeats (those with extremely large values)
                    # Normal heartbeat intervals should be < 1 second (1e9 ns)
                    if hr < 1e9:
                        run_hrs.append(hr)

            if len(run_hrs) > 1:
                hrs.extend(run_hrs[:-1])
                for i in range(1, len(run_hrs)):
                    delta_hrs.append(run_hrs[i] - run_hrs[i-1])

    return hrs, delta_hrs

def analyze_workload(hrs, delta_hrs, workload_name):
    """Perform 2-cluster analysis and compute binary clarity metrics."""
    if len(hrs) < 10:
        print(f"Warning: {workload_name} has insufficient data ({len(hrs)} points)")
        return None

    # Prepare data for clustering (HR, ΔHR)
    points = list(zip(hrs, delta_hrs))

    # Remove outliers (beyond 5 sigma)
    mean_hr = mean(hrs)
    std_hr = std(hrs)
    mean_dhr = mean(delta_hrs)
    std_dhr = std(delta_hrs)

    filtered_points = []
    for hr, dhr in points:
        if abs(hr - mean_hr) < 5 * std_hr and abs(dhr - mean_dhr) < 5 * std_dhr:
            filtered_points.append((hr, dhr))

    if len(filtered_points) < 10:
        print(f"Warning: {workload_name} has insufficient data after filtering ({len(filtered_points)} points)")
        return None

    # Fit k-means with k=2
    labels, centers = kmeans_2clusters(filtered_points)
    if labels is None:
        return None

    # Separate clusters
    cluster_0 = [p for i, p in enumerate(filtered_points) if labels[i] == 0]
    cluster_1 = [p for i, p in enumerate(filtered_points) if labels[i] == 1]

    # Distance between centers (binary separation)
    separation = distance(centers[0], centers[1])

    # Compute covariance matrices and ellipse areas
    if len(cluster_0) > 1:
        cov_0 = covariance_matrix(cluster_0)
        eigenval_0 = eigenvalues_2x2(cov_0)
        # Ellipse area = π * n_std² * sqrt(eigenvalues product)
        area_0 = math.pi * 2.0**2 * math.sqrt(eigenval_0[0] * eigenval_0[1])
    else:
        area_0 = 0.0

    if len(cluster_1) > 1:
        cov_1 = covariance_matrix(cluster_1)
        eigenval_1 = eigenvalues_2x2(cov_1)
        area_1 = math.pi * 2.0**2 * math.sqrt(eigenval_1[0] * eigenval_1[1])
    else:
        area_1 = 0.0

    avg_area = (area_0 + area_1) / 2.0
    avg_spread = math.sqrt(avg_area / math.pi) if avg_area > 0 else 0.0

    # Binary clarity metric: separation² / spread
    binary_clarity = (separation ** 2) / avg_spread if avg_spread > 0 else 0.0

    return {
        'workload': workload_name,
        'n_points': len(filtered_points),
        'centers': centers,
        'separation': separation,
        'area_0': area_0,
        'area_1': area_1,
        'avg_area': avg_area,
        'avg_spread': avg_spread,
        'binary_clarity': binary_clarity,
        'labels': labels,
        'points': filtered_points,
        'cluster_0': cluster_0,
        'cluster_1': cluster_1,
    }

def create_svg_visualization(results, output_file):
    """Create SVG visualization of all workloads with clustering results."""
    workloads = ['omni', 'stable', 'transition', 'temporal', 'diverse', 'volatile']

    # SVG dimensions
    svg_width = 1800
    svg_height = 1200
    plot_width = 280
    plot_height = 350
    margin = 20
    cols = 3
    rows = 2

    # Start SVG
    svg_lines = [
        f'<svg width="{svg_width}" height="{svg_height}" xmlns="http://www.w3.org/2000/svg">',
        '<rect width="100%" height="100%" fill="white"/>',
        '<style>',
        '  .plot-title { font: bold 14px sans-serif; }',
        '  .axis-label { font: 11px sans-serif; }',
        '  .metrics { font: 9px monospace; }',
        '  .main-title { font: bold 20px sans-serif; }',
        '  .cluster-0 { fill: #FF6B6B; opacity: 0.4; }',
        '  .cluster-1 { fill: #4ECDC4; opacity: 0.4; }',
        '  .center { stroke: black; stroke-width: 3; }',
        '  .grid { stroke: #ccc; stroke-width: 0.5; opacity: 0.5; }',
        '  .ellipse-0 { stroke: #FF6B6B; fill: none; stroke-width: 2; }',
        '  .ellipse-1 { stroke: #4ECDC4; fill: none; stroke-width: 2; }',
        '</style>',
        '<text x="900" y="30" text-anchor="middle" class="main-title">Binary 2-Cycle Analysis: (HR, ΔHR) Phase Space Clustering</text>',
    ]

    for idx, workload in enumerate(workloads):
        row = idx // cols
        col = idx % cols

        x_offset = margin + col * (plot_width + margin + 20)
        y_offset = 60 + row * (plot_height + margin + 40)

        if workload not in results or results[workload] is None:
            svg_lines.append(
                f'<text x="{x_offset + plot_width/2}" y="{y_offset + plot_height/2}" '
                f'text-anchor="middle" class="plot-title">{workload.upper()}<tspan x="{x_offset + plot_width/2}" dy="1.2em">Insufficient data</tspan></text>'
            )
            continue

        res = results[workload]
        points = res['points']
        labels = res['labels']
        centers = res['centers']

        # Find data ranges
        hrs = [p[0] for p in points]
        dhrs = [p[1] for p in points]
        min_hr, max_hr = min(hrs), max(hrs)
        min_dhr, max_dhr = min(dhrs), max(dhrs)

        # Add padding
        hr_range = max_hr - min_hr
        dhr_range = max_dhr - min_dhr
        if hr_range == 0:
            hr_range = 1
        if dhr_range == 0:
            dhr_range = 1

        min_hr -= hr_range * 0.1
        max_hr += hr_range * 0.1
        min_dhr -= dhr_range * 0.1
        max_dhr += dhr_range * 0.1

        def scale_x(hr):
            return x_offset + (hr - min_hr) / (max_hr - min_hr) * plot_width

        def scale_y(dhr):
            return y_offset + plot_height - (dhr - min_dhr) / (max_dhr - min_dhr) * plot_height

        # Draw grid
        for i in range(5):
            x = x_offset + i * plot_width / 4
            svg_lines.append(f'<line x1="{x}" y1="{y_offset}" x2="{x}" y2="{y_offset + plot_height}" class="grid"/>')
            y = y_offset + i * plot_height / 4
            svg_lines.append(f'<line x1="{x_offset}" y1="{y}" x2="{x_offset + plot_width}" y2="{y}" class="grid"/>')

        # Plot data points
        for i, (hr, dhr) in enumerate(points):
            px = scale_x(hr)
            py = scale_y(dhr)
            cluster_class = f'cluster-{labels[i]}'
            svg_lines.append(f'<circle cx="{px:.2f}" cy="{py:.2f}" r="1.5" class="{cluster_class}"/>')

        # Plot cluster centers
        for center in centers:
            cx = scale_x(center[0])
            cy = scale_y(center[1])
            svg_lines.append(f'<line x1="{cx-8}" y1="{cy-8}" x2="{cx+8}" y2="{cy+8}" class="center"/>')
            svg_lines.append(f'<line x1="{cx-8}" y1="{cy+8}" x2="{cx+8}" y2="{cy-8}" class="center"/>')

        # Draw approximate ellipses for clusters
        for cluster_idx, cluster in enumerate([res['cluster_0'], res['cluster_1']]):
            if len(cluster) > 2:
                c_hrs = [p[0] for p in cluster]
                c_dhrs = [p[1] for p in cluster]
                c_mean_hr = mean(c_hrs)
                c_mean_dhr = mean(c_dhrs)
                c_std_hr = std(c_hrs)
                c_std_dhr = std(c_dhrs)

                cx = scale_x(c_mean_hr)
                cy = scale_y(c_mean_dhr)
                rx = abs(scale_x(c_mean_hr + 2*c_std_hr) - cx)
                ry = abs(scale_y(c_mean_dhr + 2*c_std_dhr) - cy)

                svg_lines.append(
                    f'<ellipse cx="{cx:.2f}" cy="{cy:.2f}" rx="{rx:.2f}" ry="{ry:.2f}" class="ellipse-{cluster_idx}"/>'
                )

        # Add title and labels
        svg_lines.append(
            f'<text x="{x_offset + plot_width/2}" y="{y_offset - 10}" text-anchor="middle" class="plot-title">'
            f'{workload.upper()} (n={res["n_points"]})</text>'
        )
        svg_lines.append(
            f'<text x="{x_offset + plot_width/2}" y="{y_offset + plot_height + 20}" text-anchor="middle" class="axis-label">'
            f'HR (tick_interval_ns)</text>'
        )
        svg_lines.append(
            f'<text x="{x_offset - 10}" y="{y_offset + plot_height/2}" text-anchor="middle" '
            f'transform="rotate(-90 {x_offset - 10} {y_offset + plot_height/2})" class="axis-label">ΔHR (ns)</text>'
        )

        # Add metrics box
        metrics_y = y_offset + 15
        svg_lines.append(f'<rect x="{x_offset + 5}" y="{metrics_y}" width="160" height="50" fill="wheat" opacity="0.7" rx="5"/>')
        svg_lines.append(f'<text x="{x_offset + 10}" y="{metrics_y + 15}" class="metrics">Separation: {res["separation"]:.1f} ns</text>')
        svg_lines.append(f'<text x="{x_offset + 10}" y="{metrics_y + 28}" class="metrics">Avg Area: {res["avg_area"]:.1f} ns²</text>')
        svg_lines.append(f'<text x="{x_offset + 10}" y="{metrics_y + 41}" class="metrics">Binary Clarity: {res["binary_clarity"]:.2e}</text>')

    svg_lines.append('</svg>')

    with open(output_file, 'w') as f:
        f.write('\n'.join(svg_lines))

    print(f"Visualization saved to {output_file}")

def main():
    # File paths
    results_dir = '/home/rajames/CLionProjects/StarForth/experiments/l8_attractor_map/results_20251209_145907'
    heartbeat_file = f'{results_dir}/heartbeat_all.csv'
    mapping_file = f'{results_dir}/workload_mapping.csv'
    output_svg = f'{results_dir}/binary_2cycle_analysis.svg'

    print("Loading data...")
    heartbeat_data, mappings = load_data(heartbeat_file, mapping_file)

    workloads = ['omni', 'stable', 'transition', 'temporal', 'diverse', 'volatile']
    results = {}

    print("\nAnalyzing workloads...")
    for workload in workloads:
        print(f"\n{workload.upper()}:")
        hrs, delta_hrs = extract_workload_data(heartbeat_data, mappings, workload)
        print(f"  Total points: {len(hrs)}")

        if len(hrs) > 0:
            result = analyze_workload(hrs, delta_hrs, workload)
            if result:
                results[workload] = result
                print(f"  Separation: {result['separation']:.2f} ns")
                print(f"  Avg ellipse area: {result['avg_area']:.2f} ns²")
                print(f"  Binary clarity: {result['binary_clarity']:.2e}")

    print("\n" + "="*60)
    print("BINARY CLARITY RANKING (highest = clearest 2-cycle):")
    print("="*60)
    ranked = sorted([(wl, res['binary_clarity']) for wl, res in results.items()],
                   key=lambda x: x[1], reverse=True)
    for i, (wl, clarity) in enumerate(ranked, 1):
        print(f"{i}. {wl.upper():12s} → {clarity:.2e}")

    print(f"\nCreating visualization...")
    create_svg_visualization(results, output_svg)
    print("\nAnalysis complete!")

if __name__ == '__main__':
    main()