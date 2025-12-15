# Contributing to StarForth

Thank you for your interest in contributing to StarForth! This document provides guidelines for contributing to the project.

## Quick Start

1. **Fork and clone** the repository
2. **Build** the project: `make`
3. **Run tests**: `make test` (all 780+ tests must pass)
4. **Read** [ONTOLOGY.md](ONTOLOGY.md) for formal definitions and terminology
5. **Read** `docs/CLAUDE.md` for project overview
6. **Explore** `docs/03-architecture/` for architecture details

**Important:** All contributors must use terminology from [ONTOLOGY.md](ONTOLOGY.md) in code comments and documentation. This ensures precise, academically rigorous language throughout the project.

## Code Standards

### Strict ANSI C99
- No GNU extensions
- No C++ features
- Compile with `-std=c99 -Wall -Werror`
- Zero warnings policy

### Code Style
```c
/* Function comment */
void function_name(VM *vm) {
    int local_variable;              /* Lower snake_case */

    if (condition) {
        /* Body indented with 4 spaces */
    }
}
```

### Platform Abstraction
- **Never** call platform-specific APIs directly from VM code
- **Always** use HAL interfaces (`hal_time.h`, `hal_memory.h`, etc.)
- See `docs/03-architecture/hal/` for HAL documentation

### Memory Safety
- Use `vaddr_t` for VM addresses (not C pointers)
- Use `vm_load_cell()` / `vm_store_cell()` for memory access
- Enable `STRICT_PTR=1` during development
- No hidden state - all VM state in `VM` struct

## Development Workflow

### 1. Before Starting

**Check existing issues** - See if your idea/bug is already tracked

**Discuss major changes** - Open an issue for discussion before large changes

### 2. Making Changes

```bash
# Create feature branch
git checkout -b feature/your-feature-name

# Make changes, commit frequently
git add <files>
git commit -m "Brief description of change"

# Run tests after each logical change
make test
```

### 3. Testing Requirements

All contributions **must**:

✅ Pass all 780+ unit tests:
```bash
make test
```

✅ Build without warnings:
```bash
make clean && make STRICT_PTR=1
```

✅ Maintain deterministic behavior (for physics changes):
```bash
make fastest
./build/amd64/fastest/starforth --doe
# Verify 0% algorithmic variance
```

### 4. Commit Messages

Follow conventional commit format:

```
type(scope): Brief description

Longer explanation if needed, including:
- Why this change is necessary
- What behavior changes
- Any breaking changes

Fixes #issue-number (if applicable)
```

**Types:** `feat`, `fix`, `refactor`, `docs`, `test`, `perf`, `chore`

**Examples:**
```
feat(vm): Add new FORTH-79 word implementation

Implements the RECURSE word according to FORTH-79 specification.
Includes unit tests and documentation.

fix(hal): Correct TSC frequency calibration on AMD CPUs

The TSC calibration was failing on certain AMD processors due to
incorrect HPET register access. This fixes the calibration routine
to work correctly across all x86_64 CPUs.

Fixes #123

docs(hal): Update platform-implementations.md

Add section on common TSC calibration pitfalls.
```

### 5. Pull Request Process

1. **Update documentation** if needed
2. **Add tests** for new features
3. **Run full test suite**: `make test`
4. **Push to your fork**
5. **Open pull request** with:
   - Clear description of changes
   - Reference to any issues
   - Test results (all tests passing)

## Areas for Contribution

### High Priority
- **HAL Migration**: Refactoring existing code to use HAL interfaces
- **Test Coverage**: Adding tests for edge cases
- **Documentation**: Improving architecture documentation
- **Platform Support**: Implementing HAL for new platforms

### Medium Priority
- **Performance**: Optimizations with benchmark validation
- **Error Handling**: Improving error messages and diagnostics
- **Developer Tools**: Build system improvements, debugging aids

### Future Work
- **StarKernel**: UEFI boot loader, kernel HAL implementations
- **StarshipOS**: Storage drivers, networking, process model

## Code Review Guidelines

### For Contributors
- Keep PRs focused (one feature/fix per PR)
- Respond to review feedback promptly
- Be open to suggestions and refactoring

### For Reviewers
- Be constructive and respectful
- Focus on:
  - Code correctness
  - Test coverage
  - ANSI C99 compliance
  - Platform abstraction
  - Performance implications (for hot paths)

## Experimental Work

If contributing to physics subsystems or experiments:

1. **Follow DoE methodology** - See `docs/02-experiments/`
2. **Validate determinism** - 0% algorithmic variance required
3. **Document methodology** - Record experimental protocol
4. **Statistical validation** - Use ANOVA, Levene's test as appropriate
5. **Reproducibility** - Provide reproduction steps

## Documentation Standards

When documenting:

1. **Use Markdown** for all documentation
2. **Include code examples** where appropriate
3. **Add metadata headers**:
   ```markdown
   # Document Title

   **Status:** Draft | Complete | Deprecated
   **Last Updated:** YYYY-MM-DD
   **Audience:** Developers | Users | Researchers
   ```
4. **Update README** files when adding subdirectories
5. **Archive** outdated docs instead of deleting

## Testing Guidelines

### Unit Tests
```c
/* In src/test_runner/modules/my_test.c */
static void test_my_feature(void) {
    VM *vm = create_test_vm();

    /* Test setup */
    vm_push(vm, 42);

    /* Execute word */
    execute_word(vm, "MY-WORD");

    /* Assertions */
    assert(vm_pop(vm) == 42);

    destroy_test_vm(vm);
}
```

### Integration Tests
- Test VM + physics subsystems together
- Validate heartbeat coordination
- Check deterministic behavior

### Performance Tests
- Use `make bench` for quick validation
- Profile with `-O3 -fno-inline` for hot paths
- Document performance impact of changes

## Getting Help

**Questions?**
- Check `docs/` first (especially `docs/CLAUDE.md`)
- Search existing issues
- Open a new issue with `[Question]` tag

**Found a bug?**
- Check if already reported
- Include:
  - Platform (Linux, L4Re, etc.)
  - Build configuration
  - Minimal reproduction steps
  - Expected vs. actual behavior

**Security issues?**
- Do NOT open public issues
- Email: [security contact TBD]

## License

By contributing to StarForth, you agree that your contributions will be licensed under the same terms. See ./LICENSE for details.

---

**Thank you for contributing to StarForth!**

The goal is to build a formally verified, physics-driven adaptive runtime that enables StarKernel and StarshipOS. Every contribution, no matter how small, helps move this vision forward.