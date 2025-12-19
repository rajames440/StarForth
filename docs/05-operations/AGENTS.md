# Repository Guidelines

## Project Structure & Module Organization
- `src/` contains the VM core; `src/platform/` handles host glue and timing; `src/word_source/` groups FORTH words by domain using the `<area>_words.c` pattern and matching headers in `src/word_source/include/`.
- `include/` is the public boundary; keep helpers and private state in `src/` to avoid symbol drift.
- `src/test_runner/` holds the in-process harness (`test_runner.c`) and module suites under `modules/`.
- `docs/` captures design notes, experiments, and platform guides; `scripts/` bundles automation for profiling, DoE runs, and doc generation.
- `build/` is disposable output (`build/<arch>/<target>/starforth`); `conf/` seeds default images/init scripts—regenerate via scripts instead of hand-editing.

## Build, Test, and Development Commands
- `make` — default optimized build for the current architecture; binary lands in `build/<arch>/standard/starforth`.
- `make fastest` — release-tuned build with LTO/asm fast paths; use for performance validation.
- `make debug` — `-O0 -g` debug build; run `make clean` before switching profiles.
- `make test` — runs the full harness (piped BYE) via the built binary; repeat quickly with `./build/amd64/standard/starforth --run-tests`.
- `make smoke` or `make bench` — fast sanity check / microbench; helpful before pushing hot-path changes.

## Coding Style & Naming Conventions
- Strict ANSI C99, four-space indentation, same-line braces (`static vm_t *init_vm(void) {`); no C11/++ features.
- Prefer `snake_case` for functions/vars, uppercase for macros, and `g_` prefixes for file-scoped statics.
- Keep exported APIs declared in `include/` with concise `/** ... */` headers; default to `static` for internals.
- Builds are expected warning-clean (`-Wall -Wextra -Werror`); gate new behavior behind existing flags (`STRICT_PTR`, `ENABLE_HOTWORDS_CACHE`, `ENABLE_PIPELINING`).

## Testing Guidelines
- Add suites in `src/test_runner/modules/` with a `run_<area>_tests` entry; register it inside `src/test_runner/test_runner.c`.
- Reuse helpers from `src/test_runner/include/test_common.h` and keep case IDs lowercase/descriptive.
- Run `make test` before PRs; for quick checks during development, use `make smoke` or `./build/<arch>/<target>/starforth --run-tests --fail-fast`.
- When touching public APIs or hot paths, add coverage alongside the new code and note any performance measurements.

## Commit & Pull Request Guidelines
- Use sentence-style subjects under ~72 chars and keep commits narrowly scoped; include rationale and key flags in bodies.
- PRs should state intent, verification commands (e.g., `make test`, `make bench`), and any config/doc updates (init scripts, experiment docs).
- Avoid editing generated assets in `build/` or direct tweaks to seeded configs; prefer existing scripts for regeneration.
- Flag platform-specific changes early so reviewers from the relevant subsystem can weigh in.

## Security & Configuration Tips
- Treat `conf/` seeds and any disk/init artifacts as shared fixtures—copy before experiments and regenerate via `scripts/` rather than editing in place.
- Mirror existing cleanup/ownership patterns when adding platform code (`src/platform/`) to prevent resource leaks in long-running benchmarks.
