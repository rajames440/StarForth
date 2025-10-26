#!/bin/bash
#
#                                  ***   StarForth   ***
#
#  check-submodule-protection.sh- FORTH-79 Standard and ANSI C99 ONLY
#  Modified by - rajames
#  Last modified - 2025-10-26T16:05:49.109-04
#
#  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
#
#  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
#  To the extent possible under law, the author(s) have dedicated all copyright and related
#  and neighboring rights to this software to the public domain worldwide.
#  This software is distributed without any warranty.
#
#  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#  /home/rajames/CLionProjects/StarForth/scripts/check-submodule-protection.sh
#

################################################################################
# CRITICAL: Prevent accidental commits to read-only StarForth-Governance submodule
#
# This script is called from .git/hooks/pre-commit
# It blocks any commits that modify the read-only StarForth-Governance submodule
#
# Exit codes:
#   0 = No submodule changes (safe to commit)
#   1 = Submodule changes detected (commit BLOCKED)
################################################################################

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

log_error() {
    echo -e "${RED}✗${NC}  $1" >&2
}

log_success() {
    echo -e "${GREEN}✓${NC}  $1"
}

# Main
main() {
    log_info "Checking for read-only submodule changes..."

    # Check if StarForth-Governance submodule has staged changes
    if git diff --cached --name-only | grep -q "^StarForth-Governance"; then
        echo ""
        log_error "❌ CRITICAL ERROR: Changes detected in read-only submodule!"
        echo ""
        echo "${RED}════════════════════════════════════════════════════════════${NC}"
        echo "${RED}  StarForth-Governance is a READ-ONLY submodule${NC}"
        echo "${RED}════════════════════════════════════════════════════════════${NC}"
        echo ""
        echo "  Only the project maintainer (rajames) can modify governance files."
        echo ""
        echo "  If you need governance changes:"
        echo "  1. Create an issue describing the change needed"
        echo "  2. Reference this issue in your PR"
        echo "  3. The maintainer will handle it separately"
        echo ""
        echo "  To undo these changes:"
        echo "  $ git reset HEAD StarForth-Governance/"
        echo "  $ git checkout -- StarForth-Governance/"
        echo ""
        echo "${RED}════════════════════════════════════════════════════════════${NC}"
        echo ""
        return 1
    fi

    log_success "Submodule protection check passed"
    return 0
}

main "$@"