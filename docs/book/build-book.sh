#!/bin/bash
#
# StarForth Documentation Build System
# Converts Markdown + Doxygen → LaTeX → PDF/EPUB/ADOC/HTML
#
# Usage: ./build-book.sh [pdf|epub|html|all]
#

set -e  # Exit on error

# ========== Configuration ==========
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DOCS_DIR="$PROJECT_ROOT/docs"
BOOK_DIR="$DOCS_DIR/book"
GEN_DIR="$BOOK_DIR/generated"
OUTPUT_DIR="$BOOK_DIR/output"
DOXYGEN_DIR="$BOOK_DIR/doxygen"

MASTER_TEX="$BOOK_DIR/starforth-manual.tex"
OUTPUT_PDF="$OUTPUT_DIR/StarForth-Manual.pdf"
OUTPUT_EPUB="$OUTPUT_DIR/StarForth-Manual.epub"
OUTPUT_HTML="$OUTPUT_DIR/html"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ========== Functions ==========

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."

    local missing=0

    # Check pandoc
    if ! command -v pandoc &> /dev/null; then
        log_error "pandoc not found. Install: apt-get install pandoc"
        missing=1
    fi

    # Check pdflatex
    if ! command -v pdflatex &> /dev/null; then
        log_error "pdflatex not found. Install: apt-get install texlive-latex-extra texlive-fonts-recommended"
        missing=1
    fi

    # Check doxygen
    if ! command -v doxygen &> /dev/null; then
        log_error "doxygen not found. Install: apt-get install doxygen"
        missing=1
    fi

    # Optional: Check for EPUB tools
    if ! command -v pandoc &> /dev/null; then
        log_warn "pandoc needed for EPUB generation"
    fi

    if [ $missing -eq 1 ]; then
        log_error "Missing required dependencies. Aborting."
        exit 1
    fi

    log_info "All dependencies found."
}

setup_directories() {
    log_info "Setting up build directories..."
    mkdir -p "$GEN_DIR"
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$DOXYGEN_DIR"
    mkdir -p "$OUTPUT_HTML"
}

convert_markdown_to_latex() {
    log_info "Converting Markdown files to LaTeX..."

    # List of markdown files to convert (in order)
    local docs=(
        "README"
        "QUICKSTART"
        "ARCHITECTURE"
        "ABI"
        "BUILD_OPTIONS"
        "TESTING"
        "BLOCK_STORAGE_GUIDE"
        "INIT_SYSTEM"
        "INIT_TOOLS"
        "PROFILER"
        "PGO_GUIDE"
        "WORD_DEVELOPMENT"
        "DOXYGEN_STYLE_GUIDE"
        "ASM_OPTIMIZATIONS"
        "ARM64_OPTIMIZATIONS"
        "RASPBERRY_PI_BUILD"
        "L4RE_INTEGRATION"
        "L4RE_DICTIONARY_ALLOCATION"
        "SECURITY"
        "CONTRIBUTING"
        "CODE_OF_CONDUCT"
        "GAP_ANALYSIS"
        "ASM_OPTIMIZATION_STATUS"
        "ARM64_BUILD_SUCCESS"
        "ARM64_ASSEMBLY_REVIEW"
        "INLINE_ASM_COMPLETE"
        "BREAK_ME_REPORT"
    )

    for doc in "${docs[@]}"; do
        local md_file=""

        # Check docs/ directory
        if [ -f "$DOCS_DIR/$doc.md" ]; then
            md_file="$DOCS_DIR/$doc.md"
        # Check root directory (README, CONTRIBUTING)
        elif [ -f "$PROJECT_ROOT/$doc.md" ]; then
            md_file="$PROJECT_ROOT/$doc.md"
        else
            log_warn "Markdown file not found: $doc.md (skipping)"
            # Create empty placeholder
            echo "% $doc not found" > "$GEN_DIR/$doc.tex"
            continue
        fi

        local tex_file="$GEN_DIR/$doc.tex"

        log_info "  Converting $doc.md → $doc.tex"

        pandoc "$md_file" \
            --from=markdown+smart \
            --to=latex \
            --output="$tex_file" \
            --listings \
            --number-sections \
            --columns=80 \
            --wrap=auto \
            2>&1 || log_warn "Failed to convert $doc.md"
    done

    log_info "Markdown conversion complete."
}

