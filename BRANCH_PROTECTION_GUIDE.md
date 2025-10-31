# Branch Protection Rules Configuration Guide

**Phase 5: Manual GitHub Repository Configuration**

This document provides step-by-step instructions for configuring branch protection rules in GitHub to enforce the StarForth CI/CD pipeline.

---

## Overview

Branch protection rules enforce:
- ✅ Jenkins tests must pass before merge
- ✅ Only authorized users can push to protected branches
- ✅ PR reviews (optional, per your workflow)
- ✅ master branch is ALWAYS canonical source
- ✅ Prevents accidental force-pushes to production branches

---

## Access Instructions

1. Go to your repository → Settings
2. In the left sidebar, click "Branches"
3. Under "Branch protection rules", click "Add rule"
4. Follow the configuration for each branch below

---

## Branch 1: `devl` (Development Integration)

**Purpose:** Quick feedback loop, fast testing

### Configuration:

1. **Branch name pattern:** `devl`

2. **Protect matching branches:**
   - ☑️ Require a pull request before merging
   - ☑️ Require status checks to pass before merging
   - ☑️ Require branches to be up to date before merging

3. **Status checks:**
   - Search for and select: `starforth-devl` (Jenkins job)
   - This ensures devL pipeline passes

4. **Require reviews:**
   - ☐ **UNCHECKED** (per your workflow - you self-review)

5. **Require conversation resolution:**
   - ☐ Require all conversations on code to be resolved before merging

6. **User restrictions:**
   - Leave BLANK (all developers can push to devl)

7. **Advanced options:**
   - ☐ Dismiss stale pull request approvals
   - ☐ Require status checks from the latest push
   - ☐ Require deployment to succeed before merging
   - ☐ Lock branch

8. **Allow force pushes:**
   - ☐ Do not allow force pushes

9. **Allow deletions:**
   - ☐ Do not allow deletions

**Click "Create" to save**

---

## Branch 2: `test` (Test Validation)

**Purpose:** Comprehensive test suite, full validation

### Configuration:

1. **Branch name pattern:** `test`

2. **Protect matching branches:**
   - ☑️ Require a pull request before merging
   - ☑️ Require status checks to pass before merging
   - ☑️ Require branches to be up to date before merging

3. **Status checks:**
   - Search for and select: `starforth-test` (Jenkins job)

4. **Require reviews:**
   - ☐ UNCHECKED

5. **Advanced options:**
   - ☐ Lock branch

6. **Allow force pushes:**
   - ☐ Do not allow force pushes

7. **Allow deletions:**
   - ☐ Do not allow deletions

**Click "Create" to save**

---

## Branch 3: `qual` (Quality Assurance Gate)

**Purpose:** Formal verification, approval checkpoint

### Configuration:

1. **Branch name pattern:** `qual`

2. **Protect matching branches:**
   - ☑️ Require a pull request before merging
   - ☑️ Require status checks to pass before merging
   - ☑️ Require branches to be up to date before merging

3. **Status checks:**
   - Search for and select: `starforth-qual` (Jenkins job)

4. **Require reviews:**
   - ☐ UNCHECKED

5. **Advanced options:**
   - ☐ Lock branch

6. **Allow force pushes:**
   - ☐ Do not allow force pushes

7. **Allow deletions:**
   - ☐ Do not allow deletions

**Click "Create" to save**

---

## Branch 4: `prod` (Production Release)

**Purpose:** Release artifacts, maintainer-only, highly protected

### Configuration:

1. **Branch name pattern:** `prod`

2. **Protect matching branches:**
   - ☑️ Require a pull request before merging
   - ☑️ Require status checks to pass before merging
   - ☑️ Require branches to be up to date before merging

3. **Status checks:**
   - Search for and select: `starforth-prod` (Jenkins job)

4. **Require reviews:**
   - ☐ UNCHECKED (PM approval via GitHub Actions dispatch)

5. **Restrict who can push:**
   - Click "Specify who can push to matching branches"
   - Search for and add your GitHub username (you as maintainer)
   - **Only you can push to prod**

6. **Advanced options:**
   - ☑️ Lock branch (prevents accidental commits)
     - ✅ **IMPORTANT:** This means only the PM Release Approval workflow (GitHub Actions) can update prod via Jenkins

7. **Allow force pushes:**
   - ☐ Do not allow force pushes

8. **Allow deletions:**
   - ☐ Do not allow deletions

**Click "Create" to save**

---

## Branch 5: `master` (Canonical Source)

**Purpose:** Always deployable source of truth, auto-updated only via prod merge

### Configuration:

1. **Branch name pattern:** `master`

2. **Protect matching branches:**
   - ☑️ Require a pull request before merging
   - ☑️ Require status checks to pass before merging
   - ☑️ Require branches to be up to date before merging

3. **Status checks:**
   - No Jenkins job (master is auto-updated by prod pipeline)
   - Leave BLANK (prod pipeline handles the merge)

4. **Require reviews:**
   - ☐ UNCHECKED

5. **Restrict who can push:**
   - Click "Specify who can push to matching branches"
   - Add ONLY: Your GitHub username + any Jenkins bot user
   - **Only automation and you can touch master**

6. **Advanced options:**
   - ☑️ Lock branch (prevents accidental manual commits)

