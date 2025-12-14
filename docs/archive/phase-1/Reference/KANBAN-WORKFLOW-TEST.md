# Kanban Workflow Test Guide (Phase 3)

**Objective:** Test full GitHub Projects V2 Kanban automation with all status transitions
**Workflow:** `.github/workflows/capa-kanban-sync.yml`
**Lifecycle:** CREATE → IMPLEMENT → VALIDATE → APPROVE → RELEASE → CLOSED

---

## Prerequisites

### 1. GitHub Projects V2 Setup

Before testing, you MUST have a GitHub Project V2 created with:

- **Project Name:** "StarForth Quality Kanban" (or similar)
- **Status Field:** Custom field named `Status` with these exact options:
  - `CREATE` (default/backlog)
  - `IMPLEMENT` (in progress)
  - `VALIDATE` (testing)
  - `APPROVE` (approval gate)
  - `RELEASE` (ready to ship)
  - `CLOSED` (done)

**How to create this in GitHub:**
1. Go to https://github.com/rajames440/StarForth/projects
2. Click "New project"
3. Select "Table" layout
4. Name it "StarForth Quality Kanban"
5. Add custom field "Status" (single select)
6. Add the 6 options listed above
7. Note the Project ID from the URL: `https://github.com/users/rajames440/projects/N` (N is your project ID)

### 2. Repository Secrets

Configure these in GitHub Settings → Secrets and variables → Actions:

```
GITHUB_TOKEN          - (automatic, no setup needed)
PROJECT_ID            - Your GitHub Project V2 ID (from step above)
```

**Get your Project ID:**
```bash
# After creating the project, run:
gh project list --owner rajames440
# Look for "StarForth Quality Kanban" and copy the ID
```

### 3. Test Issue Preparation

Create a base test CAPA issue to use throughout testing:

```bash
# Create a test CAPA issue (if it doesn't exist)
gh issue create \
  --title "Test: Kanban Workflow Automation" \
  --body "## CAPA:

**Problem:** Testing Kanban automation workflow

**Reproduce:**
1. Create PR closing this issue
2. Run GitHub Actions workflows
3. Observe status transitions

**Expected Behavior:** Status should move through all 6 states"
```

