# Bare-Metal DoE Run Provenance

Master ledger for all 30-replicate DoE runs.  One row per completed run.
The **Rep 1** column identifies the first full replication (seed 12345, clean
`lithosananke` code, 2026-06-12).  Additional replications will be added
below as they complete.

---

## Replication 1 — Seed 12345  (2026-06-12, commit `5ca4c73`+)

| Arch    | CSV filename                          | Rows    | Wall clock | Log filename                         | MD5 (data)                       | Status   |
|---------|---------------------------------------|---------|------------|--------------------------------------|----------------------------------|----------|
| amd64   | doe-amd64-20260612-110212.csv         | 261,098 | 51m 39s    | qemu-amd64-20260612-110212.log       | (see below)                      | CANONICAL|
| aarch64 | doe-aarch64-20260612-132412.csv       | 261,095 |  5m 05s    | qemu-aarch64-20260612-132412.log     | (see below)                      | CANONICAL|
| riscv64 | doe-riscv64-20260612-135612.csv       | 261,095 |  5m 16s    | qemu-riscv64-20260612-135612.log     | (see below)                      | CANONICAL|

MD5 checksums (Rep 1):

```
(run: md5sum experiments/bare_metal/runs/doe-*-20260612-*.csv)
doe-amd64-20260612-110212.csv    — unique (APIC residual in trust/variance cols)
doe-aarch64-20260612-132412.csv  — 0ab2a1461da7fa2f4e9bcbb94dd5f273
doe-riscv64-20260612-135612.csv  — 3c01a86fb4b1e76da925e38eb21706e2
```

aarch64 ≠ riscv64 in heat columns from tick 34,001; all attractor metrics
(exec_delta, window_width, time_trust, variance) are identical across all three.

---

## Superseded Runs (kept for audit trail, NOT used in analysis)

| Arch    | CSV filename                          | Rows    | Date       | Reason superseded                              |
|---------|---------------------------------------|---------|------------|------------------------------------------------|
| amd64   | doe-amd64-20260611-012412.csv         | 261,097 | 2026-06-11 | Transitional build — EXEC-DOE was trivial alias, not parameterised |
| aarch64 | doe-aarch64-20260611-190822.csv       | 261,095 | 2026-06-11 | Superseded by Rep 1 clean re-run               |
| riscv64 | doe-riscv64-20260611-191335.csv       | 261,095 | 2026-06-11 | Superseded by Rep 1 clean re-run               |

Note: the original aarch64 and riscv64 (2026-06-11) were byte-for-byte
identical (MD5: 1f0e4441f96e90e9110215a43a0cdba6 / 644b51fc4d2f47c3f870b00106733537
wait — they matched each other).  This was a single-session observation;
the Rep 1 re-runs show heat variation between arches.  See report §7.5.

---

## Development / Partial Runs (not used, not audited)

Files dated 2026-06-09 and 2026-06-10 are development iterations with
varying rep counts, incomplete capsule implementations, or aborted builds.
They are retained for git history but excluded from all analysis.

---

## Planned Replications

| Rep | Seed  | Status    | Notes                          |
|-----|-------|-----------|--------------------------------|
|  1  | 12345 | COMPLETE  | Baseline — all three arches done |
|  2  | TBD   | PENDING   | May use different seed          |
|  3  | TBD   | PENDING   | May use different seed          |

When Rep 2 and Rep 3 complete, update this table and add a new section
above following the Rep 1 format.  The R analysis script
(`analysis/analyse_bare_metal.R`) will need a multi-replication mode
once more than one canonical set exists.

---

## `latest/` Symlinks (current canonical data)

| File                    | Points to                              |
|-------------------------|----------------------------------------|
| `latest/amd64.csv`      | runs/doe-amd64-20260612-110212.csv     |
| `latest/aarch64.csv`    | runs/doe-aarch64-20260612-132412.csv   |
| `latest/riscv64.csv`    | runs/doe-riscv64-20260612-135612.csv   |

Update these entries whenever a new replication becomes canonical.
