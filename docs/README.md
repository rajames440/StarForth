# StarForth Documentation Index

Jump to the guide you need using the links below.

## Getting Started

- [Repository Overview](../README.md) – Feature tour, project goals, and directory layout.
- [Quick Start Guide](QUICKSTART.md) – Build/test recipes and day-one workflows.
- [Installation Guide](INSTALL.md) – Platform-specific setup and prerequisites.

## Build & Tooling

- [Build Options Reference](BUILD_OPTIONS.md) – Make targets, flags, and configuration knobs.
- [PGO Guide](PGO_GUIDE.md) – Profile-guided optimisation pipeline and verification steps.
- [Init Tools](INIT_TOOLS.md) – Utilities for managing `conf/init.4th` and block images.
- [Documentation System](DOCUMENTATION_README.md) – Generating API docs and publishing outputs.

## Architecture & Internals

- [Architecture Deep Dive](ARCHITECTURE.md) – VM layout, execution model, and subsystems.
- [ABI Reference](ABI.md) – Calling conventions and binary interfaces.
- [Init System](INIT_SYSTEM.md) – Boot-time block loading and dictionary fencing.
- [Block Storage Guide](BLOCK_STORAGE_GUIDE.md) – Design and tuning of the block I/O stack.

## Platform Integration

- [Platform Abstraction Layer](PLATFORM_ABSTRACTION.md) – Host glue responsibilities and APIs.
- [Platform Build Guide](PLATFORM_BUILD_GUIDE.md) – Cross-compilation matrix and toolchains.
- [L4Re Integration Guides](L4RE_INTEGRATION.md) – Kernel porting notes with supporting
  docs: [Dictionary Allocation](L4RE_DICTIONARY_ALLOCATION.md) and [Block I/O Endpoints](l_4_re_blkio_endpoints.md).
- [Raspberry Pi Build](RASPBERRY_PI_BUILD.md) – ARM64 bring-up and validation checklist.

## Performance & Profiling

- [x86_64 Assembly Optimisations](ASM_OPTIMIZATIONS.md) – Fast paths and integration guidance.
- [ARM64 Assembly Strategy](ARM64_OPTIMIZATIONS.md) – Architecture notes and tuning tips.
- [Profiler Guide](PROFILER.md) – Runtime profiling workflow and report formats.

## Testing & Quality

- [Test Suite Guide](TESTING.md) – Harness structure, conventions, and extension points.
- [Break-Me Report](BREAK_ME_REPORT.md) – Snapshot from the exhaustive stress suite.
- [Gap Analysis](GAP_ANALYSIS.md) – Current coverage, remaining work, and quality grades.

## Contribution Guidelines

- [Contributing Guide](CONTRIBUTING.md) – Workflow expectations and review checklist.
- [Code of Conduct](CODE_OF_CONDUCT.md) – Community standards.
- [Word Development Guide](WORD_DEVELOPMENT.md) – Adding new words to the dictionary.
- [Doxygen Style Guide](DOXYGEN_STYLE_GUIDE.md) & [Quick Reference](DOXYGEN_QUICK_REFERENCE.md) – Documentation
  standards for public APIs.

---

Need something else? Run `rg --files docs` for the full inventory.
