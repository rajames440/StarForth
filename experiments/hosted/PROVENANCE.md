# Hosted StarForth DoE Run Provenance

Hosted VM counterpart to the bare-metal LithosAnanke DoE campaign.
Three replications × one platform (hosted x86_64 Linux VM) = 3 runs.
Each run is a complete 2⁴ factorial experiment (16 configs × 30 reps = 480 runs).
Same seeds, same doe.4th, same shuffle — directly comparable to bare-metal output.

---

## Outer Design — 4×4 Latin Square (arch × seed position)

Treating hosted as a 4th ISA extends the original 3×3 bare-metal design to a
4×4 cyclic Latin square.  Each ISA sees each seed in a different session slot,
blocking out the position effect (adaptive session-level memory accumulation).

| ISA      | P1 (seed A) | P2 (seed B) | P3 (seed C) | P4 (seed D) |
|----------|-------------|-------------|-------------|-------------|
| amd64    | 12345       | 67890       | 13579       | **54321**   |
| aarch64  | 54321       | 12345       | 67890       | 13579       |
| riscv64  | 13579       | 54321       | 12345       | 67890       |
| **hosted** | **67890** | **13579**   | **54321**   | **12345**   |

Each seed appears exactly once per ISA and once per position — fully balanced.

**Status:** The hosted row is complete (all 4 seeds run).  The bare-metal seed D
(54321) column is **deferred** — see Preliminary Decision below.

**Inner DoE factors** (2⁴ = 16 configurations, Fisher-Yates shuffled by seed):

**Inner DoE factors** (2⁴ = 16 configurations, Fisher-Yates shuffled by seed):

| Factor          | Low (Q48.16) | High (Q48.16) |
|-----------------|-------------|--------------|
| Entropy thresh  | 0           | 49152 (0.75) |
| CV threshold    | 0           | 9830  (0.15) |
| Temporal decay  | 0           | 32768 (0.50) |
| Stability flag  | 0           | 32768 (0.50) |

Same seed → same shuffle as bare-metal (Fisher-Yates in doe.4th Block 2054 is
deterministic, independent of host ISA).

---

## Preliminary Decision — bare-metal seed D not warranted

Seed D (54321) was run on hosted as a pilot before committing to ~61 minutes of
bare-metal QEMU time (amd64 ≈51 min TCG + aarch64 + riscv64 ≈10 min).

| Seed  | mean win_div | min   | max   | L8 mode=7 | early_exit=1 |
|-------|-------------|-------|-------|-----------|--------------|
| 12345 | 64502.9     | 63232 | 64768 | 100%      | —            |
| 67890 | 64493.3     | 63232 | 64768 | 100%      | —            |
| 13579 | 64513.1     | 63232 | 64768 | 100%      | —            |
| 54321 | 64507.7     | 64000 | 64768 | 100%      | 99.8%        |

Seed D is statistically indistinguishable from seeds A/B/C: same L8 mode-7
universal attractor, same window floor (~64500/65536 ≈ 0.984), same factor
invariance (KW p > 0.20 for all 4 factors on seeds A/B/C).  Running the
bare-metal column would add no new information.

**Decision:** bare-metal seed D deferred.  Campaign remains valid as a 3×3
bare-metal design + 4-seed hosted auxiliary.  Revisit if a specific hypothesis
requires bare-metal seed-position cross-over for one of the three ISAs.

---

## Execution Context

- **Binary:** `./build/amd64/standard/starforth` (StarForth v3.0.3)
- **Build target:** standard (optimised, STRICT_PTR=1)
- **POST:** runs in full before each DoE invocation (emulating LithosAnanke boot)
- **Invocation:**
  ```
  printf 'S" doe.4th" EXEC\n<seed> 30 EXEC-DOE\nBYE\n' | \
    ./build/amd64/standard/starforth -s --log-none 2>/dev/null | \
    grep -E "run_id|^[0-9]+,|^DOE:"
  ```
- **Date:** 2026-06-13
- **Host:** x86_64 Linux (native, no emulation)
- **Wall clock per run:** ~5s (vs ~51m for bare-metal amd64 under QEMU/TCG on ARM host)

---

## Replication 1 — Seed 12345  (2026-06-13)

| File                                        | Data rows | DOE complete | Status    |
|---------------------------------------------|-----------|--------------|-----------|
| runs/doe-hosted-20260613-144350-seed12345.csv | 480       | yes          | CANONICAL |

---

## Replication 2 — Seed 67890  (2026-06-13)

| File                                        | Data rows | DOE complete | Status    |
|---------------------------------------------|-----------|--------------|-----------|
| runs/doe-hosted-20260613-144350-seed67890.csv | 480       | yes          | CANONICAL |

---

## Replication 3 — Seed 13579  (2026-06-13)

| File                                        | Data rows | DOE complete | Status    |
|---------------------------------------------|-----------|--------------|-----------|
| runs/doe-hosted-20260613-144350-seed13579.csv | 480       | yes          | CANONICAL |

All 4 hosted DoE runs complete. Campaign closed 2026-06-13.

---

## `latest/` Contents

| File                 | Source run                                         |
|----------------------|----------------------------------------------------|
| `latest/seed12345.csv` | runs/doe-hosted-20260613-144350-seed12345.csv      |
| `latest/seed67890.csv` | runs/doe-hosted-20260613-144350-seed67890.csv      |
| `latest/seed13579.csv` | runs/doe-hosted-20260613-144350-seed13579.csv      |
| `latest/seed54321.csv` | runs/doe-hosted-20260613-155809-seed54321.csv      |

---

## Comparison Notes (vs bare-metal)

- **Shuffle identity:** same seed → identical run order (cfg, rep columns match
  bare-metal per-seed CSV byte-for-byte on the inner DoE layout)
- **L8 mode:** both platforms converge to mode 7 (omni) — same attractor
- **infer_win:** hosted returns 256 (ADAPTIVE_MIN_WINDOW_SIZE constant) vs 256
  on bare-metal — identical
- **Speed difference:** ~600× — hosted is native x86_64; bare-metal amd64 was
  QEMU/TCG software emulation on an ARM host (Oracle ARM)
- **Key comparison target:** cfg, rep, l8_mode, infer_win, infer_dec_q columns
  should be ISA-invariant; heat columns (win_div) may diverge
