// ============================================================================
// PR TEST PIPELINE JOB
// ============================================================================
// Purpose: Full test suite execution (936 tests)
// Trigger: Automatic on devL → test merge
// Duration: ~20 minutes
// Status: Version controlled for audit compliance
// Closes: #33 (CAPA-032: Groovy Job DSL Implementation)
// ============================================================================

pipelineJob('pr-test-pipeline') {
    description('''
        🧪 StarForth Test Pipeline

        Full test suite execution on test branch.
        Triggered automatically when devL merges to test.
        Branch: test
        Duration: ~20 minutes

        Stages:
        1. Checkout test branch
        2. Build (make fastest)
        3. Run full test suite (936 tests)
        4. Parse test results
        5. Generate test report

        Result: Quality metrics published
        Next: qual pipeline triggered automatically
    '''.stripIndent())

    logRotator {
        daysToKeepStr('30')
        numToKeepStr('20')
        artifactDaysToKeepStr('7')
        artifactNumToKeepStr('10')
    }

    properties {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr('20')))
    }

    // Trigger: Upstream job (pr-devl-pipeline) success
    triggers {
        upstreamTrigger {
            upstreamProjects('pr-devl-pipeline')
            triggerStrategy('ALWAYS')
        }

        // Also trigger on manual branch push to test
        githubPush()
    }

    parameters {
        stringParam('BRANCH', 'test', 'Git branch to test')
        stringParam('TEST_TIMEOUT', '1800', 'Test timeout in seconds (30 min)')
    }

    definition {
        cpsScm {
            scm {
                git {
                    remote {
                        url('https://github.com/rajames440/StarForth.git')
                        credentials('github-token')
                    }
                    branches('origin/test')
                }
            }
            scriptPath('jenkinsfiles/test/Jenkinsfile')
        }
    }

    // Post-build actions
    publishers {
        // Archive test logs
        archiveArtifacts {
            artifacts('logs/**')
            allowEmpty(true)
        }

        // Archive test binaries
        archiveArtifacts {
            artifacts('artifacts/**')
            allowEmpty(true)
        }

        // Publish test results
        junit {
            testResults('test-results/**/*.xml')
            healthScaleFactor(1.0)
            keepLongStdio(true)
        }

        // Email notification on failure
        mailer {
            recipients('${BUILD_LOG_REGEX_MATCH}')
            dontNotifyEveryUnstableBuild(false)
            sendToIndividuals(true)
            notifyEveryUnstableBuild(true)
        }

        // Performance trend chart
        publishHTML {
            reportDir('test-results/performance')
            reportFiles('performance-report.html')
            reportName('Performance Trend')
            keepAll(true)
            alwaysLinkToLastBuild(true)
        }
    }

    // Webhook for GitHub status
    webhookTriggers {
        githubPushTrigger()
    }
}

// Job Description
println("✅ Created: pr-test-pipeline")
println("   Trigger: Upstream (pr-devl-pipeline success) OR manual test branch push")
println("   Duration: ~20 minutes")
println("   Jenkinsfile: jenkinsfiles/test/Jenkinsfile")
println("   Result: Test metrics published to GitHub")