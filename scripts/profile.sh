#!/usr/bin/env bash
#
#
#                                 ***   StarForth   ***
#  profile.sh - FORTH-79 Standard and ANSI C99 ONLY
# Last modified - 8/15/25, 8:03 AM
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

# StarForth perf → REPORT.txt (symbols, no script args)
set -euo pipefail

EXE="./build/starforth"       # executable ONLY
ARGS=(--log-error --benchmark 5000)   # Use more iterations and less logging for better profiling

echo "[CLEAN]"; make clean
echo "[BUILD performance w/ symbols for profiling]"
make CFLAGS="-std=c99 -O3 -g -fno-omit-frame-pointer -DSTARFORTH_PERFORMANCE -DNDEBUG -march=native -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include -DSTRICT_PTR=1" LDFLAGS=""

[[ -x "$EXE" ]] || { echo "ERROR: $EXE missing/not executable"; exit 2; }

echo "[RECORD] perf…"
rm -f perf.data REPORT.txt
perf record -F 999 -g --call-graph dwarf -e cycles:u -- "$EXE" "${ARGS[@]}"

echo "[REPORT → REPORT.txt]"
perf report --stdio --no-children --percent-limit 0 --sort overhead,symbol,dso > REPORT.txt
echo "DONE → REPORT.txt"; head -40 REPORT.txt || true
