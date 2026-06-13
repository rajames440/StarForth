#!/usr/bin/env python3
"""
pattern_analysis.py
StarForth DoE — Pattern Analysis

Not about speed. About:
  1. win_div trajectory: does diversity trend, oscillate, or cluster
     across the 480-run sequence?
  2. Discrete level structure: win_div takes only a few Q48.16 values.
     What determines which level fires?
  3. early_exit=0 anomalies: the rare runs that stay in inference.
     What's special about them?
  4. Shuffle verification: same seed → same cfg order; different seed
     → different order. Do the shuffle patterns correlate?
  5. cfg ordering matrix: where in the run sequence does each cfg appear,
     and is that consistent across seeds?
  6. Cross-seed win_div agreement: for the same (cfg,rep), do all seeds
     produce the same win_div?
"""

import os, math
from collections import defaultdict

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
HOSTED_DIR = os.path.join(SCRIPT_DIR, "..", "latest")
OUT_DIR    = os.path.join(SCRIPT_DIR, "tables")
os.makedirs(OUT_DIR, exist_ok=True)

SEEDS = ["12345", "67890", "13579"]
Q48   = 65536.0

# ─────────────────────────────────────────────────────────────────────────────
# Load
# ─────────────────────────────────────────────────────────────────────────────
def load(seed):
    path = os.path.join(HOSTED_DIR, f"seed{seed}.csv")
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("run_id") or line.startswith("DOE"):
                continue
            p = line.split(",")
            if len(p) < 13:
                continue
            try:
                rows.append({
                    "run_id": int(p[0]), "cfg": int(p[1]), "rep": int(p[2]),
                    "f_ent": 1 if int(p[3])>0 else 0,
                    "f_cv":  1 if int(p[4])>0 else 0,
                    "f_tmp": 1 if int(p[5])>0 else 0,
                    "f_stb": 1 if int(p[6])>0 else 0,
                    "l8_mode": int(p[7]),
                    "win_div": int(p[8]),
                    "early_exit": int(p[12]),
                })
            except (ValueError, IndexError):
                pass
    return rows

data = {s: load(s) for s in SEEDS}

def mean(xs): return sum(xs)/len(xs) if xs else float('nan')
def sd(xs):
    if len(xs)<2: return 0.0
    m=mean(xs)
    return math.sqrt(sum((x-m)**2 for x in xs)/(len(xs)-1))

print("══════════════════════════════════════════════════════════════════")
print("  StarForth DoE — Pattern Analysis")
print("══════════════════════════════════════════════════════════════════\n")

# ─────────────────────────────────────────────────────────────────────────────
# 1. win_div trajectory — split into 8 windows of 60 runs each
#    Does diversity change over the session?
# ─────────────────────────────────────────────────────────────────────────────
print("── 1. win_div trajectory (8 × 60-run windows, all seeds) ───────")
print("   Window  run_ids    mean_wd   sd_wd    min_wd   max_wd")

all_runs = []
for s in SEEDS:
    all_runs.extend(data[s])
all_runs.sort(key=lambda r: (SEEDS.index(r.get("seed","12345")), r["run_id"]))
# tag seed
for s in SEEDS:
    for r in data[s]:
        r["seed"] = s

WINDOWS = 8
N = 480
WIN_SIZE = N // WINDOWS
trajectory_rows = []
for s in SEEDS:
    rows = data[s]
    print(f"\n  seed={s}")
    for w in range(WINDOWS):
        chunk = rows[w*WIN_SIZE:(w+1)*WIN_SIZE]
        wds = [r["win_div"] for r in chunk]
        mn  = mean(wds)/Q48
        s_  = sd(wds)/Q48
        lo  = min(wds)/Q48
        hi  = max(wds)/Q48
        label = f"{w*WIN_SIZE:>3}-{(w+1)*WIN_SIZE-1:<3}"
        bar_len = int((mn - 0.960) / 0.030 * 40)
        bar = "▓" * max(0, bar_len)
        print(f"    W{w+1}  [{label}]  {mn:.6f}  {s_:.6f}  {lo:.6f}  {hi:.6f}  |{bar}")
        trajectory_rows.append({"seed":s,"window":w+1,"run_lo":w*WIN_SIZE,
                                  "run_hi":(w+1)*WIN_SIZE-1,
                                  "mean_wd":mn,"sd_wd":s_,"min_wd":lo,"max_wd":hi})

