# Repository Guidelines

## Project Structure & Module Organization

- `src/` houses the VM core, with `platform/` providing time/OS glue and `test_runner/` hosting the in-process harness.
- `include/` holds public headers shared between VM code and tests so both build cleanly under `-Werror`.
- `src/word_source/` is the modular FORTH vocabulary; mirror the `*_words.c` naming when adding new word families.
- `src/test_runner/modules/` contains companion suites; ensure each file exports `run_*_tests` and is referenced in
  `test_runner.c`.
- `docs/` (manuals), `scripts/` (automation), and `disks/` (sample images) stay versioned; `build/` is disposable
  output.

## Build, Test, and Development Commands

- `make` produces `build/starforth` with the optimized defaults (STRICT_PTR=1 and assembly fast paths).
- `make debug` compiles a `-O0 -g` binary for stepping through the VM.
- `make test` or `./build/starforth -t` runs the modular harness with fail-fast logging.
- `./build/starforth --disk-img=disks/starforth-dev.img --ram-disk=10` boots the REPL against the dev image.
- `make fastest` followed by `./build/starforth --benchmark 1000 --log-none` captures throughput regressions.

## Coding Style & Naming Conventions

- Stick to ANSI C99, four-space indentation, and braces aligned with the function signature (
  `static void cleanup(void) {`).
- Use `snake_case` for identifiers, reserve `UPPER_SNAKE` for macros/constants, and suffix new word files as
  `<area>_words.c`.
- Document exported APIs with focused `/** */` blocks; keep internal comments brief and context-driven.
- Limit header exposure to what callers need, and ensure new code passes the default `-Wall -Werror` build with
  `STRICT_PTR` enabled.

## Testing Guidelines

- Add coverage in the matching `src/test_runner/modules/<area>_test.c` file using the existing `WordTestSuite`
  scaffolding.
- Register new runners inside `test_runner.c`’s `test_modules[]` array to include them in the suite.
- Name scenarios with lowercase underscores (e.g., `"max_int"`) and keep expectation strings concise for log clarity.
- Run `make test` before every PR; record `./build/starforth --benchmark 1000 --log-none` when touching
  performance-sensitive code.

## Commit & Pull Request Guidelines

- Follow the house style: optional leading emoji + imperative summary + em dash detail (
  `✨ Add block cache — tighten flush path`), ≤72 characters.
- Keep commits focused and use body paragraphs for rationale, config changes, or rollout notes.
- PRs should explain purpose, list verification commands, and link related issues or documentation updates.
- Flag compatibility or configuration changes (platform flags, STRICT_PTR tweaks) and attach artifacts when docs or
  assets change.
