# StarForth CI/CD Workflow

## Overview

This document describes the **trunk-based development workflow** for StarForth's formal verification pipeline.

```
Developer commits → devl → test → qual → prod → master (Release)
                  (5m)   (20m)  (25m)  (2m)
```

## Branch Strategy

| Branch | Purpose | Duration | Failure Behavior |
|--------|---------|----------|------------------|
| **master** | Production releases (immutable) | N/A | Protected - no direct commits |
| **prod** | Release staging | 2 min | Manual PM approval required |
| **qual** | Formal verification gate | 25 min | Auto-rejects to devl with CAPA |
| **test** | Full test suite | 20 min | Auto-rejects to devl with CAPA |
| **devl** | Development (where code lives) | 5 min | Quick feedback to developer |

## Workflow Flow

### 1. Developer Creates Feature/Fix on devl

```bash
git checkout devl
git checkout -b feature/my-fix
# ... make changes ...
git commit -m "Fix stack push error (DEFECT-001)"
git push origin feature/my-fix
# Open PR to devl branch
```

**Jenkins triggers:** `starforth-devl` pipeline
- **Time:** 5 minutes
- **Stages:** Build + Smoke Test only
- **Outcome:**
  - ✅ PASS → PR approved, merge to devl
  - ❌ FAIL → Auto-generate CAPA, request review

### 2. Code Merges to devl, Auto-flows to test

Once PR merges to devl:
```bash
# Automatic (Jenkins webhook)
git merge feature/my-fix → devl
git fetch origin
git reset --hard origin/devl  # test auto-syncs
```

**Jenkins triggers:** `starforth-test` pipeline
- **Time:** 20 minutes
- **Stages:** Full test suite, benchmarks, stress tests
- **Outcome:**
  - ✅ PASS → Auto-merge to qual
  - ❌ FAIL → Auto-generate CAPA, revert test branch

### 3. Code in test Auto-flows to qual

```bash
# Automatic
git merge test → qual
```

**Jenkins triggers:** `starforth-qual` pipeline
- **Time:** 25 minutes
- **Stages:** Isabelle/HOL formal verification + per-defect CAPA generation
- **Outcome:**
  - ✅ PASS → Notify PM "Ready for release"
  - ❌ FAIL → Auto-generate CAPA per defect, revert qual branch

### 4. PM Reviews qual Results, Approves for Release

PM receives email/Slack notification:
> "StarForth QUAL PASSED - Ready for release v2.0.0"

PM reviews CAPA documents in `docs/internal/capa/`:
- CAPA-DEFECT-001-timestamp.adoc
- CAPA-DEFECT-002-timestamp.adoc
- etc.

PM clicks "Approve for Release" in Jenkins UI (input step).

**Jenkins triggers:** `starforth-prod` pipeline
- **Time:** 2 minutes
- **Stages:** Version artifacts, tag git, release
- **Outcome:**
  - Creates: `starforth-amd64-fastest-2.0.0`
  - Tags: `v2.0.0` in git
  - Artifacts: Immutable release in GitHub Releases

### 5. Master Auto-updates (or Manual)

```bash
# Option A: Manual merge by PM
git checkout master
git merge prod
git push origin master

# Option B: Automatic via GitHub Actions/Jenkins
# (configured to auto-merge prod → master after release)
```

**MASTER STAYS CLEAN & RELEASABLE**
- Only receives merges from prod
- Protected branch - no direct commits
- Always reflects latest released version

---

## Nightly Baseline (Separate)

**Independent of PR workflow** - runs on schedule:
```
2 AM UTC daily: Jenkins nightly baseline
├─ Clones master branch
├─ Runs comprehensive: full tests, benchmarks, stress tests (1 hour)
└─ Email results to PM (rajames440@gmail.com)
```

Purpose: Trending, performance regression detection, non-blocking feedback.

---

## CAPA Process

### What is a CAPA?

**CAPA = Corrective Action & Preventive Action**

Each **CAPA represents ONE DEFECT** (code bug OR theory defect OR both).

### CAPA Lifecycle

```
Defect found in TEST or QUAL stage
           ↓
Auto-generate CAPA-{DEFECT-ID}-{timestamp}.adoc
           ↓
CAPA filed as GitHub Issue in Kanban
           ↓
Developer fixes defect, creates new PR
           ↓
New PR flows through devl → test → qual → prod → master
           ↓
CAPA closed when fix is proven in QUAL stage
```

### CAPA Format

```adoc
= CAPA-DEFECT-001-20250130-143000
:toc:

== Defect Metadata
- **Defect ID:** DEFECT-001
- **Found in Stage:** TEST
- **Severity:** HIGH
- **Category:** Code defect (Stack implementation)

== Description
Stack push operation fails when memory is near limit.
Causes segmentation fault on systems with <1GB free RAM.

== Impact
Any computation involving large data structures on memory-constrained systems fails.

== Root Cause
Buffer overflow in `stack_push()` at src/stack.c:142
Missing bounds check before memory allocation.

== Evidence
Build: https://jenkins.example.com/job/starforth-test/build-123
Log: [test-123-stack-overflow.log]

== Corrective Action Required
Fix bounds checking in stack_push() function.
Add integration test for memory-constrained systems.

== References
- Code: src/stack.c:142
- Theory: docs/src/internal/formal/Stack.thy (line 45)
- Test: tests/test_stack_limits.c

== Status
OPEN - Awaiting fix from devl branch
```

