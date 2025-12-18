# Repository Guidelines

## Project Structure & Module Organization
- Core VM and runtime live in `src/` (e.g., `vm.c`, `physics_runtime.c`); headers in `include/`.
- Tests sit under `src/test_runner/` with modules like `..._words_test.c`; shared helpers are in `src/test_runner/include/`.
- Build outputs land in `build/<arch>/<target>/` (e.g., `build/amd64/standard/starforth`); keep the tree clean by using `make clean` before releases.
- Docs and papers reside in `docs/`, `formal/`, and `papers/`; scripts and tooling live in `scripts/` and `tools/`.

## Build, Test, and Development Commands
- `make` — Standard optimized build (auto-detects arch, `TARGET=standard`).
- `make TARGET=fastest` — Maximum performance build with direct threading and asm opts.
- `make test` — Run full FORTH-79 test suite (~780 cases); required before any PR.
- `make smoke` — Quick sanity run; use during iterative development.
- `make bench` — Lightweight performance check; share results when changing hot paths.
- `./build/amd64/standard/starforth` — Launch REPL; add `--doe` to exercise deterministic physics loop for regressions.
- `make help` — Lists all available targets and tuning knobs.

## Coding Style & Naming Conventions
- Strict ANSI C99 only; no GNU/C++ extensions. Compiler flags include `-std=c99 -Wall -Werror`.
- Four-space indentation, lower_snake_case for functions/variables, and concise block comments for non-obvious logic.
- Clang-Tidy is configured via `.clang-tidy`; run `clang-tidy` on touched files when altering VM or physics code.
- Keep platform-specific code behind existing abstraction layers (e.g., blkio, platform_*); avoid direct syscalls in VM paths.

## Testing Guidelines
- Add or extend tests in `src/test_runner/modules/` using the existing naming pattern (`<domain>_words_test.c`).
- Favor fast feedback with `make smoke`; gate merges on `make test`.
- For physics/runtime changes, build `TARGET=fastest` and run `--doe` multiple times to confirm 0% algorithmic variance (hash outputs to verify identical runs).

## Commit & Pull Request Guidelines
- Use conventional commits: `type(scope): summary` (e.g., `fix(vm): prevent stack underflow`). Keep scopes aligned to modules (`vm`, `blkio`, `physics`, `docs`, `tests`).
- Small, focused commits with clear rationale. Include test commands executed in the PR description.
- PRs should link related issues, call out user-visible behavior changes, note performance/determinism impacts, and include docs updates when interfaces or terminology shift.
- Screenshots or logs are not required unless demonstrating regressions, determinism proofs, or benchmark deltas.
