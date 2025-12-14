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

# StarForth perf → REPORT.txt (symbols, no script args)
set -euo pipefail

EXE="./build/starforth"       # executable ONLY
# Target just Control Words module for detailed profiling
ARGS=(--log-error --run-tests)   # We need to run specific control word tests

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
