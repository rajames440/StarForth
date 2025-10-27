# Repository Guidelines

## Project Structure & Module Organization

StarForth keeps its ANSI C99 sources under `src/`, with the VM entry points in `src/main.c` and core execution in
`src/vm.c`. Public headers live in `include/`, while Forth word implementations are grouped by module in
`src/word_source/`. The purpose-built harness and module tests sit in `src/test_runner/` (add new suites under
`src/test_runner/modules/`). Runtime defaults such as `init.4th` reside in `conf/`, assets and utilities in `scripts/`,
and long-form documentation in `docs/src/`. Treat `StarForth-Governance/` as read-only; changes there must go through
the governance repo.

## Build, Test, and Development Commands

Use `make all` for the standard optimized build (outputs `build/starforth`). `make test` runs the compiled binary in
test mode (`./build/starforth -t`) and should be clean before every PR. Quick sanity checks rely on `make smoke`, while
`make debug` produces a symbols-rich binary for step-through debugging. Performance work typically starts from
`make fastest` (ASM + direct threading) or `make pgo` for profile-guided builds. Reset artifacts with `make clean`;
combine with `STRICT_PTR=1` when chasing pointer discipline regressions.

## Coding Style & Naming Conventions

Stick to strict ANSI C99, compiling with warnings-as-errors; verify with the default Makefile flags. Source files use
four-space indentation, braces on the same line, and snake_case identifiers for functions and locals; macros remain
SHOUT_CASE. Document public entry points with brief Doxygen comments and keep helper functions `static`. New words
mirror existing naming (`forth_word_*`) and register through the word registry tables. Honour `STRICT_PTR` expectations
and avoid introducing compiler-specific extensions.

## Testing Guidelines

Extend coverage by adding `_test.c` modules in `src/test_runner/modules/` and wiring them into the `test_modules` array
in `src/test_runner/test_runner.c`. Keep assertions granular so the fail-fast harness can pinpoint regressions; prefer
the helpers in `test_common.h`. Use `./build/starforth --run-tests` for full suites, `--fail-fast` during triage, and
`--break-me` when validating broad integrations. Aim for near-total coverage of newly exposed APIs and explain any
intentional gaps in your PR description.

## Commit & Pull Request Guidelines

Create small, topic-focused commits with imperative subjects ("fix logging regression"), wrapping bodies at ~72
characters. Ensure `make clean && make test` (optionally with `STRICT_PTR=1`) passes before you push. PRs should
summarize intent, list test evidence, link related issues, and call out any docs touched. Never modify
`StarForth-Governance/`; instead, open an issue in the governance repo and reference it. Provide screenshots or
benchmark outputs when altering performance-critical paths or user-visible behaviour.