7. **Allow force pushes:**
   - ☐ Do not allow force pushes

8. **Allow deletions:**
   - ☐ Do not allow deletions

**Click "Create" to save**

---

## Verification Checklist

After creating all 5 branch protection rules:

- [ ] `devl` rule created (tests required, no reviews)
- [ ] `test` rule created (tests required, no reviews)
- [ ] `qual` rule created (tests required, no reviews)
- [ ] `prod` rule created (tests required, maintainer-only push, locked)
- [ ] `master` rule created (locked, maintainer+automation only)

Test the rules:

1. **Test devl rule:**
   ```bash
   git checkout -b test-branch
   echo "test" > test.txt
   git add test.txt
   git commit -m "test"
   git push origin test-branch
   # Create PR → should require test to pass before merge
   ```

2. **Test master lock:**
   ```bash
   git fetch origin master
   git checkout master
   echo "test" > test.txt
   git add test.txt
   git commit -m "test"
   git push origin master
   # Should FAIL - branch is locked
   ```

3. **Verify prod lock:**
   - Attempt to push directly to prod
   - Should fail: "You do not have permission to push to this protected branch"

---

## Summary of Protection Strategy

| Branch | Purpose | Who Can Merge | Tests Required | Locked | Notes |
|--------|---------|---------------|-----------------|--------|-------|
| devl | Quick feedback | Anyone (PR required) | Jenkins devL ✅ | No | Fast iteration |
| test | Validation | Anyone (PR required) | Jenkins test ✅ | No | Full test suite |
| qual | QA Gate | Anyone (PR required) | Jenkins qual ✅ | No | Formal verification |
| prod | Release | PM only (Actions) | Jenkins prod ✅ | **YES** | Automation-managed |
| master | Canonical | Jenkins bot only | None | **YES** | Auto-updated by prod |

---

## Important Notes

### 1. GitHub Actions Can Still Push When Branch is Locked
- Locked branches prevent **manual** git push
- GitHub Actions workflows with `GITHUB_TOKEN` can still push
- This is why prod and master can be locked but still auto-merge

### 2. Status Checks Must Match Jenkins Job Names
- Exact match required: `starforth-devl`, `starforth-test`, `starforth-qual`, `starforth-prod`
- If Jenkins job names differ, update these accordingly

### 3. Require Status Checks From Latest Push
- Optional setting (currently unchecked)
- Enable if you want to require re-running tests on new pushes
- Leave unchecked for faster merges

### 4. Protecting Multiple Patterns
- If you have branch names like `devl-*` or `release/*`, use glob patterns:
  - `devl*` matches `devl`, `devl-feature`, `devl-hotfix`

### 5. Rulesets vs Branch Protection (GitHub's Newer Feature)
- This guide uses traditional "Branch protection rules" (simpler)
- GitHub also offers "Rulesets" (beta, more powerful)
- Stick with traditional rules for now

---

## Troubleshooting

### Issue: "Push rejected" on devl branch
**Check:**
1. Did your PR get merged? (Branch protection requires PR merge)
2. Did Jenkins test pass? (Status check is required)
3. Is your branch up-to-date? (Rebase if needed)

**Fix:**
```bash
git pull origin devl
git rebase origin/devl
git push origin your-branch
```

### Issue: Can't push to prod as maintainer
**Check:**
1. Is your GitHub username in "Restrict who can push"?
2. Are you on the main GitHub account (not organization account)?
3. Is the branch locked? (It should be - check if lock setting is correct)

**Fix:**
- If locked is checked, only GitHub Actions can push (intentional)
- Use "PM Release Approval Gate" workflow to trigger production release

### Issue: master branch won't update from prod
**Check:**
1. Did Jenkins prod job succeed?
2. Does the prod pipeline have auto-merge-to-master enabled?
3. Are there conflicts between prod and master?

**Fix:**
- Check Jenkins prod build logs
- Manually merge if needed: `git pull && git merge prod && git push`

---

## After Setup: Running the Full Pipeline

Once branch protection is configured:

1. **Developer creates PR against devl:**
   ```bash
   git checkout -b feature/my-feature
   # ... make changes ...
   git push origin feature/my-feature
   # GitHub prevents merge without PR
   ```

2. **Create PR on GitHub:**
   - Base: `devl`, Compare: `feature/my-feature`
   - GitHub requires Jenkins devL test to pass

3. **Merge PR when ready:**
   - Click "Merge pull request"
   - Phase 1 workflow posts timeline comment
   - Jenkins auto-triggers via webhook

4. **Pipeline auto-merges:**
   - devL passes → auto-merges to test
   - test passes → auto-merges to qual
   - qual passes → PM notified

5. **PM approves release:**
   - Actions → "PM Release Approval Gate"
   - Provide release version
   - Jenkins prod runs
   - master auto-updated to match prod (if enabled)

6. **Result:**
   - ✅ master branch is ALWAYS canonical source
   - ✅ master is ALWAYS deployable
   - ✅ Audit trail in git history

---

## Next Steps

1. ✅ **Configure all 5 branch protection rules** (this document)
2. ⏳ **Test with a real PR** to verify rules work
3. ⏳ **Create PR_WORKFLOW.md** for developer guidance

---

**Document Maintained by:** Claude Code
**Last Updated:** October 30, 2025
**Status:** Ready for manual configuration