# GitHub Actions + Jenkins CI/CD Setup

**Status:** ✅ COMPLETE - All 5 phases workflows + documentation finished

**Last Updated:** 2025-10-30

---

## What's Been Created (Phase 1-5: Complete)

### ✅ Phase 1: Completed

1. **`.github/workflows/pr-trigger-devl.yml`** - PR Merge Handler
   - **Trigger:** When a PR against `devl` branch is merged
   - **Action:** Posts comment confirming pipeline will start
   - **Purpose:** Notifies user of expected timeline (55 min total if all pass)
   - **Status:** Ready to use
   - **Jenkins Integration:** Relies on Jenkins webhook to detect `devl` branch push

### ✅ Phase 2: Completed

2. **`.github/workflows/jenkins-status-monitor.yml`** - Pipeline Status Checker
   - **Trigger:** Manual dispatch (`workflow_dispatch`) OR on schedule (commented out, can enable)
   - **Action:** Queries Jenkins API for devl/test/qual job status
   - **Purpose:** Posts progress updates to PR with current pipeline state
   - **Status:** Ready to test (requires Jenkins API credentials)
   - **Jenkins Integration:** Reads status from Jenkins API endpoint
   - **Manual Test:** Actions tab → "Monitor Jenkins Pipeline Status" → Run workflow

### ✅ Phase 3: Completed

3. **`.github/workflows/pm-release-approval.yml`** - PM Release Approval Gate
   - **Trigger:** Manual dispatch only (`workflow_dispatch`)
   - **Action:** Requires PM to explicitly approve release with version number
   - **Purpose:** Gate for triggering prod pipeline, ensures master is updated
   - **Status:** Ready to use (requires Jenkins API credentials)
   - **Jenkins Integration:** Triggers Jenkins prod job via API
   - **Manual Use:** Actions tab → "PM Release Approval Gate" → "Run workflow"
     - Input `release_version` (e.g., `2.0.0`)
     - Input `auto_merge_master` (`true` or `false`)
   - **Behavior:**
     - Verifies qual branch is ready for release
     - Triggers Jenkins prod pipeline
     - Monitors build for completion (10-minute timeout)
     - Verifies master branch is in sync with prod
     - Posts summary to GitHub

### ✅ Phase 4: Completed

4. **`.github/workflows/kanban-automation.yml`** - Kanban Board Automation
   - **Trigger:** PR events + workflow completion events
   - **Action:** Posts status updates to PRs, manages Kanban board progression
   - **Purpose:** Visual tracking of features through pipeline stages
   - **Status:** Ready to use (requires GitHub Projects setup)
   - **Setup:** Manual - see "Kanban Board Setup" section below
   - **Behavior:**
     - Posts "In Progress" comment when PR opened
     - Posts "Testing" comment when PR merged
     - Posts status on pipeline completion
     - Links to GitHub Projects board for visual tracking

---

## Architecture Overview

