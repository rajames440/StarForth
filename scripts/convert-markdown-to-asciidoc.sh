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

################################################################################
# StarForth Markdown to AsciiDoc Converter
#
# Automatically converts markdown files to AsciiDoc and intelligently
# categorizes them using Claude AI
#
# Features:
#  - Scans repo for markdown files (excluding specified ones)
#  - Converts .md to .adoc using pandoc
#  - Uses Claude to determine best category
#  - Moves files to appropriate docs subdirectory
#  - Adds TOC attributes and back-links
#  - Stages changes and deletes originals
#
# Usage: ./scripts/convert-markdown-to-asciidoc.sh
#
# Environment:
#  - CLAUDE_API_KEY: Optional, for intelligent categorization
#  - PRE_COMMIT_DRY_RUN: Set to 1 for preview mode
################################################################################

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
EXCLUDED_FILES=(
    "README.md"
    "CONTRIBUTE.md"
    "SECURITY.md"
    "CODE_OF_CONDUCT.md"
    "NOTICE.md"
)

DOCS_DIR="docs/src"
CATEGORIES=(
    "getting-started"
    "build-and-tooling"
    "platform-integration"
    "architecture-internals"
    "performance-profiling"
    "testing-quality"
    "development"
    "governance"
    "internal"
)

DRY_RUN=${PRE_COMMIT_DRY_RUN:-0}

# Logging functions
log_info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

log_success() {
    echo -e "${GREEN}✓${NC}  $1"
}

log_warn() {
    echo -e "${YELLOW}⚠${NC}  $1"
}

log_error() {
    echo -e "${RED}✗${NC}  $1" >&2
}

# Check prerequisites
check_prerequisites() {
    if ! command -v pandoc &> /dev/null; then
        log_error "pandoc not found. Install with: sudo apt-get install pandoc"
        return 1
    fi

    if [ -z "$CLAUDE_API_KEY" ]; then
        log_warn "CLAUDE_API_KEY not set. Skipping intelligent categorization."
        return 1
    fi

    return 0
}

# Check if file is in exclude list
is_excluded() {
    local file=$1
    local basename=$(basename "$file")

    # Exclude all dot files and directories
    if [[ "$basename" == .* ]]; then
        return 0
    fi

    for excluded in "${EXCLUDED_FILES[@]}"; do
        if [[ "$basename" == "$excluded" ]]; then
            return 0
        fi
    done

    if [[ "$file" == *"StarForth-Governance"* ]]; then
        return 0
    fi

    return 1
}

# Convert markdown to AsciiDoc
convert_to_asciidoc() {
    local md_file=$1
    local adoc_file="${md_file%.md}.adoc"

    if [ "$DRY_RUN" = "1" ]; then
        log_info "[DRY RUN] Would convert: $md_file → $adoc_file"
        return 0
    fi

    pandoc -f markdown -t asciidoc "$md_file" -o "$adoc_file"
    return $?
}

# Use Claude to categorize document
categorize_document() {
    local adoc_file=$1
    local filename=$(basename "$adoc_file")

    if [ "$DRY_RUN" = "1" ]; then
        log_info "[DRY RUN] Would send to Claude for categorization: $filename"
        echo "docs" # Default category for dry-run
        return 0
    fi

    local content=$(head -c 2000 "$adoc_file") # First 2000 chars for context

    local response=$(curl -s -X POST "https://api.anthropic.com/v1/messages" \
        -H "Content-Type: application/json" \
        -H "x-api-key: $CLAUDE_API_KEY" \
        -H "anthropic-version: 2023-06-01" \
        -d "{
            \"model\": \"claude-haiku-4-5-20251001\",
            \"max_tokens\": 50,
            \"system\": \"You are a documentation categorization expert. Given a document excerpt, respond with ONLY the category name (no explanation). Choose from: getting-started, build-and-tooling, platform-integration, architecture-internals, performance-profiling, testing-quality, development, governance, internal. If unsure, respond with 'docs'.\",
            \"messages\": [
                {
                    \"role\": \"user\",
                    \"content\": \"Categorize this document based on its content. Filename: $filename\n\nContent excerpt:\n$content\"
                }
            ]
        }")

    # Extract category from response
    local category=$(echo "$response" | grep -o '"text":"[^"]*"' | sed 's/"text":"\([^"]*\)"/\1/' | tr -d ' ' | head -1)

    # Validate category exists
    local valid=0
    for cat in "${CATEGORIES[@]}"; do
        if [[ "$category" == "$cat" ]]; then
            valid=1
            break
        fi
    done

    if [ $valid -eq 0 ]; then
        category="docs"
    fi

    echo "$category"
}

