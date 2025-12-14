#!/usr/bin/env bash
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

# profile_control.sh - Target Control Words performance bottleneck

set -euo pipefail

EXE="./build/starforth"
echo "[CLEAN]"; make clean
echo "[BUILD for control words profiling]"
make CFLAGS="-std=c99 -O3 -g -fno-omit-frame-pointer -DSTARFORTH_PERFORMANCE -DNDEBUG -march=native -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include -DSTRICT_PTR=1" LDFLAGS=""

[[ -x "$EXE" ]] || { echo "ERROR: $EXE missing/not executable"; exit 2; }

echo "[RECORD] Control Words perf profile..."
rm -f perf.data CONTROL_REPORT.txt

# Create a minimal FORTH program that hammers control flow
cat > control_test.forth << 'EOF'
: HAMMER_IF 1000 0 DO I 2 MOD IF 42 ELSE 24 THEN DROP LOOP ;
: HAMMER_LOOPS 100 0 DO 10 0 DO I J + DROP LOOP LOOP ;
: HAMMER_NESTED 50 0 DO I 3 MOD DUP IF DUP 2 = IF 99 ELSE 88 THEN ELSE 77 THEN DROP LOOP ;

HAMMER_IF HAMMER_IF HAMMER_IF HAMMER_IF HAMMER_IF
HAMMER_LOOPS HAMMER_LOOPS HAMMER_LOOPS
HAMMER_NESTED HAMMER_NESTED HAMMER_NESTED HAMMER_NESTED
EOF

echo "Running targeted control flow benchmark..."
# Use --log-none and timeout to avoid cleanup segfault
timeout 10s perf record -F 999 -g --call-graph dwarf -e cycles:u -- "$EXE" --log-none < control_test.forth 2>/dev/null || true

echo "[REPORT → CONTROL_REPORT.txt]"
perf report --stdio --no-children --percent-limit 1.0 --sort overhead,symbol,dso > CONTROL_REPORT.txt
echo "DONE → CONTROL_REPORT.txt"; head -50 CONTROL_REPORT.txt || true

rm -f control_test.forth
