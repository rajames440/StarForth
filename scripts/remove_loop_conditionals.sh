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

# L8 FINAL INTEGRATION: Remove all ENABLE_LOOP_X conditional compilation directives
# This script removes #if ENABLE_LOOP_X/#endif blocks while preserving the code inside

set -e

echo "Removing ENABLE_LOOP conditional compilation directives..."

# Files to process
FILES=(
    "src/vm.c"
    "src/inference_engine.c"
    "src/doe_metrics.c"
)

for file in "${FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "Skipping $file (not found)"
        continue
    fi

    echo "Processing $file..."

    # Create backup
    cp "$file" "$file.bak"

    # Remove ENABLE_LOOP_X conditionals using perl for multi-line matching
    # Strategy: Remove #if ENABLE_LOOP_X lines and their corresponding #endif lines
    # Keep the code between them

    perl -i -pe '
        # Remove #if ENABLE_LOOP_X lines
        s/^#if\s+ENABLE_LOOP_[0-9]_\w+\s*$/\/\* L8 FINAL INTEGRATION: Loop always-on \*\//;

        # Remove #endif comments for ENABLE_LOOP_X
        s/^#endif\s*\/\*\s*ENABLE_LOOP_[0-9]_\w+\s*\*\/\s*$/\/\* End always-on loop \*\//;
    ' "$file"

    echo "  ✓ Processed $file"
done

echo "✓ All files processed. Backups saved as .bak files"