generate_doxygen() {
    log_info "Generating Doxygen documentation..."

    cd "$PROJECT_ROOT"

    # Create temporary Doxyfile with LaTeX output enabled
    cp Doxyfile "$DOXYGEN_DIR/Doxyfile.book"

    # Modify Doxyfile for book generation
    sed -i 's/^GENERATE_LATEX.*/GENERATE_LATEX = YES/' "$DOXYGEN_DIR/Doxyfile.book"
    sed -i "s|^OUTPUT_DIRECTORY.*|OUTPUT_DIRECTORY = $DOXYGEN_DIR|" "$DOXYGEN_DIR/Doxyfile.book"
    sed -i 's/^LATEX_OUTPUT.*/LATEX_OUTPUT = latex/' "$DOXYGEN_DIR/Doxyfile.book"
    sed -i 's/^GENERATE_HTML.*/GENERATE_HTML = NO/' "$DOXYGEN_DIR/Doxyfile.book"
    sed -i 's/^GENERATE_XML.*/GENERATE_XML = YES/' "$DOXYGEN_DIR/Doxyfile.book"

    # Run doxygen
    doxygen "$DOXYGEN_DIR/Doxyfile.book" > /dev/null 2>&1 || log_warn "Doxygen warnings occurred"

    log_info "Doxygen documentation generated."
}

build_pdf() {
    log_info "Building PDF from LaTeX..."

    cd "$BOOK_DIR"

    # Run pdflatex multiple times for cross-references
    log_info "  Running pdflatex (pass 1/3)..."
    pdflatex -interaction=nonstopmode -output-directory="$OUTPUT_DIR" "$MASTER_TEX" > /dev/null 2>&1 || true

    log_info "  Running pdflatex (pass 2/3)..."
    pdflatex -interaction=nonstopmode -output-directory="$OUTPUT_DIR" "$MASTER_TEX" > /dev/null 2>&1 || true

    log_info "  Running pdflatex (pass 3/3)..."
    pdflatex -interaction=nonstopmode -output-directory="$OUTPUT_DIR" "$MASTER_TEX" > "$OUTPUT_DIR/pdflatex.log" 2>&1 || {
        log_error "PDF generation failed. Check $OUTPUT_DIR/pdflatex.log"
        tail -50 "$OUTPUT_DIR/pdflatex.log"
        return 1
    }

    if [ -f "$OUTPUT_DIR/starforth-manual.pdf" ]; then
        mv "$OUTPUT_DIR/starforth-manual.pdf" "$OUTPUT_PDF"
        log_info "PDF generated: $OUTPUT_PDF"
        log_info "  Size: $(du -h "$OUTPUT_PDF" | cut -f1)"
    else
        log_error "PDF not generated"
        return 1
    fi
}

