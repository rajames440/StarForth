#!/usr/bin/env bash
#
# StarForth LaTeX Book Builder
# =============================
# Generates comprehensive LaTeX documentation source that can be:
#   - Compiled to PDF with pdflatex
#   - Converted to other formats
#   - Read/edited as source
#
# README is ALWAYS first, banner image is included
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
LATEX_DIR="${BUILD_DIR}/latex"

VERSION="1.1.0"
DATE="$(date '+%B %d, %Y')"

# ---- Dependency checks -------------------------------------------------------
check_deps() {
    if ! command -v pandoc >/dev/null 2>&1; then
        die "pandoc is required for LaTeX generation. Install with: sudo apt-get install pandoc"
    fi

    if ! command -v pdflatex >/dev/null 2>&1; then
        warn "pdflatex not found - you won't be able to compile the LaTeX to PDF"
        warn "Install with: sudo apt-get install texlive-latex-base texlive-latex-extra"
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

# ---- Generate LaTeX book -----------------------------------------------------
generate_latex_book() {
    log "Generating comprehensive LaTeX book..."

    mkdir -p "${LATEX_DIR}"

    # Copy banner image
    if [[ -f "${REPO_ROOT}/banner.png" ]]; then
        cp "${REPO_ROOT}/banner.png" "${LATEX_DIR}/" 2>/dev/null || true
        log "  Copied banner.png"
    fi
    if [[ -f "${REPO_ROOT}/docs/assets/banner.png" ]]; then
        mkdir -p "${LATEX_DIR}/assets"
        cp "${REPO_ROOT}/docs/assets/banner.png" "${LATEX_DIR}/assets/" 2>/dev/null || true
    fi

    # Create master LaTeX document
    cat > "${LATEX_DIR}/StarForth-Manual.tex" <<'HEADER'
\documentclass[11pt,letterpaper,oneside]{book}

% ---- Packages ----------------------------------------------------------------
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage[margin=1in]{geometry}
\usepackage{graphicx}
\usepackage{hyperref}
\usepackage{xcolor}
\usepackage{fancyhdr}
\usepackage{titlesec}
\usepackage{listings}
\usepackage{tocloft}
\usepackage{bookmark}
% Doxygen-specific packages
\usepackage{ifthen}
\usepackage{array}
\usepackage{makeidx}
\usepackage{textcomp}
\usepackage{courier}
\usepackage{changepage}
\usepackage{natbib}
% Load doxygen.sty from doxygen subdirectory
\makeatletter
\def\input@path{{doxygen/}}
\makeatother
\usepackage{doxygen}

% ---- Hyperref setup ----------------------------------------------------------
\hypersetup{
    colorlinks=true,
    linkcolor=blue,
    filecolor=magenta,
    urlcolor=cyan,
    pdftitle={StarForth Complete Manual},
    pdfauthor={Robert A. James},
    pdfsubject={FORTH-79 Virtual Machine},
    pdfkeywords={FORTH, VM, StarForth, embedded systems}
}

% ---- Code listings setup -----------------------------------------------------
\lstset{
    basicstyle=\ttfamily\small,
    breaklines=true,
    frame=single,
    numbers=left,
    numberstyle=\tiny\color{gray},
    keywordstyle=\color{blue},
    commentstyle=\color{green!60!black},
    stringstyle=\color{orange},
    showstringspaces=false,
    tabsize=4
}

% ---- Pandoc compatibility ----------------------------------------------------
% Define \passthrough command for pandoc-generated LaTeX
\providecommand{\passthrough}[1]{#1}

% ---- Headers and footers -----------------------------------------------------
\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{\leftmark}
\fancyhead[R]{StarForth Manual}
\fancyfoot[C]{\thepage}
\renewcommand{\headrulewidth}{0.4pt}
\renewcommand{\footrulewidth}{0.4pt}

% ---- Title formatting --------------------------------------------------------
\titleformat{\chapter}[display]
{\normalfont\huge\bfseries}{\chaptertitlename\ \thechapter}{20pt}{\Huge}
\titlespacing*{\chapter}{0pt}{-20pt}{40pt}

% ---- Document ----------------------------------------------------------------
\begin{document}

% Title page
\begin{titlepage}
    \centering
    \vspace*{2cm}

    {\Huge\bfseries StarForth Complete Manual\par}
    \vspace{0.5cm}
    {\Large The State of the Union\par}
    \vspace{2cm}

    {\large High-Performance FORTH-79 Virtual Machine\par}
    \vspace{1cm}

    {\large Version VERSION_PLACEHOLDER\par}
    {\large DATE_PLACEHOLDER\par}
    \vspace{3cm}

    {\large Robert A. James\par}
    {\normalsize Quality Assured By: Santino\par}
    \vfill

    {\small This comprehensive manual includes:\par}
    {\small $\bullet$ README and Project Overview\par}
    {\small $\bullet$ Installation Guide\par}
    {\small $\bullet$ Testing Documentation\par}
    {\small $\bullet$ Architecture Documentation\par}
    {\small $\bullet$ Complete Reference Material\par}

    \vspace{1cm}
    {\footnotesize Released into the public domain under\\
    Creative Commons Zero v1.0 Universal\par}
\end{titlepage}

% Table of Contents
\tableofcontents
\clearpage

HEADER

    # Replace placeholders
    sed -i "s/VERSION_PLACEHOLDER/${VERSION}/g" "${LATEX_DIR}/StarForth-Manual.tex"
    sed -i "s/DATE_PLACEHOLDER/${DATE}/g" "${LATEX_DIR}/StarForth-Manual.tex"

    # Add API Documentation chapter - INCLUDE ALL DOXYGEN LATEX FILES
    log "  Adding API Documentation (INCLUDING ALL DOXYGEN LaTeX)..."

    # Copy ALL Doxygen LaTeX files to our build directory
    if [[ -d "${REPO_ROOT}/docs/api/latex" ]]; then
        log "  Copying Doxygen LaTeX files..."
        mkdir -p "${LATEX_DIR}/doxygen"
        cp "${REPO_ROOT}/docs/api/latex"/*.tex "${LATEX_DIR}/doxygen/" 2>/dev/null || true
        cp "${REPO_ROOT}/docs/api/latex"/*.sty "${LATEX_DIR}/doxygen/" 2>/dev/null || true
        cp "${REPO_ROOT}/docs/api/latex"/Makefile.* "${LATEX_DIR}/doxygen/" 2>/dev/null || true
        # Copy any PDF diagrams
        cp "${REPO_ROOT}/docs/api/latex"/*.pdf "${LATEX_DIR}/doxygen/" 2>/dev/null || true

        # Fix Doxygen LaTeX files for compatibility with our build
        log "  Fixing Doxygen LaTeX compatibility issues..."
        # Remove problematic unicode characters and fix escaping issues
        # FIX: Don't do global \+ replacement - it breaks things like STRICT\_PTR
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/Star\\+Forth/StarForth/g' {} \;
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/markdown\\+Table\\+Body\\+None/markdownTableBodyNone/g' {} \;
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/█/ /g' {} \;

        # Fix box-drawing characters that may still exist
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/╔/+/g; s/╗/+/g; s/╚/+/g; s/╝/+/g; s/║/|/g; s/═/=/g; s/├/+/g; s/┤/+/g; s/┬/+/g; s/┴/+/g; s/┼/+/g; s/─/-/g; s/│/|/g; s/┌/+/g; s/┐/+/g; s/└/+/g; s/┘/+/g' {} \;

        # Remove problematic emojis and special characters (LaTeX doesn't handle Unicode emojis well)
        # Replace common emojis with text equivalents
        # This needs to be comprehensive - ANY emoji will break LaTeX
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i '
            s/🧠/ /g;
            s/🧱/ /g;
            s/⚡/ /g;
            s/📂/ /g;
            s/⚙️/ /g;
            s/🪪/ /g;
            s/🌟/ /g;
            s/🔥/ /g;
            s/✅/ /g;
            s/🎉/ /g;
            s/💾/ /g;
            s/🔬/ /g;
            s/💻/ /g;
            s/🌀/ /g;
            s/🚀/ /g;
            s/🧑‍🚀/ /g;
            s/🧑‍/ /g;
            s/🏆/ /g;
            s/⭐/ /g;
            s/🛸/ /g;
            s/🧰/ /g;
            s/🧪/ /g;
            s/📜/ /g;
            s/📝/ /g;
            s/🖨️/ /g;
            s/📄/ /g;
            s/📚/ /g;
        ' {} \;

        # Remove HTML tags that Doxygen sometimes leaves in (they break LaTeX math mode)
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i '
            s/<strong>//g;
            s/<\/strong>//g;
            s/<em>//g;
            s/<\/em>//g;
            s/<tt>//g;
            s/<\/tt>//g;
            s/<code>//g;
            s/<\/code>//g;
            s/<blockquote>//g;
            s/<\/blockquote>//g;
            s/$<$//g;
            s/$>$//g;
        ' {} \;

        # Remove ESCAPED HTML tags (Doxygen format: $<$strong$>$ etc.)
        # These are what's breaking the LaTeX compilation
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i '
            s/\$<\$strong\$>\$//g;
            s/\$<\$\/strong\$>\$//g;
            s/\$<\$em\$>\$//g;
            s/\$<\$\/em\$>\$//g;
            s/\$<\$tt\$>\$//g;
            s/\$<\$\/tt\$>\$//g;
            s/\$<\$code\$>\$//g;
            s/\$<\$\/code\$>\$//g;
            s/\$<\$blockquote\$>\$//g;
            s/\$<\$\/blockquote\$>\$//g;
            s/\$<\$i\$>\$//g;
            s/\$<\$\/i\$>\$//g;
            s/\$<\$b\$>\$//g;
            s/\$<\$\/b\$>\$//g;
        ' {} \;

        # Fix underscore patterns that break LaTeX
        # First fix the escaped underscore slash pattern (\/_ should be -)
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/\\\/\_/-/g' {} \;

        # Remove Doxygen \+ control sequences (these cause "Undefined control sequence" errors)
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/\\+//g' {} \;

        # Fix backslash-uppercase patterns that LaTeX interprets as commands
        # E.g. USE\ASM\OPT=1 should be USE_ASM_OPT=1
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/\\ASM/_ASM/g; s/\\OPT/_OPT/g; s/\\X86/_X86/g; s/\\ARM/_ARM/g; s/\\64/_64/g; s/\\32/_32/g; s/\\PTR/\_PTR/g' {} \;

        # Fix backslash in paths (word\source should be word\_source)
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/word\\source/word\\_source/g; s/control\\forth/control\\_forth/g' {} \;

        # Fix ALL bare underscores in {\ttfamily ...} to be \_ (proper LaTeX escaping)
        # This regex finds ttfamily blocks and escapes underscores in them
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i 's/{\\ttfamily \([^}]*\)_\([^}]*\)}/{\\ttfamily \1\\_\2}/g' {} \;

        # Then remove _text_ emphasis patterns (replace _text_ with text)
        find "${LATEX_DIR}/doxygen" -name "*.tex" -type f -exec sed -i '
            s/_\([^_]*\)_/\1/g;
        ' {} \;

        success "Copied and fixed Doxygen LaTeX files"
    fi

    cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'APIDOCS'

% ---- Part I: API Documentation ----
\part{API Documentation}

This part contains the complete Doxygen-generated API documentation for all StarForth modules, structures, and functions.

APIDOCS

    # Now include the main Doxygen content - extract the \input lines from refman.tex
    if [[ -f "${REPO_ROOT}/docs/api/latex/refman.tex" ]]; then
        log "  Extracting Doxygen content structure..."
        # Extract all \input{} lines from refman.tex and convert them to include from doxygen/ subdir
        grep "^\\\\input{" "${REPO_ROOT}/docs/api/latex/refman.tex" | \
            sed 's|\\input{|\\input{doxygen/|g' >> "${LATEX_DIR}/StarForth-Manual.tex"

        cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'APIDOCS_END'

\clearpage

APIDOCS_END
        success "Added Doxygen API documentation structure"
    else
        warn "Doxygen refman.tex not found - API docs will be placeholder only"
        cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'APIDOCS_FALLBACK'

\chapter{API Documentation}

The Doxygen API documentation should be generated first.
Run \texttt{doxygen Doxyfile} in the repository root to generate the full API documentation.

\clearpage

APIDOCS_FALLBACK
    fi

    # Add Man Pages chapter
    log "  Adding Man Pages..."
    if [[ -f "${REPO_ROOT}/man/starforth.1" ]]; then
        # Convert man page to text and then include
        if command -v man >/dev/null 2>&1; then
            MANWIDTH=80 man -l "${REPO_ROOT}/man/starforth.1" 2>/dev/null > "${LATEX_DIR}/manpage.txt" || true
            if [[ -f "${LATEX_DIR}/manpage.txt" ]]; then
                cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'MANSTART'

% ---- Chapter: Man Page Reference ----
\chapter{Man Page Reference}

The \texttt{starforth(1)} manual page provides command-line reference:

\begin{verbatim}
MANSTART
                cat "${LATEX_DIR}/manpage.txt" >> "${LATEX_DIR}/StarForth-Manual.tex"
                cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'MANEND'
\end{verbatim}

\clearpage

MANEND
            fi
        fi
    fi

    # Add GNU Info Documentation chapter
    log "  Adding GNU Info Documentation..."
    if [[ -f "${DOCS_DIR}/starforth.texi" ]]; then
        cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'INFODOCS'

% ---- Chapter: GNU Info Documentation ----
\chapter{GNU Info Documentation}

StarForth includes comprehensive GNU Texinfo documentation accessible via:

\begin{verbatim}
info starforth
\end{verbatim}

The Texinfo documentation source is available at \texttt{docs/starforth.texi} and covers:

\begin{itemizedlist}
\item Getting Started
\item Command Line Interface
\item FORTH Words Reference
\item Block Storage System
\item Profiling and Performance
\item Testing Infrastructure
\item Packaging and Distribution
\end{itemizedlist}

\clearpage

INFODOCS
    fi

    # Convert markdown files to LaTeX and include them
    # README MUST BE FIRST!
    log "  Adding Part II: User Documentation"
    cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'PARTII'

% ---- Part II: User Documentation ----
\part{User Documentation}

PARTII
    local MD_FILES=(
        "README.md:README and Project Overview"
        "INSTALL.md:Installation Guide"
        "docs/TESTING.md:Testing Guide"
        "docs/ARCHITECTURE.md:Architecture"
        "docs/ABI.md:Application Binary Interface (ABI)"
        "docs/GAP_ANALYSIS.md:Gap Analysis"
        "docs/BREAK_ME_REPORT.md:Break Me Diagnostic Report"
    )

    cd "${REPO_ROOT}"

    for entry in "${MD_FILES[@]}"; do
        IFS=':' read -r file title <<< "$entry"

        if [[ -f "$file" ]]; then
            log "  Adding: $file"

            local basename=$(basename "$file" .md)
            local tex_file="${LATEX_DIR}/${basename}.tex"

            # Convert markdown to LaTeX
            pandoc "$file" \
                -f markdown \
                -t latex \
                --listings \
                -o "$tex_file" 2>/dev/null || {
                warn "Failed to convert $file, creating placeholder"
                echo "\\section{Error}" > "$tex_file"
                echo "Could not convert $file" >> "$tex_file"
            }

            # Fix box-drawing and other unicode characters in converted LaTeX
            if [[ -f "$tex_file" ]]; then
                # Replace box-drawing characters with ASCII equivalents
                sed -i 's/╔/+/g; s/╗/+/g; s/╚/+/g; s/╝/+/g' "$tex_file"
                sed -i 's/║/|/g; s/═/=/g' "$tex_file"
                sed -i 's/├/+/g; s/┤/+/g; s/┬/+/g; s/┴/+/g; s/┼/+/g' "$tex_file"
                sed -i 's/─/-/g; s/│/|/g' "$tex_file"
                sed -i 's/┌/+/g; s/┐/+/g; s/└/+/g; s/┘/+/g' "$tex_file"
            fi

            # Include in master document
            cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<EOF

% ---- Chapter: $title ----
\\chapter{$title}
\\input{$basename.tex}
\\clearpage

EOF
        fi
    done

    # Close document
    cat >> "${LATEX_DIR}/StarForth-Manual.tex" <<'FOOTER'

% ---- Appendix: License ----
\appendix
\chapter{License}

StarForth is released into the public domain under the Creative Commons Zero v1.0 Universal license.

\section{CC0 1.0 Universal}

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see \url{http://creativecommons.org/publicdomain/zero/1.0/}.

\end{document}
FOOTER

    success "LaTeX book source created: ${LATEX_DIR}/StarForth-Manual.tex"
}

# ---- Compile LaTeX to PDF ---------------------------------------------------
compile_latex_pdf() {
    if ! command -v pdflatex >/dev/null 2>&1; then
        warn "Skipping PDF compilation (pdflatex not found)"
        warn "Install with: sudo apt-get install texlive-latex-base texlive-latex-extra"
        return 1
    fi

    log "Compiling LaTeX to PDF..."
    cd "${LATEX_DIR}"

    # Run pdflatex three times for TOC, references, and cross-references
    log "  First pass (structure)..."
    pdflatex -interaction=nonstopmode StarForth-Manual.tex >/dev/null 2>&1 || {
        warn "First pdflatex pass had warnings (expected)"
    }

    log "  Second pass (references)..."
    pdflatex -interaction=nonstopmode StarForth-Manual.tex >/dev/null 2>&1 || {
        warn "Second pdflatex pass had warnings"
    }

    log "  Third pass (final)..."
    pdflatex -interaction=nonstopmode StarForth-Manual.tex >/dev/null 2>&1 || {
        warn "Third pdflatex pass had warnings"
    }

    if [[ -f "StarForth-Manual.pdf" ]]; then
        success "PDF compiled from LaTeX: ${LATEX_DIR}/StarForth-Manual.pdf"

        # Copy to main build directory
        cp StarForth-Manual.pdf "${BUILD_DIR}/StarForth-Manual.pdf"
        success "Copied to: ${BUILD_DIR}/StarForth-Manual.pdf"

        # Clean up auxiliary files
        rm -f *.aux *.log *.out *.toc *.lof *.lot 2>/dev/null || true

        return 0
    else
        warn "PDF compilation failed - check ${LATEX_DIR}/StarForth-Manual.log"
        return 1
    fi
}

# ---- Convert LaTeX to DocBook ------------------------------------------------
convert_to_docbook() {
    if ! command -v pandoc >/dev/null 2>&1; then
        warn "Skipping DocBook generation (pandoc not found)"
        return 1
    fi

    log "Converting LaTeX to DocBook..."
    cd "${LATEX_DIR}"

    # Use pandoc to convert LaTeX to DocBook 5
    pandoc StarForth-Manual.tex \
        -f latex \
        -t docbook5 \
        --standalone \
        --toc \
        -o "${BUILD_DIR}/StarForth-Manual.xml" 2>/dev/null || {
        warn "DocBook conversion had warnings"
    }

    if [[ -f "${BUILD_DIR}/StarForth-Manual.xml" ]]; then
        success "DocBook generated: ${BUILD_DIR}/StarForth-Manual.xml"
        return 0
    else
        warn "DocBook generation failed"
        return 1
    fi
}

# ---- Convert to EPUB ---------------------------------------------------------
convert_to_epub() {
    if ! command -v pandoc >/dev/null 2>&1; then
        warn "Skipping EPUB generation (pandoc not found)"
        return 1
    fi

    log "Converting to EPUB..."

    # Try to convert from LaTeX first (best quality)
    if [[ -f "${LATEX_DIR}/StarForth-Manual.tex" ]]; then
        log "  Converting from LaTeX source..."
        cd "${LATEX_DIR}"

        pandoc StarForth-Manual.tex \
            -f latex \
            -t epub3 \
            --standalone \
            --toc \
            --toc-depth=3 \
            --epub-metadata=<(cat <<EOF
<dc:title>StarForth Complete Manual</dc:title>
<dc:creator>Robert A. James</dc:creator>
<dc:date>$(date +%Y-%m-%d)</dc:date>
<dc:language>en</dc:language>
<dc:publisher>StarForth Project</dc:publisher>
<dc:description>Complete manual for StarForth FORTH-79 Virtual Machine including API, ABI, and user documentation</dc:description>
<dc:rights>CC0 1.0 Universal</dc:rights>
EOF
) \
            --metadata title="StarForth Complete Manual" \
            --metadata author="Robert A. James" \
            --metadata lang="en" \
            -o "${BUILD_DIR}/StarForth-Manual.epub" 2>/dev/null || {
            warn "EPUB conversion from LaTeX had warnings"
        }
    fi

    if [[ -f "${BUILD_DIR}/StarForth-Manual.epub" ]]; then
        success "EPUB generated: ${BUILD_DIR}/StarForth-Manual.epub"

        # Validate EPUB if epubcheck is available
        if command -v epubcheck >/dev/null 2>&1; then
            log "  Validating EPUB..."
            epubcheck "${BUILD_DIR}/StarForth-Manual.epub" >/dev/null 2>&1 && \
                success "EPUB validation passed" || \
                warn "EPUB validation had warnings (file is still usable)"
        fi

        return 0
    else
        warn "EPUB generation failed"
        return 1
    fi
}

# ---- Main build process ------------------------------------------------------
main() {
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  📝 StarForth Complete Documentation Builder"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "Generating: LaTeX → PDF, DocBook, EPUB"
    echo ""

    check_deps
    echo ""

    log "Setting up build directory..."
    mkdir -p "${LATEX_DIR}"
    mkdir -p "${BUILD_DIR}"

    echo ""
    generate_api_docs || warn "API docs generation skipped"

    echo ""
    generate_latex_book

    # Track what formats were generated successfully
    local formats_generated=()
    local formats_failed=()

    # Generate PDF
    echo ""
    if compile_latex_pdf; then
        formats_generated+=("PDF")
    else
        formats_failed+=("PDF")
    fi

    # Generate DocBook
    echo ""
    if convert_to_docbook; then
        formats_generated+=("DocBook")
    else
        formats_failed+=("DocBook")
    fi

    # Generate EPUB
    echo ""
    if convert_to_epub; then
        formats_generated+=("EPUB")
    else
        formats_failed+=("EPUB")
    fi

    # Final summary
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    success "Documentation generation complete!"
    echo ""
    echo "Generated files:"
    echo "  📝 LaTeX Source:  ${LATEX_DIR}/StarForth-Manual.tex"

    if [[ ${#formats_generated[@]} -gt 0 ]]; then
        echo ""
        echo "Successfully generated formats:"
        for format in "${formats_generated[@]}"; do
            case "$format" in
                "PDF")
                    echo "  📄 PDF:          ${BUILD_DIR}/StarForth-Manual.pdf"
                    ;;
                "DocBook")
                    echo "  📋 DocBook XML:  ${BUILD_DIR}/StarForth-Manual.xml"
                    ;;
                "EPUB")
                    echo "  📚 EPUB:         ${BUILD_DIR}/StarForth-Manual.epub"
                    ;;
            esac
        done
    fi

    if [[ ${#formats_failed[@]} -gt 0 ]]; then
        echo ""
        warn "Failed to generate: ${formats_failed[*]}"
        echo "  Install missing dependencies to enable all formats"
    fi

    echo ""
    echo "You can:"
    echo "  • Edit the .tex file and recompile:"
    echo "    cd ${LATEX_DIR} && pdflatex StarForth-Manual.tex"
    echo "  • View the PDF: xdg-open ${BUILD_DIR}/StarForth-Manual.pdf"
    echo "  • Read the EPUB: ebook-viewer ${BUILD_DIR}/StarForth-Manual.epub"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
}

main "$@"