#!/bin/bash
#
# build-docs.sh - Build StarForth documentation in multiple formats
# Converts Markdown → DocBook → PDF/HTML
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS_DIR="$PROJECT_ROOT/docs"
BUILD_DIR="$DOCS_DIR/build"
GENERATED_DIR="$BUILD_DIR/generated"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "════════════════════════════════════════════════════════════"
echo "   📚 Building StarForth Documentation"
echo "════════════════════════════════════════════════════════════"
echo ""

# Check for required tools
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo -e "${RED}✗${NC} Required tool '$1' not found"
        echo "  Install with: $2"
        return 1
    else
        echo -e "${GREEN}✓${NC} Found $1"
        return 0
    fi
}

echo "Checking for required tools..."
MISSING_TOOLS=0

check_tool "pandoc" "sudo apt-get install pandoc" || MISSING_TOOLS=$((MISSING_TOOLS + 1))
check_tool "xsltproc" "sudo apt-get install xsltproc" || MISSING_TOOLS=$((MISSING_TOOLS + 1))

# Optional but recommended
if ! command -v "fop" &> /dev/null; then
    echo -e "${YELLOW}⚠${NC}  Optional: fop (for PDF generation)"
    echo "  Install with: sudo apt-get install fop"
    HAS_FOP=0
else
    echo -e "${GREEN}✓${NC} Found fop"
    HAS_FOP=1
fi

if [ $MISSING_TOOLS -gt 0 ]; then
    echo ""
    echo -e "${RED}Error: Missing required tools. Please install them first.${NC}"
    exit 1
fi

echo ""
echo "Creating build directories..."
mkdir -p "$BUILD_DIR"
mkdir -p "$GENERATED_DIR"

# Generate Doxygen API documentation first
echo ""
echo "Generating Doxygen API documentation..."
if command -v doxygen &> /dev/null; then
    cd "$PROJECT_ROOT"
    doxygen Doxyfile > /dev/null 2>&1 || echo -e "${YELLOW}⚠${NC}  Doxygen had warnings (continuing...)"
    if [ -d "docs/api/html" ]; then
        echo -e "${GREEN}✓${NC} Doxygen HTML API docs generated"
    fi
    cd "$SCRIPT_DIR"
else
    echo -e "${YELLOW}⚠${NC}  Doxygen not found - skipping API docs generation"
fi

# Function to convert Markdown to DocBook
md_to_docbook() {
    local input="$1"
    local output="$2"
    local title="$3"

    echo "  Converting $(basename "$input")..."

    # Use pandoc to convert Markdown to DocBook 4
    # Note: We generate section content only (not standalone) for XInclude
    pandoc -f markdown -t docbook4 \
        --wrap=preserve \
        -o "$output.tmp" \
        "$input"

    # Post-process to remove id attributes to avoid duplicates
    # sed removes all id="..." attributes from XML tags
    sed 's/ id="[^"]*"//g' "$output.tmp" > "$output"
    rm "$output.tmp"
}

echo "Converting Markdown files to DocBook..."

# User documentation
md_to_docbook "$PROJECT_ROOT/README.md" "$GENERATED_DIR/intro-generated.xml" "Introduction"
md_to_docbook "$PROJECT_ROOT/QUICKSTART.md" "$GENERATED_DIR/quickstart-generated.xml" "Quick Start"
md_to_docbook "$PROJECT_ROOT/TESTING.md" "$GENERATED_DIR/testing-generated.xml" "Testing"
md_to_docbook "$PROJECT_ROOT/CONTRIBUTING.md" "$GENERATED_DIR/contributing-generated.xml" "Contributing"
md_to_docbook "$PROJECT_ROOT/SECURITY.md" "$GENERATED_DIR/security-generated.xml" "Security"
md_to_docbook "$PROJECT_ROOT/CODE_OF_CONDUCT.md" "$GENERATED_DIR/coc-generated.xml" "Code of Conduct"

# Developer documentation
md_to_docbook "$DOCS_DIR/GAP_ANALYSIS.md" "$GENERATED_DIR/gap-analysis-generated.xml" "Gap Analysis"
md_to_docbook "$DOCS_DIR/L4RE_INTEGRATION.md" "$GENERATED_DIR/l4re-generated.xml" "L4Re Integration"
md_to_docbook "$DOCS_DIR/L4RE_DICTIONARY_ALLOCATION.md" "$GENERATED_DIR/l4re-dict-generated.xml" "L4Re Dictionary Allocation"
md_to_docbook "$DOCS_DIR/RASPBERRY_PI_BUILD.md" "$GENERATED_DIR/rpi-generated.xml" "Raspberry Pi Build"

