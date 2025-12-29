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

################################################################################
# arXiv Submission Preparation Script
# Creates a self-contained tarball ready for arXiv upload
################################################################################

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "================================================================================"
echo "  arXiv Submission Preparation"
echo "================================================================================"
echo ""

# Step 1: Clean build
echo "[1/4] Cleaning build artifacts..."
make clean > /dev/null 2>&1
echo "      ✓ Clean complete"

# Step 2: Build PDF to verify everything works
echo "[2/4] Building PDF to verify compilation..."
make > /dev/null 2>&1
if [ ! -f "main.pdf" ]; then
    echo "      ✗ Build failed! Fix errors before submission."
    exit 1
fi
echo "      ✓ Build successful ($(ls -lh main.pdf | awk '{print $5}'), $(pdfinfo main.pdf 2>/dev/null | grep Pages | awk '{print $2}') pages)"

# Step 3: Clean again (don't include build artifacts in submission)
echo "[3/4] Cleaning for submission..."
make clean > /dev/null 2>&1
echo "      ✓ Clean complete"

# Step 4: Create tarball
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
TARBALL="arxiv-submission-${TIMESTAMP}.tar.gz"
echo "[4/4] Creating submission tarball..."
cd src
tar -czf "../${TARBALL}" *.tex *.bib 2>/dev/null
cd ..
echo "      ✓ Created: ${TARBALL}"

echo ""
echo "================================================================================"
echo "  Submission Ready"
echo "================================================================================"
echo ""
echo "Archive: ${TARBALL}"
echo "Size:    $(ls -lh ${TARBALL} | awk '{print $5}')"
echo ""
echo "Contents:"
tar -tzf "${TARBALL}" | head -20
echo ""
echo "Next steps:"
echo "  1. Upload ${TARBALL} to arXiv"
echo "  2. arXiv will extract and build main.tex"
echo "  3. Verify the generated PDF matches your local build"
echo ""