build_epub() {
    log_info "Building EPUB from LaTeX..."

    # Generate EPUB from markdown sources (easier than LaTeX → EPUB)
    local md_files=()

    for doc in "$DOCS_DIR"/*.md "$PROJECT_ROOT"/README.md; do
        if [ -f "$doc" ]; then
            md_files+=("$doc")
        fi
    done

    pandoc "${md_files[@]}" \
        --from=markdown+smart \
        --to=epub \
        --output="$OUTPUT_EPUB" \
        --toc \
        --toc-depth=3 \
        --epub-cover-image="$BOOK_DIR/cover.png" \
        --metadata title="StarForth Complete Manual" \
        --metadata author="Robert A. James" \
        --metadata language="en-US" \
        2>&1 || {
            log_warn "EPUB generation failed (cover image might be missing)"
            # Try without cover
            pandoc "${md_files[@]}" \
                --from=markdown+smart \
                --to=epub \
                --output="$OUTPUT_EPUB" \
                --toc \
                --toc-depth=3 \
                --metadata title="StarForth Complete Manual" \
                --metadata author="Robert A. James" \
                --metadata language="en-US" \
                2>&1 || log_error "EPUB generation failed"
        }

    if [ -f "$OUTPUT_EPUB" ]; then
        log_info "EPUB generated: $OUTPUT_EPUB"
        log_info "  Size: $(du -h "$OUTPUT_EPUB" | cut -f1)"
    fi
}

build_adoc() {
    log_info "Building AsciiDoc from Markdown..."

    local output_adoc="$OUTPUT_DIR/StarForth-Manual.adoc"

    # Concatenate all markdown files
    cat "$PROJECT_ROOT/README.md" "$DOCS_DIR"/*.md > "$OUTPUT_DIR/combined.md" 2>/dev/null || true

    # Convert to AsciiDoc
    pandoc "$OUTPUT_DIR/combined.md" \
        --from=markdown \
        --to=asciidoc \
        --output="$output_adoc" \
        --standalone \
        2>&1 || log_error "AsciiDoc conversion failed"

    if [ -f "$output_adoc" ]; then
        log_info "AsciiDoc generated: $output_adoc"
        log_info "  Size: $(du -h "$output_adoc" | cut -f1)"
    fi

    rm -f "$OUTPUT_DIR/combined.md"
}

build_html() {
    log_info "Building multi-page HTML site..."

    # Create HTML structure
    mkdir -p "$OUTPUT_HTML/css"
    mkdir -p "$OUTPUT_HTML/js"
    mkdir -p "$OUTPUT_HTML/images"

    # Generate improved CSS (fixed TOC issue)
    cat > "$OUTPUT_HTML/css/starforth.css" << 'EOF'
/* StarForth Documentation CSS - Dark Theme with CRT Amber */

:root {
    --bg-dark: #1a1a1a;
    --bg-code: #2a2a2a;
    --text-amber: #ffb000;
    --text-white: #f0f0f0;
    --text-gray: #a0a0a0;
    --link-blue: #4f99ff;
    --border-color: #404040;
    --toc-bg: rgba(30, 30, 30, 0.95);
}

* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
}

body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background-color: var(--bg-dark);
    color: var(--text-white);
    line-height: 1.6;
    font-size: 16px;
}

/* ========== FIXED: TOC Scrolling Issue ========== */
#toc-sidebar {
    position: fixed;
    top: 60px;  /* Below header */
    left: 0;
    width: 280px;
    height: calc(100vh - 60px);  /* Full viewport height minus header */
    background: var(--toc-bg);
    border-right: 2px solid var(--border-color);
    overflow-y: auto;  /* FIXED: Now scrolls! */
    overflow-x: hidden;
    padding: 20px 15px;
    z-index: 100;
    transition: transform 0.3s ease;
}

#toc-sidebar.hidden {
    transform: translateX(-100%);
}

#toc-sidebar::-webkit-scrollbar {
    width: 8px;
}

#toc-sidebar::-webkit-scrollbar-track {
    background: var(--bg-dark);
}

#toc-sidebar::-webkit-scrollbar-thumb {
    background: var(--border-color);
    border-radius: 4px;
}

#toc-sidebar::-webkit-scrollbar-thumb:hover {
    background: var(--text-gray);
}

#toc-sidebar h2 {
    color: var(--text-amber);
    font-size: 1.2em;
    margin-bottom: 15px;
    padding-bottom: 10px;
    border-bottom: 1px solid var(--border-color);
}

#toc-sidebar ul {
    list-style: none;
    padding-left: 0;
}

#toc-sidebar ul ul {
    padding-left: 15px;
    margin-top: 5px;
}

#toc-sidebar li {
    margin: 8px 0;
}

#toc-sidebar a {
    color: var(--link-blue);
    text-decoration: none;
    display: block;
    padding: 5px 10px;
    border-radius: 4px;
    transition: all 0.2s;
    font-size: 0.95em;
}

#toc-sidebar a:hover {
    background: var(--bg-code);
    color: var(--text-amber);
    padding-left: 15px;
}

#toc-sidebar a.active {
    background: var(--bg-code);
    color: var(--text-amber);
    font-weight: bold;
    border-left: 3px solid var(--text-amber);
    padding-left: 12px;
}

/* ========== Main Content ========== */
#main-content {
    margin-left: 300px;  /* TOC width + margin */
    padding: 80px 40px 40px 40px;
    max-width: 900px;
}