# Optimization documentation
md_to_docbook "$DOCS_DIR/ASM_OPTIMIZATIONS.md" "$GENERATED_DIR/x86-opt-generated.xml" "x86_64 Optimizations"
md_to_docbook "$DOCS_DIR/ARM64_OPTIMIZATIONS.md" "$GENERATED_DIR/arm64-opt-generated.xml" "ARM64 Optimizations"

# Doxygen documentation
if [ -f "$DOCS_DIR/DOXYGEN_STYLE_GUIDE.md" ]; then
    md_to_docbook "$DOCS_DIR/DOXYGEN_STYLE_GUIDE.md" "$GENERATED_DIR/doxygen-generated.xml" "Doxygen Style Guide"
fi

# Create a simple architecture overview if not exists
if [ ! -f "$DOCS_DIR/ARCHITECTURE.md" ]; then
    echo "  Creating architecture overview..."
    cat > "$GENERATED_DIR/architecture-generated.xml" <<'EOF'
<section>
  <title>Architecture Overview</title>
  <para>
    StarForth is built on a clean, modular architecture designed for embedded systems and microkernel environments.
  </para>
  <section>
    <title>Core Components</title>
    <itemizedlist>
      <listitem><para><emphasis>VM Core</emphasis> - Virtual machine with 5MB memory space</para></listitem>
      <listitem><para><emphasis>Dictionary</emphasis> - Word lookup with 256-bucket hash optimization</para></listitem>
      <listitem><para><emphasis>Compiler</emphasis> - Threaded code compilation</para></listitem>
      <listitem><para><emphasis>Stack Management</emphasis> - Data and return stack with bounds checking</para></listitem>
      <listitem><para><emphasis>Block System</emphasis> - 1KB blocks for persistent storage</para></listitem>
    </itemizedlist>
  </section>
  <section>
    <title>Memory Layout</title>
    <para>
      The VM uses a 5MB unified memory space divided into dictionary area and user data/blocks.
    </para>
  </section>
</section>
EOF
else
    md_to_docbook "$DOCS_DIR/ARCHITECTURE.md" "$GENERATED_DIR/architecture-generated.xml" "Architecture"
fi

