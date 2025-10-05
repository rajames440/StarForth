#!/usr/bin/env bash
#
# StarForth HTML Book Builder
# ============================
# Builds comprehensive HTML documentation in two formats:
#   1. Single-page HTML - Everything in one scrollable page
#   2. Multi-page HTML - Separate pages with navigation
#
# Both formats use the existing docs/css/dark.css WITHOUT modification
#
set -euo pipefail

# ---- Colors and formatting ---------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${BLUE}•${NC} $*"; }
success() { echo -e "${GREEN}✓${NC} $*"; }
warn() { echo -e "${YELLOW}⚠${NC} $*"; }
error() { echo -e "${RED}✖${NC} $*" >&2; }
die() { error "$*"; exit 1; }

# ---- Environment setup -------------------------------------------------------
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &>/dev/null && pwd )"
REPO_ROOT="$(realpath "${SCRIPT_DIR}/..")"
DOCS_DIR="${REPO_ROOT}/docs"
BUILD_DIR="${DOCS_DIR}/build"
HTML_DIR="${BUILD_DIR}/html"
CSS_FILE="${DOCS_DIR}/css/dark.css"

VERSION="1.1.0"
DATE="$(date '+%B %d, %Y')"

# ---- Dependency checks -------------------------------------------------------
check_deps() {
    if ! command -v pandoc >/dev/null 2>&1; then
        die "pandoc is required for HTML generation. Install with: sudo apt-get install pandoc"
    fi

    if [[ ! -f "$CSS_FILE" ]]; then
        warn "CSS file not found: $CSS_FILE"
        warn "HTML will be generated without styling"
    fi
}

# ---- Generate API Documentation ----------------------------------------------
generate_api_docs() {
    log "Generating Doxygen API documentation..."

    if ! command -v doxygen >/dev/null 2>&1; then
        warn "doxygen not found - API docs will be referenced but not embedded"
        return 1
    fi

    cd "${REPO_ROOT}"
    doxygen Doxyfile >/dev/null 2>&1 || {
        warn "Doxygen generation failed"
        return 1
    }

    success "Doxygen API documentation generated"
    return 0
}

# ---- Markdown to HTML conversion ---------------------------------------------
convert_md_to_html() {
    local input="$1"
    local output="$2"

    if [[ ! -f "$input" ]]; then
        warn "Markdown file not found: $input"
        return 1
    fi

    pandoc "$input" \
        -f markdown \
        -t html \
        --standalone \
        --no-highlight \
        2>/dev/null > "$output"

    return 0
}

# ---- Man page to HTML conversion ---------------------------------------------
convert_man_to_html() {
    local man_file="${REPO_ROOT}/man/starforth.1"
    local output="${HTML_DIR}/temp-manpage.html"

    if [[ ! -f "$man_file" ]]; then
        warn "Man page not found: $man_file"
        return 1
    fi

    # Convert man page to text, then to HTML
    if command -v man >/dev/null 2>&1; then
        MANWIDTH=80 man -l "$man_file" 2>/dev/null | \
            pandoc -f plain -t html --standalone 2>/dev/null > "$output" || return 1
        return 0
    fi

    return 1
}

# ---- Info docs to HTML conversion --------------------------------------------
convert_info_to_html() {
    local texi_file="${DOCS_DIR}/starforth.texi"
    local output="${HTML_DIR}/temp-info.html"

    if [[ ! -f "$texi_file" ]]; then
        warn "Texinfo source not found: $texi_file"
        return 1
    fi

    if command -v makeinfo >/dev/null 2>&1; then
        makeinfo --html --no-split "$texi_file" -o "$output" 2>/dev/null || return 1
        return 0
    fi

    return 1
}

