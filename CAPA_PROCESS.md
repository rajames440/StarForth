# CAPA Process - Change Management & Audit Trail

**StarForth Change Management via CAPA (Corrective/Preventive Action)**

**Document Version:** 1.0
**Last Updated:** October 30, 2025
**Audience:** Developers, QA, PM, Governance

---

## Overview

A **CAPA** (Corrective/Preventive Action) is a documented change request that tracks:
- **WHAT** changed (code, documentation, build system)
- **WHY** it changed (bug fix, feature, refactor, enhancement)
- **HOW** it was validated (tests passed, reviews, approvals)
- **WHO** approved it (QA, PM)
- **WHEN** it was closed (audit trail)

Every PR in StarForth **MUST** be associated with a CAPA. This creates a governance-ready audit trail.

---

## CAPA Lifecycle

```
┌─────────────────────────────────────────────────────────────┐
│ STEP 1: CREATE CAPA (GitHub Issue)                          │
│ • Issue title describes change                              │
│ • Issue body documents reason & impact                      │
│ • Issue gets ID (e.g., #123)                                │
│ • Status: OPEN                                              │
└────────────────────┬────────────────────────────────────────┘
                     │
┌─────────────────────▼────────────────────────────────────────┐
│ STEP 2: IMPLEMENT (Create PR)                               │
│ • Developer: git checkout -b feature/capa-123               │
│ • Commit with reference: "CAPA #123: Add feature X"         │
│ • Create PR linking to issue: "Closes #123"                 │
│ • PR triggers GitHub Actions (devL → test → qual)          │
│ • Status: IN PROGRESS                                       │
└────────────────────┬────────────────────────────────────────┘
                     │
┌─────────────────────▼────────────────────────────────────────┐
│ STEP 3: VALIDATE (GitHub Actions)                           │
│ • devL: Build + smoke test                                  │
│ • test: Full test suite                                     │
│ • qual: Formal verification (Isabelle)                      │
│ • All pass? → Ready for closure decision                    │
│ • Any fail? → Back to STEP 2 (fix & re-test)               │
│ • Status: VALIDATING                                        │
└────────────────────┬────────────────────────────────────────┘
                     │
         ┌───────────┴──────────────┬──────────────────┐
         │                          │                  │
    ┌────▼──────┐          ┌────────▼────────┐  ┌─────▼──────┐
    │ AUTO      │          │ QA MANUAL       │  │ PM MANUAL  │
    │ CLOSURE   │          │ APPROVAL        │  │ APPROVAL   │
    └────┬──────┘          └────────┬────────┘  │ (RELEASE)  │
         │                          │           └─────┬──────┘
         │                    ┌─────▼──────────┐     │
         │                    │ QA Reviews &   │     │
         │                    │ Approves CAPA  │     │
         │                    └─────┬──────────┘     │
         │                          │                │
         └──────────┬───────────────┘                │
                    │                                │
             ┌──────▼────────────────────────────────▼─────┐
             │ STEP 4: APPROVE (Jenkins prod pipeline)     │
             │ • Jenkins runs qual validation              │
             │ • Archives artifacts                        │
             │ • Creates git tag: v{version}               │
             │ • Merges prod → master                      │
             │ • Status: CLOSED                            │
             └──────┬─────────────────────────────────────┘
                    │
             ┌──────▼─────────────────────────────┐
             │ STEP 5: RELEASED                   │
             │ • master branch updated            │
             │ • GitHub issue auto-closed         │
             │ • CAPA marked CLOSED in records    │
             │ • Governance evidence collected    │
             └────────────────────────────────────┘
```

---

## CAPA ID Format

**GitHub Issues = CAPA IDs**

```
Issue URL: https://github.com/rajames440/StarForth/issues/123
CAPA ID: #123 (or CAPA-#123)
Reference in commit: "CAPA #123: Description"
Reference in PR: "Closes #123"
```

**Recommended:**
- Use GitHub issue as primary CAPA ID
- Format commits: `feat: CAPA #123 - Add MYWORD function`
- Format PR title: `CAPA #123: Implement new arithmetic word`

---

## Three Closure Types

### Type 1: AUTO Closure
**When:** Low-risk changes that don't require QA approval

