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

# Kanban Workflow Setup Checklist
# Run this script to prepare for Kanban workflow testing

set -e

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║        Kanban Workflow Setup Checklist (Phase 3)              ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Color codes
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Track completion
COMPLETED=0
TOTAL=5

# Step 1: Check GitHub CLI installed
echo -e "${BLUE}[1/5] Checking GitHub CLI installation...${NC}"
if command -v gh &> /dev/null; then
    echo -e "${GREEN}✓ GitHub CLI installed${NC}"
    gh --version
    ((COMPLETED++))
else
    echo -e "${RED}✗ GitHub CLI not installed${NC}"
    echo "  Install from: https://cli.github.com"
    exit 1
fi
echo ""

# Step 2: Check GitHub authentication
echo -e "${BLUE}[2/5] Checking GitHub authentication...${NC}"
if gh auth status &>/dev/null; then
    USER=$(gh auth status --show-token 2>&1 | grep "Logged in to" | awk '{print $NF}' || echo "unknown")
    echo -e "${GREEN}✓ GitHub authentication valid${NC}"
    gh auth status
    ((COMPLETED++))
else
    echo -e "${RED}✗ Not authenticated with GitHub${NC}"
    echo "  Run: gh auth login"
    exit 1
fi
echo ""

# Step 3: Verify StarForth repo access
echo -e "${BLUE}[3/5] Checking StarForth repository access...${NC}"
if gh repo view rajames440/StarForth &>/dev/null; then
    echo -e "${GREEN}✓ StarForth repository accessible${NC}"
    ((COMPLETED++))
else
    echo -e "${RED}✗ Cannot access StarForth repository${NC}"
    echo "  Verify: https://github.com/rajames440/StarForth"
    exit 1
fi
echo ""

# Step 4: Check for GitHub Project V2
echo -e "${BLUE}[4/5] Checking GitHub Projects...${NC}"
echo "  Fetching projects..."
PROJECTS=$(gh project list --owner rajames440 2>&1 || echo "")

if echo "$PROJECTS" | grep -q "StarForth Quality Kanban"; then
    echo -e "${GREEN}✓ StarForth Quality Kanban project exists${NC}"
    echo "$PROJECTS" | grep "StarForth Quality Kanban"

    # Extract project ID
    PROJECT_ID=$(echo "$PROJECTS" | grep "StarForth Quality Kanban" | awk '{print $1}')
    echo ""
    echo -e "${YELLOW}PROJECT_ID: $PROJECT_ID${NC}"
    echo "  Save this ID - you'll need it for testing"
    ((COMPLETED++))
else
    echo -e "${YELLOW}⚠ StarForth Quality Kanban project NOT found${NC}"
    echo ""
    echo "  You need to create it manually:"
    echo "    1. Go to: https://github.com/rajames440/StarForth/projects"
    echo "    2. Click 'New project'"
    echo "    3. Choose 'Table' layout"
    echo "    4. Name: 'StarForth Quality Kanban'"
    echo "    5. Add custom field 'Status' (single select) with options:"
    echo "       - CREATE"
    echo "       - IMPLEMENT"
    echo "       - VALIDATE"
    echo "       - APPROVE"
    echo "       - RELEASE"
    echo "       - CLOSED"
    echo ""
    echo "  After creating, run this script again."
    exit 1
fi
echo ""

# Step 5: Verify workflow file on master
echo -e "${BLUE}[5/5] Checking Kanban workflow file...${NC}"
if git ls-remote origin | grep -q "refs/heads/master"; then
    WORKFLOW_EXISTS=$(git ls-tree -r origin/master --name-only | grep "capa-kanban-sync.yml" || echo "")
    if [ -n "$WORKFLOW_EXISTS" ]; then
        echo -e "${GREEN}✓ Kanban workflow exists on master${NC}"
        echo "  File: $WORKFLOW_EXISTS"
        ((COMPLETED++))
    else
        echo -e "${RED}✗ Kanban workflow NOT on master branch${NC}"
        echo "  The file needs to be pushed to master:"
        echo "  git push origin master"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠ Cannot verify - not in a git repository${NC}"
fi
echo ""

# Summary
echo "╔════════════════════════════════════════════════════════════════╗"
if [ $COMPLETED -eq $TOTAL ]; then
    echo -e "${GREEN}✓ All prerequisites satisfied! Ready for testing.${NC}"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Next steps:"
    echo "  1. Create test CAPA issue:"
    echo "     gh issue create --title 'Test: Kanban' --body '## CAPA:\\n\\nTesting Kanban automation'"
    echo ""
    echo "  2. Run the full test:"
    echo "     bash KANBAN-WORKFLOW-TEST.md"
    echo ""
    echo "  3. Or follow the manual test guide:"
    echo "     cat KANBAN-WORKFLOW-TEST.md"
else
    echo -e "${RED}✗ Setup incomplete ($COMPLETED/$TOTAL)${NC}"
    echo "╚════════════════════════════════════════════════════════════════╝"
    exit 1
fi