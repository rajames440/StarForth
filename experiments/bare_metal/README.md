# Bare-Metal DoE Experiment

This directory holds data and analysis from the LithosAnanke kernel's
Design of Experiments (DoE) runs — blind full-factorial 2⁴ experiments
that measure the L8 Jacquard mode selector's effect on the Steady-State
Machine across all three supported architectures (amd64, aarch64, riscv64).

---

## Directory Layout

```
experiments/bare_metal/
├── runs/          ← timestamped canonical CSVs from every acceptance run
├── latest/        ← arch-named copies of the most recent run (human-readable)
│   ├── amd64.csv
│   ├── aarch64.csv
│   └── riscv64.csv
└── analysis/
    ├── charts/    ← generated SVG/PNG charts
    ├── report/    ← timestamped LaTeX / Markdown reports
    └── tables/    ← generated summary tables
```

`runs/` is the canonical archive.  `latest/` is the eyeball-friendly shortcut
— always the most recent run per architecture, overwritten on each new run.

---

## Running the DoE

The DoE runs automatically when the kernel boots because `init.4th` calls it.
The standard acceptance command runs all three architectures sequentially:

```bash
make -f Makefile.starkernel ARCH=amd64   clean qemu
make -f Makefile.starkernel ARCH=aarch64 clean qemu
make -f Makefile.starkernel ARCH=riscv64 clean qemu
```

Each run executes 48 trials (16 L8 configs × 3 reps, Fisher-Yates shuffled),
captures ~1.27 million heartbeat rows per architecture, and writes two files:

| File | Path |
|------|------|
| Timestamped canonical CSV | `experiments/bare_metal/runs/doe-<arch>-<YYYYMMDD-HHMMSS>.csv` |
| Latest convenience copy   | `experiments/bare_metal/latest/<arch>.csv` |

**Run architectures sequentially, never in parallel.** All three QEMU
instances use `accel=tcg` (software emulation). Concurrent runs compete for
host CPU and corrupt the timing signal that the DoE is measuring.

---

## Disabling the DoE

To boot into the REPL without running the experiment, comment out the last
two lines of `capsules/init.4th`:

```forth
Block 2049
( first init.4th )
: STAR 42 EMIT ;
: STARS 0 DO STAR LOOP ;
: MARGIN 30 SPACES ;
: BAR MARGIN 5 STARS CR ;
: BLIP MARGIN STAR CR ;
: F CR BAR BLIP BAR BLIP BLIP CR ;
( S" Hermes" BIRTH )
( S" Artemis" BIRTH )
( S" doe.4th" EXEC   )   ← comment this out
( 123456 3 L8-DOE    )   ← comment this out
```

The kernel will boot to the `ok>` REPL with no experiment running.
Commenting both lines leaves `doe.4th` unloaded so none of its words
(`L8-DOE`, `WL-NAME`, etc.) are defined, which is the cleanest state for
interactive sessions.

---

## Changing the Seed and Rep Count

The DoE entry point is `L8-DOE ( seed reps -- )`.
The call in `init.4th` is:

```forth
123456 3 L8-DOE
```

- **Seed** — any non-zero integer.  The same seed always produces the same
  shuffled run order, so results are reproducible.  Change the seed to
  explore a different permutation; different seeds are statistically
  equivalent but verify shuffle-independence.

- **Reps** — trials per L8 configuration (1–200).  3 reps × 16 configs =
  48 runs, which takes roughly 25–30 minutes per architecture under TCG.
  Increase for higher statistical power; decrease for quick smoke checks.

```forth
( quick smoke check — 1 rep, 16 runs total )
42 1 L8-DOE

( full study — 10 reps, 160 runs )
987654 10 L8-DOE
```

---

## What Are Capsules?

A **capsule** is a named blob of FORTH-79 source text stored in `capsules/`.
The kernel's `EXEC` word loads a capsule by filename and interprets it as
FORTH source.  `BIRTH` (commented out in `init.4th`) would instead spawn an
isolated child VM whose sole personality is that capsule's code.

There are two roles:

| Role | Who uses it | What it does |
|------|-------------|--------------|
| **Init capsule** | Mama VM at boot | Defines the VM's vocabulary and behavior |
| **Workload capsule** | DoE machinery | Provides a computational task to time |

`init.4th` is the Mama VM's init capsule — executed exactly once at kernel
boot.  The numbered files (`init-0.4th` … `init-9.4th`) and the L8 variant
files (`init-l8-*.4th`) are workload capsules used by the DoE.

---

## `.4th` File Structure

Every `.4th` file must follow StarForth's block format.  The block system
maps source text to 1024-byte logical blocks; the `Block NNNN` header tells
the loader which block slot to fill.

**Mandatory rules:**

1. The first line of each logical block must be `Block NNNN` (capital B,
   single space, decimal integer).
2. Block numbers must be unique within a single capsule file.
3. Blocks are loaded in file order and executed top-to-bottom.
4. Each block can hold up to 1024 bytes of source text.
5. Comments use `( ... )` — parentheses with spaces inside.
6. Word definitions use `: NAME ... ;` — standard FORTH-79.

**Minimal capsule skeleton:**

```forth
Block 3100
( My capsule description )

: MY-WORD ( -- )
  42 . CR ;

MY-WORD
```

**Multi-block capsule:**

