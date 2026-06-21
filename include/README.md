# include/

Public header files for the StarForth VM and StarKernel. Shared between
the hosted build and the bare-metal kernel build (`__STARKERNEL__` gate).

## Core VM headers

| Header | Contents |
|--------|---------|
| `vm.h` | `VM` struct, `DictEntry`, `DictPhysics`, `RollingWindowOfTruth`, `HeartbeatState`, `CapsuleDesc` — the entire VM state |
| `vm_api.h` | External VM API callable from C |
| `vm_debug.h` | Debug utilities |
| `vm_host.h` | Hosted-build interface |
| `vm_asm_opt.h` | ASM optimisation hooks (x86_64) |
| `vm_asm_opt_arm64.h` | ASM optimisation hooks (AArch64) |
| `vm_asm_opt_riscv64.h` | ASM optimisation hooks (RISC-V 64) |
| `vm_inner_interp_asm.h` | Inner interpreter (x86_64) |
| `vm_inner_interp_arm64.h` | Inner interpreter (AArch64) |
| `vm_inner_interp_riscv64.h` | Inner interpreter (RISC-V 64) |

## Physics engine headers

| Header | Contents |
|--------|---------|
| `physics_runtime.h` | Main physics coordinator |
| `physics_hotwords_cache.h` | Loop #1 — hot-words cache |
| `physics_metadata.h` | Metadata tracking |
| `physics_pipelining_metrics.h` | Loop #4 — pipeline metrics |
| `physics_execution_hooks.h` | Execution instrumentation |
| `rolling_window_of_truth.h` | Loop #2 — circular execution history |
| `rolling_window_knobs.h` | Rolling window tuning parameters |
| `inference_engine.h` | Loops #5/#6 — ANOVA + statistical inference |
| `ssm_jacquard.h` | L8 Jacquard steady-state machine |

## Block / storage headers

| Header | Contents |
|--------|---------|
| `block_subsystem.h` | Logical → physical block mapper |
| `blkio.h` | Block I/O interface |
| `blkio_factory.h` | Block I/O backend factory |
| `blkcfg.h` | Block configuration constants |

## Platform / misc headers

| Header | Contents |
|--------|---------|
| `starforth_config.h` | Build-time configuration constants |
| `version.h` | Version strings |
| `arch_detect.h` | Architecture detection macros |
| `platform_alloc.h` | Platform memory allocation |
| `platform_lock.h` | Platform locking primitives |
| `platform_time.h` | Platform time interface |
| `math_portable.h` | Portable math |
| `q48_16.h` | Q48.16 fixed-point arithmetic |
| `profiler.h` | Performance profiler |
| `doe_metrics.h` | DoE metrics structs |
| `word_registry.h` | Word registration API |
| `dictionary_management.h` | Dictionary API |
| `dictionary_heat_optimization.h` | Heat tracking API |
| `memory_management.h` | Dictionary allocator |
| `repl.h` | REPL interface |
| `io.h` | I/O interface |
| `log.h` | Logging interface |
| `cli.h` | CLI parsing interface |
| `startup.h` | VM startup helpers |
| `starkernel/` | Kernel-only headers |

## See also

- [`src/README.md`](../src/README.md) — implementation files
- [Project root](../README.md)