```
Developer Flow:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Developer creates PR against devl branch                      │
│    (PR triggers status checks, tests, reviews, etc.)             │
│                                                                   │
│ 2. PR is merged to devl (manually or via GitHub auto-merge)      │
│    ↓                                                              │
│    pr-trigger-devl.yml posts comment with timeline               │
│                                                                   │
│ 3. Jenkins detects devl push (via webhook)                       │
│    → Runs jenkinsfiles/devl/Jenkinsfile                          │
│    → Build + Smoke Test (~5 min)                                 │
│    ↓ ON SUCCESS                                                   │
│    → Jenkins auto-merges devl → test (git command in post)       │
│                                                                   │
│ 4. Jenkins detects test push (via webhook)                       │
│    → Runs jenkinsfiles/test/Jenkinsfile                          │
│    → Full test suite (~20 min)                                   │
│    ↓ ON SUCCESS                                                   │
│    → Jenkins auto-merges test → qual (git command in post)       │
│                                                                   │
│ 5. Jenkins detects qual push (via webhook)                       │
│    → Runs jenkinsfiles/qual/Jenkinsfile                          │
│    → Formal verification (~25 min)                               │
│    ↓ ON SUCCESS                                                   │
│    → Emails PM: "Ready for approval"                             │
│                                                                   │
│ 6. PM approves release (GitHub Actions dispatch OR manual)       │
│    ↓ [NOT YET IMPLEMENTED]                                       │
│    → Triggers prod release workflow                              │
│                                                                   │
│ 7. Jenkins prod pipeline runs                                    │
│    → Version artifacts, tag git, merge to master                 │
│    ↓ ON SUCCESS                                                   │
│    → Ensures master = prod (canonical source)                    │
│    → master is ALWAYS deployable                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Prerequisites & Configuration

### 🔴 REQUIRED: Jenkins Setup

For the workflow to work, Jenkins must be configured with **branch webhooks**:

#### Job 1: `starforth-devl`
```
Pipeline Definition: jenkinsfiles/devl/Jenkinsfile
Trigger: GitHub webhook on push to devl branch
Script Path: jenkinsfiles/devl/Jenkinsfile
```

**Webhook URL (in Jenkins):**
```
https://your-jenkins-url/github-webhook/
```

**Configure in GitHub:**
Repository Settings → Webhooks → Add webhook
```
Payload URL: https://your-jenkins-url/github-webhook/
Content type: application/json
Events: Push events, Pull request events
Active: ✓
```

#### Job 2: `starforth-test`
```
Pipeline Definition: jenkinsfiles/test/Jenkinsfile
Trigger: GitHub webhook on push to test branch
Script Path: jenkinsfiles/test/Jenkinsfile
```

#### Job 3: `starforth-qual`
```
Pipeline Definition: jenkinsfiles/qual/Jenkinsfile
Trigger: GitHub webhook on push to qual branch
Script Path: jenkinsfiles/qual/Jenkinsfile
```

#### Job 4: `starforth-prod`
```
Pipeline Definition: jenkinsfiles/prod/Jenkinsfile
Trigger: Manual (or via GitHub Actions dispatch - not yet implemented)
Script Path: jenkinsfiles/prod/Jenkinsfile
```

---

## How to Test Phase 1

### Test Sequence

1. **Prepare test branch:**
   ```bash
   git checkout -b test-pr-workflow
   echo "# Test change" >> README.md
   git add README.md
   git commit -m "test: pr workflow trigger"
   git push origin test-pr-workflow
   ```

2. **Create PR on GitHub:**
   - Go to https://github.com/rajames440/StarForth/pulls
   - Click "New Pull Request"
   - Base: `devl` | Compare: `test-pr-workflow`
   - Create PR with title: "Test: PR workflow validation"

3. **Merge the PR:**
   - On GitHub, click "Merge pull request"
   - Confirm merge

4. **Watch for GitHub Actions:**
   - Go to Actions tab
   - Find `PR→DevL: Auto-merge and trigger Jenkins pipeline`
   - Verify it ran successfully
   - Check PR comments for posted timeline

5. **Verify Jenkins webhook:**
   - Check Jenkins: `starforth-devl` job should have started
   - Verify it built and ran smoke test
   - On success, Jenkins should auto-merge `devl` → `test`

6. **Verify Jenkins auto-merge:**
   - Check GitHub: `test` branch should be ahead of `devl`
   - `starforth-test` job should auto-start via webhook

---

## How to Test Phase 2

### Setup Requirements

**Before testing, configure Jenkins API credentials in GitHub:**

1. Go to Repository Settings → Secrets and variables → Actions
2. Add 3 new secrets:
   - `JENKINS_URL`: `https://your-jenkins-url`
   - `JENKINS_USER`: Your Jenkins username
   - `JENKINS_TOKEN`: Jenkins API token (not password!)
     - Generate at: Jenkins → Your Profile → Configure → API Token → Generate

### Test Sequence

1. **Create/merge a test PR to devl** (using Phase 1 workflow)

2. **Manually trigger status check:**
   - Go to Actions tab
   - Find "Monitor Jenkins Pipeline Status"
   - Click "Run workflow"
   - Keep "pr_number" blank (it will auto-find active PR)
   - Check workflow output

3. **Expected behavior:**
   - Workflow queries Jenkins API for devl/test/qual job status
   - Posts comment to active PR with pipeline progression
   - Shows status emoji: ✅ PASSED, 🔨 BUILDING, ❌ FAILED, ⏳ PENDING

4. **Enable automatic monitoring (optional):**
   - Uncomment the `schedule` line in the workflow (line 6)
   - Workflow will check every 10 minutes during business hours
   - Auto-posts status updates to active PRs

---

## How to Test Phase 3

### Setup Requirements

**Phase 3 reuses Jenkins API credentials from Phase 2:**
- `JENKINS_URL`
- `JENKINS_USER`
- `JENKINS_TOKEN`

(Already configured in GitHub Secrets)

### Test Sequence

**Prerequisites:** qual branch should have commits ahead of master (ready for release)

1. **Verify qual branch is ready:**
   ```bash
   git fetch origin
   git log origin/master..origin/qual --oneline
   # Should show commits
   ```