Note the issue number returned (e.g., #147). **Use this number throughout the test.**

---

## Test Execution

### Step 1: Create Test CAPA Issue

```bash
# If not already created, create the test issue
ISSUE_NUM=147  # Replace with actual issue number if different

# Verify issue exists
gh issue view $ISSUE_NUM --repo rajames440/StarForth
```

**Expected Output:**
- Issue title: "Test: Kanban Workflow Automation"
- Issue body contains: "## CAPA:"
- Issue has label: `type:capa` (should be auto-added)
- GitHub Projects should show this issue in CREATE column

---

### Step 2: Test CREATE → IMPLEMENT Transition

**Trigger:** When PR is opened that references the issue

```bash
# 1. Create a test feature branch
git checkout -b test/kanban-workflow-$RANDOM
echo "# Test Kanban" > TEST_KANBAN.md
git add TEST_KANBAN.md
git commit -m "test: Kanban workflow automation

Closes #$ISSUE_NUM"

# 2. Push and create PR
git push origin test/kanban-workflow-$RANDOM

# 3. Create PR (note: adjust branch name to match what you pushed)
BRANCH=$(git rev-parse --abbrev-ref HEAD)
gh pr create \
  --base master \
  --head "$BRANCH" \
  --title "Test: Kanban CREATE→IMPLEMENT" \
  --body "This PR tests Kanban automation.

Closes #$ISSUE_NUM"
```

**Capture PR number:**
```bash
PR_NUM=$(gh pr list --state open --search "Kanban CREATE" --json number -q ".[0].number")
echo "PR Number: $PR_NUM"
```

**Verification - Step 2A: PR Opened Event Trigger**

The workflow `capa-kanban-sync.yml` should trigger immediately when PR opens:

```bash
# Check workflow execution
gh run list \
  --workflow capa-kanban-sync.yml \
  --limit 1 \
  --status completed

# View workflow logs
gh run view <run-id> --log
```

**Expected in logs:**
```
pr-opened-to-implement: Extract issue number from PR
Issue extracted: $ISSUE_NUM
Fetching project info...
Setting project item status to IMPLEMENT
Successfully updated status: IMPLEMENT
```

**Verification - Step 2B: Check Kanban Status**

```bash
# Query GraphQL to verify status
gh api graphql -f query='
  query {
    organization(login: "rajames440") {
      projectV2(number: <PROJECT_ID>) {
        items(first: 10) {
          nodes {
            id
            content {
              ... on Issue {
                number
                title
              }
            }
            fieldValues(first: 20) {
              nodes {
                ... on ProjectV2ItemFieldSingleSelectValue {
                  field {
                    name
                  }
                  name
                }
              }
            }
          }
        }
      }
    }
  }
'
```

**Expected output:** Issue #$ISSUE_NUM should have Status = `IMPLEMENT`

---

### Step 3: Test IMPLEMENT → VALIDATE Transition

**Trigger:** When all checks pass on the PR

```bash
# Checks should pass automatically if tests pass
# The workflow monitors "workflow_run" event for test completion

# Wait for test workflow to complete
sleep 30

# Check if test workflows completed
gh run list --limit 5 --status completed
```

**Verification - Step 3A: Workflow Completion Trigger**

The workflow listens for test job completion:

```bash
# View latest run of capa-kanban-sync
gh run list \
  --workflow capa-kanban-sync.yml \
  --limit 1 \
  --json status

# Expected: "completed"
```

**Verification - Step 3B: Check Status in Kanban**

```bash
# Re-run the GraphQL query from Step 2B
# Expected output: Issue #$ISSUE_NUM should have Status = `VALIDATE`
```

**Expected in workflow logs:**
```
workflow-success-to-validate: Triggered by test workflow completion
Fetching project info...
Setting status to VALIDATE
```

---

### Step 4: Test VALIDATE → APPROVE Transition

**Trigger:** When comment pattern `✅ QA Approved` is posted on PR

```bash
# Post QA approval comment
gh pr comment $PR_NUM \
  --body "✅ QA Approved

FMEA Decision: NO_FMEA
Regression Test: Added new regression tests
Ready for qualification phase."
```

**Verification - Step 4A: Comment Trigger**

```bash
# List comments on PR to verify it was posted
gh pr view $PR_NUM --json comments -q ".comments[] | select(.body | contains(\"QA Approved\"))"
```

**Expected output:** Comment containing `✅ QA Approved` appears on PR

**Verification - Step 4B: Workflow Execution**

```bash
# Wait a moment for workflow to process
sleep 5

# Check latest workflow run
gh run list \
  --workflow capa-kanban-sync.yml \
  --limit 1 \
  --log | grep -A 5 "comment-qa-approved"
```

**Expected in logs:**
```
comment-qa-approved: Comment pattern detected: QA Approved
Updating project status to APPROVE
```

**Verification - Step 4C: Check Kanban Status**

Re-run GraphQL query from Step 2B:
```bash
# Expected output: Issue #$ISSUE_NUM should have Status = `APPROVE`
```

---

### Step 5: Test APPROVE → RELEASE Transition

**Trigger:** When comment pattern `✅ PM Release vX.Y.Z` is posted on PR

```bash
# Post PM release comment with version
gh pr comment $PR_NUM \
  --body "✅ PM Release v0.9.5

Approved for release. Version bump and tagging in progress."
```

**Verification - Step 5A: Comment Detection**

```bash
# Verify comment was posted
gh pr view $PR_NUM --json comments -q ".comments[] | select(.body | contains(\"PM Release\"))"
```

**Verification - Step 5B: Workflow Execution**

```bash
# Check workflow logs for release comment handler
gh run list \
  --workflow capa-kanban-sync.yml \
  --limit 1 \
  --log | grep -A 5 "comment-pm-release"
```

**Expected in logs:**
```
comment-pm-release: Comment pattern detected: PM Release
Version extracted: v0.9.5
Updating status to RELEASE
```

**Verification - Step 5C: Check Kanban Status**

Re-run GraphQL query:
```bash
# Expected: Issue #$ISSUE_NUM should have Status = `RELEASE`
```

---

### Step 6: Test RELEASE → CLOSED Transition

**Trigger:** When PR is merged to master

```bash
# Merge the PR
gh pr merge $PR_NUM --merge --auto
# Or if not ready for auto-merge, manually merge via GitHub UI

# Verify merge
gh pr view $PR_NUM --json state
# Expected: "MERGED"
```

**Verification - Step 6A: PR Closed Event**

```bash
# The workflow should trigger on PR closed + merged event
# Check workflow run
gh run list \
  --workflow capa-kanban-sync.yml \
  --limit 1 \
  --log | grep -A 5 "pr-closed-to-closed"
```

**Expected in logs:**
```
pr-closed-to-closed: PR merged, updating status to CLOSED
Fetching project info...
Status updated to CLOSED
```

**Verification - Step 6B: Final Kanban Status**

Re-run GraphQL query:
```bash
# Expected: Issue #$ISSUE_NUM should have Status = `CLOSED`
```

---

## Quick Test Summary (All Steps)

Run this sequence to test the full lifecycle:

```bash
#!/bin/bash
set -e

ISSUE_NUM=147  # Change to your test issue number
PROJECT_ID=<YOUR_PROJECT_ID>  # Set this from prerequisites

echo "=== Kanban Workflow Full Lifecycle Test ==="
echo "Issue: #$ISSUE_NUM"
echo "Project ID: $PROJECT_ID"
echo ""

# Step 1: Create PR (triggers CREATE→IMPLEMENT)
echo "1. Creating test PR..."
BRANCH="test/kanban-$(date +%s)"
git checkout -b "$BRANCH"
echo "# Kanban Test" > TEST_KANBAN.md
git add TEST_KANBAN.md
git commit -m "test: Kanban workflow

Closes #$ISSUE_NUM"
git push origin "$BRANCH"

PR_NUM=$(gh pr create \
  --base master \
  --head "$BRANCH" \
  --title "Test: Kanban Workflow" \
  --body "Closes #$ISSUE_NUM" \
  --json number -q ".number")

echo "   ✓ PR created: #$PR_NUM"
sleep 10

# Step 2: Wait for tests to complete (triggers IMPLEMENT→VALIDATE)
echo ""
echo "2. Waiting for tests to complete..."
sleep 30
echo "   ✓ Tests should have completed"

# Step 3: Post QA approval (triggers VALIDATE→APPROVE)
echo ""
echo "3. Posting QA approval..."
gh pr comment $PR_NUM \
  --body "✅ QA Approved

FMEA Decision: NO_FMEA
Ready for qualification."
echo "   ✓ QA approval posted"
sleep 10

# Step 4: Post PM release (triggers APPROVE→RELEASE)
echo ""
echo "4. Posting PM release..."
gh pr comment $PR_NUM \
  --body "✅ PM Release v0.9.5"
echo "   ✓ PM release posted"
sleep 10

# Step 5: Merge PR (triggers RELEASE→CLOSED)
echo ""
echo "5. Merging PR..."
gh pr merge $PR_NUM --merge || echo "   (Manual merge required via GitHub UI)"
sleep 10
echo "   ✓ PR merged"

echo ""
echo "=== Test Complete ==="
echo "Verify in GitHub Projects: Issue #$ISSUE_NUM should show Status = CLOSED"
```

Save this as `test-kanban.sh` and run:

```bash
chmod +x test-kanban.sh
./test-kanban.sh
```

---

## Verification Checklist

After running the test, verify all transitions:

- [ ] **CREATE→IMPLEMENT**: Check Kanban board after PR opens - Status should be `IMPLEMENT`
- [ ] **IMPLEMENT→VALIDATE**: Check Kanban board after tests pass - Status should be `VALIDATE`
- [ ] **VALIDATE→APPROVE**: Check Kanban board after QA comment - Status should be `APPROVE`
- [ ] **APPROVE→RELEASE**: Check Kanban board after PM comment - Status should be `RELEASE`
- [ ] **RELEASE→CLOSED**: Check Kanban board after PR merge - Status should be `CLOSED`
- [ ] **GitHub Actions logs**: All workflow steps executed without errors
- [ ] **Issue labels**: Issue maintains `type:capa` label throughout
- [ ] **PR comments**: All approval and release comments posted successfully

---

## Troubleshooting

### Issue 1: Workflow doesn't trigger on PR open

**Problem:** `pr-opened-to-implement` job doesn't run when PR is created

**Root Cause:**
- GitHub Projects V2 API permissions not configured
- Workflow file not in master branch
- `PROJECT_ID` not set in secrets

**Solution:**
```bash
# 1. Verify workflow file exists on master
git ls-remote origin | grep capa-kanban-sync.yml

# 2. Check GitHub Actions secrets
gh secret list

# 3. Re-run seed job to regenerate files
# Or manually push workflow updates:
git add .github/workflows/capa-kanban-sync.yml
git commit -m "fix: Update Kanban workflow"
git push origin master
```

### Issue 2: Kanban status doesn't change

**Problem:** PR is created but Kanban board shows no status update

**Root Cause:**
- Project field name doesn't match `Status` exactly
- Status options don't match (case-sensitive): `CREATE`, `IMPLEMENT`, `VALIDATE`, `APPROVE`, `RELEASE`, `CLOSED`
- GraphQL API errors

**Solution:**
```bash
# 1. Verify project structure
gh api graphql -f query='
  query {
    organization(login: "rajames440") {
      projectV2(number: <PROJECT_ID>) {
        fields(first: 20) {
          nodes {
            ... on ProjectV2SingleSelectField {
              name
              options {
                name
              }
            }
          }
        }
      }
    }
  }
'

# 2. Check field name is exactly "Status" (case-sensitive)
# 3. Check all 6 options exist with exact names
```

### Issue 3: GraphQL mutations fail

**Problem:** Workflow logs show GraphQL errors like "Field does not exist" or "Invalid value"

**Root Cause:**
- Field ID or option ID not found dynamically
- Wrong field type used in mutation
- API token lacks permissions

**Solution:**
```bash
# 1. Verify token has project permissions
gh api user
# Should show "public_repo" and "read:org" scopes

# 2. Check token is stored correctly in secrets
gh secret view GITHUB_TOKEN
# (This will show if it's set, not the value)

# 3. Manually test GraphQL query
gh api graphql -f query='
  query {
    viewer {
      login
    }
  }
'
# Should return your username
```

### Issue 4: Comment patterns not detected

**Problem:** QA Approved or PM Release comments don't trigger transitions

**Root Cause:**
- Comment body doesn't match regex exactly
- Pattern uses `✅` but message uses different emoji or character
- Regex needs exact format

**Solution:**
```bash
# 1. Use EXACT pattern when posting comments:
#    For QA: "✅ QA Approved"
#    For PM: "✅ PM Release v<VERSION>"

# 2. Verify comment was posted
gh pr comment $PR_NUM --body "✅ QA Approved

Test comment"

# 3. Check workflow logs for comment parsing
gh run list --workflow capa-kanban-sync.yml --limit 1 --log | grep -i "comment"
```

---

## Advanced Debugging

### View Raw Workflow Logs

```bash
# Get latest run ID
RUN_ID=$(gh run list --workflow capa-kanban-sync.yml --limit 1 --json databaseId -q ".[0].databaseId")

# View full logs
gh run view $RUN_ID --log

# Save logs to file
gh run view $RUN_ID --log > kanban-workflow.log
cat kanban-workflow.log
```

### Check GraphQL Directly

```bash
# Query project items manually
gh api graphql -f query='
  query {
    organization(login: "rajames440") {
      projectV2(number: <PROJECT_ID>) {
        items(first: 10, after: null) {
          nodes {
            id
            content {
              ... on Issue {
                number
                title
                labels(first: 5) {
                  nodes {
                    name
                  }
                }
              }
            }
            fieldValues(first: 20) {
              nodes {
                ... on ProjectV2ItemFieldSingleSelectValue {
                  field {
                    name
                  }
                  name
                }
              }
            }
          }
        }
      }
    }
  }
'
```

### Simulate Workflow Steps Locally

Test pattern matching:

```bash
# Test regex patterns from workflow
BODY="✅ QA Approved

FMEA Decision: NO_FMEA"

# Test QA pattern
if [[ "$BODY" =~ ✅\ QA\ Approved ]]; then
  echo "QA pattern matches"
else
  echo "QA pattern does NOT match"
fi

# Test PM pattern
BODY2="✅ PM Release v0.9.5"
if [[ "$BODY2" =~ ✅\ PM\ Release\ v([0-9.]+) ]]; then
  VERSION="${BASH_REMATCH[1]}"
  echo "PM pattern matches, version: $VERSION"
else
  echo "PM pattern does NOT match"
fi
```

---

## Expected Timeline

| Step | Action | Expected Time |
|------|--------|----------------|
| 1 | Create PR | Immediate |
| 2 | Workflow triggers | < 1 minute |
| 3 | Status = IMPLEMENT | < 2 minutes |
| 4 | Tests complete | 5-10 minutes |
| 5 | Status = VALIDATE | < 1 minute after tests |
| 6 | Post QA comment | Manual (< 30 seconds) |
| 7 | Status = APPROVE | < 1 minute after comment |
| 8 | Post PM comment | Manual (< 30 seconds) |
| 9 | Status = RELEASE | < 1 minute after comment |
| 10 | Merge PR | Manual (< 30 seconds) |
| 11 | Status = CLOSED | < 1 minute after merge |

**Total execution time:** ~20 minutes (mostly waiting for tests)

---

## Success Criteria

✅ **Test is SUCCESSFUL if all of these are true:**

1. Issue #$ISSUE_NUM appears in Kanban project
2. Status transitions through all 6 states in order
3. Workflow logs show no errors or exceptions
4. GitHub Actions workflow completes with status "success"
5. Comments trigger expected status changes
6. Final status is `CLOSED` after PR merge

✅ **Next steps after successful test:**

1. Document results in `CAPA-032-KANBAN-VALIDATION.md`
2. Update GitHub Actions secrets with production project ID
3. Test with real CAPA issue (#1XX)
4. Deploy to production CI/CD
5. Monitor workflow execution for 1 week

---

## References

- **Workflow File:** `.github/workflows/capa-kanban-sync.yml`
- **Project Setup:** https://github.com/rajames440/StarForth/projects
- **GraphQL API Docs:** https://docs.github.com/en/graphql
- **GitHub Actions Docs:** https://docs.github.com/en/actions