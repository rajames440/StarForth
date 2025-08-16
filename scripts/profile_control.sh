#!/usr/bin/env bash
#
#
#                                 ***   StarForth   ***
#  profile_control.sh - FORTH-79 Standard and ANSI C99 ONLY
# Last modified - 8/15/25, 10:24 AM
#  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
#
# This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
#  To the extent possible under law, the author(s) have dedicated all copyright and related
#  and neighboring rights to this software to the public domain worldwide.
#  This software is distributed without any warranty.
#
#  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
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
