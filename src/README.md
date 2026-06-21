# src/

StarForth VM source tree — hosted build (Linux / L4Re). All files are strict
ANSI C99 with `-Wall -Werror`.

## Top-level files

| File | Purpose |
|------|---------|
| `main.c` | Entry point, CLI parsing, DoE mode |
| `vm.c` | Interpreter loop, stacks, dictionary state |
| `vm_api.c` | External VM API |
| `vm_bootstrap.c` | VM bootstrap initialization |
| `vm_debug.c` | Debugging utilities |
| `vm_time.c` | Time-related VM operations |
| `repl.c` | Read-eval-print loop |
| `cli.c` | CLI argument parsing |
| `io.c` | I/O operations |
| `log.c` | Logging infrastructure |
| `memory_management.c` | Dictionary allocator |
| `dictionary_management.c` | Dictionary allocation & search |
| `dictionary_heat_optimization.c` | Loop #1 — execution heat tracking |
| `word_registry.c` | Word registration system |
| `block_subsystem.c` | Logical → physical block mapper |
| `blkio_*.c` | Block I/O backends (file, RAM, factory) |
| `stack_management.c` | Stack operations |
| `math_portable.c` | Portable math functions |
| `profiler.c` | Performance profiling |
| `heartbeat_export.c` | Heartbeat metrics export |
| `ssm_jacquard.c` | L8 Jacquard steady-state machine |
| `doe_metrics.c` | Design of Experiments metrics (2^7 factorial) |
| `physics_runtime.c` | Main physics coordinator |
| `physics_hotwords_cache.c` | Loop #1 — hot-words caching |
| `physics_metadata.c` | Metadata tracking |
| `physics_pipelining_metrics.c` | Loop #4 — word transition prediction |
| `physics_execution_hooks.c` | Execution instrumentation |
| `rolling_window_of_truth.c` | Loop #2 — circular execution history |
| `inference_engine.c` | Loops #5/#6 — ANOVA + statistical inference |

## Subdirectories

| Directory | Contents |
|-----------|----------|
| [`word_source/`](word_source/README.md) | 29 FORTH-79 word implementation files |
| [`test_runner/`](test_runner/README.md) | 1000+ test cases and harness |
| [`starkernel/`](starkernel/README.md) | LithosAnanke bare-metal kernel (37 files) |
| `platform/` | Platform abstraction: `linux/time.c`, `l4re/time.c` |

## See also

- [`include/`](../include/README.md) — header files
- [`Makefile`](../Makefile) — hosted build system
- [Project root](../README.md)
