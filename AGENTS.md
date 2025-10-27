# Repository Guidelines

## Project Structure & Module Organization

StarForth C99 sources live under `src/`; VM entry points begin in `src/main.c` and core execution continues in
`src/vm.c`. Public headers reside in `include/`. Forth word definitions are grouped by module in `src/word_source/`.
Test harness code sits in `src/test_runner/`, with module suites under `src/test_runner/modules/`. Runtime defaults such
as `init.4th` sit in `conf/`, helper scripts in `scripts/`, and long-form docs in `docs/src/`; physics design notes live
under `docs/src/internal/` (see `PHYSICS_SCHEDULING_PLAN.adoc` and `PHYSICS_SIGNAL_MAP.adoc`). Treat
`StarForth-Governance/` as read-only and coordinate changes via the governance repo.

## Build, Test, and Development Commands

Run `make all` for the optimized binary at `build/starforth`. Use `make smoke` for quick validation, `make test` for the
full test mode (`./build/starforth --run-tests`), and `make debug` when you need symbol-rich output. Performance work
starts from `make fastest` or `make pgo`; both expect a clean tree. Reset artifacts with `make clean`, and combine with
`STRICT_PTR=1` while chasing pointer-safety regressions. Enable the profiler (`PROFILE=1 make test`) when validating new
physics signals.

## Coding Style & Naming Conventions

Code compiles as strict ANSI C99 with warnings treated as errors. Use four-space indentation, brace-on-same-line style,
and snake_case for functions and locals; macros stay in SHOUT_CASE. Document public entry points with brief Doxygen
comments, keep helpers `static`, and register new Forth words via the `forth_word_*` patterns and registry tables. Avoid
compiler-specific extensions and honor `STRICT_PTR` invariants.

## Testing Guidelines

Add new `_test.c` files under `src/test_runner/modules/` and wire them into `test_modules` in
`src/test_runner/test_runner.c`. Prefer the utilities in `test_common.h` for assertions. Run suites with
`./build/starforth --run-tests`; append `--fail-fast` during triage and `--break-me` for broad coverage sweeps. Target
high coverage for new APIs, and document intentional gaps in your PR. Exercise physics metadata paths with
profiler-backed tests and verify telemetry snapshots remain deterministic.

## Physics Metadata & Signals

Phase 1 physics metadata extends `DictEntry.physics` with fields such as `temperature_q8`, `last_active_ns`,
`mass_bytes`, and `avg_latency_ns`; map derivations back to the thermodynamic plan in
`docs/src/internal/PHYSICS_SCHEDULING_PLAN.adoc`. Populate metrics from existing hooks: `DictEntry.entropy`, profiler
counters (`include/profiler.h`), and monotonic clocks exposed by the L4Re KIP. When adding new telemetry sources,
consult `docs/src/internal/PHYSICS_SIGNAL_MAP.adoc` to align with approved kernel capabilities (`l4_kip_clock_ns`,
`l4_scheduler_info`, loader capabilities like `vbus_storage`). Keep additions cache-friendly (≤24 bytes per entry),
prefer 64-bit fixed-point math, and reserve unused slots (`acl_hint`, `pubsub_mask`) for future governance directives.

## Adaptive Optimization Pathways

Prototype advanced pathways (e.g., JIT emitters, word inlining, timing/core balancing) behind feature flags and allow
the physics inference engine to select strategies based on telemetry. Document each pathway under `docs/src/internal/`
with activation knobs, fallback behaviour, and scheduler touchpoints. Keep runtime toggles centralized in the physics
controller (e.g., `physics_mode`, `scheduler_policy`) and ensure every option degrades gracefully to the baseline
interpreter. Profile new pathways with `PROFILE=1 make test` and capture before/after metrics so governance can approve
promotion beyond experimental status.

## Commit & Pull Request Guidelines

Write topic-focused commits with imperative subjects (e.g., `fix logging regression`) and wrap descriptions near 72
characters. Before pushing, ensure `make clean && make test` passes (optionally with `STRICT_PTR=1`). PRs should
summarize intent, list test evidence, link any tracking issues, and include screenshots or benchmarks when altering
user-visible or performance-critical paths.
