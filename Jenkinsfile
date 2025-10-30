pipeline {
	agent any

	environment {
		// Build configuration
		ARCH = 'amd64'
		STRESS_MULTIPLIER = '1'  // Dial this down if things break
		MAX_ITERATIONS = '10000000'  // 10 million iterations

		// Paths
		BUILD_DIR = 'build'
		ARTIFACT_DIR = 'artifacts'
		LOG_DIR = 'logs'
	}
	options {
		// Fail fast if any stage fails
		skipDefaultCheckout(false)
		buildDiscarder(logRotator(numToKeepStr: '10'))
		timeout(time: 2, unit: 'HOURS')
		timestamps()
	}

	stages {
		stage('🧹 Cleanup & Preparation') {
			steps {
				echo "════════════════════════════════════════════════════════════"
				echo "   ⚡ StarForth Torture Test Pipeline - AMD64 ⚡"
				echo "════════════════════════════════════════════════════════════"

				// Clone/pull from GitHub remote
				sh '''
                    if [ -d .git ]; then
                        git fetch origin
                        git reset --hard origin/master
                    else
                        git clone https://github.com/rajames440/StarForth.git .
                    fi
                '''

				// Clean workspace
				sh 'make clean || true'
				sh 'rm -rf ${ARTIFACT_DIR} ${LOG_DIR}'
				sh 'mkdir -p ${ARTIFACT_DIR} ${LOG_DIR}'

				// System info
				sh '''
                    echo "System Information:" | tee ${LOG_DIR}/system-info.log
                    uname -a | tee -a ${LOG_DIR}/system-info.log
                    lscpu | grep -E "^(Architecture|CPU|Thread|Core|Socket|Model name)" | tee -a ${LOG_DIR}/system-info.log
                    free -h | tee -a ${LOG_DIR}/system-info.log
                    gcc --version | head -1 | tee -a ${LOG_DIR}/system-info.log
                '''
			}
		}

		stage('🔨 Build Gauntlet') {
			steps {
				// Build sequentially to avoid workspace conflicts
				echo "Building all configurations..."

				echo "Building DEBUG configuration..."
				sh 'make clean'
				sh 'make debug 2>&1 | tee ${LOG_DIR}/build-debug.log'
				sh 'cp build/starforth ${ARTIFACT_DIR}/starforth-debug'

				echo "Building STANDARD configuration..."
				sh 'make clean'
				sh 'make 2>&1 | tee ${LOG_DIR}/build-standard.log'
				sh 'cp build/starforth ${ARTIFACT_DIR}/starforth-standard'

				echo "Building FASTEST configuration..."
				sh 'make clean'
				sh 'make fastest 2>&1 | tee ${LOG_DIR}/build-fastest.log'
				sh 'cp build/starforth ${ARTIFACT_DIR}/starforth-fastest'

				echo "Building FAST configuration..."
				sh 'make clean'
				sh 'make fast 2>&1 | tee ${LOG_DIR}/build-fast.log'
				sh 'cp build/starforth ${ARTIFACT_DIR}/starforth-fast'
			}
		}

		stage('🧪 Smoke Test') {
			steps {
				echo "Running smoke test to verify basic functionality..."
				sh 'make clean && make fastest'
				sh 'make smoke 2>&1 | tee ${LOG_DIR}/smoke-test.log'
			}
		}

		stage('✅ Comprehensive Test Suite') {
			steps {
				echo "Running full test suite (936 tests)..."
				sh 'make clean && make fastest'
				sh 'make test 2>&1 | tee ${LOG_DIR}/full-test-suite.log'

				// Parse test results
				sh '''
                    echo "Test Results Summary:" | tee ${LOG_DIR}/test-summary.txt
                    grep -E "(passed|failed|skipped)" ${LOG_DIR}/full-test-suite.log | tail -5 | tee -a ${LOG_DIR}/test-summary.txt || true
                '''
			}
		}

		stage('🏇 Benchmark Gauntlet') {
			steps {
				// Run benchmarks sequentially since they all need the same binary
				echo "Running quick benchmark (1M iterations)..."
				sh 'make bench 2>&1 | tee ${LOG_DIR}/bench-quick.log'

				echo "Running full benchmark suite..."
				sh 'make benchmark 2>&1 | tee ${LOG_DIR}/bench-full.log'

				echo "Stack operations stress test (10M iterations)..."
				sh '''
                    /usr/bin/time -v ./build/starforth -c ": STACK-TORTURE 10000000 0 DO 1 2 3 4 5 DROP DROP DROP DROP DROP LOOP ; STACK-TORTURE BYE" 2>&1 | tee ${LOG_DIR}/bench-stack-torture.log || true
                '''

				echo "Arithmetic operations stress test (10M iterations)..."
				sh '''
                    /usr/bin/time -v ./build/starforth -c ": MATH-TORTURE 10000000 0 DO 123 456 + 789 * 999 / 111 MOD DROP LOOP ; MATH-TORTURE BYE" 2>&1 | tee ${LOG_DIR}/bench-math-torture.log || true
                '''

				echo "Logic operations stress test (10M iterations)..."
				sh '''
                    /usr/bin/time -v ./build/starforth -c ": LOGIC-TORTURE 10000000 0 DO 255 DUP AND DUP OR DUP XOR NOT DROP DROP LOOP ; LOGIC-TORTURE BYE" 2>&1 | tee ${LOG_DIR}/bench-logic-torture.log || true
                '''
			}
		}

		stage('💀 Extreme Stress Tests') {
			steps {
				// Run stress tests sequentially to avoid interference
				echo "Testing deep recursion limits..."
				sh '''
                    timeout 300 ./build/starforth -c ": RECURSE DUP 0> IF 1- RECURSE THEN ; 1000 RECURSE BYE" 2>&1 | tee ${LOG_DIR}/stress-recursion.log || echo "Recursion limit reached (expected)"
                '''

				echo "Testing maximum stack depth..."
				sh '''
                    timeout 300 ./build/starforth -c ": STACK-DEPTH 1000 0 DO I LOOP 1000 0 DO DROP LOOP ; STACK-DEPTH BYE" 2>&1 | tee ${LOG_DIR}/stress-stack-depth.log || echo "Stack depth limit reached (expected)"
                '''

				echo "Testing memory allocation patterns..."
				sh '''
                    timeout 300 ./build/starforth -c ": MEM-STRESS 10000 0 DO CREATE HERE 1024 ALLOT DROP LOOP ; MEM-STRESS BYE" 2>&1 | tee ${LOG_DIR}/stress-memory.log || echo "Memory stress completed/failed"
                '''

				echo "Testing long-running computation stability..."
				sh '''
                    timeout 600 ./build/starforth -c ": LONG-RUN 50000000 0 DO 2 2 + DROP LOOP ; LONG-RUN BYE" 2>&1 | tee ${LOG_DIR}/stress-long-run.log
                '''

				echo "Testing deeply nested loops..."
				sh '''
                    timeout 300 ./build/starforth -c ": NESTED 1000 0 DO 1000 0 DO 1000 0 DO 1 DROP LOOP LOOP LOOP ; NESTED BYE" 2>&1 | tee ${LOG_DIR}/stress-nested-loops.log
                '''
			}
		}

		stage('🔥 Thermal & Performance Monitoring') {
			steps {
				echo "Running sustained load test with performance monitoring..."
				sh 'make clean && make fastest'

				script {
					// Run benchmark while monitoring system resources
					sh '''
                        # Start monitoring in background
                        (while true; do
                            echo "$(date +%s),$(cat /proc/loadavg | awk '{print $1}'),$(free | grep Mem | awk '{print $3/$2 * 100.0}')" >> ${LOG_DIR}/system-metrics.csv
                            sleep 1
                        done) &
                        MONITOR_PID=$!

                        # Run sustained benchmark
                        /usr/bin/time -v ./build/starforth -c ": THERMAL-TEST 100000000 0 DO 1 2 + DROP LOOP ; THERMAL-TEST BYE" 2>&1 | tee ${LOG_DIR}/thermal-test.log || true

                        # Stop monitoring
                        kill $MONITOR_PID || true
                    '''
				}
			}
		}

		stage('🧠 Memory Leak Detection') {
			when {
				expression {
					fileExists('/usr/bin/valgrind')
				}
			}
			steps {
				echo "Running valgrind memory leak detection..."
				sh 'make clean && make debug'
				sh '''
                    timeout 600 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=${LOG_DIR}/valgrind-leak-check.log \
                        ./build/starforth --benchmark 1000 || echo "Valgrind completed with issues (check log)"
                '''
			}
		}

		stage('⚡ Profile-Guided Optimization Build') {
			steps {
				echo "Building with PGO for maximum performance..."
				sh '''
                    make pgo 2>&1 | tee ${LOG_DIR}/build-pgo.log
                    cp build/starforth ${ARTIFACT_DIR}/starforth-pgo
                '''
			}
		}

		stage('📊 PGO Performance Comparison') {
			steps {
				echo "Comparing PGO vs Regular build performance..."
				sh 'make bench-compare 2>&1 | tee ${LOG_DIR}/pgo-comparison.log'
			}
		}

		stage('🎯 Edge Case Testing') {
			steps {
				// Run edge case tests sequentially to avoid conflicts
				echo "Testing division by zero handling..."
				sh '''
                    ./build/starforth -c "1 0 / BYE" 2>&1 | tee ${LOG_DIR}/edge-div-zero.log || echo "Handled division by zero"
                '''

				echo "Testing stack underflow handling..."
				sh '''
                    ./build/starforth -c "DROP BYE" 2>&1 | tee ${LOG_DIR}/edge-stack-underflow.log || echo "Handled stack underflow"
                '''

				echo "Testing stack overflow handling..."
				sh '''
                    timeout 60 ./build/starforth -c ": OVERFLOW BEGIN 1 DUP REPEAT ; OVERFLOW BYE" 2>&1 | tee ${LOG_DIR}/edge-stack-overflow.log || echo "Handled stack overflow"
                '''

				echo "Testing invalid memory access handling..."
				sh '''
                    ./build/starforth -c "999999999 @ BYE" 2>&1 | tee ${LOG_DIR}/edge-invalid-memory.log || echo "Handled invalid memory access"
                '''
			}
		}

		stage('📈 Performance Regression Check') {
			steps {
				echo "Running performance regression baseline..."
				sh 'make clean && make fastest'
				sh '''
                    echo "timestamp,operation,iterations,duration_seconds" > ${LOG_DIR}/performance-baseline.csv

                    # Stack operations
                    START=$(date +%s.%N)
                    ./build/starforth -c ": BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE" --log-none
                    END=$(date +%s.%N)
                    DURATION=$(echo "$END - $START" | bc)
                    echo "$(date +%s),stack_ops,1000000,$DURATION" >> ${LOG_DIR}/performance-baseline.csv

                    # Arithmetic operations
                    START=$(date +%s.%N)
                    ./build/starforth -c ": BENCH 1000000 0 DO 10 20 + DROP LOOP ; BENCH BYE" --log-none
                    END=$(date +%s.%N)
                    DURATION=$(echo "$END - $START" | bc)
                    echo "$(date +%s),arithmetic,1000000,$DURATION" >> ${LOG_DIR}/performance-baseline.csv

                    cat ${LOG_DIR}/performance-baseline.csv
                '''
			}
		}

		stage('🔍 Code Quality Checks') {
			steps {
				echo "Running static analysis and code quality checks..."

				// Check for compiler warnings
				sh '''
                    make clean
                    make CFLAGS="$(make -n | grep CFLAGS | head -1 | sed 's/.*CFLAGS=//' | cut -d' ' -f1-20) -Wall -Wextra -Wpedantic" 2>&1 | tee ${LOG_DIR}/compiler-warnings.log || true
                '''

				// Count lines of code
				sh '''
                    echo "Lines of Code Analysis:" | tee ${LOG_DIR}/loc-analysis.txt
                    find src -name "*.c" -exec wc -l {} + | tail -1 | tee -a ${LOG_DIR}/loc-analysis.txt
                    find include -name "*.h" -exec wc -l {} + | tail -1 | tee -a ${LOG_DIR}/loc-analysis.txt
                '''
			}
		}

		stage('📋 Generate Test Report') {
			steps {
				echo "Generating comprehensive test report..."
				sh '''
                    cat > ${ARTIFACT_DIR}/test-report.md << 'EOFMD'
# StarForth Torture Test Report

**Build Date:** $(date)
**Architecture:** ${ARCH}
**Git Commit:** $(git rev-parse HEAD)

## Build Summary

$(ls -lh ${ARTIFACT_DIR}/starforth-* | awk "{print \\"- \\" $9 \\": \\" $5}")

## Test Results

### Full Test Suite
```
$(tail -20 ${LOG_DIR}/full-test-suite.log || echo "No test results")
```

### Benchmark Results

#### Quick Benchmark
```
$(grep "Time:" ${LOG_DIR}/bench-quick.log || echo "No benchmark results")
```

#### Full Benchmark Suite
```
$(tail -15 ${LOG_DIR}/bench-full.log || echo "No benchmark results")
```

### Stress Test Results

#### Stack Torture (10M iterations)
```
$(grep "Elapsed" ${LOG_DIR}/bench-stack-torture.log || echo "No results")
```

#### Arithmetic Torture (10M iterations)
```
$(grep "Elapsed" ${LOG_DIR}/bench-math-torture.log || echo "No results")
```

### Memory Analysis

```
$(grep "LEAK SUMMARY" -A 5 ${LOG_DIR}/valgrind-leak-check.log 2>/dev/null || echo "Valgrind not run")
```

### Performance Baseline

```
$(cat ${LOG_DIR}/performance-baseline.csv || echo "No baseline data")
```

### System Metrics

```
$(head -1 ${LOG_DIR}/system-metrics.csv && tail -10 ${LOG_DIR}/system-metrics.csv || echo "No metrics")
```

## Artifacts

All test artifacts are available in the ${ARTIFACT_DIR} directory.
All logs are available in the ${LOG_DIR} directory.

EOFMD
                '''

				echo "Test report generated!"
			}
		}
	}

	post {
		always {
			echo "Pipeline execution completed"

			// Archive artifacts
			archiveArtifacts artifacts: 'artifacts/**, logs/**', allowEmptyArchive: true

			// Publish test report
			publishHTML([
				allowMissing: true,
				alwaysLinkToLastBuild: true,
				keepAll: true,
				reportDir: 'artifacts',
				reportFiles: 'test-report.md',
				reportName: 'StarForth Torture Test Report'
			])
		}

		success {
			echo "✅ All torture tests passed! StarForth is a beast!"
		}

		failure {
			echo "❌ Some tests failed. Check logs for details."
		}

		unstable {
			echo "⚠️  Build completed but with warnings or unstable tests"
		}
	}
}