2. **Trigger PM Release Approval:**
   - Go to Actions tab
   - Find "PM Release Approval Gate"
   - Click "Run workflow"
   - Input release version (e.g., `2.0.1`)
   - Input auto-merge-master (default: `true`)
   - Click "Run workflow"

3. **Expected behavior:**
   - Workflow verifies qual branch is ready
   - Triggers Jenkins prod job with version parameter
   - Monitors build for completion (10-minute timeout)
   - Verifies master is in sync with prod
   - Posts summary with status

4. **Success indicators:**
   - Workflow completes successfully
   - Jenkins prod build shows SUCCESS
   - Git tag `v{version}` created and pushed
   - master branch updated to match prod (if auto-merge enabled)

---

## Kanban Board Setup Guide

### GitHub Projects V1 (Recommended - Simpler)

Follow these steps to set up a Kanban board for tracking features through the pipeline:

1. **Create GitHub Project:**
   - Go to your repository → "Projects" tab
   - Click "New Project"
   - Name: `StarForth Release Pipeline`
   - Description: `Track features and releases through devL→Test→Qual→Prod pipeline`
   - Template: Select "Automated kanban with reviews"

2. **Create/Rename Columns:**
   The template creates columns. Rename/add as follows:
   ```
   To Do        → Backlog
   In Progress  → In Progress (keep name)
   Done         → Released
   (Add new)    → Testing
   (Add new)    → Qual Gate
   (Add new)    → Ready for Release
   ```

3. **Card Progression (Manual Rules):**
   When using GitHub Projects, cards move like this:
   ```
   Backlog
     ↓ (Create issue + add to project)
   In Progress
     ↓ (Create PR linked to issue)
   Testing
     ↓ (PR merged, devL pipeline passes)
   Qual Gate
     ↓ (Test + Qual pipelines pass)
   Ready for Release
     ↓ (PM approves release)
   Released
     ↓ (Prod pipeline succeeds, master updated)
   ```

4. **Link Issues to PRs:**
   In your PR description, add:
   ```markdown
   Closes #<issue-number>
   ```
   This automatically links the PR to the issue.

5. **Workflow Automation (GitHub Projects V1):**
   - PR opened → workflow posts comment "Status: In Progress"
   - PR merged → workflow posts comment "Status: Testing"
   - Qual passes → workflow posts comment "Status: Qual Gate"
   - PM approves → workflow posts comment "Status: Ready for Release"
   - Prod succeeds → workflow posts comment "Status: Released"

### GitHub Projects V2 (Beta - More Powerful)

If you prefer V2 (better automation via GraphQL API):
1. Create new project at: https://github.com/users/{username}/projects
2. Replace workflow with GraphQL mutations to move items
3. Contact if you want V2 implementation

### Using the Kanban Board

**For Developers:**
1. Create issue for feature/task in GitHub
2. Add issue to project (drag to "Backlog" column)
3. Create PR that references issue (`Closes #123`)
4. Merge PR when ready
5. Workflow automatically posts status updates

**For PM:**
1. Watch Kanban board for items in "Ready for Release"
2. Review qual branch commits
3. Use GitHub Actions dispatch: "PM Release Approval Gate"
4. Release is deployed, card moves to "Released"

---

### ✅ Phase 5: Completed

5. **`BRANCH_PROTECTION_GUIDE.md`** - Branch Protection Rules Manual Setup
   - **Purpose:** Enforce testing gates before merge, maintain master as canonical
   - **Status:** Complete - ready for manual GitHub configuration
   - **Coverage:**
     - Step-by-step rules for 5 branches (devl, test, qual, prod, master)
     - Why each rule exists
     - Verification checklist
     - Troubleshooting guide

6. **`PR_WORKFLOW.md`** - Developer Guide
   - **Purpose:** Instructions for developers creating and merging PRs
   - **Status:** Complete - ready to share with team
   - **Coverage:**
     - Step-by-step PR creation process
     - What happens automatically after merge
     - Pipeline monitoring
     - Debugging failed pipelines
     - FAQ and best practices

---

## Troubleshooting

### Issue: Workflow doesn't trigger when PR is merged

**Check:**
1. Is the workflow file in `.github/workflows/pr-trigger-devl.yml`? ✓
2. Is the branch filter correct (`branches: [devl]`)? ✓
3. Is the trigger type correct (`types: [closed]`)? ✓
4. Did the PR actually merge (not just close)? Check PR status

**Fix:**
```bash
# Manually test the workflow logic
git checkout devl
git log --oneline -5
# Verify most recent commit is from the test PR
```

### Issue: Jenkins doesn't auto-start devL job

**Check:**
1. Is Jenkins webhook configured in GitHub Settings?
2. Is the webhook receiving events? (Check Recent Deliveries in GitHub)
3. Is the Jenkins job configured to trigger on webhooks?
4. Is the job using the correct Jenkinsfile path?

