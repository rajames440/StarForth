#!/bin/bash
#
#                                  ***   StarForth   ***
#
#  asciidoc-to-latex.sh- FORTH-79 Standard and ANSI C99 ONLY
#  Modified by - rajames
#  Last modified - 2025-10-26T16:05:12.152-04
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
#  /home/rajames/CLionProjects/StarForth/scripts/asciidoc-to-latex.sh
#

#
# Convert AsciiDoc files to LaTeX
#
# This script finds all .adoc files in the project and converts them to LaTeX
# Output is organized in ./docs/latex/ maintaining the directory structure
#
# Usage: ./scripts/asciidoc-to-latex.sh
#

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/docs/latex"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
print_header() {
    echo -e "${GREEN}=== $1 ===${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

print_success() {
    echo -e "${GREEN}✓${NC}  $1"
}

print_error() {
    echo -e "${RED}✗${NC}  $1" >&2
}

# Check prerequisites
check_prerequisites() {
    print_header "Checking prerequisites"

    if ! command -v asciidoctor &> /dev/null; then
        print_error "asciidoctor not found. Install with:"
        echo "  gem install asciidoctor asciidoctor-latex"
        exit 1
    fi
    print_success "asciidoctor is available"
}

# Convert a single AsciiDoc file to LaTeX
convert_file() {
    local adoc_file=$1
    local output_file=$2

    mkdir -p "$(dirname "$output_file")"

    # Use asciidoctor with LaTeX backend
    asciidoctor -b latex -o "$output_file" "$adoc_file" 2>/dev/null || {
        print_error "Failed to convert: $adoc_file"
        return 1
    }

    print_success "Converted: $adoc_file → $output_file"
    return 0
}

# Main conversion loop
main() {
    print_header "AsciiDoc to LaTeX Converter"
    echo ""

    check_prerequisites
    echo ""

    # Clean output directory
    print_info "Preparing output directory: $OUTPUT_DIR"
    rm -rf "$OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
    echo ""

    print_header "Converting AsciiDoc files to LaTeX"

    local converted_count=0
    local failed_count=0

    # Find all .adoc files (excluding .git and node_modules)
    while IFS= read -r adoc_file; do
        # Calculate relative path for output
        local rel_path="${adoc_file#./}"
        local output_file="$OUTPUT_DIR/${rel_path%.adoc}.tex"

        # Convert the file
        if convert_file "$adoc_file" "$output_file"; then
            ((converted_count++))
        else
            ((failed_count++))
        fi
    done < <(find . -name "*.adoc" -type f ! -path "*/.git/*" ! -path "*/node_modules/*" 2>/dev/null | sort)

    echo ""
    print_header "Conversion Summary"

    if [ $converted_count -gt 0 ]; then
        print_success "Converted $converted_count file(s) to LaTeX"
    fi

    if [ $failed_count -gt 0 ]; then
        print_error "Failed to convert $failed_count file(s)"
        exit 1
    fi

    echo ""
    print_success "LaTeX output ready: $OUTPUT_DIR"
    echo ""
    echo "📝 Generated LaTeX files:"
    find "$OUTPUT_DIR" -name "*.tex" -type f | head -10
    if [ $(find "$OUTPUT_DIR" -name "*.tex" -type f | wc -l) -gt 10 ]; then
        echo "... and $(( $(find "$OUTPUT_DIR" -name "*.tex" -type f | wc -l) - 10 )) more"
    fi
}

main "$@"