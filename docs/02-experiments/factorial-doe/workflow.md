# Factorial DoE Analysis Workflow

## End-to-End Process

```
StarForth Repo
  └─ ./scripts/run_factorial_doe.sh
       │
       ├─ Builds all 64 configurations
       ├─ Runs 1,920+ tests (randomized)
       └─ Outputs: experiment_results.csv + logs
            │
            └─→ Copy to StarForth-DoE-Analysis repo
                 │
                 ├─ Load CSV into Python/R
                 ├─ Separate factors from metrics
                 ├─ Compute main effects & interactions
                 ├─ Identify optimal subsets
                 └─ Generate reports & visualizations
```

## Phase 1: Data Collection (StarForth Repo)

### Step 1: Run Experiment

```bash
cd /home/rajames/CLionProjects/StarForth

# Quick validation (optional)
./scripts/run_factorial_doe.sh --runs-per-config 1 TEST_01

# Full production run
./scripts/run_factorial_doe.sh --runs-per-config 30 2025_11_19_FULL_FACTORIAL
```

### Step 2: Wait for Completion
Monitor in another terminal:
```bash
watch -n 5 'wc -l /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL/experiment_results.csv'
```

Expected progression:
- 0 min: 1 line (header)
- 30 min: ~500 lines
- 2 hours: ~1,500 lines
- 2.5 hours: 1,921 lines (complete)

### Step 3: Verify Completion
```bash
tail /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL/experiment_results.csv
echo "---"
cat /home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL/experiment_summary.txt
```

## Phase 2: Data Transfer & Setup

### Step 1: Copy to Analysis Repo
```bash
RESULTS_DIR="/home/rajames/CLionProjects/StarForth-DoE/experiments/2025_11_19_FULL_FACTORIAL"
ANALYSIS_REPO="/home/rajames/CLionProjects/StarForth-DoE-Analysis"

mkdir -p "$ANALYSIS_REPO/data"
cp -r "$RESULTS_DIR" "$ANALYSIS_REPO/data/2025_11_19_FULL_FACTORIAL"
```

### Step 2: Verify Transfer
```bash
ls -lah "$ANALYSIS_REPO/data/2025_11_19_FULL_FACTORIAL/"
wc -l "$ANALYSIS_REPO/data/2025_11_19_FULL_FACTORIAL/experiment_results.csv"
```

## Phase 3: Analysis (StarForth-DoE-Analysis Repo)

### Load Data (Python)
```python
import pandas as pd
import numpy as np
from scipy import stats
import matplotlib.pyplot as plt

# Load results_run_01_2025_12_08
df = pd.read_csv("data/2025_11_19_FULL_FACTORIAL/experiment_results.csv")
print(f"Loaded {len(df)} runs across {len(df.groupby('configuration'))} configurations")

# Separate factors and metrics
factors = df[['enable_loop_1_heat_tracking',
               'enable_loop_2_rolling_window',
               'enable_loop_3_linear_decay',
               'enable_loop_4_pipelining_metrics',
               'enable_loop_5_window_inference',
               'enable_loop_6_decay_inference']]

metrics = df[['total_lookups', 'cache_hit_percent', 'vm_workload_duration_ns_q48', ...]]

print(f"Shape: {len(df)} rows × {len(df.columns)} columns")
print(f"Factors: {len(factors.columns)}")
print(f"Metrics: {len(metrics.columns)}")
```

### Main Effects Analysis
```python
# For each loop, compute average metric change when loop goes 0→1
main_effects = {}
for loop in factors.columns:
    loop_name = loop.replace('enable_', '').replace('_', ' ').title()
    metric_when_on = df[df[loop] == 1]['vm_workload_duration_ns_q48'].mean()
    metric_when_off = df[df[loop] == 0]['vm_workload_duration_ns_q48'].mean()

    effect = metric_when_on - metric_when_off
    pct_change = 100 * (effect / metric_when_off)

    main_effects[loop_name] = {
        'effect': effect,
        'pct_change': pct_change,
        'on_mean': metric_when_on,
        'off_mean': metric_when_off,
    }

# Print sorted by impact
for loop, effect in sorted(main_effects.items(), key=lambda x: abs(x[1]['pct_change']), reverse=True):
    print(f"{loop:30} {effect['pct_change']:+7.2f}% (ON: {effect['on_mean']:.0f}, OFF: {effect['off_mean']:.0f})")
```

Expected output:
```
Loop 6 Decay Inference         +12.3% (ON: 8,800,000, OFF: 7,840,000)
Loop 5 Window Inference        +8.5% (ON: 8,600,000, OFF: 7,920,000)
Loop 4 Pipelining Metrics      +6.2% (ON: 8,400,000, OFF: 7,900,000)
Loop 3 Linear Decay            +4.1% (ON: 8,200,000, OFF: 7,870,000)
Loop 2 Rolling Window           +3.0% (ON: 8,100,000, OFF: 7,850,000)
Loop 1 Heat Tracking            +2.1% (ON: 8,010,000, OFF: 7,850,000)
```

