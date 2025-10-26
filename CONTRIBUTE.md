# 🤝 **Contributing to StarForth**

Thank you for your interest in contributing to StarForth! This document outlines how to get involved.

## Getting Started

1. **Read the Docs** – Start with [docs/src/README.adoc](docs/src/README.adoc) for a complete guide to the project
   structure, architecture, and development workflow.

2. **Build & Test** – Follow the build instructions in the main README to compile StarForth and run the test suite.

3. **Explore the Codebase** – Start with `src/main.c` and `src/vm.c` to understand the core interpreter.

## How to Contribute

### Reporting Issues

- Use clear, descriptive titles
- Include reproduction steps
- Specify your build environment (OS, compiler, architecture)
- Attach test output or logs if relevant

### Code Changes

1. **Create a feature branch** from `master`
2. **Write clean, ANSI C99-compliant code** – use `make` to verify with strict warnings enabled
3. **Add tests** for new functionality – use the existing test harness in `tests/`
4. **Document changes** – update Doxygen comments and relevant .adoc files
5. **Keep commits atomic** – one logical change per commit with clear messages
6. **Test thoroughly** – run `make test` and `make smoke` before submitting

### ⚠️ Important: Read-Only Submodule

**StarForth-Governance is a read-only submodule.** Only the project maintainer can modify governance files.

If you accidentally modify files in `StarForth-Governance/`:

```bash
# The pre-commit hook will BLOCK your commit with an error message

# To undo the changes:
git reset HEAD StarForth-Governance/
git checkout -- StarForth-Governance/
```

**To request governance changes:**

1. Create a GitHub issue describing what needs to change
2. Link the issue in your PR comments
3. The maintainer will handle the governance changes separately

This protection prevents broken repository states where governance commits exist in the parent repo but not in the
submodule.

### Pull Request Process

1. Ensure all tests pass: `make clean && make test`
2. Verify pointer safety: `make STRICT_PTR=1 test`
3. **Do NOT modify StarForth-Governance/** (it's read-only)
4. Write a clear PR description explaining your changes
5. Link any related issues
6. Request review from maintainers

## Code Standards

- **Language:** ANSI C99 (no C11+ features)
- **Warnings:** Zero compiler warnings with `-Wall -Wextra`
- **Style:** Linux kernel style (see `ARCHITECTURE.md`)
- **Comments:** Doxygen-compatible doc comments for public APIs
- **Testing:** 100% coverage of public functions with fail-fast tests

## Architecture & Design

StarForth uses:

- **Direct-threaded interpretation** for execution
- **Modular word dictionary** with independent subsystems
- **Strict memory discipline** (STRICT_PTR mode enforces pointer bounds)
- **Platform abstraction layer** for portability

See [docs/src/architecture-internals/ARCHITECTURE.adoc](docs/src/architecture-internals/ARCHITECTURE.adoc) for deep-dive
details.

## Questions?

- Check [docs/src/README.adoc](docs/src/README.adoc) for comprehensive documentation
- Review existing test cases in `tests/` for usage examples
- Open a discussion issue on GitHub

---

**Thank you for helping make StarForth faster, cleaner, and more reliable!**