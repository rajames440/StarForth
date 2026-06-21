# scripts/

Utility scripts for building, profiling, DoE experiments, and documentation.

## DoE / experiment scripts

| Script | Purpose |
|--------|---------|
| `run_doe.sh` | Run a standard DoE experiment |
| `run_factorial_doe.sh` | 2^7 factorial DoE |
| `run_factorial_doe_with_heartbeat.sh` | Factorial DoE with heartbeat metrics |
| `run_nested_doe.sh` | Nested DoE design |
| `run_ablation_study.sh` | Physics loop ablation |
| `run_optimization_doe.sh` | Optimization-focused DoE |
| `run_quick_doe_test.sh` | Quick smoke-check DoE (1 rep) |
| `extract_doe.sh` | Extract CSV rows from QEMU serial logs |

## Build / profile scripts

| Script | Purpose |
|--------|---------|
| `perf-profile.sh` | Linux `perf` profiling harness |
| `perf-profile_control.sh` | Control run for perf comparison |
| `pgo-workload.sh` | PGO workload for profile-guided optimization build |
| `rundisk.sh` | Boot a disk image in QEMU |
| `qemu_screenshot.sh` | Capture QEMU serial output |

## FORTH capsule scripts

| Script | Purpose |
|--------|---------|
| `4th-apply-init.sh` | Apply init.4th patches to capsule set |
| `4th-update-init.sh` | Update init.4th across capsule variants |

## Documentation scripts

| Script | Purpose |
|--------|---------|
| `asciidoc-to-latex.sh` | Convert AsciiDoc to LaTeX |
| `convert-markdown-to-asciidoc.sh` | Markdown → AsciiDoc |
| `doxygen-xml-to-asciidoc.py` | Doxygen XML → AsciiDoc |
| `generate-doxygen-appendix.sh` | Build Doxygen API appendix |
| `build_patent_structure.sh` | Scaffold patent document structure |
| `setup-ide-integration.sh` | Configure IDE integration |
| `analyze_heartbeat_stability.R` | R analysis of heartbeat stability data |
| `analyze_opp2_regression.py` | Python regression analysis |
| `gen_pe_reloc.py` | PE relocation table generator (see also `tools/`) |
| `remove_loop_conditionals.sh` | Remove loop conditional guards |

## See also

- [`experiments/bare_metal/README.md`](../experiments/bare_metal/README.md) — DoE protocols
- [Project root](../README.md)
