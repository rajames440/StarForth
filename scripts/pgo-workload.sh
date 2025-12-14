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
# pgo-workload.sh - Comprehensive profiling workload for PGO
# Exercises all major code paths to generate realistic profile data
#

set -e

TARGET="${1:-./build/starforth}"

if [ ! -x "$TARGET" ]; then
    echo "Error: $TARGET not found or not executable"
    exit 1
fi

echo "Running comprehensive PGO profiling workload..."
echo "Target: $TARGET"
echo ""

# 1. Run all unit tests (exercises all word implementations)
echo "1/7 Running unit tests..."
$TARGET --run-tests --log-none --fail-fast || echo "  (some tests may fail in instrumented build)"

# 2. Run stress tests (exercises deep call stacks, edge cases)
echo "2/7 Running stress tests..."
$TARGET --stress-tests --log-none || echo "  (some stress tests may fail in instrumented build)"

# 3. Run integration tests (exercises complete programs)
echo "3/7 Running integration tests..."
$TARGET --integration --log-none || echo "  (some integration tests may fail in instrumented build)"

# 4. Run benchmarks (exercises hot paths at scale)
echo "4/7 Running benchmarks (5000 iterations)..."
$TARGET --benchmark 5000 --log-none || true

# 5. Exercise REPL with typical interactive workload
echo "5/7 Running REPL workload..."
cat <<'EOF' | $TARGET --log-none 2>/dev/null || true
( Stack operations )
1 2 3 DROP SWAP DUP OVER ROT
.S

( Arithmetic )
10 20 + 30 40 * 100 50 / 7 3 MOD
.S

( Comparison and logic )
5 10 < 10 10 = 15 10 > 0= NOT
AND OR XOR

( Memory operations )
VARIABLE TEST
42 TEST !
TEST @ .

( String operations )
: HELLO ." Hello, World!" CR ;
HELLO

( Control flow - IF/ELSE/THEN )
: ABS DUP 0< IF NEGATE THEN ;
-42 ABS .
42 ABS .

( Control flow - DO/LOOP )
: COUNTDOWN 10 0 DO I . LOOP ;
COUNTDOWN

( Control flow - BEGIN/UNTIL )
: COUNT-UP 0 BEGIN DUP . 1+ DUP 5 = UNTIL DROP ;
COUNT-UP

( Defining words )
: SQUARE DUP * ;
: CUBE DUP DUP * * ;
10 SQUARE .
5 CUBE .

( CONSTANT and VARIABLE )
42 CONSTANT ANSWER
VARIABLE COUNTER
0 COUNTER !
ANSWER .
COUNTER @ .

( CREATE/DOES> )
: CONSTANT CREATE , DOES> @ ;
100 CONSTANT HUNDRED
HUNDRED .

( Dictionary manipulation )
: TEMP 123 ;
' TEMP >BODY
FORGET TEMP

( Compilation state )
: TEST1 [ 42 ] LITERAL . ;
TEST1

( Return stack )
: RSTACK-TEST 10 >R 20 >R R> . R> . ;
RSTACK-TEST

( Double-cell arithmetic )
1000000 2000000 D+
D.

( Nested definitions )
: OUTER : INNER 42 . ; INNER ;
OUTER

( Mixed mode arithmetic )
10 20 M* MD.
100 10 M/ .

( Format operations )
42 0 <# # # #> TYPE CR

( Vocabulary operations )
VOCABULARY TEST-VOCAB
FORTH DEFINITIONS
WORDS

( Block operations - if disk available )
0 BLOCK DROP
1 BLOCK DROP
EMPTY-BUFFERS

( Editor simulation - if blocks available )
( 100 LIST would list block 100 )

( Immediate words and compilation )
: [CHAR] CHAR POSTPONE LITERAL ; IMMEDIATE
: TEST [CHAR] A EMIT CR ;
TEST

( Recursive definitions )
: FACTORIAL DUP 1 > IF DUP 1- FACTORIAL * ELSE DROP 1 THEN ;
5 FACTORIAL .

( Deeply nested control flow )
: NESTED
  10 0 DO
    I 2 MOD 0= IF
      I 3 MOD 0= IF
        I .
      THEN
    THEN
  LOOP
;
NESTED

( String handling )
: COUNT-TEST S" Test String" DROP C@ . ;
COUNT-TEST

( ALLOT and HERE )
HERE 100 ALLOT HERE SWAP - .

( Error recovery )
( Intentional stack underflow - should recover )
DEPTH .

BYE
EOF

# 6. Exercise block system with disk image (if available)
if [ -f "disks/rajames-rajames-1.0.img" ]; then
    echo "6/7 Running block I/O workload..."
    cat <<'EOF' | $TARGET --disk-img=disks/rajames-rajames-1.0.img --disk-ro --log-none 2>/dev/null || true
( Block operations )
0 BLOCK DROP
1 BLOCK DROP
10 BLOCK DROP
100 BLOCK DROP
1000 BLOCK DROP
2048 BLOCK DROP
( LIST operations )
2048 LIST
EMPTY-BUFFERS
BYE
EOF
else
    echo "6/7 Skipping block I/O (no disk image)"
fi

# 7. Exercise profiler itself
echo "7/7 Running with profiler enabled..."
cat <<'EOF' | $TARGET --profile 2 --profile-report --log-none 2>/dev/null || true
: PROFILER-TEST
  1000 0 DO
    I DUP * DROP
  LOOP
;
PROFILER-TEST
BYE
EOF

echo ""
echo "✓ PGO profiling workload complete!"
echo "  Profile data (.gcda files) generated for optimization"