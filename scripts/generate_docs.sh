#!/usr/bin/env bash
#
#                                  ***   StarForth   ***
#
#  generate_docs.sh- FORTH-79 Standard and ANSI C99 ONLY
#  Modified by - rajames
#  Last modified - 2025-10-23T10:55:25.121-04
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
#  /home/rajames/CLionProjects/StarForth/scripts/generate_docs.sh
#

# StarForth Documentation Generation Script
# Generates API documentation in multiple formats: HTML, PDF, AsciiDoc, Markdown
# Requires: doxygen, graphviz, pandoc, asciidoctor (optional), pdflatex (optional)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Output directories
API_DIR="docs/api"
HTML_DIR="$API_DIR/html"
LATEX_DIR="$API_DIR/latex"
XML_DIR="$API_DIR/xml"
DOCBOOK_DIR="$API_DIR/docbook"
MAN_DIR="$API_DIR/man"
ASCIIDOC_DIR="$API_DIR/asciidoc"
MARKDOWN_DIR="$API_DIR/markdown"

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       StarForth Documentation Generator v1.0          ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check required tools
echo -e "${YELLOW}→ Checking required tools...${NC}"

check_tool() {
    if command -v "$1" &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} $1 found"
        return 0
    else
        echo -e "  ${RED}✗${NC} $1 not found"
        return 1
    fi
}

DOXYGEN_OK=0
GRAPHVIZ_OK=0
PANDOC_OK=0
ASCIIDOCTOR_OK=0
PDFLATEX_OK=0

check_tool doxygen && DOXYGEN_OK=1 || true
check_tool dot && GRAPHVIZ_OK=1 || true
check_tool pandoc && PANDOC_OK=1 || true
check_tool asciidoctor && ASCIIDOCTOR_OK=1 || true
check_tool pdflatex && PDFLATEX_OK=1 || true

if [ $DOXYGEN_OK -eq 0 ]; then
    echo -e "${RED}ERROR: doxygen is required but not installed${NC}"
    echo "Install with: sudo apt-get install doxygen (Debian/Ubuntu)"
    echo "Or: brew install doxygen (macOS)"
    exit 1
fi

echo ""

# Clean old documentation
echo -e "${YELLOW}→ Cleaning old documentation...${NC}"
rm -rf "$API_DIR"
mkdir -p "$API_DIR"

# Run Doxygen
echo -e "${YELLOW}→ Running Doxygen to generate documentation...${NC}"
doxygen Doxyfile 2>&1 | grep -v "warning:" || true

if [ ! -d "$HTML_DIR" ]; then
    echo -e "${RED}ERROR: Doxygen failed to generate HTML documentation${NC}"
    exit 1
fi

echo -e "${GREEN}✓ HTML documentation generated in $HTML_DIR${NC}"

# Check for warnings
if [ -f "$API_DIR/doxygen_warnings.log" ]; then
    WARNING_COUNT=$(wc -l < "$API_DIR/doxygen_warnings.log")
    if [ "$WARNING_COUNT" -gt 0 ]; then
        echo -e "${YELLOW}⚠  $WARNING_COUNT Doxygen warnings logged in $API_DIR/doxygen_warnings.log${NC}"
    fi
fi

# Generate PDF from LaTeX if pdflatex is available
if [ $PDFLATEX_OK -eq 1 ] && [ -d "$LATEX_DIR" ]; then
    echo -e "${YELLOW}→ Generating PDF documentation from LaTeX...${NC}"
    cd "$LATEX_DIR"
    make pdf > /dev/null 2>&1 || true
    if [ -f "refman.pdf" ]; then
        cp refman.pdf "$API_DIR/StarForth-API-Reference.pdf"
        echo -e "${GREEN}✓ PDF documentation generated: $API_DIR/StarForth-API-Reference.pdf${NC}"
    else
        echo -e "${YELLOW}⚠  PDF generation failed (this is optional)${NC}"
    fi
    cd "$PROJECT_ROOT"
else
    echo -e "${YELLOW}⚠  Skipping PDF generation (pdflatex not available)${NC}"
fi

# Generate AsciiDoc from XML using pandoc
if [ $PANDOC_OK -eq 1 ] && [ -d "$XML_DIR" ]; then
    echo -e "${YELLOW}→ Converting XML to AsciiDoc...${NC}"
    mkdir -p "$ASCIIDOC_DIR"

    # Combine XML files and convert to AsciiDoc
    if [ -f "$XML_DIR/index.xml" ]; then
        pandoc -f docbook -t asciidoc "$XML_DIR/index.xml" -o "$ASCIIDOC_DIR/starforth-api.adoc" 2>/dev/null || \
            echo -e "${YELLOW}⚠  AsciiDoc conversion had issues (this is optional)${NC}"

        if [ -f "$ASCIIDOC_DIR/starforth-api.adoc" ]; then
            echo -e "${GREEN}✓ AsciiDoc documentation generated in $ASCIIDOC_DIR${NC}"

            # Generate HTML from AsciiDoc if asciidoctor is available
            if [ $ASCIIDOCTOR_OK -eq 1 ]; then
                echo -e "${YELLOW}→ Rendering AsciiDoc to HTML...${NC}"
                asciidoctor "$ASCIIDOC_DIR/starforth-api.adoc" -o "$ASCIIDOC_DIR/starforth-api.html" 2>/dev/null || true
                if [ -f "$ASCIIDOC_DIR/starforth-api.html" ]; then
                    echo -e "${GREEN}✓ AsciiDoc HTML generated: $ASCIIDOC_DIR/starforth-api.html${NC}"
                fi
            fi
        fi
    fi