# ---- Generate single-page HTML book ------------------------------------------
generate_single_page_html() {
    log "Generating single-page HTML book..."

    local OUTPUT="${HTML_DIR}/StarForth-Manual.html"

    # Start HTML document
    cat > "$OUTPUT" <<'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StarForth Complete Manual - The State of the Union</title>
    <link rel="stylesheet" href="css/dark.css">
    <style>
        /* Smooth scrolling */
        html { scroll-behavior: smooth; }

        /* Navigation tweaks */
        nav.toc {
            position: sticky;
            top: 0;
            background: var(--bg-2);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 1rem 1.25rem;
            margin-bottom: 2rem;
            z-index: 100;
        }

        nav.toc ul {
            list-style: none;
            margin: 0;
            padding: 0;
        }

        nav.toc li {
            margin: 0.25rem 0;
        }

        nav.toc a {
            display: block;
            padding: 0.25rem 0.5rem;
        }

        nav.toc a:hover {
            background: var(--bg-3);
            border-radius: 6px;
        }
    </style>
</head>
<body>
<div class="book">
EOF

    # Title page
    cat >> "$OUTPUT" <<EOF
<div class="titlepage">
    <h1 class="title">StarForth Complete Manual</h1>
    <h2 class="subtitle">The State of the Union</h2>
    <div class="author">Robert A. James</div>
    <div class="releaseinfo">Version $VERSION</div>
    <div class="pubdate">$DATE</div>
    <div class="abstract">
        <p>This comprehensive manual contains all StarForth documentation including API reference, man pages, GNU info documentation, and complete guides.</p>
    </div>
</div>

<nav class="toc">
    <p><b>Table of Contents</b></p>
    <ul>
        <li><a href="#overview">Overview</a></li>
        <li><a href="#readme">README</a></li>
        <li><a href="#installation">Installation Guide</a></li>
        <li><a href="#testing">Testing Guide</a></li>
        <li><a href="#architecture">Architecture</a></li>
        <li><a href="#gap-analysis">Gap Analysis</a></li>
        <li><a href="#break-me">Break Me Report</a></li>
        <li><a href="#manpage">Man Page</a></li>
        <li><a href="#api">API Documentation</a></li>
    </ul>
</nav>
EOF

    # Overview section
    cat >> "$OUTPUT" <<'EOF'
<div class="chapter" id="overview">
    <div class="titlepage">
        <h2 class="title">Overview</h2>
    </div>
    <p>StarForth is a production-ready FORTH-79 virtual machine with modern extensions, designed for embedded systems, operating system development, and interactive programming.</p>

    <h3>Key Features</h3>
    <ul class="itemizedlist">
        <li>100% FORTH-79 standard compliant</li>
        <li>High-performance direct-threaded VM</li>
        <li>Inline assembly optimizations (x86_64, ARM64)</li>
        <li>Profile-guided optimization support</li>
        <li>Comprehensive test suite (783 tests, 93.5% pass rate)</li>
        <li>Block storage system with versioning</li>
        <li>Word execution profiling and hot word analysis</li>
        <li>Multi-architecture support</li>
    </ul>

    <h3>Performance</h3>
    <p>StarForth achieves exceptional performance:</p>
    <ul class="itemizedlist">
        <li>50-100 million operations/second on modern hardware</li>
        <li>Zero-overhead abstractions</li>
        <li>Direct-threaded VM architecture</li>
        <li>LTO (Link-Time Optimization)</li>
        <li>Profile-guided optimization</li>
    </ul>
</div>
EOF

    # Copy banner image to build directory if it exists
    if [[ -f "${REPO_ROOT}/banner.png" ]]; then
        cp "${REPO_ROOT}/banner.png" "${HTML_DIR}/" 2>/dev/null || true
    fi
    if [[ -f "${REPO_ROOT}/docs/assets/banner.png" ]]; then
        mkdir -p "${HTML_DIR}/assets"
        cp "${REPO_ROOT}/docs/assets/banner.png" "${HTML_DIR}/assets/" 2>/dev/null || true
    fi

    # Convert and add markdown documentation
    # README MUST BE FIRST!
    local MD_FILES=(
        "README.md:readme:README"
        "INSTALL.md:installation:Installation Guide"
        "docs/TESTING.md:testing:Testing Guide"
        "docs/ARCHITECTURE.md:architecture:Architecture"
        "docs/GAP_ANALYSIS.md:gap-analysis:Gap Analysis"
        "docs/BREAK_ME_REPORT.md:break-me:Break Me Report"
    )

    for entry in "${MD_FILES[@]}"; do
        IFS=':' read -r file id title <<< "$entry"
        local full_path="${REPO_ROOT}/${file}"

        if [[ -f "$full_path" ]]; then
            log "  Adding: $file"

            # Convert markdown to HTML fragment
            # Fix image paths for banner.png
            local html_content=$(pandoc "$full_path" -f markdown -t html 2>/dev/null | \
                sed 's|src="banner\.png"|src="banner.png"|g' | \
                sed 's|src="assets/banner\.png"|src="assets/banner.png"|g' || \
                echo "<p>Error converting $file</p>")

            cat >> "$OUTPUT" <<EOF

<div class="chapter" id="$id">
    <div class="titlepage">
        <h2 class="title">$title</h2>
    </div>
    $html_content
</div>
EOF
        fi
    done

    # Add man page
    if convert_man_to_html; then
        log "  Adding: Man Page"
        local man_content=$(cat "${HTML_DIR}/temp-manpage.html" | \
            sed -n '/<body>/,/<\/body>/p' | \
            sed '/<body>/d;/<\/body>/d' 2>/dev/null || echo "<p>Man page conversion failed</p>")

        cat >> "$OUTPUT" <<EOF

<div class="chapter" id="manpage">
    <div class="titlepage">
        <h2 class="title">Man Page Reference</h2>
    </div>
    $man_content
</div>
EOF
    fi

    # Add API documentation - COPY all Doxygen HTML files and link to them
    log "  Adding: API Documentation (linking to Doxygen HTML)"

    # Copy entire Doxygen HTML output to build directory
    if [[ -d "${REPO_ROOT}/docs/api/html" ]]; then
        log "  Copying Doxygen API HTML files..."
        mkdir -p "${HTML_DIR}/api"
        cp -r "${REPO_ROOT}/docs/api/html"/* "${HTML_DIR}/api/" 2>/dev/null || true
        success "Copied Doxygen API documentation"
    fi

    cat >> "$OUTPUT" <<'EOF'

<div class="chapter" id="api">
    <div class="titlepage">
        <h2 class="title">API Documentation</h2>
    </div>
    <p>Complete API documentation generated from source code using Doxygen.</p>

    <p><strong>📚 <a href="api/index.html">Browse Full API Documentation</a></strong></p>

    <h3>Key Modules</h3>
    <ul class="itemizedlist">
        <li><a href="api/vm_8h.html">VM Core (vm.h)</a> - Virtual machine implementation</li>
        <li><a href="api/stack__management_8h.html">Stack Management (stack_management.h)</a> - Data and return stack operations</li>
        <li><a href="api/dictionary__management_8h.html">Dictionary Management (dictionary_management.h)</a> - Word definitions and lookup</li>
        <li><a href="api/block__subsystem_8h.html">Block Subsystem (block_subsystem.h)</a> - Block storage and I/O</li>
        <li><a href="api/profiler_8h.html">Profiler (profiler.h)</a> - Word execution profiling</li>
        <li><a href="api/annotated.html">All Data Structures</a> - Complete list of all structures</li>
        <li><a href="api/files.html">All Files</a> - Complete source file documentation</li>
    </ul>
</div>
EOF

    # Close HTML document
    cat >> "$OUTPUT" <<'EOF'

<hr>
<div class="chapter">
    <p style="text-align: center; color: var(--muted);">
        <strong>License:</strong> Released into the public domain under Creative Commons Zero v1.0 Universal
        <br><br>
        <strong>Made with ❤️ by Robert A. James</strong>
        <br>
        <strong>Quality assured by Santino</strong>
    </p>
</div>

</div><!-- .book -->
</body>
</html>
EOF

    success "Single-page HTML book created: $OUTPUT"

    # Clean up temp files
    rm -f "${HTML_DIR}"/temp-*.html 2>/dev/null || true

    return 0
}

# ---- Generate multi-page HTML book -------------------------------------------
generate_multi_page_html() {
    log "Generating multi-page HTML book..."

    local MULTI_DIR="${HTML_DIR}/book"
    mkdir -p "$MULTI_DIR"

    # Navigation helper function
    generate_nav() {
        local current="$1"
        cat <<'NAV'
<nav class="toc" style="margin-bottom: 2rem;">
    <p><b>StarForth Manual</b></p>
    <ul>
        <li><a href="index.html">Home</a></li>
        <li><a href="readme.html">README</a></li>
        <li><a href="installation.html">Installation</a></li>
        <li><a href="testing.html">Testing</a></li>
        <li><a href="architecture.html">Architecture</a></li>
        <li><a href="gap-analysis.html">Gap Analysis</a></li>
        <li><a href="break-me.html">Break Me Report</a></li>
        <li><a href="manpage.html">Man Page</a></li>
        <li><a href="../api/index.html">API Docs</a></li>
    </ul>
</nav>
NAV
    }

    # Generate index.html
    log "  Creating index.html"
    cat > "${MULTI_DIR}/index.html" <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StarForth Complete Manual</title>
    <link rel="stylesheet" href="../css/dark.css">
</head>
<body>
<div class="book">

<div class="titlepage">
    <h1 class="title">StarForth Complete Manual</h1>
    <h2 class="subtitle">The State of the Union</h2>
    <div class="author">Robert A. James</div>
    <div class="releaseinfo">Version $VERSION</div>
    <div class="pubdate">$DATE</div>
</div>

$(generate_nav "index")

<div class="chapter">
    <div class="titlepage">
        <h2 class="title">Welcome to StarForth</h2>
    </div>

    <p>StarForth is a production-ready FORTH-79 virtual machine with modern extensions, designed for embedded systems, operating system development, and interactive programming.</p>

    <h3>Key Features</h3>
    <ul class="itemizedlist">
        <li>100% FORTH-79 standard compliant</li>
        <li>High-performance direct-threaded VM</li>
        <li>Inline assembly optimizations (x86_64, ARM64)</li>
        <li>Profile-guided optimization support</li>
        <li>Comprehensive test suite (783 tests, 93.5% pass rate)</li>
        <li>Block storage system with versioning</li>
        <li>Word execution profiling</li>
        <li>Multi-architecture support</li>
    </ul>

    <h3>Quick Links</h3>
    <ul class="itemizedlist">
        <li><a href="readme.html">README</a> - Project overview and quick start</li>
        <li><a href="installation.html">Installation Guide</a> - Build and installation instructions</li>
        <li><a href="testing.html">Testing Guide</a> - Test suite documentation</li>
        <li><a href="../api/index.html">API Documentation</a> - Complete API reference</li>
    </ul>
</div>

</div><!-- .book -->
</body>
</html>
EOF

    # Copy banner images to multi-page directory
    if [[ -f "${REPO_ROOT}/banner.png" ]]; then
        cp "${REPO_ROOT}/banner.png" "${MULTI_DIR}/" 2>/dev/null || true
    fi
    if [[ -f "${REPO_ROOT}/docs/assets/banner.png" ]]; then
        mkdir -p "${MULTI_DIR}/assets"
        cp "${REPO_ROOT}/docs/assets/banner.png" "${MULTI_DIR}/assets/" 2>/dev/null || true
    fi

    # Generate individual pages for each markdown file
    # README MUST BE FIRST!
    local PAGES=(
        "README.md:readme:README"
        "INSTALL.md:installation:Installation Guide"
        "docs/TESTING.md:testing:Testing Guide"
        "docs/ARCHITECTURE.md:architecture:Architecture"
        "docs/GAP_ANALYSIS.md:gap-analysis:Gap Analysis"
        "docs/BREAK_ME_REPORT.md:break-me:Break Me Diagnostic Report"
    )

    for entry in "${PAGES[@]}"; do
        IFS=':' read -r file slug title <<< "$entry"
        local full_path="${REPO_ROOT}/${file}"

        if [[ -f "$full_path" ]]; then
            log "  Creating ${slug}.html"

            # Convert markdown to HTML and fix image paths
            local html_content=$(pandoc "$full_path" -f markdown -t html 2>/dev/null | \
                sed 's|src="banner\.png"|src="banner.png"|g' | \
                sed 's|src="assets/banner\.png"|src="assets/banner.png"|g' || \
                echo "<p>Error converting $file</p>")

            cat > "${MULTI_DIR}/${slug}.html" <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>$title - StarForth Manual</title>
    <link rel="stylesheet" href="../css/dark.css">
</head>
<body>
<div class="book">

$(generate_nav "$slug")

<div class="chapter">
    <div class="titlepage">
        <h2 class="title">$title</h2>
    </div>
    $html_content
</div>

</div><!-- .book -->
</body>
</html>
EOF
        fi
    done

    # Generate man page
    if convert_man_to_html; then
        log "  Creating manpage.html"
        local man_content=$(cat "${HTML_DIR}/temp-manpage.html" | \
            sed -n '/<body>/,/<\/body>/p' | \
            sed '/<body>/d;/<\/body>/d' 2>/dev/null || echo "<p>Man page conversion failed</p>")

        cat > "${MULTI_DIR}/manpage.html" <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Man Page - StarForth Manual</title>
    <link rel="stylesheet" href="../css/dark.css">
</head>
<body>
<div class="book">

$(generate_nav "manpage")

<div class="chapter">
    <div class="titlepage">
        <h2 class="title">Man Page Reference</h2>
    </div>
    $man_content
</div>

</div><!-- .book -->
</body>
</html>
EOF
    fi

    success "Multi-page HTML book created in: $MULTI_DIR"

    # Clean up temp files
    rm -f "${HTML_DIR}"/temp-*.html 2>/dev/null || true

    return 0
}

# ---- Main build process ------------------------------------------------------
main() {
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  📖 StarForth HTML Book Builder"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    # Check dependencies
    check_deps
    echo ""

    # Setup directories
    log "Setting up build directories..."
    mkdir -p "$HTML_DIR"

    # Copy CSS to build directory so paths work correctly
    if [[ -f "$CSS_FILE" ]]; then
        mkdir -p "${HTML_DIR}/css"
        cp "$CSS_FILE" "${HTML_DIR}/css/" 2>/dev/null || true
        log "Copied dark.css to build directory"
    fi

    # Generate API documentation
    echo ""
    generate_api_docs || warn "API docs generation skipped"

    # Generate both formats
    echo ""
    log "Generating HTML documentation..."
    echo ""

    generate_single_page_html
    echo ""
    generate_multi_page_html

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    success "HTML book generation complete!"
    echo ""
    echo "  📄 Single-page: ${HTML_DIR}/StarForth-Manual.html"
    echo "  📚 Multi-page:  ${HTML_DIR}/book/index.html"
    echo ""
    echo "  Both formats use your existing dark.css styling!"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
}

main "$@"