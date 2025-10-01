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
