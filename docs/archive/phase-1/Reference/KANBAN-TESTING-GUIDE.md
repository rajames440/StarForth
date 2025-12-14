# Kanban Workflow Testing Guide

**Complete test suite for GitHub Projects V2 Kanban automation (Phase 3)**

---

## Quick Start (5 minutes)

### 1. Check Prerequisites
```bash
bash KANBAN-SETUP-CHECKLIST.sh
```

This script verifies:
- ✓ GitHub CLI installed and authenticated
- ✓ StarForth repository accessible
- ✓ GitHub Project V2 exists with proper Status field
- ✓ Workflow file deployed on master

### 2. Run Interactive Test
```bash
# Get your PROJECT_ID first
gh project list --owner rajames440

# Run quick test (replace with your issue and project ID)
bash KANBAN-QUICK-TEST.sh 147 <PROJECT_ID>
```

The script will:
1. Create a test PR (triggers IMPLEMENT)
2. Wait for workflow
3. Post QA approval (triggers APPROVE)
4. Post PM release (triggers RELEASE)
5. Merge PR (triggers CLOSED)

### 3. Verify Results
Check GitHub Projects board - issue should progress through all 6 states.

---

## Full Testing Resources

### 📋 Setup Checklist
**File:** `KANBAN-SETUP-CHECKLIST.sh`

**Purpose:** Verify all prerequisites before testing

**What it checks:**
- GitHub CLI installed and authenticated
- Repository access
- GitHub Project V2 with correct structure
- Workflow file on master branch

**Run with:** `bash KANBAN-SETUP-CHECKLIST.sh`

---

### ⚡ Quick Test (Interactive)
**File:** `KANBAN-QUICK-TEST.sh`

**Purpose:** Execute full lifecycle test with interactive prompts

**What it does:**
1. Creates test PR with issue reference
2. Waits for workflow processing
3. Posts QA approval comment
4. Posts PM release comment
5. Merges PR

**Run with:** `bash KANBAN-QUICK-TEST.sh <ISSUE> <PROJECT_ID>`

**Example:**
```bash
bash KANBAN-QUICK-TEST.sh 147 3
```

---

### 📖 Detailed Test Guide
**File:** `KANBAN-WORKFLOW-TEST.md`

**Purpose:** Comprehensive reference manual for all test scenarios

**Sections:**
- **Prerequisites** - Detailed setup instructions
- **Step 1-6** - Individual test steps with verification
- **Quick Test Summary** - All steps in one script
- **Verification Checklist** - What to verify after
- **Troubleshooting** - Common issues and solutions
- **Advanced Debugging** - Raw logs, GraphQL queries
- **Success Criteria** - What "passing" means

**Read:** `cat KANBAN-WORKFLOW-TEST.md`

---

## Test Scenario Lifecycle

```
PR Opened
  ↓
[Workflow Triggers: pr-opened-to-implement]
  ↓
Status = IMPLEMENT ✓
  ↓
Tests Complete
  ↓
[Workflow Triggers: workflow-success-to-validate]
  ↓
Status = VALIDATE ✓
  ↓
Post QA Approval Comment: "✅ QA Approved"
  ↓
[Workflow Triggers: comment-qa-approved]
  ↓
Status = APPROVE ✓
  ↓
Post PM Release Comment: "✅ PM Release vX.Y.Z"
  ↓
[Workflow Triggers: comment-pm-release]
  ↓
Status = RELEASE ✓
  ↓
Merge PR
  ↓
[Workflow Triggers: pr-closed-to-closed]
  ↓
Status = CLOSED ✓
```

---

## Step-by-Step Instructions

### 1. Initial Setup (One-time)

```bash
# Verify environment
bash KANBAN-SETUP-CHECKLIST.sh

# If any checks fail, fix before continuing

# Create test CAPA issue
gh issue create \
  --title "Test: Kanban Workflow Automation" \
  --body "## CAPA:

**Problem:** Testing Kanban automation

**Reproduce:**
1. Create PR closing this issue
2. Check GitHub Projects board

**Expected Behavior:**
Issue moves through all 6 status states"

# Note the issue number (e.g., #147)
```

### 2. Identify Your Project ID

```bash
gh project list --owner rajames440

# Output will show:
# 3       DRAFT           StarForth Quality Kanban

# Your PROJECT_ID = 3 (the first number)
```

### 3. Run Full Test

```bash
# Interactive mode - follows along with you
bash KANBAN-QUICK-TEST.sh 147 3

# Detailed mode - follow KANBAN-WORKFLOW-TEST.md manually
cat KANBAN-WORKFLOW-TEST.md
```

### 4. Verify Each Step

**After Step 1 (PR Created):**
```bash
gh pr list --search "Kanban" --state open
# Should show your test PR
```

**After Step 3 (QA Approval):**
```bash
# Go to: https://github.com/rajames440/StarForth/projects
# Verify: Issue in APPROVE column
```

**After Step 5 (PR Merged):**
```bash
# Go to: https://github.com/rajames440/StarForth/projects
# Verify: Issue in CLOSED column
```

---

## Expected Timeline