# Add TOC attributes to AsciiDoc file
add_toc_attributes() {
    local adoc_file=$1

    if [ "$DRY_RUN" = "1" ]; then
        log_info "[DRY RUN] Would add TOC attributes to: $adoc_file"
        return 0
    fi

    # Get first line (title)
    local first_line=$(head -n 1 "$adoc_file")

    # Create temp file with title + TOC attributes + rest
    {
        echo "$first_line"
        echo ":toc: left"
        echo ":toc-title: Contents"
        echo ":toclevels: 3"
        echo ""
        tail -n +2 "$adoc_file"
    } > "$adoc_file.tmp"

    mv "$adoc_file.tmp" "$adoc_file"
}

# Add back-to-README link
add_back_link() {
    local adoc_file=$1
    local relative_path=$2

    if [ "$DRY_RUN" = "1" ]; then
        log_info "[DRY RUN] Would add back-link to: $adoc_file"
        return 0
    fi

    # Find line number after :toclevels:
    local line_num=$(grep -n "^:toclevels:" "$adoc_file" | head -1 | cut -d: -f1)

    if [ -n "$line_num" ]; then
        sed -i "${line_num}a\\
xref:${relative_path}[← Back to Documentation Index]\n" "$adoc_file"
    fi
}

# Move file to appropriate directory
move_to_category() {
    local adoc_file=$1
    local category=$2

    if [ "$DRY_RUN" = "1" ]; then
        if [ "$category" = "docs" ]; then
            log_info "[DRY RUN] Would move: $adoc_file → $DOCS_DIR/"
        else
            log_info "[DRY RUN] Would move: $adoc_file → $DOCS_DIR/$category/"
        fi
        return 0
    fi

    local basename=$(basename "$adoc_file")
    local target
    local rel_path

    # If category is "docs" (fallback/default), put directly in docs/ root
    if [ "$category" = "docs" ]; then
        mkdir -p "$DOCS_DIR"
        target="$DOCS_DIR/$basename"
        rel_path="./README.adoc"  # Same directory
    else
        # Otherwise create category subdirectory
        mkdir -p "$DOCS_DIR/$category"
        target="$DOCS_DIR/$category/$basename"
        rel_path="../README.adoc"  # One level up
    fi

    mv "$adoc_file" "$target"
    add_back_link "$target" "$rel_path"

    echo "$target"
}

# Main processing loop
main() {
    log_info "Scanning for markdown files to convert..."

    local api_available=0
    check_prerequisites && api_available=1

    if [ $api_available -eq 0 ]; then
        log_warn "Claude API not available - will move files to ./docs/ as fallback"
        echo ""
    fi

    local converted_count=0
    local failed_count=0

    # Find all markdown files
    while IFS= read -r md_file; do
        if is_excluded "$md_file"; then
            log_info "Skipping excluded file: $md_file"
            continue
        fi

        log_info "Processing: $md_file"

        # Convert to AsciiDoc
        if ! convert_to_asciidoc "$md_file"; then
            log_error "Failed to convert: $md_file"
            ((failed_count++))
            continue
        fi

        local adoc_file="${md_file%.md}.adoc"

        # Add TOC attributes
        add_toc_attributes "$adoc_file"

        # Categorize using Claude (if available), otherwise use docs/ as safe default
        local category="docs"
        if [ $api_available -eq 1 ]; then
            local claude_category=$(categorize_document "$adoc_file" 2>/dev/null)
            if [ -n "$claude_category" ] && [ "$claude_category" != "docs" ]; then
                category="$claude_category"
                log_success "Categorized as: $category"
            else
                log_warn "Claude categorization failed or returned default, using: docs"
                category="docs"
            fi
        else
            log_warn "Claude API unavailable - using fallback category: docs"
            category="docs"
        fi

        # Move to appropriate directory (ensures file is never lost)
        local final_path=$(move_to_category "$adoc_file" "$category")
        log_success "Moved to: $final_path"

        # Stage the new AsciiDoc file (will be added if this is run from pre-commit)
        if [ -n "$(git rev-parse --git-dir 2>/dev/null)" ]; then
            git add "$final_path" 2>/dev/null || true
        fi

        # Delete original markdown
        if [ "$DRY_RUN" != "1" ]; then
            rm "$md_file"
            log_success "Deleted: $md_file"
        fi

        ((converted_count++))

    done < <(find . -name "*.md" -type f ! -path "*/StarForth-Governance/*" ! -path "*/.git/*" 2>/dev/null)

    # Summary
    echo ""
    if [ "$DRY_RUN" = "1" ]; then
        log_info "DRY RUN: No changes were made"
    fi

    if [ $converted_count -gt 0 ]; then
        log_success "Converted $converted_count markdown file(s) to AsciiDoc"
    fi

    if [ $failed_count -gt 0 ]; then
        log_warn "Failed to convert $failed_count file(s) - they were NOT deleted"
    fi

    return $failed_count
}

main "$@"