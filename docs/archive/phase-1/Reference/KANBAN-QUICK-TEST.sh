#!/bin/bash
#
#  StarForth — Steady-State Virtual Machine Runtime
#
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
# See the License for the specific language governing permissions and
# limitations under the License.
#
# StarForth — Steady-State Virtual Machine Runtime
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#

# Kanban Workflow Quick Test - Follow along step by step
# This script walks you through the full lifecycle test

set -e

ISSUE_NUM=${1:-147}  # Use argument or default to 147
PROJECT_ID=${2:-}    # Must be provided

if [ -z "$PROJECT_ID" ]; then
    echo "Usage: $0 <ISSUE_NUM> <PROJECT_ID>"
    echo ""
    echo "Example: $0 147 3"
    echo ""
    echo "Get PROJECT_ID from: gh project list --owner rajames440"
    exit 1
fi

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║           Kanban Workflow Quick Test (Full Lifecycle)         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Issue Number: #$ISSUE_NUM"
echo "Project ID: $PROJECT_ID"
echo ""

# Verify issue exists
echo "Checking if issue #$ISSUE_NUM exists..."
if ! gh issue view $ISSUE_NUM &>/dev/null; then
    echo "ERROR: Issue #$ISSUE_NUM not found"
    echo ""
    echo "Create test issue first:"
    echo "  gh issue create --title 'Test: Kanban Workflow' \\"
    echo "    --body '## CAPA:\\n\\nTesting Kanban automation'"
    exit 1
fi
echo "✓ Issue #$ISSUE_NUM found"
echo ""

# Step 1: Create PR
echo "════════════════════════════════════════════════════════════════"
echo "STEP 1: Create test PR (triggers CREATE→IMPLEMENT)"
echo "════════════════════════════════════════════════════════════════"
echo ""

BRANCH="test/kanban-$(date +%s)"
echo "Creating branch: $BRANCH"

git checkout -b "$BRANCH" || git checkout "$BRANCH"
echo "# Kanban Test" > TEST_KANBAN.md
git add TEST_KANBAN.md
git commit -m "test: Kanban workflow test

Closes #$ISSUE_NUM"
git push origin "$BRANCH"

echo ""
echo "Creating PR..."
PR_NUM=$(gh pr create \
  --base master \
  --head "$BRANCH" \
  --title "Test: Kanban Workflow Automation" \
  --body "This PR tests the full Kanban automation lifecycle.

Closes #$ISSUE_NUM" \
  --json number -q ".number")

echo "✓ PR created: #$PR_NUM"
echo ""
read -p "Press Enter to continue to Step 2..."
echo ""

# Step 2: Wait for workflow
echo "════════════════════════════════════════════════════════════════"
echo "STEP 2: Workflow triggers - CHECK KANBAN BOARD"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Waiting for workflow to process PR..."
echo "(This should trigger pr-opened-to-implement)"
echo ""

for i in {1..10}; do
    sleep 3
    echo -n "."
done
echo ""
echo ""
echo "✓ Workflow processed"
echo ""
echo "ACTION: Check GitHub Projects board"
echo "  Go to: https://github.com/rajames440/StarForth/projects"
echo "  Verify: Issue #$ISSUE_NUM is in 'IMPLEMENT' column"
echo ""
read -p "Press Enter once you verify status = IMPLEMENT..."
echo ""

# Step 3: Post QA approval
echo "════════════════════════════════════════════════════════════════"
echo "STEP 3: Post QA Approval (triggers VALIDATE→APPROVE)"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Posting QA approval comment on PR #$PR_NUM..."

gh pr comment $PR_NUM \
  --body "✅ QA Approved

FMEA Decision: NO_FMEA
Regression tests: Added
Ready for qualification phase."

echo "✓ QA approval posted"
echo ""
sleep 5

echo "Waiting for workflow to process..."
for i in {1..6}; do
    sleep 2
    echo -n "."
done
echo ""
echo ""

echo "ACTION: Check GitHub Projects board"
echo "  Expected: Issue #$ISSUE_NUM is in 'APPROVE' column"
echo ""
read -p "Press Enter once you verify status = APPROVE..."
echo ""

# Step 4: Post PM release
echo "════════════════════════════════════════════════════════════════"
echo "STEP 4: Post PM Release (triggers APPROVE→RELEASE)"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Posting PM release comment on PR #$PR_NUM..."

gh pr comment $PR_NUM \
  --body "✅ PM Release v0.9.5

Approved for release."

echo "✓ PM release posted"
echo ""
sleep 5

echo "Waiting for workflow to process..."
for i in {1..6}; do
    sleep 2
    echo -n "."
done
echo ""
echo ""

echo "ACTION: Check GitHub Projects board"
echo "  Expected: Issue #$ISSUE_NUM is in 'RELEASE' column"
echo ""
read -p "Press Enter once you verify status = RELEASE..."
echo ""

# Step 5: Merge PR
echo "════════════════════════════════════════════════════════════════"
echo "STEP 5: Merge PR (triggers RELEASE→CLOSED)"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Merging PR #$PR_NUM..."

if gh pr merge $PR_NUM --merge --auto 2>/dev/null; then
    echo "✓ PR auto-merged"
else
    echo "⚠ Could not auto-merge. Merging manually..."
    gh pr merge $PR_NUM --merge
fi

echo ""
sleep 10

echo "Waiting for workflow to process..."
for i in {1..6}; do
    sleep 2
    echo -n "."
done
echo ""
echo ""

echo "ACTION: Check GitHub Projects board"
echo "  Expected: Issue #$ISSUE_NUM is in 'CLOSED' column"
echo ""
read -p "Press Enter once you verify status = CLOSED..."
echo ""

# Final verification
echo "════════════════════════════════════════════════════════════════"
echo "TEST COMPLETE!"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Verification:"
echo "  ✓ Issue #$ISSUE_NUM created"
echo "  ✓ PR #$PR_NUM created (triggers IMPLEMENT)"
echo "  ✓ QA Approved comment posted (triggers APPROVE)"
echo "  ✓ PM Release comment posted (triggers RELEASE)"
echo "  ✓ PR merged (triggers CLOSED)"
echo ""
echo "Expected final state:"
echo "  - Kanban board shows Issue #$ISSUE_NUM in 'CLOSED' column"
echo "  - All workflow runs completed successfully"
echo "  - No errors in GitHub Actions logs"
echo ""
echo "Next steps:"
echo "  1. Document results"
echo "  2. Test with real CAPA issue"
echo "  3. Deploy to production"
echo ""