#main-content.full-width {
    margin-left: 20px;
}

/* ========== Header ========== */
header {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    height: 60px;
    background: var(--bg-code);
    border-bottom: 2px solid var(--border-color);
    padding: 0 20px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    z-index: 200;
}

header h1 {
    color: var(--text-amber);
    font-size: 1.5em;
    font-family: 'Courier New', monospace;
}

#toc-toggle {
    background: var(--link-blue);
    color: white;
    border: none;
    padding: 8px 16px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 0.9em;
    transition: background 0.2s;
}

#toc-toggle:hover {
    background: var(--text-amber);
}

/* ========== Typography ========== */
h1, h2, h3, h4, h5, h6 {
    color: var(--text-amber);
    margin-top: 1.5em;
    margin-bottom: 0.8em;
    font-weight: 600;
}

h1 { font-size: 2.2em; border-bottom: 3px solid var(--border-color); padding-bottom: 10px; }
h2 { font-size: 1.8em; border-bottom: 2px solid var(--border-color); padding-bottom: 8px; }
h3 { font-size: 1.4em; }
h4 { font-size: 1.2em; }

p {
    margin: 1em 0;
}

a {
    color: var(--link-blue);
    text-decoration: none;
    transition: color 0.2s;
}

a:hover {
    color: var(--text-amber);
    text-decoration: underline;
}

/* ========== Code Blocks ========== */
pre, code {
    font-family: 'Courier New', Consolas, monospace;
    font-size: 0.95em;
}

code {
    background: var(--bg-code);
    color: var(--text-amber);
    padding: 2px 6px;
    border-radius: 3px;
}

pre {
    background: var(--bg-code);
    color: var(--text-amber);
    padding: 20px;
    border-radius: 8px;
    border: 1px solid var(--border-color);
    overflow-x: auto;
    margin: 1.5em 0;
    line-height: 1.5;
}

pre code {
    background: transparent;
    padding: 0;
    border-radius: 0;
}

/* ========== Tables ========== */
table {
    width: 100%;
    border-collapse: collapse;
    margin: 1.5em 0;
    background: var(--bg-code);
}

th, td {
    padding: 12px;
    text-align: left;
    border: 1px solid var(--border-color);
}

th {
    background: var(--bg-dark);
    color: var(--text-amber);
    font-weight: 600;
}

tr:hover {
    background: rgba(255, 176, 0, 0.1);
}

/* ========== Lists ========== */
ul, ol {
    margin: 1em 0;
    padding-left: 2em;
}

li {
    margin: 0.5em 0;
}

/* ========== Blockquotes ========== */
blockquote {
    border-left: 4px solid var(--text-amber);
    padding-left: 20px;
    margin: 1.5em 0;
    color: var(--text-gray);
    font-style: italic;
}