**Examples:**
- ✅ Documentation updates (README, comments)
- ✅ Test additions (no code changes)
- ✅ Build system improvements (no logic changes)
- ✅ Minor refactoring (no behavior change)

**Process:**
1. Developer creates CAPA issue
2. Commits code with "CAPA #123: [description]"
3. Creates PR → GitHub Actions runs tests
4. All tests pass → PR merged
5. **AUTOMATIC:** GitHub auto-closes issue via "Closes #123" in PR
6. **STOPS at QA gate:** Waiting for PM review
7. PM reviews & decides: release or hold

**Approval:** Bypasses QA, **requires PM approval to release**

### Type 2: QA Manual Approval
**When:** Changes that need validation before PM can decide

**Examples:**
- ✅ New feature implementation (validated by tests)
- ✅ Bug fixes (needs test verification)
- ✅ Complex refactoring (needs regression testing)
- ✅ Performance improvements (needs benchmarking)

**Process:**
1. Developer creates CAPA issue with details
2. Creates PR with commits referencing CAPA
3. GitHub Actions validates (devL → test → qual all pass)
4. **STOP at QA gate:** Waiting for QA approval
5. QA reviews:
   - Does implementation match CAPA description?
   - Do all tests pass?
   - Any edge cases missed?
6. QA approves in GitHub: "✅ QA Approved - Ready for PM decision"
7. **STOP at PM gate:** Waiting for PM release decision
8. PM reviews & approves: "✅ PM Approval - Release v2.0.1"
9. PM triggers Jenkins prod job via GitHub Actions
10. Jenkins prod runs, tags release, merges to master

**Approval:** **QA first, then PM** (PM always final authority)

### Type 3: Security/Critical Patch (PM Direct Review)
**When:** Security fixes or critical bugs that need direct PM attention

**Examples:**
- ✅ Security vulnerability patch
- ✅ Critical bug affecting core functionality
- ✅ Data loss prevention fix

**Process:**
1. Developer creates CAPA (marked SECURITY or CRITICAL)
2. Creates PR with detailed security/impact analysis
3. GitHub Actions validates (devL → test → qual all pass)
4. **STOP at PM gate:** PM reviews directly (may bypass QA for speed in emergencies)
5. PM approves: "✅ PM Approval - Critical Release v2.0.1"
6. PM triggers Jenkins prod job
7. Jenkins prod runs, tags, releases

**Approval:** **PM only** (QA optional for speed in true emergencies)

---

## Decision Matrix: Which Closure Type?

| Change Type | Auto | QA Manual | PM Manual | Notes |
|-------------|------|-----------|-----------|-------|
| Docs only | ✅ | | | No code impact |
| Comment/formatting | ✅ | | | No behavior change |
| New test | ✅ | | | Just validation |
| Bug fix | | ✅ | ✅ | QA verifies, PM releases |
| New feature | | ✅ | ✅ | Needs testing & release decision |
| Refactor | | ✅ | ✅ | Needs regression test & release |
| Performance | | ✅ | ✅ | Needs benchmarking & release |
| Security fix | | ✅ | ✅ | Critical path, needs review & release |
| Build system | ✅ | | | Unless breaking change |
| Version bump | | | ✅ | PM only - explicit release |

---

## Creating a CAPA (For Developers)

### Step 1: Create GitHub Issue

Go to: https://github.com/rajames440/StarForth/issues/new

**Title:**
```
[TYPE] Short description of change

Types: [feat], [fix], [refactor], [perf], [test], [docs], [chore]
Example: [feat] Add MYWORD arithmetic operation
```

**Description:**
```markdown
## What is this CAPA?
Brief description of what's being changed.

## Why is this change needed?
- Reasoning
- Use cases
- Motivation

## Type of Change
- [ ] Bug fix (non-breaking)
- [ ] New feature
- [ ] Breaking change
- [ ] Refactoring
- [ ] Performance improvement
- [ ] Documentation update

## Expected Closure Type
- [ ] Auto (docs/tests only)
- [ ] QA Manual (feature/fix, needs testing)
- [ ] PM Manual (release/version)

## Implementation Plan
- Step 1: ...
- Step 2: ...
- Step 3: ...

## Testing Plan
- [ ] Unit tests pass
- [ ] Smoke test passes
- [ ] Full test suite passes
- [ ] Benchmarks (if applicable)

## Related Issues
Closes: (none if new feature)
Relates to: (other issues)
```