# ─────────────────────────────────────────────────────────────────────────────
# 2. Discrete level structure
#    win_div takes only a few distinct Q48.16 values. List them all.
# ─────────────────────────────────────────────────────────────────────────────
print("\n── 2. Discrete win_div levels (all seeds pooled) ────────────────")

level_counts  = defaultdict(int)
level_cfg     = defaultdict(set)   # which cfgs produce this level
level_early   = defaultdict(list)  # early_exit vals at each level
for s in SEEDS:
    for r in data[s]:
        lv = r["win_div"]
        level_counts[lv] += 1
        level_cfg[lv].add(r["cfg"])
        level_early[lv].append(r["early_exit"])

print(f"  {'level (raw)':>12}  {'Q48.16':>10}  {'count':>7}  {'%total':>7}  "
      f"{'n_cfgs':>7}  {'early%':>7}")
total = sum(level_counts.values())
level_rows = []
for lv in sorted(level_counts.keys()):
    cnt  = level_counts[lv]
    pct  = 100*cnt/total
    n_cfg= len(level_cfg[lv])
    epct = 100*sum(level_early[lv])/len(level_early[lv])
    print(f"  {lv:>12}  {lv/Q48:>10.6f}  {cnt:>7}  {pct:>6.1f}%  "
          f"{n_cfg:>7}  {epct:>6.1f}%")
    level_rows.append({"level_raw":lv,"level_norm":lv/Q48,"count":cnt,
                        "pct_total":pct,"n_cfgs":n_cfg,"pct_early":epct})

# Which cfgs produce each non-modal level?
modal_level = max(level_counts, key=level_counts.get)
print(f"\n  Modal level = {modal_level} ({modal_level/Q48:.6f}) — "
      f"{level_counts[modal_level]} runs ({100*level_counts[modal_level]/total:.1f}%)")
print(f"\n  Non-modal level → cfg membership:")
for lv in sorted(level_counts.keys()):
    if lv == modal_level: continue
    cfgs = sorted(level_cfg[lv])
    print(f"    level {lv:>6} ({lv/Q48:.6f}) → cfg: {cfgs}")

# ─────────────────────────────────────────────────────────────────────────────
# 3. early_exit=0 anomalies
# ─────────────────────────────────────────────────────────────────────────────
print("\n── 3. early_exit=0 anomalies (full list) ────────────────────────")
print(f"  {'seed':>8}  {'run_id':>7}  {'cfg':>4}  {'rep':>4}  "
      f"{'ent':>4}  {'cv':>4}  {'tmp':>4}  {'stb':>4}  {'win_div':>10}")

anomaly_rows = []
for s in SEEDS:
    for r in data[s]:
        if r["early_exit"] == 0:
            print(f"  {s:>8}  {r['run_id']:>7}  {r['cfg']:>4}  {r['rep']:>4}  "
                  f"{r['f_ent']:>4}  {r['f_cv']:>4}  {r['f_tmp']:>4}  "
                  f"{r['f_stb']:>4}  {r['win_div']:>10} ({r['win_div']/Q48:.6f})")
            anomaly_rows.append({**r,"seed":s})

print(f"\n  Total anomalies: {len(anomaly_rows)} / {total} runs "
      f"({100*len(anomaly_rows)/total:.2f}%)")

# Look for patterns in anomalies
if anomaly_rows:
    print("\n  Pattern check — are anomalies clustered by cfg?")
    acfg = defaultdict(list)
    for r in anomaly_rows:
        acfg[r["cfg"]].append(r["seed"])
    for cfg, seeds in sorted(acfg.items()):
        print(f"    cfg {cfg:>2}: seeds {seeds}")

    print("\n  Pattern check — by run_id position:")
    positions = [(r["run_id"], r["seed"]) for r in anomaly_rows]
    print(f"    run_ids: {[p[0] for p in positions]}")
    print(f"    first runs? {all(r['run_id'] < 5 for r in anomaly_rows)}")