# Create FORTH-79 standard reference
echo "  Converting FORTH-79 standard..."
cat > "$GENERATED_DIR/forth79-generated.xml" <<'EOF'
<section>
  <title>FORTH-79 Standard</title>
  <para>
    StarForth implements the FORTH-79 standard. For complete standard details, see FORTH-79.txt in the project root.
  </para>
  <section>
    <title>Implemented Word Sets</title>
    <itemizedlist>
      <listitem><para>Stack Manipulation (DUP, DROP, SWAP, OVER, ROT, etc.)</para></listitem>
      <listitem><para>Arithmetic (+ - * / MOD, etc.)</para></listitem>
      <listitem><para>Comparison (&lt; &gt; = 0= etc.)</para></listitem>
      <listitem><para>Logical (AND OR XOR NOT)</para></listitem>
      <listitem><para>Control Flow (IF ELSE THEN, BEGIN UNTIL WHILE REPEAT, DO LOOP +LOOP)</para></listitem>
      <listitem><para>Defining Words (: ; CONSTANT VARIABLE CREATE DOES&gt;)</para></listitem>
      <listitem><para>Dictionary (FIND ' &gt;BODY IMMEDIATE)</para></listitem>
      <listitem><para>Memory (@ ! C@ C! ALLOT HERE)</para></listitem>
      <listitem><para>Compilation ([ ] LITERAL COMPILE)</para></listitem>
      <listitem><para>I/O (EMIT TYPE CR SPACE KEY)</para></listitem>
      <listitem><para>Return Stack (&gt;R R&gt; R@)</para></listitem>
      <listitem><para>Vocabulary System (VOCABULARY DEFINITIONS FORTH)</para></listitem>
      <listitem><para>Block System (BLOCK UPDATE FLUSH LIST LOAD)</para></listitem>
    </itemizedlist>
  </section>
</section>
EOF

# Create API reference section with link to Doxygen
echo "  Generating API reference..."
cat > "$GENERATED_DIR/api-reference-generated.xml" <<'EOF'
<section>
  <title>API Reference (Doxygen)</title>
  <para>
    Complete API documentation for StarForth internal functions and data structures is available
    in the Doxygen-generated documentation. The API reference includes detailed documentation for:
  </para>
  <itemizedlist>
    <listitem><para><emphasis>VM Core</emphasis> - Virtual machine initialization, execution, and state management</para></listitem>
    <listitem><para><emphasis>Stack Management</emphasis> - Data stack, return stack, control-flow stack operations</para></listitem>
    <listitem><para><emphasis>Dictionary</emphasis> - Word lookup, registration, and management</para></listitem>
    <listitem><para><emphasis>Memory System</emphasis> - VM memory access, bounds checking, block operations</para></listitem>
    <listitem><para><emphasis>Compiler</emphasis> - Word compilation, control flow, threaded code generation</para></listitem>
    <listitem><para><emphasis>I/O System</emphasis> - Input/output operations, TIB management</para></listitem>
    <listitem><para><emphasis>Profiler</emphasis> - Performance profiling and execution tracking</para></listitem>
    <listitem><para><emphasis>Word Modules</emphasis> - All 19 word category implementations</para></listitem>
  </itemizedlist>

  <section>
    <title>Accessing API Documentation</title>
    <para>
      To view the complete API documentation, open the Doxygen HTML output:
    </para>
    <programlisting>
# Generate API docs (if not already generated)
make docs

# Open in browser
xdg-open docs/api/html/index.html    # Linux
open docs/api/html/index.html        # macOS
start docs/api/html/index.html       # Windows
    </programlisting>
  </section>

  <section>
    <title>Key API Components</title>

    <section>
      <title>VM Initialization and Lifecycle</title>
      <itemizedlist>
        <listitem><para><literal>vm_init(VM *vm)</literal> - Initialize virtual machine</para></listitem>
        <listitem><para><literal>vm_reset(VM *vm)</literal> - Reset VM to initial state</para></listitem>
        <listitem><para><literal>vm_cleanup(VM *vm)</literal> - Clean up VM resources</para></listitem>
      </itemizedlist>
    </section>

    <section>
      <title>Stack Operations</title>
      <itemizedlist>
        <listitem><para><literal>vm_push(VM *vm, cell_t value)</literal> - Push to data stack</para></listitem>
        <listitem><para><literal>vm_pop(VM *vm)</literal> - Pop from data stack</para></listitem>
        <listitem><para><literal>vm_rpush(VM *vm, cell_t value)</literal> - Push to return stack</para></listitem>
        <listitem><para><literal>vm_rpop(VM *vm)</literal> - Pop from return stack</para></listitem>
      </itemizedlist>
    </section>

    <section>
      <title>Memory Access</title>
      <itemizedlist>
        <listitem><para><literal>vm_addr_ok(VM *vm, vaddr_t addr, size_t len)</literal> - Validate address range</para></listitem>
        <listitem><para><literal>vm_load_cell(VM *vm, vaddr_t addr)</literal> - Load cell from VM memory</para></listitem>
        <listitem><para><literal>vm_store_cell(VM *vm, vaddr_t addr, cell_t value)</literal> - Store cell to VM memory</para></listitem>
        <listitem><para><literal>vm_allot(VM *vm, size_t bytes)</literal> - Allocate VM memory</para></listitem>
      </itemizedlist>
    </section>

    <section>
      <title>Dictionary Operations</title>
      <itemizedlist>
        <listitem><para><literal>vm_find(VM *vm, const char *name, size_t len)</literal> - Find word in dictionary</para></listitem>
        <listitem><para><literal>vm_register_word(VM *vm, const char *name, word_func_t func, uint8_t flags)</literal> - Register new word</para></listitem>
        <listitem><para><literal>vm_execute_word(VM *vm, DictEntry *entry)</literal> - Execute dictionary entry</para></listitem>
      </itemizedlist>
    </section>
  </section>

  <section>
    <title>Building with API Documentation</title>
    <para>
      API documentation is automatically generated as part of the documentation build process:
    </para>
    <programlisting>
make docs        # Generate all documentation (includes API docs)
make docs-html   # Generate HTML API docs only
make docs-open   # Generate and open API docs in browser
    </programlisting>
  </section>

  <section>
    <title>Contributing API Documentation</title>
    <para>
      StarForth uses Doxygen-style comments for API documentation. See the Doxygen Style Guide
      appendix for formatting guidelines and examples.
    </para>
  </section>
</section>
EOF

# Create word reference
echo "  Generating word reference..."
cat > "$GENERATED_DIR/words-generated.xml" <<'EOF'
<section>
  <title>Complete Word Reference</title>
  <para>
    This chapter provides a complete reference of all words implemented in StarForth.
  </para>
  <section>
    <title>Generating Word List</title>
    <para>
      To get a complete list of available words, run:
    </para>
    <programlisting>
echo "WORDS" | ./build/starforth
    </programlisting>
  </section>
  <section>
    <title>Word Categories</title>
    <para>
      Words are organized into the following modules. See REQUIRED_FORTH-79_WORDS.txt for the complete required word set.
    </para>
  </section>
</section>
EOF

echo ""
echo "Building DocBook book..."
cp "$DOCS_DIR/starforth-book.xml" "$BUILD_DIR/"

# Generate HTML
echo "  Generating HTML..."
xsltproc \
    --xinclude \
    --output "$BUILD_DIR/starforth-manual.html" \
    /usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl \
    "$BUILD_DIR/starforth-book.xml" 2>/dev/null || {
    # Try alternative stylesheet path
    xsltproc \
        --xinclude \
        --output "$BUILD_DIR/starforth-manual.html" \
        http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl \
        "$BUILD_DIR/starforth-book.xml"
}

# Inject dark theme CSS
echo "  Applying dark theme CSS..."
if [ -f "$BUILD_DIR/starforth-manual.html" ]; then
    # Create CSS file if it doesn't exist
    if [ ! -f "$BUILD_DIR/starforth-dark.css" ]; then
        cat > "$BUILD_DIR/starforth-dark.css" <<'CSSEOF'
/* StarForth Manual - Dark/High-Contrast Theme */
/* Professional book-style layout optimized for readability */

:root {
    --bg-primary: #0a0a0a;
    --bg-secondary: #1a1a1a;
    --bg-tertiary: #252525;
    --bg-code: #1a1a1a;
    --text-primary: #e8e8e8;
    --text-secondary: #b8b8b8;
    --text-muted: #888888;
    --accent-primary: #00d4ff;
    --accent-secondary: #0099cc;
    --accent-success: #00ff88;
    --accent-warning: #ffcc00;
    --accent-error: #ff3366;
    --border-color: #333333;
    --shadow-color: rgba(0, 212, 255, 0.1);
}

* {
    box-sizing: border-box;
}

body {
    background-color: var(--bg-primary) !important;
    color: var(--text-primary) !important;
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    font-size: 16px;
    line-height: 1.7;
    max-width: 1200px;
    margin: 0 auto;
    padding: 2rem;
}

/* Title Page Styling */
.book > .titlepage {
    background: linear-gradient(135deg, var(--bg-secondary) 0%, var(--bg-primary) 100%);
    border: 2px solid var(--accent-primary);
    border-radius: 8px;
    padding: 3rem;
    margin-bottom: 3rem;
    box-shadow: 0 4px 20px var(--shadow-color);
}

h1.title {
    font-size: 3rem;
    text-align: center;
    color: var(--accent-success) !important;
    text-shadow: 0 0 20px rgba(0, 255, 136, 0.3);
    margin-bottom: 1rem;
    font-weight: 700;
    letter-spacing: 0.5px;
}

h2.subtitle {
    font-size: 1.5rem;
    text-align: center;
    color: var(--accent-primary) !important;
    margin-bottom: 2rem;
    font-weight: 400;
}

/* Author, version, copyright */
.titlepage .author,
.titlepage .releaseinfo,
.titlepage .copyright,
.titlepage .pubdate,
.titlepage .abstract {
    color: var(--text-secondary) !important;
    text-align: center;
}

.titlepage .abstract {
    background-color: var(--bg-tertiary);
    border-left: 4px solid var(--accent-primary);
    padding: 1.5rem;
    margin-top: 2rem;
    border-radius: 4px;
}

.abstract .title {
    color: var(--accent-primary) !important;
    font-size: 1.2rem;
    margin-bottom: 1rem;
}

/* Horizontal rules */
hr {
    border: none;
    border-top: 2px solid var(--border-color);
    margin: 2rem 0;
}

/* Headings */
h1, h2, h3, h4, h5, h6 {
    color: var(--accent-primary) !important;
    font-weight: 600;
    margin-top: 2rem;
    margin-bottom: 1rem;
    line-height: 1.3;
}

h1 { font-size: 2.5rem; border-bottom: 3px solid var(--accent-primary); padding-bottom: 0.5rem; }
h2 { font-size: 2rem; border-bottom: 2px solid var(--accent-secondary); padding-bottom: 0.4rem; }
h3 { font-size: 1.6rem; color: var(--accent-success) !important; }
h4 { font-size: 1.3rem; color: var(--text-primary) !important; }
h5 { font-size: 1.1rem; color: var(--text-secondary) !important; }
h6 { font-size: 1rem; color: var(--text-muted) !important; }

/* Table of Contents */
.toc {
    background-color: var(--bg-secondary);
    border: 2px solid var(--border-color);
    border-radius: 8px;
    padding: 2rem;
    margin: 2rem 0;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.3);
}