```forth
Block 3100
( Block 1: helpers )
: HELPER ( n -- n*2 ) 2 * ;

Block 3101
( Block 2: main logic )
: MAIN ( -- )
  10 0 DO I HELPER . CR LOOP ;

MAIN
```

The block number namespace is shared across all loaded capsules.
Convention used in this repository:

| Range | Contents |
|-------|----------|
| 2048–2099 | init.4th (Mama VM boot sequence) |
| 2100–2199 | doe.4th (DoE machinery) |
| 3000–3999 | Workload capsules (init-0 … init-9, init-l8-*) |
| 4000+     | User-defined capsules |

---

## Adding a Custom Workload Capsule

**Step 1 — Create the file.**

Add `capsules/my-workload.4th` using block numbers in the 4000+ range:

```forth
Block 4000
( my-workload.4th - description of what this measures )

: MY-COMPUTE ( n -- )
  0 SWAP 0 DO I 3 * + LOOP DROP ;

Block 4001
( main entry point )
: RUN-MY-WORKLOAD ( -- )
  500 0 DO I MY-COMPUTE LOOP ;

RUN-MY-WORKLOAD
```

The last line should execute the workload so `EXEC` runs it immediately when
the capsule is loaded.

**Step 2 — Wire it into the DoE.**

Open `capsules/doe.4th` and add your capsule to the workload dispatch table.
Find `WL-HI` (Block 2057) and replace one of the existing entries, or extend
the range:

```forth
Block 2057
: WL-HI ( n -- c-addr u )
  CASE
    0 OF S" init-8.4th"             ENDOF
    1 OF S" init-9.4th"             ENDOF
    2 OF S" init-l8-diverse.4th"    ENDOF
    3 OF S" init-l8-omni.4th"       ENDOF
    4 OF S" init-l8-stable.4th"     ENDOF
    5 OF S" init-l8-temporal.4th"   ENDOF
    6 OF S" init-l8-transition.4th" ENDOF
    7 OF S" my-workload.4th"        ENDOF   ← replace slot 7
    DROP S" init-0.4th"
  ENDCASE ;
```

There are 16 workload slots total (0–7 in `WL-LO`, 0–7 in `WL-HI`).
The DoE machinery picks workloads blindly from these slots — your capsule
will appear in the shuffled run matrix alongside the built-in workloads.

**Step 3 — Run the experiment.**

```bash
make -f Makefile.starkernel ARCH=amd64 clean qemu
```

Your workload's heartbeat rows will appear in the CSV under whatever
`CURR-WL` index maps to `my-workload.4th`.  Match by the `DOE-RUN` marker
lines in the CSV:

```
DOE-RUN,run_id,cfg,wl_id,rep
```

---

## CSV Format

Each row emitted by the `[HADES][DOE ]` serial tag is one heartbeat tick
during a workload execution.  Extract with:

```bash
grep -aP '\[HADES\]\[DOE \]' logs2/qemu-amd64-<timestamp>.log \
  | sed 's/.*\[DOE \] //' > my.csv
```

Columns (15 total):

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `tick_number` | uint32 | Monotonic heartbeat counter |
| 2 | `elapsed_ns` | uint64 | Nanoseconds since run start |
| 3 | `tick_interval_ns` | uint64 | Interval from prior tick |
| 4 | `cache_hits_delta` | uint32 | Hot-words cache hits this tick |
| 5 | `bucket_hits_delta` | uint32 | Bucket hits this tick |
| 6 | `word_executions_delta` | uint32 | Words executed this tick |
| 7 | `hot_word_count` | uint64 | Words with heat ≥ threshold |
| 8 | `avg_word_heat_q48` | uint64 | Mean heat (raw Q48.16 integer) |
| 9 | `window_width` | uint32 | L8's target rolling window size |
| 10 | `actual_window_size` | uint32 | True analysis width: min(total_executions, window_width) |
| 11 | `predicted_label_hits` | uint32 | ANOVA early-exit confirmations (L8 validation signal) |
| 12 | `jitter_bits` | uint64 | Estimated jitter (IEEE 754 bit pattern) |
| 13 | `apic_ticks` | uint64 | APIC timer monotonic count |
| 14 | `time_trust_q48` | uint64 | Time-trust score (Q48.16) |
| 15 | `variance_q48` | uint64 | Timing variance (Q48.16) |

`avg_word_heat_q48` is a raw fixed-point integer. To convert to a human-readable
heat value: `avg_word_heat = avg_word_heat_q48 / 65536.0`.

`jitter_bits` is the IEEE 754 double-precision bit pattern of the jitter in
nanoseconds.  In R: `readBin(as.raw(…), "double")`.  In Python:
`struct.unpack('d', struct.pack('Q', n))[0]`.

---

## Interpreting `predicted_label_hits`

This column is the feedback-loop closure signal.

Each non-zero value means the inference engine ran ANOVA on the current
execution window and confirmed the L8 selector's config choice correlated
with the subsequent execution pattern — an "early exit" because the
statistical test converged without needing all data.

- **High rate** → L8 chose well; the system settled quickly into a stable regime.
- **Low rate** → L8 is still searching; the workload is novel or transient.
- **Zero throughout** → The workload ended before the inference engine had
  enough data, or the window is too small to trigger ANOVA.

This is the metric that closes the loop between "L8 made a choice" and
"that choice was actually validated by what the VM did next."