# ─────────────────────────────────────────────────────────────────────────────
# 4. Shuffle verification
#    Same seed → same cfg ordering across runs.
#    Different seeds → different ordering.
#    Check: are the 3 seed orderings correlated?
# ─────────────────────────────────────────────────────────────────────────────
print("\n── 4. Shuffle pattern: cfg sequence per seed ────────────────────")

# Get cfg ordering (as list indexed by run_id) for each seed
orderings = {}
for s in SEEDS:
    orderings[s] = [r["cfg"] for r in sorted(data[s], key=lambda r: r["run_id"])]

# Spearman rank correlation between orderings
def spearman(a, b):
    n = len(a)
    def ranks(x):
        s = sorted(range(n), key=lambda i: x[i])
        r = [0]*n
        i = 0
        while i < n:
            j = i
            while j < n and x[s[j]] == x[s[i]]: j += 1
            avg = (i + j - 1) / 2.0 + 1
            for k in range(i, j): r[s[k]] = avg
            i = j
        return r
    ra, rb = ranks(a), ranks(b)
    d2 = sum((ra[i]-rb[i])**2 for i in range(n))
    return 1 - 6*d2 / (n*(n**2-1))

pairs = [("12345","67890"), ("12345","13579"), ("67890","13579")]
print("  Spearman correlation of cfg ordering between seeds:")
for s1, s2 in pairs:
    rho = spearman(orderings[s1], orderings[s2])
    print(f"    seed {s1} vs {s2}: ρ = {rho:+.6f}  "
          f"({'correlated' if abs(rho)>0.1 else 'independent (good)'})")

# How many positions agree between seeds?
print("\n  Agreement at each run position (cfg same in both seeds):")
for s1, s2 in pairs:
    matches = sum(1 for i in range(480) if orderings[s1][i] == orderings[s2][i])
    print(f"    seed {s1} vs {s2}: {matches}/480 positions match "
          f"({100*matches/480:.1f}%)")

# ─────────────────────────────────────────────────────────────────────────────
# 5. cfg position matrix
#    For each cfg (0-15), at what run_ids does it appear?
#    Show first, median, last position per seed.
# ─────────────────────────────────────────────────────────────────────────────
print("\n── 5. cfg position distribution (first/median/last run_id) ─────")
print(f"  {'cfg':>4}  ", end="")
for s in SEEDS:
    print(f"  seed={s} [1st/med/lst]      ", end="")
print()

cfg_positions = {}
for s in SEEDS:
    pos = defaultdict(list)
    for r in data[s]:
        pos[r["cfg"]].append(r["run_id"])
    cfg_positions[s] = pos

