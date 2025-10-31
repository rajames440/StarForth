#!/bin/bash
# Pipeline Validation Script - Test components before running Jenkins

set -e

echo "════════════════════════════════════════════════════════════"
echo "  🔍 StarForth Pipeline Validation"
echo "════════════════════════════════════════════════════════════"
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASS=0
FAIL=0
WARN=0

# Helper functions
pass() {
    echo -e "${GREEN}✅ PASS${NC}: $1"
    ((PASS++))
}

fail() {
    echo -e "${RED}❌ FAIL${NC}: $1"
    ((FAIL++))
}

warn() {
    echo -e "${YELLOW}⚠️  WARN${NC}: $1"
    ((WARN++))
}

echo "1️⃣  Checking Build System"
echo "════════════════════════════════════════════════════════════"

if command -v make &> /dev/null; then
    pass "make found"
else
    fail "make not found"
fi

if command -v gcc &> /dev/null; then
    pass "gcc found ($(gcc --version | head -1 | cut -d' ' -f3))"
else
    fail "gcc not found"
fi

echo ""
echo "2️⃣  Checking Formal Verification"
echo "════════════════════════════════════════════════════════════"

if command -v isabelle &> /dev/null; then
    ISABELLE_VERSION=$(isabelle version 2>&1 | head -1)
    pass "Isabelle found ($ISABELLE_VERSION)"
else
    warn "Isabelle not found - formal verification will be skipped"
fi

if [ -f docs/REFINEMENT_CAPA.adoc ]; then
    pass "REFINEMENT_CAPA.adoc exists"
else
    warn "REFINEMENT_CAPA.adoc not found - run 'make refinement-init'"
fi

if [ -f docs/REFINEMENT_ANNOTATIONS.adoc ]; then
    pass "REFINEMENT_ANNOTATIONS.adoc exists"
else
    warn "REFINEMENT_ANNOTATIONS.adoc not found"
fi

if [ -f docs/REFINEMENT_ROADMAP.adoc ]; then
    pass "REFINEMENT_ROADMAP.adoc exists"
else
    warn "REFINEMENT_ROADMAP.adoc not found"
fi

echo ""
echo "3️⃣  Checking Documentation Generation"
echo "════════════════════════════════════════════════════════════"

if command -v doxygen &> /dev/null; then
    pass "doxygen found"
else
    warn "doxygen not found - API docs generation will be skipped"
fi

if [ -f scripts/asciidoc-to-latex.sh ]; then
    pass "asciidoc-to-latex.sh found"
else
    warn "asciidoc-to-latex.sh not found"
fi

echo ""
echo "4️⃣  Checking Packaging"
echo "════════════════════════════════════════════════════════════"

if command -v fpm &> /dev/null; then
    FPM_VERSION=$(fpm --version 2>&1 | head -1)
    pass "fpm found ($FPM_VERSION)"
else
    warn "fpm not found - package builds will be skipped"
    echo "     Install with: sudo apt-get install ruby-dev && gem install fpm"
fi

echo ""
echo "5️⃣  Checking Memory Testing"
echo "════════════════════════════════════════════════════════════"

if command -v valgrind &> /dev/null; then
    pass "valgrind found"
else
    warn "valgrind not found - memory leak detection will be skipped"
fi

echo ""
echo "6️⃣  Checking Directories"
echo "════════════════════════════════════════════════════════════"

if [ -d src ]; then
    pass "src directory exists"
else
    fail "src directory not found"
fi

if [ -d include ]; then
    pass "include directory exists"
else
    fail "include directory not found"
fi

if [ -d docs ]; then
    pass "docs directory exists"
else
    fail "docs directory not found"
fi

echo ""
echo "7️⃣  Testing Makefile Targets"
echo "════════════════════════════════════════════════════════════"

# Test that key targets exist in Makefile
if grep -q "^refinement-status:" Makefile; then
    pass "refinement-status target exists"
else
    fail "refinement-status target not found in Makefile"
fi

if grep -q "^refinement-annotate-check:" Makefile; then
    pass "refinement-annotate-check target exists"
else
    fail "refinement-annotate-check target not found in Makefile"
fi

if grep -q "^docs-isabelle:" Makefile; then
    pass "docs-isabelle target exists"
else
    fail "docs-isabelle target not found in Makefile"
fi

if grep -q "^deb:" Makefile; then
    pass "deb target exists"
else
    fail "deb target not found in Makefile"
fi

if grep -q "^rpm:" Makefile; then
    pass "rpm target exists"
else
    fail "rpm target not found in Makefile"
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  📊 Validation Summary"
echo "════════════════════════════════════════════════════════════"
echo -e "  ${GREEN}Passed:${NC}  $PASS"
echo -e "  ${YELLOW}Warnings:${NC} $WARN"
echo -e "  ${RED}Failed:${NC}  $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    if [ $WARN -eq 0 ]; then
        echo -e "${GREEN}✅ Pipeline is ready to run!${NC}"
        exit 0
    else
        echo -e "${YELLOW}⚠️  Pipeline will run with some features disabled${NC}"
        exit 0
    fi
else
    echo -e "${RED}❌ Fix the failures above before running pipeline${NC}"
    exit 1
fi