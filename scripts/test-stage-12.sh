#!/bin/bash
# Test Stage 12: Performance Regression Check (locally)

set -x  # Enable debug output to see every command

echo "════════════════════════════════════════════════════════════"
echo "  Testing Stage 12: Performance Regression Check"
echo "════════════════════════════════════════════════════════════"
echo ""

# Setup (like Jenkins does)
BUILD_DIR='build'
LOG_DIR='logs'
ARTIFACT_DIR='artifacts'

mkdir -p ${LOG_DIR} ${ARTIFACT_DIR}

echo "Step 1: Clean and build fastest"
make clean
make fastest

echo ""
echo "Step 2: Run performance baseline tests"

# This is the exact script from Jenkins Stage 12
echo "timestamp,operation,iterations,duration_seconds" > ${LOG_DIR}/performance-baseline.csv

echo "Test 1: Stack operations (1M iterations)..."
START=$(date +%s.%N)
echo "  Running: ./build/starforth -c ': BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE' --log-none"
./build/starforth -c ": BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE" --log-none
RESULT=$?
END=$(date +%s.%N)
echo "  Exit code: $RESULT"
echo "  START: $START"
echo "  END: $END"
DURATION=$(echo "$END - $START" | bc)
echo "  DURATION: $DURATION"
echo "$(date +%s),stack_ops,1000000,$DURATION" >> ${LOG_DIR}/performance-baseline.csv

echo ""
echo "Test 2: Arithmetic operations (1M iterations)..."
START=$(date +%s.%N)
echo "  Running: ./build/starforth -c ': BENCH 1000000 0 DO 10 20 + DROP LOOP ; BENCH BYE' --log-none"
./build/starforth -c ": BENCH 1000000 0 DO 10 20 + DROP LOOP ; BENCH BYE" --log-none
RESULT=$?
END=$(date +%s.%N)
echo "  Exit code: $RESULT"
echo "  START: $START"
echo "  END: $END"
DURATION=$(echo "$END - $START" | bc)
echo "  DURATION: $DURATION"
echo "$(date +%s),arithmetic,1000000,$DURATION" >> ${LOG_DIR}/performance-baseline.csv

echo ""
echo "Step 3: Show results"
cat ${LOG_DIR}/performance-baseline.csv

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  ✅ Stage 12 Test Complete"
echo "════════════════════════════════════════════════════════════"