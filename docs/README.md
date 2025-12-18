# StarForth Documentation

This directory contains all documentation for the StarForth project, organized by topic.

## 📖 START HERE: Formal Definitions

**[ONTOLOGY.md](ONTOLOGY.md)** - Authoritative definitions for all technical terms, explicit metaphor labeling, and academic writing guidelines.

**[ACADEMIC_WORDING_GUIDELINES.md](ACADEMIC_WORDING_GUIDELINES.md)** - Language discipline rules to maintain scientific rigor.

**All contributors and academic authors must reference the ontology for term definitions.**

---

## Quick Navigation

- **[01-getting-started/](01-getting-started/)** - New user guides and quick-start documentation
- **[02-experiments/](02-experiments/)** - Experimental design, execution guides, and protocols
- **[03-architecture/](03-architecture/)** - System architecture and design documentation
- **[04-quality/](04-quality/)** - Testing, validation, audits, and quality assurance
- **[05-operations/](05-operations/)** - CI/CD, workflows, and operational procedures
- **[06-research/](06-research/)** - Academic research and publication materials
- **[07-session-logs/](07-session-logs/)** - Historical development session summaries
- **[src/](src/)** - Detailed technical documentation (internals, APIs, guides)
- **[archive/](archive/)** - Outdated or superseded documentation (kept for reference)
- **[assets/](assets/)** - Binary files and archives (should be moved to appropriate storage)

## Current Active Work

- **HAL Migration**: Hardware Abstraction Layer enabling StarKernel - See `starkernel/hal/migration-plan.md`
- **StarKernel Development**: Path to UEFI-bootable kernel - See `starkernel/hal/starkernel-integration.md`
- **StarKernel Formal Verification**: Isabelle/HOL proofs - See `starkernel/` directory
- **Physics Engine**: Physics-driven adaptive runtime - See `03-architecture/physics-engine/`
- **Experiments**: Design of Experiments validation - See `02-experiments/`

## Contributing

When adding new documentation:
1. Place it in the appropriate numbered directory based on its purpose
2. Use descriptive filenames in lowercase with hyphens
3. Add a README.md to new subdirectories explaining their contents
4. Move outdated docs to `archive/` rather than deleting them