for cfg in range(16):
    print(f"  {cfg:>4}  ", end="")
    for s in SEEDS:
        positions = sorted(cfg_positions[s][cfg])
        med = positions[len(positions)//2]
        print(f"  {positions[0]:>4}/{med:>4}/{positions[-1]:>4}          ", end="")
    print()

# ─────────────────────────────────────────────────────────────────────────────
# 6. Cross-seed win_div agreement for same (cfg, rep)
#    If the shuffle is independent but the physics is deterministic given
#    the same cfg, the same (cfg,rep) pair should produce similar win_div
#    regardless of which position in the sequence it lands.
# ─────────────────────────────────────────────────────────────────────────────
print("\n── 6. Cross-seed win_div for same (cfg,rep) ─────────────────────")
print("   Do the same experimental conditions produce the same win_div")
print("   regardless of where in the shuffle they appear?\n")

# Build index: (cfg, rep) → win_div per seed
cfg_rep_wd = defaultdict(dict)
for s in SEEDS:
    for r in data[s]:
        key = (r["cfg"], r["rep"])
        cfg_rep_wd[key][s] = r["win_div"]

# Find (cfg,rep) pairs that appear in all 3 seeds and where seeds disagree
all_three  = {k:v for k,v in cfg_rep_wd.items() if len(v)==3}
same_val   = sum(1 for v in all_three.values() if len(set(v.values()))==1)
diff_val   = sum(1 for v in all_three.values() if len(set(v.values()))>1)

print(f"  (cfg,rep) pairs appearing in all 3 seeds: {len(all_three)}")
print(f"  Identical win_div across all 3 seeds:     {same_val} ({100*same_val/len(all_three):.1f}%)")
print(f"  Differing win_div across seeds:            {diff_val} ({100*diff_val/len(all_three):.1f}%)")

if diff_val > 0:
    print(f"\n  Divergent (cfg,rep) pairs:")
    print(f"  {'(cfg,rep)':>12}  {'s=12345':>10}  {'s=67890':>10}  {'s=13579':>10}  delta")
    shown = 0
    for key, vals in sorted(all_three.items()):
        if len(set(vals.values())) > 1:
            v = [vals.get(s,None) for s in SEEDS]
            v_nn = [x for x in v if x is not None]
            delta = max(v_nn) - min(v_nn)
            v_str = [f"{x/Q48:.6f}" if x else "      --" for x in v]
            print(f"  {str(key):>12}  {v_str[0]:>10}  {v_str[1]:>10}  {v_str[2]:>10}  {delta}")
            shown += 1
            if shown >= 20:
                remaining = diff_val - shown
                if remaining > 0:
                    print(f"  ... and {remaining} more")
                break

# ─────────────────────────────────────────────────────────────────────────────
# 7. win_div autocorrelation — does each run's diversity depend on the previous?
# ─────────────────────────────────────────────────────────────────────────────
print("\n── 7. win_div lag-1 autocorrelation ────────────────────────────")
print("   Does the diversity of run N predict run N+1?")
print("   (Same cfg appearing in different positions = position effect)\n")

for s in SEEDS:
    rows = sorted(data[s], key=lambda r: r["run_id"])
    wds  = [r["win_div"] for r in rows]
    n    = len(wds)
    m    = mean(wds)
    # Pearson r at lag 1
    cov  = sum((wds[i]-m)*(wds[i+1]-m) for i in range(n-1)) / (n-1)
    var  = sum((x-m)**2 for x in wds) / (n-1)
    acf  = cov / var if var > 0 else 0
    print(f"  seed={s}: lag-1 autocorrelation r = {acf:+.6f}")

# ─────────────────────────────────────────────────────────────────────────────
# 8. Save pattern tables
# ─────────────────────────────────────────────────────────────────────────────
with open(os.path.join(OUT_DIR, "pattern_trajectory.csv"), "w") as f:
    f.write("seed,window,run_lo,run_hi,mean_wd,sd_wd,min_wd,max_wd\n")
    for r in trajectory_rows:
        f.write(f"{r['seed']},{r['window']},{r['run_lo']},{r['run_hi']},"
                f"{r['mean_wd']:.6f},{r['sd_wd']:.6f},{r['min_wd']:.6f},{r['max_wd']:.6f}\n")

with open(os.path.join(OUT_DIR, "pattern_levels.csv"), "w") as f:
    f.write("level_raw,level_norm,count,pct_total,n_cfgs,pct_early\n")
    for r in level_rows:
        f.write(f"{r['level_raw']},{r['level_norm']:.6f},{r['count']},"
                f"{r['pct_total']:.1f},{r['n_cfgs']},{r['pct_early']:.1f}\n")

with open(os.path.join(OUT_DIR, "pattern_anomalies.csv"), "w") as f:
    f.write("seed,run_id,cfg,rep,f_ent,f_cv,f_tmp,f_stb,win_div\n")
    for r in anomaly_rows:
        f.write(f"{r['seed']},{r['run_id']},{r['cfg']},{r['rep']},"
                f"{r['f_ent']},{r['f_cv']},{r['f_tmp']},{r['f_stb']},{r['win_div']}\n")

print("\n\nTables written: pattern_trajectory.csv, pattern_levels.csv, pattern_anomalies.csv")
print("\n══════════════════════════════════════════════════════════════════")
print("  Pattern analysis complete.")
print("══════════════════════════════════════════════════════════════════")
