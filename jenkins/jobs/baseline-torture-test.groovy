// ============================================================================
// BASELINE TORTURE TEST JOB
// ============================================================================
// Purpose: Nightly full torture test of master branch
// Trigger: Scheduled (0 0 * * * - midnight daily)
// Duration: ~2 hours
// Status: Version controlled for audit compliance
// Closes: #33 (CAPA-032: Groovy Job DSL Implementation)
// ============================================================================

pipelineJob('baseline-torture-test') {
    description('''
        🔥 StarForth Baseline Torture Test

        Full torture test of master branch - runs nightly.
        Includes:
        - All build configurations (debug, standard, fast, fastest)
        - Cross-compilation (ARM64 RPi4)
        - Smoke tests
        - Full test suite (936 tests)
        - Benchmark suite
        - Extreme stress tests

        Duration: ~2 hours
        Artifacts: Logs, binaries, test results
    '''.stripIndent())

    logRotator {
        daysToKeepStr('30')
        numToKeepStr('10')
        artifactDaysToKeepStr('7')
        artifactNumToKeepStr('5')
    }

    properties {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr('10')))
    }

    // Schedule: Daily at midnight UTC
    triggers {
        cron('0 0 * * *')
    }

    definition {
        cps {
            // Load Jenkinsfile from repository
            script('''
                @Library('starforth-shared') _

                // Load baseline torture test pipeline
                node {
                    stage('Checkout') {
                        checkout scm
                    }

                    // Load and execute baseline Jenkinsfile
                    load 'Jenkinsfile'
                }
            '''.stripIndent())

            sandbox(true)
        }
    }

    // Notification on failure
    properties {
        pipelineTriggers {
            triggers {
                bitBucketPush()
            }
        }
    }
}

// Job Description
println("✅ Created: baseline-torture-test")
println("   Schedule: Daily at 00:00 UTC")
println("   Duration: ~2 hours")
println("   Jenkinsfile: /Jenkinsfile (root)")