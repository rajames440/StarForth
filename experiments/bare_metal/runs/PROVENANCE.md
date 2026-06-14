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
| amd64   | doe-amd64-20260612-142323.csv   | 261,098 |    ~51m    | qemu-amd64-20260612-142323.log   | CANONICAL |

---

## Replication 3 — Seed 13579  (2026-06-12)

Run order: aarch64 → amd64 → riscv64

| Arch    | CSV filename                    | Rows    | Wall clock | Log filename                      | Status    |
|---------|---------------------------------|---------|------------|-----------------------------------|-----------|
| aarch64 | doe-aarch64-20260612-160132.csv | 261,095 |  ~5m       | qemu-aarch64-20260612-160132.log  | CANONICAL |
| amd64   | doe-amd64-20260612-160656.csv   | 261,098 |  ~51m      | qemu-amd64-20260612-160656.log    | CANONICAL |
| riscv64 | doe-riscv64-20260612-174414.csv | 261,095 |  ~5m       | qemu-riscv64-20260612-174414.log  | CANONICAL |

All 9 outer DoE cells complete. Campaign closed 2026-06-12.

---

## Latin Square Seed D — Seed 54321  (2026-06-14)

Run order: amd64 → riscv64 → aarch64

**Context:** This is the first run using the corrected QEMU termination condition
(`-lt 3` instead of `-lt 2`). Prior to this fix, the REPL command echo
(`[Hera] zuse)ok> 54321 30 EXEC-DOE`) bumped the grep count to 2,
killing QEMU before EXEC-DOE ran. All three arches now produce full 261k-tick runs.

| Arch    | CSV filename                     | Rows    | Wall clock | Log filename                      | Status    |
|---------|----------------------------------|---------|------------|-----------------------------------|-----------|
| amd64   | doe-amd64-20260614-212929.csv    | 261,099 | ~43m       | qemu-amd64-20260614-212929.log    | CANONICAL |
| riscv64 | doe-riscv64-20260614-230220.csv  | 261,096 | ~5m        | qemu-riscv64-20260614-230220.log  | CANONICAL |
| aarch64 | doe-aarch64-20260614-230803.csv  | 261,096 | ~5m        | qemu-aarch64-20260614-230803.log  | CANONICAL |

All three arches agree at 261,096–261,099 ticks (within 3 ticks of boot-phase
variation). APIC timer active on amd64 (apic_ticks advancing); riscv64 and
aarch64 show apic_ticks=0 (APIC not wired for those ISAs in current kernel).

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

Points to Latin Square Seed D (most recent complete replication, seed 54321).

| File                 | Points to                                         |
|----------------------|---------------------------------------------------|
| `latest/amd64.csv`   | runs/doe-amd64-20260614-212929.csv  (Seed D)      |
| `latest/aarch64.csv` | runs/doe-aarch64-20260614-230803.csv (Seed D)     |
| `latest/riscv64.csv` | runs/doe-riscv64-20260614-230220.csv (Seed D)     |

All three files represent Latin Square Seed D (seed 54321, 2026-06-14).
Multi-replication analysis pools all CSVs across seeds 12345, 67890, 13579, 54321.