**Result:** Gets issue ID (e.g., #42)

### Step 2: Create Feature Branch

```bash
git checkout devl
git pull origin devl
git checkout -b feature/capa-42-myword-operation
```

### Step 3: Implement & Commit

```bash
# Make changes...
git add src/word_source/arithmetic_words.c tests/test_arithmetic.c
git commit -m "feat: CAPA #42 - Implement MYWORD arithmetic operation

Implements the MYWORD word that performs operation X.
This closes the feature request in CAPA #42.

Tests: All 936 tests pass, including new MYWORD tests.
"
```

### Step 4: Create PR

Push branch:
```bash
git push origin feature/capa-42-myword-operation
```

On GitHub, create PR:
- **Title:** `CAPA #42: Implement MYWORD arithmetic operation`
- **Description:**
```markdown
Closes #42

## What does this PR do?
Implements the MYWORD operation as described in CAPA #42.

## Validation
- [x] Runs locally: `make test` - all pass
- [x] Smoke test: `make smoke` - passes
- [x] No compiler warnings: `-Wall -Werror` clean
- [x] Closure type: QA Manual (feature, needs validation)

## Testing Evidence
Test results: `make test` shows all 936 tests pass.
Benchmark: No performance regression detected.
```

### Step 5: Wait for Validation

GitHub Actions automatically:
1. Runs devL (build + smoke)
2. Runs test (full suite)
3. Runs qual (Isabelle verification)

PR will show: ✅ All checks passed (or ❌ if failed)

### Step 6: Request Approval (Depending on Type)

**If Auto:** You're done, PR auto-merges and issue auto-closes

**If QA Manual:**
```
Comment in PR:
@qa-user QA Approval Requested for CAPA #42
All tests pass. Ready for validation.
```

**If PM Manual:**
```
Comment in PR:
@pm-user PM Approval for Release
QA has validated. Ready for PM release decision.
```

---

## QA Approval Process (For QA Role)

### Checklist for QA Approval

```markdown
## QA Validation Checklist for CAPA #42

- [ ] **Requirements Verification**
  - Does implementation match CAPA description?
  - Are all stated requirements met?

- [ ] **Test Coverage**
  - All 936 tests pass? ✅
  - New tests added for new functionality?
  - Edge cases covered?

- [ ] **Regression Testing**
  - No performance degradation?
  - No broken existing features?
  - Smoke test passes?

- [ ] **Code Quality**
  - No compiler warnings (-Wall -Werror clean)?
  - Comments explain non-obvious logic?
  - Follows ANSI C99 standard?

- [ ] **Documentation**
  - Code is properly documented?
  - README updated if needed?
  - CLAUDE.md updated if needed?

## QA Decision

- [ ] ✅ **APPROVED** - Ready for PM release decision
  Comment: "✅ QA Approved CAPA #42 - All validation passed"

- [ ] ❌ **REJECTED** - Needs rework
  Comment: "❌ QA Review Failed - [specific issues]"
  Action: Developer fixes issues, re-submits
```

---

## PM Release Decision Process (For PM Role)

### Checklist for PM Approval

```markdown
## PM Release Decision for CAPA #42

### Pre-Release Checks
- [ ] QA has approved? (CAPA #42 validation passed)
- [ ] No blockers from other CAPAs?
- [ ] All critical fixes in this release?
- [ ] Timing appropriate for release?

### Release Metadata
- [ ] Version number decided: v2.0.1
- [ ] Release notes drafted
- [ ] Changelog updated
- [ ] Announcement prepared (if needed)

### Release Decision
- [ ] ✅ **APPROVED FOR RELEASE**
  Version: v2.0.1
  Comment: "✅ PM Release Approved v2.0.1"
  Action: Trigger Jenkins prod pipeline

- [ ] ❌ **HOLD** - Not ready yet
  Comment: "⏸️ PM Hold - Reason: [waiting for X]"
  Action: Will revisit after [date/event]
```

### Triggering Jenkins Prod Pipeline

Once PM approves:

1. Go to GitHub Actions
2. Find "PM Release Approval Gate" workflow
3. Click "Run workflow"
4. Input: `release_version: 2.0.1`
5. Input: `auto_merge_master: true`
6. Click "Run"

Jenkins will:
- Run prod pipeline
- Tag git: v2.0.1
- Merge prod → master
- Archive artifacts
- Email confirmation

---

## CAPA Closure

### Automatic Closure

**GitHub auto-closes when:**
```
PR contains "Closes #42" in description
↓
PR merges successfully
↓
GitHub Actions completes
↓
Issue #42 auto-closes
```

### Manual Closure (If Needed)

If issue doesn't auto-close:
1. Go to GitHub issue #42
2. Click "Close issue"
3. Comment: "Closed via Jenkins prod CAPA-2024-10-30-v2.0.1"

---

## Audit Trail & Governance

**Every CAPA creates a record of:**

✅ **What** changed - Git commits with CAPA ID
✅ **Why** changed - Issue description
✅ **How** validated - GitHub Actions test results
✅ **Who** approved - PR approvals (QA, PM)
✅ **When** released - Git tag + Jenkins timestamp

**This evidence is collected in:**
- GitHub issue (changeset description)
- Git commit history (what changed)
- Git tags (version released)
- Jenkins logs (validation & release timestamp)
- PR comments (approval decisions)

**Later bundled for governance repo:**
- VALIDATION_EVIDENCE.md
- SIGNOFF.md (approved by whom, when)

---

## Examples

### Example 1: Documentation CAPA (Auto Closure)

```
Issue #1: Update README for new users
  ↓
Developer: creates PR, edits README.md
  ↓
GitHub Actions: skips tests (docs only)
  ↓
PR merged
  ↓
Issue auto-closes
  ↓
No Jenkins prod needed
```

### Example 2: Feature CAPA (QA Manual)

```
Issue #42: Implement MYWORD arithmetic
  ↓
Developer: implements feature, commits with "CAPA #42"
  ↓
GitHub Actions: devL ✅ test ✅ qual ✅
  ↓
QA reviews & approves: "✅ QA Approved"
  ↓
PM sees all green, releases: triggers prod
  ↓
Jenkins prod: tags v2.0.1, merges to master
  ↓
Issue auto-closes
```

### Example 3: Bug Fix CAPA (QA Manual)

```
Issue #99: Stack underflow in DROP
  ↓
Developer: fixes bug, adds test, commits "CAPA #99"
  ↓
GitHub Actions: devL ✅ test ✅ (new test catches bug)
  ↓
QA reviews regression test: "✅ Bug verified fixed"
  ↓
PM releases: v2.0.2
  ↓
Jenkins prod: tags v2.0.2
```

---

## Rules

1. **EVERY PR must reference a CAPA** (use "Closes #123")
2. **EVERY CAPA must have test evidence** (test results from GitHub Actions)
3. **QA approval required** for features/fixes (before release)
4. **PM approval ALWAYS required** for production release
5. **Git tags mark releases** (v{version} = released to master)
6. **GitHub issues are immutable records** (for audit trail)

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| PR doesn't reference CAPA | Add "Closes #123" to PR description, update issue |
| Tests fail, can't approve | Reject in QA, ask developer to fix |
| QA approved but PM hesitant | PM can hold release, discuss with QA |
| Issue doesn't auto-close | Manually close with Jenkins timestamp |
| Need to revert release | Create new CAPA #X "Revert v2.0.1" |

---

## Related Documents

- **PR_WORKFLOW.md** - How developers create PRs (must reference CAPA)
- **QA_PROCEDURE.md** - (to be created) QA approval process details
- **PM_PROCEDURE.md** - (to be created) PM release process details
- **VALIDATION_PLAN.md** - (to be created) Test cases to validate this process
- **GOVERNANCE_HANDOFF.md** - (to be created) Bundle contents for governance repo

---

**Last Updated:** October 30, 2025
**Maintained by:** Robert A. James
**Status:** Ready for review