.toc > p > b {
    font-size: 1.8rem;
    color: var(--accent-success) !important;
    display: block;
    margin-bottom: 1.5rem;
    text-align: center;
}

.toc dl {
    margin: 0;
    padding: 0;
}

.toc dt {
    margin-top: 1rem;
    font-weight: 600;
}

.toc dd {
    margin-left: 1.5rem;
}

.toc a {
    color: var(--accent-primary) !important;
    text-decoration: none;
    transition: all 0.2s ease;
    display: inline-block;
    padding: 0.25rem 0;
}

.toc a:hover {
    color: var(--accent-success) !important;
    text-shadow: 0 0 8px var(--accent-success);
    transform: translateX(4px);
}

/* Part/Chapter titles */
.part > .titlepage h1,
.chapter > .titlepage h2 {
    background: linear-gradient(90deg, var(--accent-primary) 0%, var(--accent-success) 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
}

/* Links */
a {
    color: var(--accent-primary) !important;
    text-decoration: none;
    border-bottom: 1px solid transparent;
    transition: all 0.2s ease;
}

a:hover {
    color: var(--accent-success) !important;
    border-bottom: 1px solid var(--accent-success);
    text-shadow: 0 0 8px var(--accent-success);
}

a:visited {
    color: var(--accent-secondary) !important;
}

/* Paragraphs */
p {
    margin-bottom: 1rem;
    color: var(--text-primary);
}

/* Code and literals */
code, .literal, tt {
    background-color: var(--bg-code) !important;
    color: var(--accent-warning) !important;
    padding: 0.2rem 0.5rem;
    border-radius: 4px;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 0.9em;
    border: 1px solid var(--border-color);
}

pre {
    background-color: var(--bg-code) !important;
    color: var(--text-primary) !important;
    padding: 1.5rem;
    border-radius: 6px;
    border-left: 4px solid var(--accent-primary);
    overflow-x: auto;
    margin: 1.5rem 0;
    line-height: 1.5;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.4);
}

