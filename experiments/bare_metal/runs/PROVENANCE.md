# Bare-Metal DoE Run Provenance

Master ledger for all 30-replicate DoE runs.
Three replications × three architectures = 9 runs total.
Each run is itself a complete 2⁴ factorial experiment (16 configs × 30 reps = 480 runs),
making this a nested/hierarchical DoE: 9 outer cells × 480 inner runs.

---

## Outer DoE Design — Run Order Matrix

Balanced so each architecture appears in each position exactly once:

| Position | Rep 1 (seed 12345) | Rep 2 (seed 67890) | Rep 3 (seed 13579) |
|----------|--------------------|--------------------|--------------------|
| 1st run  | amd64              | riscv64            | aarch64            |
| 2nd run  | aarch64            | aarch64            | amd64              |
| 3rd run  | riscv64            | amd64              | riscv64            |

riscv64 occupies all three positions across reps (3rd, 1st, 2nd) — best achievable
balance given the first two orderings.

**Inner DoE factors** (2⁴ = 16 configurations, shuffled by seed via Fisher-Yates):

| Factor          | Low    | High   |
|-----------------|--------|--------|
| Entropy thresh  | 0.50   | 0.75   |
| CV threshold    | 0.10   | 0.15   |
| Temporal decay  | 0.30   | 0.50   |
| Stability flag  | 0      | 1      |

Config presentation order is shuffled per-run using the DoE seed (doe.4th Block 2054).
Same seed → same shuffle (reproducible). Different seeds → independent permutations.

**Note on inner DoE CSV extraction:** `EMIT-ROW` output (480-row per-run summaries)
is fragmented by the 10µs heartbeat interrupt firing mid-print.  These rows are not
reliably extractable from existing logs.  Fix planned: atomic row buffer in doe.4th
before next campaign.  Current analysis uses heartbeat stream only.

---

## Replication 1 — Seed 12345  (2026-06-12)

Run order: amd64 → aarch64 → riscv64

| Arch    | CSV filename                    | Rows    | Wall clock | Log filename                     | Status    |
|---------|---------------------------------|---------|------------|----------------------------------|-----------|
| amd64   | doe-amd64-20260612-110212.csv   | 261,098 | 51m 39s    | qemu-amd64-20260612-110212.log   | CANONICAL |
| aarch64 | doe-aarch64-20260612-132412.csv | 261,095 |  5m 05s    | qemu-aarch64-20260612-132412.log | CANONICAL |
| riscv64 | doe-riscv64-20260612-135612.csv | 261,095 |  5m 16s    | qemu-riscv64-20260612-135612.log | CANONICAL |

MD5: aarch64 ≠ riscv64 in heat columns from tick 34,001; all attractor metrics
(exec_delta, window_width, time_trust, variance) identical across all three.

---

## Replication 2 — Seed 67890  (2026-06-12)

Run order: riscv64 → aarch64 → amd64

| Arch    | CSV filename                    | Rows    | Wall clock | Log filename                     | Status    |
|---------|---------------------------------|---------|------------|----------------------------------|-----------|
| riscv64 | doe-riscv64-20260612-141129.csv | 261,095 |  5m 21s    | qemu-riscv64-20260612-141129.log | CANONICAL |
| aarch64 | doe-aarch64-20260612-141722.csv | 261,095 |  5m 10s    | qemu-aarch64-20260612-141722.log | CANONICAL |
| amd64   | doe-amd64-20260612-142323.csv   | TBD     |    ~51m    | qemu-amd64-20260612-142323.log   | RUNNING   |

---

## Replication 3 — Seed 13579  (planned)

Run order: aarch64 → amd64 → riscv64

| Arch    | CSV filename | Rows | Wall clock | Log filename | Status  |
|---------|-------------|------|------------|--------------|---------|
| aarch64 | TBD         | TBD  | ~5m        | TBD          | PENDING |
| amd64   | TBD         | TBD  | ~51m       | TBD          | PENDING |
| riscv64 | TBD         | TBD  | ~5m        | TBD          | PENDING |

---

## Superseded Runs (kept for audit trail, NOT used in analysis)

| Arch    | CSV filename                    | Rows    | Date       | Reason superseded                                          |
|---------|---------------------------------|---------|------------|------------------------------------------------------------|
| amd64   | doe-amd64-20260611-012412.csv   | 261,097 | 2026-06-11 | Transitional build — EXEC-DOE was trivial alias            |
| aarch64 | doe-aarch64-20260611-190822.csv | 261,095 | 2026-06-11 | Superseded by Rep 1 clean re-run                           |
| riscv64 | doe-riscv64-20260611-191335.csv | 261,095 | 2026-06-11 | Superseded by Rep 1 clean re-run                           |

The original aarch64 and riscv64 (2026-06-11) were byte-for-byte identical.
This was a single-session observation; Rep 1 re-runs show heat variation between
arches while attractor metrics remain invariant.  See report §7.5.

---

## Development / Partial Runs (not used, not audited)

Files dated 2026-06-09 and 2026-06-10 are development iterations with varying
rep counts, incomplete capsule implementations, or aborted builds.  Retained
for git history, excluded from all analysis.

---

## `latest/` Contents (current canonical data)

Points to Rep 1 until multi-replication analysis is implemented.

| File                 | Points to                              |
|----------------------|----------------------------------------|
| `latest/amd64.csv`   | runs/doe-amd64-20260612-110212.csv     |
| `latest/aarch64.csv` | runs/doe-aarch64-20260612-141722.csv   |
| `latest/riscv64.csv` | runs/doe-riscv64-20260612-141129.csv   |

Note: latest/ currently holds a mix of Rep 1 (amd64) and Rep 2 (aarch64, riscv64).
Update all three to a consistent replication once multi-rep analysis is ready.
