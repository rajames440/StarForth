#!/bin/bash
#
#                                  ***   StarForth   ***
#
#  convert-doxygen-docbook.sh- FORTH-79 Standard and ANSI C99 ONLY
#  Modified by - rajames
#  Last modified - 2025-10-23T10:55:25.286-04
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
#  /home/rajames/CLionProjects/StarForth/scripts/convert-doxygen-docbook.sh
#

#
# convert-doxygen-docbook.sh - Convert Doxygen DocBook 5.0 to DocBook 4.5
# and integrate into StarForth book
#

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <doxygen-docbook-dir> <output-file>"
    exit 1
fi

DOXY_DIR="$1"
OUTPUT="$2"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

if [ ! -f "$DOXY_DIR/index.xml" ]; then
    echo -e "${RED}Error: Doxygen DocBook index.xml not found in $DOXY_DIR${NC}"
    exit 1
fi

echo "Converting Doxygen DocBook 5.0 to DocBook 4.5..."

# Create a temporary combined file from Doxygen's index
TEMP_COMBINED="$OUTPUT.temp_combined.xml"
TEMP_CONVERTED="$OUTPUT.temp_converted.xml"

# Use xsltproc with XInclude processing to resolve all includes
echo "  Resolving XIncludes..."
xmllint --xinclude --noent "$DOXY_DIR/index.xml" > "$TEMP_COMBINED" 2>/dev/null || {
    echo -e "${YELLOW}  Warning: xmllint failed, trying xsltproc...${NC}"

    # Try xsltproc with identity transform + xinclude
    xsltproc --xinclude --output "$TEMP_COMBINED" - "$DOXY_DIR/index.xml" 2>/dev/null <<'XSLT' || {
<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/xlink/Transform">
  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>
</xsl:stylesheet>
XSLT
        echo -e "${YELLOW}  Warning: XInclude resolution failed, using index as-is${NC}"
        cp "$DOXY_DIR/index.xml" "$TEMP_COMBINED"
    }
}

# Convert DocBook 5.0 to DocBook 4.5 using pandoc
echo "  Converting DocBook 5.0 → DocBook 4.5..."
# Pandoc will output sect1/sect2 elements without a root element
pandoc -f docbook -t docbook4 \
    --wrap=preserve \
    "$TEMP_COMBINED" 2>/dev/null | \
    sed 's/ id="[^"]*"//g' | \
    sed 's/ xml:id="[^"]*"//g' | \
    sed 's/ xml:lang="[^"]*"//g' > "$OUTPUT"

# Check if output is non-empty
if [ ! -s "$OUTPUT" ]; then
    echo -e "${YELLOW}  Warning: pandoc produced no output, trying alternative...${NC}"

    # Alternative: Use sed to downgrade namespace and extract content
    sed 's|xmlns="http://docbook.org/ns/docbook"|xmlns="http://www.oasis-open.org/docbook/xml/4.5"|g' "$TEMP_COMBINED" | \
    sed 's|version="5.0"|version="4.5"|g' | \
    sed 's|xmlns:xlink="http://www.w3.org/1999/xlink"||g' | \
    sed 's|xlink:||g' | \
    sed -n '/<\/info>/,$p' | \
    sed '1d' | \
    grep -v '^<book' | \
    grep -v '^</book>' | \
    grep -v '^<index/>' | \
    sed 's/ id="[^"]*"//g' | \
    sed 's/ xml:id="[^"]*"//g' | \
    sed 's/ xml:lang="[^"]*"//g' > "$OUTPUT"
fi

# Cleanup
rm -f "$TEMP_COMBINED" "$TEMP_CONVERTED"

echo -e "${GREEN}✓${NC} Conversion complete: $OUTPUT"