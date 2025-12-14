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

###############################################################################
# AsciiDoc Build Script for Peer Review Submission Package
#
# This script generates professional HTML5 documentation with:
# - Two-column layout (content + sidebar navigation)
# - Table of contents
# - Cross-references
# - Mathematical notation support
#
# Requirements: asciidoctor (gem install asciidoctor)
###############################################################################

set -e

echo "╔═══════════════════════════════════════════════════════════════════════╗"
echo "║  Building AsciiDoc Peer Review Submission Package                     ║"
echo "╚═══════════════════════════════════════════════════════════════════════╝"
echo

# Check if asciidoctor is installed
if ! command -v asciidoctor &> /dev/null; then
    echo "❌ ERROR: asciidoctor not found"
    echo
    echo "Install with:"
    echo "  gem install asciidoctor"
    echo "  gem install asciidoctor-mathematical  # for math support"
    echo
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/html"

echo "Source directory: ${SCRIPT_DIR}"
echo "Output directory: ${OUTPUT_DIR}"
echo

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Define AsciiDoc attributes for professional styling
ASCIIDOC_OPTS=(
    "-b html5"
    "-a toc=left"
    "-a toclevels=3"
    "-a icons=font"
    "-a stem=latexmath"
    "-a source-highlighter=highlight.js"
    "-a highlightjs-languages=python,c,bash,asciidoc"
    "-a sectanchors"
    "-a sectlinks"
    "-a linkattrs"
    "-a experimental"
    "-a source-indent=0"
    "-D ${OUTPUT_DIR}"
)

echo "Building HTML documentation..."
echo

# Generate main index
echo "→ Generating index.html..."
asciidoctor "${ASCIIDOC_OPTS[@]}" "${SCRIPT_DIR}/index.adoc"

# Generate individual sections
for section in 01-summary.adoc 03-claims.adoc 04-methodology.adoc 05-results.adoc 06-completion.adoc; do
    if [ -f "${SCRIPT_DIR}/${section}" ]; then
        echo "→ Generating ${section%.adoc}.html..."
        asciidoctor "${ASCIIDOC_OPTS[@]}" "${SCRIPT_DIR}/${section}"
    fi
done

echo
echo "✓ Build complete!"
echo
echo "╔═══════════════════════════════════════════════════════════════════════╗"
echo "║  Documentation Generated Successfully                                 ║"
echo "╚═══════════════════════════════════════════════════════════════════════╝"
echo
echo "Output files:"
ls -lh "${OUTPUT_DIR}"/*.html 2>/dev/null || echo "  (HTML files not yet generated)"
echo
echo "Next steps:"
echo "  1. Open in browser:"
echo "     macOS:  open ${OUTPUT_DIR}/index.html"
echo "     Linux:  xdg-open ${OUTPUT_DIR}/index.html"
echo "     Windows: start ${OUTPUT_DIR}/index.html"
echo
echo "  2. Features:"
echo "     ✓ Two-column layout (content + sidebar navigation)"
echo "     ✓ Table of contents with live links"
echo "     ✓ Cross-references between sections"
echo "     ✓ Syntax highlighting for code"
echo "     ✓ Mathematical notation support"
echo
echo "  3. For production:"
echo "     - Convert HTML to PDF using wkhtmltopdf or similar"
echo "     - Or submit HTML directly to venues supporting it"
echo
echo "Troubleshooting:"
echo "  - If math doesn't render: gem install asciidoctor-mathematical"
echo "  - If icons missing: ensure 'icons=font' attribute is set"
echo "  - For custom styling: edit custom.css and include with -a stylesheet=custom.css"
echo

# Optional: Open in default browser if requested
if [ "$1" == "--open" ] || [ "$1" == "-o" ]; then
    echo "Opening in browser..."
    if command -v xdg-open &> /dev/null; then
        xdg-open "${OUTPUT_DIR}/index.html"
    elif command -v open &> /dev/null; then
        open "${OUTPUT_DIR}/index.html"
    else
        echo "Please open manually: ${OUTPUT_DIR}/index.html"
    fi
fi