pre code {
    background: none !important;
    padding: 0;
    border: none;
    color: var(--text-primary) !important;
}

/* Lists */
ul, ol {
    margin: 1rem 0;
    padding-left: 2rem;
}

li {
    margin-bottom: 0.5rem;
    color: var(--text-primary);
}

ul li::marker {
    color: var(--accent-primary);
}

ol li::marker {
    color: var(--accent-success);
    font-weight: 600;
}

dt {
    font-weight: 600;
    color: var(--accent-primary) !important;
    margin-top: 1rem;
}

dd {
    margin-left: 2rem;
    margin-bottom: 1rem;
    color: var(--text-primary);
}

/* Tables */
table {
    width: 100%;
    border-collapse: collapse;
    margin: 1.5rem 0;
    background-color: var(--bg-secondary);
    border-radius: 6px;
    overflow: hidden;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
}

thead {
    background-color: var(--bg-tertiary);
}

th {
    padding: 1rem;
    text-align: left;
    color: var(--accent-success) !important;
    font-weight: 600;
    border-bottom: 2px solid var(--accent-primary);
}

td {
    padding: 0.8rem 1rem;
    border-bottom: 1px solid var(--border-color);
    color: var(--text-primary);
}

tr:last-child td {
    border-bottom: none;
}

tr:hover {
    background-color: var(--bg-tertiary);
}

