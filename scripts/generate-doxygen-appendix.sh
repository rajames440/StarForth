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

#
# Generate Doxygen API documentation as AsciiDoc appendix files
#
# This script:
# 1. Runs Doxygen to generate XML output (temporary in /tmp only)
# 2. Converts Doxygen XML to individual AsciiDoc files (one per source file)
# 3. Organizes output into docs/src/appendix/ (flat structure)
# 4. Creates an index file for all generated documentation
#
# Usage: ./scripts/generate-doxygen-appendix.sh
#
# rajames says : read the specification above and implement it.
# The existing stuff below is wrong!

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DOXYFILE="$PROJECT_ROOT/Doxyfile"
DOXYGEN_OUTPUT="/tmp/starforth-doxygen-$$"
DOXYGEN_XML="$DOXYGEN_OUTPUT/xml"
DOCS_APPENDIX="$PROJECT_ROOT/docs/src/appendix"
CONVERSION_SCRIPT="$SCRIPT_DIR/doxygen-xml-to-asciidoc.py"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Utility functions
print_header() {
    echo -e "${GREEN}=== $1 ===${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}" >&2
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Check prerequisites
check_prerequisites() {
    print_header "Checking prerequisites"

    if [ ! -f "$DOXYFILE" ]; then
        print_error "Doxyfile not found at $DOXYFILE"
        exit 1
    fi
    print_success "Doxyfile found"

    if ! command -v doxygen &> /dev/null; then
        print_error "doxygen not found in PATH"
        exit 1
    fi
    print_success "doxygen is available"

    if [ ! -f "$CONVERSION_SCRIPT" ]; then
        print_error "Conversion script not found at $CONVERSION_SCRIPT"
        exit 1
    fi
    print_success "Conversion script found"

    if ! command -v python3 &> /dev/null; then
        print_error "python3 not found in PATH"
        exit 1
    fi
    print_success "python3 is available"
}

# Generate Doxygen XML (temporary, in /tmp)
generate_doxygen() {
    print_header "Generating Doxygen XML output (temporary)"

    # Clean previous output
    if [ -d "$DOXYGEN_OUTPUT" ]; then
        print_info "Cleaning previous Doxygen output..."
        rm -rf "$DOXYGEN_OUTPUT"
    fi

    print_info "Running doxygen to /tmp..."
    mkdir -p "$DOXYGEN_OUTPUT"
    cd "$PROJECT_ROOT"

    # Create temp Doxyfile with OUTPUT_DIRECTORY override
    TEMP_DOXYFILE="/tmp/Doxyfile.$$"
    cp "$DOXYFILE" "$TEMP_DOXYFILE"

    # Replace OUTPUT_DIRECTORY to point to /tmp
    sed -i "s|^OUTPUT_DIRECTORY.*|OUTPUT_DIRECTORY = $DOXYGEN_OUTPUT|g" "$TEMP_DOXYFILE"

    # Run doxygen with temp config
    doxygen "$TEMP_DOXYFILE" > /dev/null 2>&1 || {
        print_error "Doxygen generation failed"
        rm -f "$TEMP_DOXYFILE"
        exit 1
    }
    rm -f "$TEMP_DOXYFILE"

    # Clean up docs/api if Doxygen created it anyway
    # We only want output in /tmp and docs/src/appendix
    if [ -d "$PROJECT_ROOT/docs/api" ]; then
        print_info "Cleaning up docs/api (artifact from Doxygen)..."
        rm -rf "$PROJECT_ROOT/docs/api"
    fi

    if [ ! -d "$DOXYGEN_XML" ]; then
        print_error "Doxygen XML output directory not created"
        exit 1
    fi

    if [ ! -f "$DOXYGEN_XML/index.xml" ]; then
        print_error "Doxygen index.xml not found"
        exit 1
    fi

    print_success "Doxygen XML generated to $DOXYGEN_OUTPUT"
}

# Convert Doxygen XML to AsciiDoc (individual files per source)
convert_to_asciidoc() {
    # Run conversion in a subshell to capture output separately
    (
        print_header "Converting Doxygen XML to AsciiDoc"
        print_info "Creating individual files per source..."
        python3 "$CONVERSION_SCRIPT" "$DOXYGEN_XML" "$DOCS_APPENDIX" || {
            print_error "Conversion failed"
            exit 1
        }
        print_success "Conversion completed"
    ) >&2  # Send all output to stderr, not stdout
}

# Main execution
main() {
    print_header "StarForth Doxygen Appendix Generator (Flat Structure)"

    check_prerequisites
    generate_doxygen
    convert_to_asciidoc

    # Cleanup temporary Doxygen output
    print_info "Cleaning up temporary Doxygen files..."
    rm -rf "$DOXYGEN_OUTPUT"

    # Print summary
    print_header "Generation Complete"
    echo ""
    echo "Generated appendix files in: $DOCS_APPENDIX"
    echo ""
    echo "Directory structure:"
    if [ -d "$DOCS_APPENDIX" ]; then
        find "$DOCS_APPENDIX" -type f -name "*.adoc" | wc -l | xargs echo "  Total .adoc files:"
        echo ""
        tree "$DOCS_APPENDIX" 2>/dev/null || find "$DOCS_APPENDIX" -type f -name "*.adoc" | head -20
    fi
    echo ""
    print_success "API documentation appendix ready!"
}

# Run main function
main "$@"