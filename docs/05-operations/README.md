# Operations

This directory contains operational procedures, workflows, CI/CD documentation, and AI agent collaboration guidelines.

## Contents

- **backlog-gatekeeper.md** - Backlog management and prioritization process
- **workflow.md** - Development workflow and branching strategy

## Development Workflow

### Standard Development Cycle

1. **Branch:** Create feature branch from `master`
2. **Develop:** Make changes, commit frequently
3. **Test:** `make test` - All 936+ tests must pass
4. **Validate:** Run DoE if physics changes
5. **Review:** Self-review or peer review
6. **Merge:** Merge to `master` when complete

### Release Process

1. Tag release: `git tag v1.x.x`
2. Build all targets: `make clean && make fastest`
3. Run full test suite: `make test`
4. Validate determinism: `./starforth --doe`
5. Generate documentation: `make book`
6. Create release artifacts

## Build Targets

```bash
make                    # Standard optimized build
make fastest            # Maximum performance (ASM + LTO)
make pgo                # Profile-guided optimization
make debug              # Debug symbols
make test               # Run 936+ tests
make bench              # Quick benchmark
```

## CI/CD (Future)

**Planned automation:**
- Automated builds for all platforms (Linux, L4Re, StarKernel)
- Test suite execution on every commit
- DoE regression checks on physics changes
- Release artifact generation
- Documentation builds (PDF + HTML)

## Deployment

### Linux / L4Re
```bash
make PLATFORM=linux
./build/amd64/standard/starforth
```

### StarKernel (Future)
```bash
make PLATFORM=kernel
# Creates starforth.efi (UEFI bootable image)
# Deploy to ESP: /boot/efi/EFI/BOOT/BOOTX64.EFI
# Or test with QEMU: qemu-system-x86_64 -bios OVMF.fd ...
```

## AI Agent Collaboration

See `../AGENTS.md` and `../AI_AGENT_MANDATORY_README.md` for guidelines on working with AI coding assistants.

## Task Management

Current task tracking: `../TASK_LIST.md` (may be superseded by GitHub Issues after migration)