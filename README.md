# StarForth

**FORTH-79 virtual machine with a physics-grounded adaptive runtime.** Written in strict ANSI C99. Runs on Linux, L4Re/Fiasco.OC, and bare metal. Formally verified. Deterministic by proof.

The hosted VM under StarshipOS — and the foundation for [LithosAnanke](../../tree/lithosananke).

---

## What makes it different

Seven feedback loops drive self-optimization at runtime while preserving full determinism:

| Loop | Mechanism | Role |
|---|---|---|
| #1 | Execution heat tracking | Identifies hot words |
| #2 | Rolling window of truth | Deterministic history capture |
| #3 | Linear decay | Prevents heat accumulation |
| #4 | Pipelining metrics | Word-transition prediction |
| #5 | Window width inference (Levene's test) | Adaptive window sizing |
| #6 | Decay slope inference (OLS regression) | Slope tuning |
| #7 | Adaptive heartrate | Load-responsive tick timing |

**Result:** ~0% CV algorithmic variance across 90 experimental runs. Adaptive and reproducible.  
**Formal proof:** 2-axiom Isabelle/HOL framework in `proof/` — sorry-free, machine-checkable.

---

## Quick Start

```bash
make                          # standard build
make fastest                  # LTO + direct threading
make test                     # 800+ test suite
make smoke                    # quick sanity check
./build/amd64/standard/starforth              # interactive REPL
./build/amd64/standard/starforth -c "1 2 + . BYE"
```

---

## Documentation

| | |
|---|---|
| [Getting Started](docs/01-getting-started/README.md) | Build, install, first steps |
| [Architecture](docs/03-architecture/README.md) | VM internals, physics engine, heartbeat system |
| [Feedback Loops](docs/03-architecture/physics-engine/) | All 7 loops in detail |
| [Formal Proofs](proof/) | Isabelle/HOL — 2 axioms, 21 theory files |
| [Research & DoE](docs/06-research/README.md) | Experimental methodology, results |
| [Quality & Validation](docs/04-quality/README.md) | Test coverage, regression tracking |
| [Ontology](ONTOLOGY.md) | Formal definitions — cite this for academic work |
| [Academic Paper](docs/FINAL_REPORT/) | 121-page peer-review submission |

---

## License

[Starship License 1.0 (SL-1.0)](LICENSE) — free for personal, research, and educational use.
Commercial use requires a separate agreement.
Attribution to **R.A. James (Captain Bob)** must be preserved in all distributions.

**Patent pending.** USPTO provisional filed December 2025 — physics-grounded self-adaptive runtime system. This license does not grant patent rights. Licensing inquiries: rajames440@gmail.com

---

*Robert A. James (Captain Bob) · Systems Engineer · Hacking since 1973*