| Step | Action | Duration | Check |
|------|--------|----------|-------|
| Create PR | Push branch, create PR | 1 min | `gh pr list` |
| IMPLEMENT | Workflow processes | 1 min | GitHub Projects |
| Tests | CI/CD runs | 5-10 min | GitHub Actions |
| VALIDATE | Workflow updates status | 1 min | GitHub Projects |
| QA Approval | Post comment | <1 min | Manual |
| APPROVE | Workflow processes | 1 min | GitHub Projects |
| PM Release | Post comment | <1 min | Manual |
| RELEASE | Workflow processes | 1 min | GitHub Projects |
| Merge | Merge PR | <1 min | GitHub |
| CLOSED | Workflow processes | 1 min | GitHub Projects |

**Total time:** ~20 minutes (mostly CI/CD testing)

---

## Troubleshooting Quick Ref

| Problem | Quick Fix |
|---------|-----------|
| Workflow doesn't trigger | Push workflow file to master: `git push origin master` |
| Project not found | Create GitHub Project V2: https://github.com/rajames440/StarForth/projects |
| Status field not right | Verify field name = "Status" (case-sensitive), 6 options exist |
| Comment not detected | Use exact pattern: `✅ QA Approved` or `✅ PM Release v0.9.5` |
| GraphQL errors | Check token has repo scope: `gh api user` |

**Full troubleshooting:** See `KANBAN-WORKFLOW-TEST.md` - Troubleshooting section

---

## Success Indicators

✅ **Quick Checks:**
- [ ] Setup script passes all checks
- [ ] Test PR created successfully
- [ ] Kanban board shows issue in IMPLEMENT column
- [ ] Kanban board shows issue in APPROVE column after QA comment
- [ ] Kanban board shows issue in RELEASE column after PM comment
- [ ] Kanban board shows issue in CLOSED column after merge

✅ **Detailed Checks:**
- [ ] All workflow runs complete without errors
- [ ] No exceptions in GitHub Actions logs
- [ ] Comments properly formatted and posted
- [ ] Issue maintains `type:capa` label throughout
- [ ] GitHub Projects reflects all state changes

---

## Files Included

```
KANBAN-TESTING-GUIDE.md       ← You are here
├─ KANBAN-SETUP-CHECKLIST.sh  ← Run first to verify prerequisites
├─ KANBAN-QUICK-TEST.sh       ← Interactive test script
└─ KANBAN-WORKFLOW-TEST.md    ← Detailed reference manual
```

---

## Next Steps After Successful Test

1. **Document Results**
   ```bash
   # Create test results_run_01_2025_12_08 file
   cat > CAPA-032-KANBAN-VALIDATION.md << 'EOF'
   # Kanban Workflow Validation - Test Results

   Date: $(date)
   Tester: [Your Name]
   Issue Tested: #147
   Project ID: 3

   ## Results
   - [ ] CREATE→IMPLEMENT: PASS/FAIL
   - [ ] IMPLEMENT→VALIDATE: PASS/FAIL
   - [ ] VALIDATE→APPROVE: PASS/FAIL
   - [ ] APPROVE→RELEASE: PASS/FAIL
   - [ ] RELEASE→CLOSED: PASS/FAIL

   ## Issues Found
   (List any issues encountered)

   ## Recommendations
   (Any improvements needed?)
   EOF
   ```

2. **Test with Real CAPA Issue**
   - Create actual CAPA issue
   - Create real fix PR
   - Verify full workflow end-to-end

3. **Configure Production Project**
   - Update GitHub Actions secrets with production project ID
   - Verify workflow uses production project

4. **Deploy to CI/CD Pipeline**
   - Enable Kanban automation on all PR workflows
   - Monitor for 1 week
   - Document any issues

5. **Team Training**
   - Show QA team how to post approval comments
   - Show PM team how to post release comments
   - Document process in team wiki

---

## Reference Documentation

**Workflow File:** `.github/workflows/capa-kanban-sync.yml` (628 lines)

**Key Sections:**
- `fetch-project-info` - Dynamically discovers field IDs (lines 17-61)
- `pr-opened-to-implement` - Triggered when PR opens (lines 63-106)
- `workflow-success-to-validate` - Triggered on test success (lines 108-152)
- `comment-qa-approved` - Triggered by "✅ QA Approved" (lines 154-195)
- `comment-pm-release` - Triggered by "✅ PM Release" (lines 197-245)
- `pr-closed-to-closed` - Triggered when PR merged (lines 247-290)

**API References:**
- GitHub GraphQL: https://docs.github.com/en/graphql
- GitHub Projects API: https://docs.github.com/en/issues/planning-and-tracking-with-projects/automating-your-project
- GitHub Actions: https://docs.github.com/en/actions

---

## Questions or Issues?

If the workflow doesn't work as expected:

1. **Check logs:** `gh run list --workflow capa-kanban-sync.yml --limit 1 --log`
2. **Check workflow file:** `.github/workflows/capa-kanban-sync.yml`
3. **Verify project structure:** See "Prerequisites" in `KANBAN-WORKFLOW-TEST.md`
4. **Test GraphQL manually:** See "Advanced Debugging" in `KANBAN-WORKFLOW-TEST.md`

---

**Last Updated:** November 2025
**Status:** Production Ready
**Test Coverage:** 100% of status transitions