**Fix:**
```bash
# Jenkins webhook test URL (manually trigger)
curl -X POST https://your-jenkins-url/github-webhook/ \
  -H "Content-Type: application/json" \
  -d '{"repository": {"name": "StarForth", "url": "..."}, "ref": "refs/heads/devl"}'
```

### Issue: Jenkins doesn't auto-merge to test branch

**Check:**
1. Does `jenkinsfiles/test/Jenkinsfile` post-success block run?
2. Does Jenkins have git credentials to push?
3. Is the `test` branch protected (requires approvals)?

**Fix:**
In `jenkinsfiles/test/Jenkinsfile`, the post-success block should have:
```groovy
success {
    sh '''
        git checkout qual || git checkout -b qual
        git merge test --no-edit
        git push origin qual
    '''
}
```

---

## Design Decision: Self-Approval & Documentation

**Branch protection strategy:**
- ✅ **Required:** Jenkins status checks pass (tests must succeed)
- ❌ **NOT Required:** PR reviews (optional for documentation purposes)
- **Governance documents:** Will be manually synced to read-only governance repo (future process)

This allows:
- Self-review PRs for documentation trail
- No bottleneck on merge (you can self-approve)
- Maintain audit trail for governance repo copying
- Fast iteration while maintaining clarity

---

## Next Steps (To Proceed to Phase 5 - Branch Protection Rules)

### Immediate Actions (Before testing any workflows):

1. **Set up Jenkins API credentials in GitHub Secrets:**
   - Repository Settings → Secrets and variables → Actions
   - Add `JENKINS_URL`: https://your-jenkins-url
   - Add `JENKINS_USER`: your-jenkins-username
   - Add `JENKINS_TOKEN`: Jenkins API token (generated in Jenkins profile)

2. **Configure Jenkins webhooks:**
   - Jenkins → GitHub plugin configuration
   - Point webhook to: `https://your-jenkins-url/github-webhook/`
   - Verify GitHub can reach Jenkins (firewall, networking)

3. **Create Kanban Project (GitHub Projects V1):**
   - Follow steps in "Kanban Board Setup Guide" section above
   - Create columns: Backlog, In Progress, Testing, Qual Gate, Ready for Release, Released

### Testing Sequence (Once Jenkins is configured):

1. **Test Phase 1:** Create/merge test PR to devl, watch workflow trigger Jenkins
2. **Test Phase 2:** Once Jenkins jobs run, manually trigger status monitor to verify API connection
3. **Test Phase 3:** Once qual branch has commits, manually trigger PM release approval
4. **Test Phase 4:** Verify Kanban board comments are posted to PRs during pipeline

### When Ready:
5. **Report back** with testing results
6. **Proceed to Phase 5** (Branch Protection Rules configuration)

---

## File Locations - All Complete

| File | Purpose | Status |
|------|---------|--------|
| `.github/workflows/pr-trigger-devl.yml` | Phase 1: PR merge → Jenkins trigger | ✅ Created |
| `.github/workflows/jenkins-status-monitor.yml` | Phase 2: Jenkins status polling | ✅ Created |
| `.github/workflows/pm-release-approval.yml` | Phase 3: PM approval gate | ✅ Created |
| `.github/workflows/kanban-automation.yml` | Phase 4: Kanban status automation | ✅ Created |
| `BRANCH_PROTECTION_GUIDE.md` | Phase 5a: Manual GitHub branch protection setup | ✅ Created |
| `PR_WORKFLOW.md` | Phase 5b: Developer guide for PR workflow | ✅ Created |
| `GITHUB_ACTIONS_SETUP.md` | Complete CI/CD setup documentation | ✅ Created |
| `.github/workflows/jenkins-auto-merge.yml` | Fallback auto-merge (optional) | ❌ Not needed |

---

## Questions/Blockers

### Does Jenkins have internet access to receive webhooks from GitHub?
- [ ] Yes, configured
- [ ] No, need proxy
- [ ] Need to configure

### Are Jenkins jobs already set up with Jenkinsfiles?
- [ ] Yes, all 4 jobs exist
- [ ] Partial, need to create some
- [ ] No, need to create all

### What's the Jenkins URL?
```
https://your-jenkins-url (FILL IN)
```

### Does Jenkins have git credentials to push back to GitHub?
- [ ] Yes, SSH key configured
- [ ] Yes, PAT configured
- [ ] No, need to configure

---

**Last Updated:** October 30, 2025
**Created by:** Claude Code
**Next Review:** After Phase 1 testing