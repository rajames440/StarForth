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
# Convert AsciiDoc files to src
#
# This script finds all .adoc files in the project and converts them to src
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

# Convert a single AsciiDoc file to src
convert_file() {
    local adoc_file=$1
    local output_file=$2

    mkdir -p "$(dirname "$output_file")"

    # Use asciidoctor with src backend
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