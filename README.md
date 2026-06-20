# StarForth v3.1.0

**FORTH-79 virtual machine built on Compudynamics** — a physics-based adaptive
runtime discipline coined by R.A. James, treating execution heat, decay, and
inference as genuine thermodynamic phenomena governed by conservation laws.
Strict ANSI C99. Linux · L4Re · bare metal. Formally verified. Deterministic by proof.

Two deployment targets, one codebase:
- **`master`** — hosted VM (Linux · L4Re · POSIX)
- **[`lithosananke`](../../tree/lithosananke)** — UEFI bare-metal kernel (amd64 · aarch64 · riscv64)

---

## The Claim That Matters

**0.000% CV algorithmic variance across 90 experimental runs. Formally proved.**

A self-optimizing runtime that is also provably deterministic. Those two properties
appear contradictory. They are not — because Compudynamics is governed by a
conservation law:

> **K≡1.0 (James Law):** Normalized total execution energy is conserved across all
> loop configurations and all workloads.

Validated across 38,400 experimental runs (128 loop configurations × 30 replicates).
Proved in Isabelle/HOL. That is the patent.

---

## Compudynamics — Seven Feedback Loops

Six loops instrument and adapt the runtime. A seventh coordinates them.
Two are disabled by default — DoE confirmed they are harmful in the majority
of configurations.

| Loop | Mechanism | Role | Default |
|---|---|---|---|
| L1 | Execution heat tracking | Hot-word identification | Off — harmful in 86% of configs |
| L2 | Rolling window of truth | Deterministic history capture | On |
| L3 | Linear decay | Prevents heat accumulation | On |
| L4 | Pipelining / transition prediction | Speculative word prefetch | Off — harmful in 100% of configs |
| L5 | Window width inference (Levene's test) | Adaptive window sizing | On |
| L6 | Decay slope inference (OLS regression) | Continuous slope self-tuning | On |
| L7 | Adaptive heartrate | Load-responsive tick coordinator | On |

### L8 Jacquard — Loop Gate Selector

L8 is a 128-state, 7-bit loop gate selector. Each state is a bitmask over L1–L7 —
like a Jacquard loom punch card, it enables and disables loops simultaneously.
128 possible patterns. The system converges on attractor states (stable / volatile /
diverse / temporal / transition / omni) based on live execution statistics.

---

## Word-Level ACL

Every dictionary entry carries its own access control record: TTL · allow · mode · pin.

- **Strict mode** — re-checked on every execution
- **TTL mode** — adaptive countdown, amortised by execution heat
- **Pinned** — one-way ratchet; frozen permanently

Two permanent console layers: emergency (`ok>`) and superuser zuse (`zuse)ok>`).
CA root is embedded in the capsule hash — a tampered image is rejected at birth.
Formally proved: 5 Isabelle/HOL theory files covering pin monotonicity, TTL bounds,
no-escalation, emergency bypass, and inherit semantics.

---

## LithosAnanke — Bare-Metal Target

*Lithos* (stone) + *Ananke* (necessity). The UEFI microkernel under StarshipOS.

LithosAnanke boots StarForth directly from UEFI firmware with no OS underneath.
No libc. No scheduler. No dynamic allocator in the traditional sense — the primary
organizational unit is a **capsule**: a 64-byte, cache-line-aligned, content-addressed
immutable descriptor. Identity is content. Mutation produces a new capsule with a new ID.
Persistence is an event log of immutable capsules. There is no mutable state to corrupt.

ACL overhead on bare metal: **+0.0054%–+0.0088%**, CV = **0.000%** across a 3×3
Latin-square DoE campaign (amd64 · aarch64 · riscv64 × 3 seeds × 30 replicates).
Three orders of magnitude below the measurement floor. Effectively free.

| Milestone | | Status |
|---|---|---|
| M0–M6 | UEFI boot · PMM · VMM · IDT · APIC · heap | ✅ Complete |
| M7 | StarForth VM + POST (936 tests) + parity hash | ✅ Complete |
| M7.1 | Capsule birth protocol · Mama FORTH vocabulary | 🔄 In Progress |
| M8 | REPL — keyboard input, interactive Forth | Planned |
| M9 | Block storage — AHCI driver | Planned |

---

## By the Numbers

| | |
|---|---|
| Algorithmic variance | 0.000% CV — 90 experimental runs, formally proved |
| Conservation law | K≡1.0 (James Law) — validated across 38,400 runs |
| DoE campaign | 128 loop configurations × 30 replicates |
| Test suite | 936+ tests · 24 modules |
| Formal proofs | 24 Isabelle/HOL theory files · 2-axiom framework · sorry-free |
| FORTH-79 words | 295 registered primitives |
| VM memory | 5 MB (2 MB dictionary · user blocks · log) |
| Platforms | Linux · L4Re/Fiasco.OC · UEFI bare metal (amd64 · aarch64 · riscv64) |
| Patent | USPTO provisional · December 2025 |
| Published | SSRN abstract 5930134 — *Steady-State Convergence in an Adaptive Runtime* |

---

## Quick Start

### Hosted VM (Linux)

```bash
make                                              # standard build
make fastest                                      # LTO + direct threading
make test                                         # 936+ test suite
./build/amd64/standard/starforth                  # interactive REPL
./build/amd64/standard/starforth -c "1 2 + . BYE"
```

### LithosAnanke (bare metal)

```bash
make -f Makefile.starkernel                       # amd64 default
make -f Makefile.starkernel ARCH=aarch64
make -f Makefile.starkernel ARCH=riscv64
make -f Makefile.starkernel qemu                  # run in QEMU with OVMF
```

Artifacts: `build/amd64/kernel/starkernel_loader.efi` · `build/amd64/kernel/starkernel_kernel.elf`

---

## Documentation

| | |
|---|---|
| [Architecture](docs/03-architecture/OVERVIEW.md) | VM internals, physics engine, heartbeat |
| [Feedback Loops](docs/03-architecture/physics-engine/feedback-loops.md) | All 7 loops in detail |
| [Word ACL System](docs/03-architecture/word-acl/DESIGN.md) | Security design + proofs |
| [Formal Proofs](proof/) | Isabelle/HOL — 24 theory files |
| [Ontology](ONTOLOGY.md) | Formal definitions — cite this for academic work |
| [Research Paper](papers/James_Steady-State_Convergence_Adaptive_Runtime.pdf) | SSRN |
| [Getting Started](docs/01-getting-started/DEVELOPER.md) | Build, install, first steps |
| [Roadmap](ROADMAP.md) | StarshipOS · FPGA feasibility · timeline |

---

## License

[Starship License 1.0 (SL-1.0)](LICENSE) — free for personal, research, and educational use.
Commercial use requires a separate agreement.
Attribution to **R.A. James (Captain Bob)** must be preserved in all distributions.

**Patent pending.** USPTO provisional filed December 2025. This license does not grant
patent rights. Licensing inquiries: rajames440@gmail.com

---

*Robert A. James (Captain Bob) · Systems Engineer · Hacking since 1973*