/* Blockquotes */
blockquote {
    background-color: var(--bg-secondary);
    border-left: 4px solid var(--accent-success);
    padding: 1rem 1.5rem;
    margin: 1.5rem 0;
    border-radius: 4px;
    font-style: italic;
    color: var(--text-secondary);
}

/* Emphasized text */
em, i {
    color: var(--accent-warning);
    font-style: italic;
}

strong, b {
    color: var(--accent-success);
    font-weight: 600;
}

/* Sections */
.section, .sect1, .sect2, .chapter, .appendix {
    margin-bottom: 2rem;
}

/* Navigation */
.navheader, .navfooter {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 6px;
    padding: 1rem;
    margin: 2rem 0;
}

/* Index */
.index {
    column-count: 2;
    column-gap: 2rem;
}

@media (max-width: 768px) {
    .index {
        column-count: 1;
    }
}

/* Print styles */
@media print {
    body {
        background: white !important;
        color: black !important;
        font-size: 11pt;
    }

    .toc, .navheader, .navfooter {
        border-color: #ccc !important;
    }

    a {
        color: #0066cc !important;
        text-decoration: underline;
    }

    code, pre {
        background: #f5f5f5 !important;
        color: black !important;
        border-color: #ccc !important;
    }
}

/* Scrollbar styling */
::-webkit-scrollbar {
    width: 12px;
    height: 12px;
}

::-webkit-scrollbar-track {
    background: var(--bg-primary);
}

::-webkit-scrollbar-thumb {
    background: var(--accent-primary);
    border-radius: 6px;
}

::-webkit-scrollbar-thumb:hover {
    background: var(--accent-success);
}

/* Selection */
::selection {
    background-color: var(--accent-primary);
    color: var(--bg-primary);
}

::-moz-selection {
    background-color: var(--accent-primary);
    color: var(--bg-primary);
}
CSSEOF
    fi

    # Inject CSS link into HTML
    sed -i 's|</head>|<link rel="stylesheet" href="starforth-dark.css"></head>|' "$BUILD_DIR/starforth-manual.html"
    # Remove inline bgcolor/colors from body tag
    sed -i 's|<body[^>]*>|<body>|' "$BUILD_DIR/starforth-manual.html"
    echo -e "${GREEN}✓${NC} Dark theme applied"
fi

# Generate PDF if fop is available
if [ $HAS_FOP -eq 1 ]; then
    echo "  Generating PDF..."

    # First create FO file with custom parameters for better PDF output
    xsltproc \
        --xinclude \
        --stringparam fop1.extensions 1 \
        --stringparam paper.type A4 \
        --stringparam body.start.indent 0pt \
        --stringparam title.margin.left 0pt \
        --output "$BUILD_DIR/starforth-manual.fo" \
        /usr/share/xml/docbook/stylesheet/docbook-xsl/fo/docbook.xsl \
        "$BUILD_DIR/starforth-book.xml" 2>/dev/null || {
        xsltproc \
            --xinclude \
            --stringparam fop1.extensions 1 \
            --output "$BUILD_DIR/starforth-manual.fo" \
            http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl \
            "$BUILD_DIR/starforth-book.xml" 2>/dev/null
    }

    # Convert FO to PDF (capture errors but don't fail)
    if fop -fo "$BUILD_DIR/starforth-manual.fo" -pdf "$BUILD_DIR/starforth-manual.pdf" > /dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} PDF generated successfully"
    else
        echo -e "${YELLOW}⚠${NC}  PDF generation had warnings (PDF may still be usable)"
        # Try again without redirecting stderr to see if it actually succeeded
        if [ -f "$BUILD_DIR/starforth-manual.pdf" ] && [ -s "$BUILD_DIR/starforth-manual.pdf" ]; then
            echo -e "${GREEN}✓${NC} PDF file was created despite warnings"
        fi
    fi
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo -e "${GREEN}✓ Documentation build complete!${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Output files:"
echo "  HTML: $BUILD_DIR/starforth-manual.html"
if [ $HAS_FOP -eq 1 ] && [ -f "$BUILD_DIR/starforth-manual.pdf" ]; then
    echo "  PDF:  $BUILD_DIR/starforth-manual.pdf"
fi
echo "  Build artifacts: $BUILD_DIR/"
echo ""