### Interaction Analysis
```python
# Two-way interactions: Do loops amplify/suppress each other?
interactions = {}

for i, loop_a in enumerate(factors.columns):
    for j, loop_b in enumerate(factors.columns):
        if i >= j:
            continue  # Only upper triangle

        # Four cases: (0,0), (0,1), (1,0), (1,1)
        case_00 = df[(df[loop_a] == 0) & (df[loop_b] == 0)]['vm_workload_duration_ns_q48'].mean()
        case_01 = df[(df[loop_a] == 0) & (df[loop_b] == 1)]['vm_workload_duration_ns_q48'].mean()
        case_10 = df[(df[loop_a] == 1) & (df[loop_b] == 0)]['vm_workload_duration_ns_q48'].mean()
        case_11 = df[(df[loop_a] == 1) & (df[loop_b] == 1)]['vm_workload_duration_ns_q48'].mean()

        # Interaction effect: (1,1) - (1,0) - (0,1) + (0,0)
        interaction = (case_11 - case_10 - case_01 + case_00)

        loop_a_name = loop_a.replace('enable_', '').replace('_', ' ')
        loop_b_name = loop_b.replace('enable_', '').replace('_', ' ')

        if abs(interaction) > 100000:  # Only report large interactions
            interactions[f"{loop_a_name} × {loop_b_name}"] = interaction

# Print sorted interactions
for pair, interaction in sorted(interactions.items(), key=lambda x: abs(x[1]), reverse=True):
    print(f"{pair:40} {interaction:+12,.0f}")
```

### Optimal Configuration Search
```python
# Find best k configurations by performance metric
k = 10
best_configs = df.nlargest(k, 'vm_workload_duration_ns_q48')[
    ['configuration', 'vm_workload_duration_ns_q48', 'enable_loop_1_heat_tracking',
     'enable_loop_2_rolling_window', 'enable_loop_3_linear_decay',
     'enable_loop_4_pipelining_metrics', 'enable_loop_5_window_inference',
     'enable_loop_6_decay_inference']
].drop_duplicates(subset=['configuration'])

print("Top 10 Configurations by Performance:")
print("─" * 100)
for idx, row in best_configs.iterrows():
    loops = (
        f"L1:{row['enable_loop_1_heat_tracking']} " +
        f"L2:{row['enable_loop_2_rolling_window']} " +
        f"L3:{row['enable_loop_3_linear_decay']} " +
        f"L4:{row['enable_loop_4_pipelining_metrics']} " +
        f"L5:{row['enable_loop_5_window_inference']} " +
        f"L6:{row['enable_loop_6_decay_inference']}"
    )
    print(f"{row['configuration']:20} | {row['vm_workload_duration_ns_q48']:12,.0f} ns | {loops}")
```

### Visualization Example
```python
# Main effects plot
import matplotlib.pyplot as plt

fig, axes = plt.subplots(2, 3, figsize=(15, 10))
fig.suptitle('2^6 Factorial DoE Main Effects', fontsize=16)

loop_names = [f"Loop {i+1}" for i in range(6)]
metric_key = 'vm_workload_duration_ns_q48'

for idx, loop in enumerate(factors.columns):
    ax = axes[idx // 3, idx % 3]

    off_mean = df[df[loop] == 0][metric_key].mean()
    on_mean = df[df[loop] == 1][metric_key].mean()

    bars = ax.bar(['OFF', 'ON'], [off_mean, on_mean], color=['lightcoral', 'lightgreen'])
    ax.set_ylabel('Workload Duration (ns)')
    ax.set_title(f'{loop_names[idx]}')
    ax.set_ylim([7000000, 10000000])

    # Add value labels
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height/1e6:.1f}M', ha='center', va='bottom')

plt.tight_layout()
plt.savefig('main_effects.png', dpi=150)
print("Saved: main_effects.png")
```

## Phase 4: Reporting

### Key Outputs to Generate

1. **Main Effects Summary** (which loops matter most?)
   - Table of individual loop impacts
   - Sorted by effect size

2. **Interaction Matrix** (do loops amplify/suppress?)
   - 6×6 table of two-way interactions
   - Highlights synergies/conflicts

3. **Optimal Configuration** (best 10-20 subsets)
   - Recommend top 3-5 production configs
   - Show Pareto frontier (performance vs complexity)

4. **Statistical Summary**
   - Variance by configuration
   - Confidence intervals on estimates
   - Regression model R²

### Example Report Structure

```markdown
# Factorial DoE Analysis Report - 2025-11-19

## Executive Summary
- Tested all 64 feedback loop combinations
- 1,920 experimental runs collected
- Key finding: Loops 5+6 show strong synergy (+18% combined)

## Main Effects
| Loop | Impact | Confidence | Recommendation |
|------|--------|-----------|---|
| Loop 6 (Decay Inference) | +12.3% | p<0.001 | **Enabled** |
| Loop 5 (Window Inference) | +8.5% | p<0.001 | **Enabled** |
| Loop 4 (Pipelining) | +6.2% | p<0.01 | Enabled |
| Loop 3 (Decay) | +4.1% | p<0.05 | Enabled |
| Loop 2 (Window) | +3.0% | p<0.05 | Enabled |
| Loop 1 (Heat) | +2.1% | p>0.05 | Optional |

## Recommended Configurations
1. **111111** (All loops): +30% vs baseline
2. **111110** (Except heat): +28% vs baseline, simpler
3. **011111** (Except heat, decay): +24% vs baseline

## Trade-off Analysis
- Full stack (111111): Best performance, highest code complexity
- Recommended (111110): 28% improvement, 95% of benefit
- Minimal set (001110): 18% improvement, 50% code overhead

---
```

## Checklist

- [ ] Run factorial DoE script to completion
- [ ] Verify 1,920 rows in experiment_results.csv
- [ ] Copy results to analysis repo
- [ ] Load data in Python/R
- [ ] Compute main effects (6 loops)
- [ ] Compute interactions (2-way or higher)
- [ ] Identify top 5-10 configurations
- [ ] Generate visualizations
- [ ] Write summary report
- [ ] Decide on production configuration

---

**Timeline:** Collection (2-4 hrs) + Analysis (2-4 hrs) = **6-8 hours total** for comprehensive factorial DoE.