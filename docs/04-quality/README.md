# Quality Assurance

This directory contains testing, validation, audits, and quality assurance processes for StarForth.

## Quality Processes

- **[audits/](audits/)** - Code audits, implementation reviews, codebase audits
- **[phase-tracking/](phase-tracking/)** - Phase completion checklists and session summaries
- **[regression/](regression/)** - Regression detection framework and reports
- **[validation/](validation/)** - Validation protocols and comprehensive physics validation

## When to Use

**Before Committing:**
- Run `make test` - All 936+ tests must pass
- Run `make` with `-Wall -Werror` - Zero warnings required

**Before Releasing:**
- Run full validation suite
- Execute DoE mode: `./starforth --doe`
- Verify 0% algorithmic variance

**After Major Changes:**
- Run regression detection framework
- Compare DoE results with baseline
- Update validation documentation

**Phase Completion:**
- Create implementation audit
- Run comprehensive physics validation
- Document results in phase-tracking/

## Test Coverage

### Unit Tests (POST - Power-On Self Test)
- Q48.16 fixed-point arithmetic
- Inference statistics (ANOVA early-exit)
- Decay slope inference (exponential regression)

### Dictionary Tests
- FORTH-79 word compliance across 18 modules
- Stack words (DUP, DROP, SWAP, ROT, etc.)
- Arithmetic words (+, -, *, /, MOD, etc.)
- Control words (IF, ELSE, THEN, DO, LOOP, etc.)
- Defining words (:, ;, CREATE, DOES>, VARIABLE, CONSTANT)

### Integration Tests
- VM + physics subsystems
- Heartbeat coordination
- Block I/O subsystem

### DoE Validation
- **Gold Standard:** 0% algorithmic variance across 90 experimental runs
- Deterministic behavior guarantee
- Reproducible across platforms (Linux, L4Re)

## Quality Metrics

**Current Status:**
- ✅ 936+ tests passing
- ✅ 0% algorithmic variance (formally validated)
- ✅ Zero warnings with `-Wall -Werror`
- ✅ Strict ANSI C99 compliance

## See Also

- Comprehensive validation protocol: `validation/comprehensive-physics-validation.md`
- Break-me testing report: `validation/break-me-report.md`
- Regression detection framework: `regression/detection-framework.md`