else
    echo -e "${YELLOW}⚠  Skipping AsciiDoc generation (pandoc not available)${NC}"
fi

# Generate Markdown from XML using pandoc
if [ $PANDOC_OK -eq 1 ] && [ -d "$XML_DIR" ]; then
    echo -e "${YELLOW}→ Converting XML to Markdown...${NC}"
    mkdir -p "$MARKDOWN_DIR"

    if [ -f "$XML_DIR/index.xml" ]; then
        pandoc -f docbook -t gfm "$XML_DIR/index.xml" -o "$MARKDOWN_DIR/starforth-api.md" 2>/dev/null || \
            echo -e "${YELLOW}⚠  Markdown conversion had issues (this is optional)${NC}"

        if [ -f "$MARKDOWN_DIR/starforth-api.md" ]; then
            echo -e "${GREEN}✓ Markdown documentation generated in $MARKDOWN_DIR${NC}"
        fi
    fi
else
    echo -e "${YELLOW}⚠  Skipping Markdown generation (pandoc not available)${NC}"
fi

# Generate man pages index
if [ -d "$MAN_DIR" ]; then
    echo -e "${YELLOW}→ Generating man pages index...${NC}"
    MAN_COUNT=$(find "$MAN_DIR" -name "*.3" | wc -l)
    echo -e "${GREEN}✓ Generated $MAN_COUNT man pages in $MAN_DIR${NC}"
fi

# Create documentation index
echo -e "${YELLOW}→ Creating documentation index...${NC}"
cat > "$API_DIR/README.md" << 'EOF'
# StarForth API Documentation

This directory contains comprehensive API documentation for StarForth in multiple formats.

## Available Documentation Formats

### HTML (Recommended)
Open `html/index.html` in your web browser for the full interactive API reference.

```bash
# Open in default browser
xdg-open docs/api/html/index.html  # Linux
open docs/api/html/index.html      # macOS
start docs/api/html/index.html     # Windows
```

### PDF
`StarForth-API-Reference.pdf` - Complete API reference in PDF format (if generated).

### AsciiDoc
- `asciidoc/starforth-api.adoc` - Source AsciiDoc format
- `asciidoc/starforth-api.html` - Rendered HTML from AsciiDoc

### Markdown
`markdown/starforth-api.md` - GitHub-Flavored Markdown format for easy viewing on GitHub.

### Man Pages
`man/*.3` - Traditional Unix man pages (use with `man -l man/vm_init.3`).

## Quick Links

- **Main Page**: [html/index.html](html/index.html)
- **File List**: [html/files.html](html/files.html)
- **Function Index**: [html/globals_func.html](html/globals_func.html)
- **Data Structures**: [html/annotated.html](html/annotated.html)

## Regenerating Documentation

To regenerate all documentation formats:

```bash
make docs        # Generate all formats
make docs-html   # HTML only
make docs-pdf    # PDF only (requires pdflatex)
```

## Requirements

- **Required**: doxygen, graphviz
- **Optional**: pandoc (for AsciiDoc/Markdown), asciidoctor (for AsciiDoc HTML), pdflatex (for PDF)

Install on Ubuntu/Debian:
```bash
sudo apt-get install doxygen graphviz pandoc texlive-latex-base texlive-latex-extra
gem install asciidoctor  # Optional
```

Install on macOS:
```bash
brew install doxygen graphviz pandoc
brew install --cask mactex  # For PDF, or use basictex
gem install asciidoctor     # Optional
```

---

**Generated by StarForth Documentation Generator**
EOF

echo -e "${GREEN}✓ Documentation index created: $API_DIR/README.md${NC}"

# Summary
echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║              Documentation Generation Complete         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}Documentation generated successfully!${NC}"
echo ""
echo "Available formats:"
[ -d "$HTML_DIR" ] && echo -e "  ${GREEN}✓${NC} HTML:      $HTML_DIR/index.html"
[ -f "$API_DIR/StarForth-API-Reference.pdf" ] && echo -e "  ${GREEN}✓${NC} PDF:       $API_DIR/StarForth-API-Reference.pdf"
[ -f "$ASCIIDOC_DIR/starforth-api.adoc" ] && echo -e "  ${GREEN}✓${NC} AsciiDoc:  $ASCIIDOC_DIR/starforth-api.adoc"
[ -f "$MARKDOWN_DIR/starforth-api.md" ] && echo -e "  ${GREEN}✓${NC} Markdown:  $MARKDOWN_DIR/starforth-api.md"
[ -d "$MAN_DIR" ] && echo -e "  ${GREEN}✓${NC} Man Pages: $MAN_DIR/*.3"
echo ""
echo "To view HTML documentation:"
echo -e "  ${YELLOW}make docs-open${NC}"
echo "  or"
echo -e "  ${YELLOW}xdg-open $HTML_DIR/index.html${NC}"
echo ""
