# Hosted StarForth DoE Run Provenance

Hosted VM counterpart to the bare-metal LithosAnanke DoE campaign.
Three replications × one platform (hosted x86_64 Linux VM) = 3 runs.
Each run is a complete 2⁴ factorial experiment (16 configs × 30 reps = 480 runs).
Same seeds, same doe.4th, same shuffle — directly comparable to bare-metal output.

---

## Outer Design — Latin Square (3 × 3, arch × position)

The LithosAnanke campaign used this correct 3×3 Latin square across 3 ISAs:

| Position | Rep 1 (seed 12345) | Rep 2 (seed 67890) | Rep 3 (seed 13579) |
|----------|--------------------|--------------------|---|
| 1st      | amd64              | aarch64            | riscv64            |
| 2nd      | riscv64            | amd64              | aarch64            |
| 3rd      | aarch64            | riscv64            | amd64              |

Each arch appears exactly once in each position and each rep — fully balanced.
On the hosted platform there is only one execution environment, so the outer
Latin square collapses to a single column.  The three seeds are run in the
order 12345 → 67890 → 13579, matching the column-1 (amd64) position ordering
from the bare-metal design.

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

All 3 hosted DoE runs complete. Campaign closed 2026-06-13.

---

## `latest/` Contents

| File                 | Source run                                         |
|----------------------|----------------------------------------------------|
| `latest/seed12345.csv` | runs/doe-hosted-20260613-144350-seed12345.csv    |
| `latest/seed67890.csv` | runs/doe-hosted-20260613-144350-seed67890.csv    |
| `latest/seed13579.csv` | runs/doe-hosted-20260613-144350-seed13579.csv    |

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