/* ========== Mobile Responsive ========== */
@media (max-width: 768px) {
    #toc-sidebar {
        transform: translateX(-100%);
    }

    #toc-sidebar.visible {
        transform: translateX(0);
    }

    #main-content {
        margin-left: 0;
        padding: 80px 20px 40px 20px;
    }
}
EOF

    log_info "  CSS generated with fixed scrolling TOC"

    # Generate index.html and individual pages
    log_info "  Generating HTML pages..."

    # Convert each markdown to HTML
    for md_file in "$DOCS_DIR"/*.md; do
        if [ -f "$md_file" ]; then
            local basename=$(basename "$md_file" .md)
            local html_file="$OUTPUT_HTML/${basename}.html"

            # Use pandoc's default HTML5 template (no custom template needed)
            pandoc "$md_file" \
                --from=markdown+smart \
                --to=html5 \
                --output="$html_file" \
                --standalone \
                --css=css/starforth.css \
                --toc \
                --toc-depth=3 \
                --metadata title="StarForth: $basename" \
                --metadata lang=en \
                2>&1 || log_warn "Failed to convert $basename.md"
        fi
    done

    # Generate index page
    cat > "$OUTPUT_HTML/index.html" << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StarForth Complete Manual</title>
    <link rel="stylesheet" href="css/starforth.css">
</head>
<body>
    <header>
        <h1>⭐ StarForth Manual</h1>
        <button id="toc-toggle">Toggle TOC</button>
    </header>

    <nav id="toc-sidebar">
        <h2>Table of Contents</h2>
        <ul>
            <li><a href="README.html">Overview</a></li>
            <li><a href="QUICKSTART.html">Quick Start</a></li>
            <li><a href="ARCHITECTURE.html">Architecture</a></li>
            <li><a href="BLOCK_STORAGE_GUIDE.html">Block Storage</a></li>
            <li><a href="WORD_DEVELOPMENT.html">Word Development</a></li>
            <li><a href="SECURITY.html">Security</a></li>
            <li><a href="TESTING.html">Testing</a></li>
            <li><a href="GAP_ANALYSIS.html">Gap Analysis</a></li>
        </ul>
    </nav>

    <main id="main-content">
        <h1>StarForth Complete Manual</h1>
        <p>Version 2.0.0 - FORTH-79 Virtual Machine Implementation</p>

        <h2>Welcome</h2>
        <p>This is the complete technical documentation for StarForth, organized into multiple sections.</p>

        <h2>Quick Links</h2>
        <ul>
            <li><a href="README.html">Project Overview</a></li>
            <li><a href="QUICKSTART.html">Getting Started</a></li>
            <li><a href="ARCHITECTURE.html">System Architecture</a></li>
            <li><a href="WORD_DEVELOPMENT.html">Developer Guide</a></li>
        </ul>
    </main>

    <script>
        // TOC toggle functionality
        document.getElementById('toc-toggle').addEventListener('click', function() {
            document.getElementById('toc-sidebar').classList.toggle('hidden');
            document.getElementById('main-content').classList.toggle('full-width');
        });

        // Active link highlighting
        const links = document.querySelectorAll('#toc-sidebar a');
        links.forEach(link => {
            link.addEventListener('click', function() {
                links.forEach(l => l.classList.remove('active'));
                this.classList.add('active');
            });
        });
    </script>
</body>
</html>
EOF

    log_info "HTML site generated: $OUTPUT_HTML/index.html"
}

clean_build() {
    log_info "Cleaning build artifacts..."
    rm -rf "$GEN_DIR"
    rm -rf "$OUTPUT_DIR"
    rm -rf "$DOXYGEN_DIR"
    log_info "Clean complete."
}

# ========== Main Script ==========

show_usage() {
    echo "Usage: $0 [pdf|epub|html|adoc|all|clean]"
    echo ""
    echo "Options:"
    echo "  pdf    - Build PDF manual"
    echo "  epub   - Build EPUB ebook"
    echo "  html   - Build multi-page HTML site"
    echo "  adoc   - Build AsciiDoc version"
    echo "  all    - Build all formats (default)"
    echo "  clean  - Remove build artifacts"
    exit 1
}

main() {
    local target="${1:-all}"

    echo "========================================="
    echo "StarForth Documentation Build System"
    echo "========================================="
    echo ""

    case "$target" in
        clean)
            clean_build
            exit 0
            ;;
        pdf|epub|html|adoc|all)
            ;;
        *)
            show_usage
            ;;
    esac

    check_dependencies
    setup_directories

    # Always convert markdown and generate doxygen
    convert_markdown_to_latex
    generate_doxygen

    # Build requested formats
    case "$target" in
        pdf)
            build_pdf
            ;;
        epub)
            build_epub
            ;;
        html)
            build_html
            ;;
        adoc)
            build_adoc
            ;;
        all)
            build_pdf
            build_epub
            build_html
            build_adoc
            ;;
    esac

    echo ""
    log_info "==========================================="
    log_info "Build complete!"
    log_info "==========================================="
    log_info "Output directory: $OUTPUT_DIR"

    if [ -f "$OUTPUT_PDF" ]; then
        log_info "PDF: $OUTPUT_PDF"
    fi
    if [ -f "$OUTPUT_EPUB" ]; then
        log_info "EPUB: $OUTPUT_EPUB"
    fi
    if [ -d "$OUTPUT_HTML" ]; then
        log_info "HTML: $OUTPUT_HTML/index.html"
    fi
}

main "$@"