---

## GitHub Issue (Kanban) Integration

Each CAPA auto-creates a GitHub Issue:

```
Title: [CAPA-DEFECT-001] Stack push buffer overflow
Labels: defect, high-priority, stack
Assignee: (auto-assign to dev who triggered it)
Body: (CAPA document content)
```

PM reviews in GitHub Kanban board:
```
[To Do] → [In Progress] → [In Review] → [Done]
 ↑
CAPAs appear here automatically
```

---

## Jenkins Jobs Configuration

### Job: starforth-devl
**Trigger:** PR opened/updated on devl branch
**Jenkinsfile:** `jenkinsfiles/devl/Jenkinsfile`
**Timeout:** 10 minutes
**On Success:** Merge PR
**On Failure:** Request code review, auto-generate CAPA

### Job: starforth-test
**Trigger:** Push to devl (auto-sync to test)
**Jenkinsfile:** `jenkinsfiles/test/Jenkinsfile`
**Timeout:** 30 minutes
**On Success:** Auto-merge test → qual
**On Failure:** Revert test branch, auto-generate CAPA

### Job: starforth-qual
**Trigger:** Push to test (auto-sync to qual)
**Jenkinsfile:** `jenkinsfiles/qual/Jenkinsfile`
**Timeout:** 45 minutes
**On Success:** Email PM "Ready for release"
**On Failure:** Revert qual branch, auto-generate CAPA

### Job: starforth-prod
**Trigger:** Manual Jenkins input step (PM approval)
**Jenkinsfile:** `jenkinsfiles/prod/Jenkinsfile`
**Timeout:** 5 minutes
**On Success:** Tag git (v2.0.0), create GitHub Release
**On Failure:** Alert PM, don't touch master

### Job: starforth-nightly
**Trigger:** Cron `0 2 * * *` (2 AM UTC daily)
**Jenkinsfile:** `Jenkinsfile` (baseline)
**Timeout:** 2 hours
**On Complete:** Email results (success/failure)

---

## Quick Reference: Developer Workflow

```bash
# 1. Create feature branch
git checkout devl
git pull origin devl
git checkout -b feature/fix-defect-001

# 2. Make changes
vim src/stack.c
# ... edit code ...

# 3. Test locally
make test
make smoke

# 4. Commit with reference to defect
git add src/stack.c
git commit -m "Fix stack push bounds check (DEFECT-001)"

# 5. Push and create PR
git push origin feature/fix-defect-001
# Open PR on GitHub (devl ← feature/fix-defect-001)

# 6. Jenkins runs starforth-devl (5 min)
# [Watch in Jenkins UI or GitHub PR status]

# 7. If PASS → PR approved, merge
# If FAIL → Fix issues, push again

# 8. Once merged to devl:
#    - Auto-syncs to test (starforth-test runs, 20 min)
#    - If test PASS → Auto-syncs to qual (starforth-qual runs, 25 min)
#    - If qual PASS → PM notified, manually approves for release
```

---

## Troubleshooting

### "My PR failed in devl stage - what do I do?"

1. Check Jenkins log: Build → Console Output
2. Fix the issue locally
3. `git push origin feature/my-fix` again
4. Jenkins re-runs automatically
5. Repeat until it passes

### "Test branch failed - how do I fix?"

1. Check what failed: `starforth-test` Jenkins job
2. Issue is likely a regression in devl code
3. Create new PR on devl to fix it
4. Once fixed, test will auto-sync and pass

### "Qual found defects - how do I respond?"

1. PM receives email with list of CAPAs
2. Each CAPA is a separate GitHub Issue
3. Assign them to yourself or team
4. Create PR on devl to fix each one
5. Each fix goes through the full workflow again
6. Once all fixed, qual will pass and ready for release

### "I need to rollback a release"

```bash
git checkout master
git revert v2.0.0  # Creates a new commit that undoes v2.0.0
git tag v2.0.1
git push origin master v2.0.1
# New release becomes v2.0.1 (fix of v2.0.0)
```

---

## Key Principles

1. **Devl is the source of truth** - all code goes here first
2. **Automatic flow** - code auto-merges devl → test → qual (unless it fails)
3. **Manual release gate** - only PM approves qual → prod
4. **One defect = one CAPA** - atomic tracking and fixing
5. **Master stays clean** - always releasable, only updated from prod
6. **Immutable artifacts** - released binaries tagged and archived forever

---

## Timeline Example: Releasing v2.0.0

```
Monday 9 AM:   Dev commits to devl (feature/fix-defect-001)
Monday 9:05:   devl pipeline runs (PASS) → merged
Monday 9:10:   test pipeline auto-starts
Monday 9:30:   test passes → auto-merges to qual
Monday 9:35:   qual pipeline auto-starts
Monday 10:00:  qual passes → PM gets email "Ready for release"
Monday 10:15:  PM reviews CAPAs, clicks "Approve for Release"
Monday 10:17:  prod pipeline runs
Monday 10:19:  ✅ v2.0.0 tagged in git, released
Monday 10:20:  master branch auto-updated
Monday 10:30:  Release notes emailed to stakeholders
```

**Total time from commit to release:** ~1.5 hours (mostly waiting for pipelines)

---

## Questions?

See: `JENKINS_SETUP.md` for how to configure Jenkins
See: `CAPA_PROCESS.md` for detailed CAPA handling
See: Individual `Jenkinsfile` comments for stage details