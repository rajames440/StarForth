// ============================================================================
// PR QUAL PIPELINE JOB
// ============================================================================
// Purpose: Formal verification and quality checks (Isabelle proofs)
// Trigger: Automatic on test → qual merge
// Duration: ~25 minutes
// Status: Version controlled for audit compliance
// Closes: #33 (CAPA-032: Groovy Job DSL Implementation)
// ============================================================================

pipelineJob('pr-qual-pipeline') {
    description('''
        ✅ StarForth Qual Pipeline

        Formal verification and quality gate.
        Triggered automatically when test merges to qual.
        Branch: qual
        Duration: ~25 minutes

        Stages:
        1. Checkout qual branch
        2. Build with full verification (make qual)
        3. Run Isabelle formal verification
        4. Code quality metrics (SonarQube)
        5. Security scanning
        6. Generate qual report

        Results:
        - Isabelle proof coverage
        - Code quality metrics
        - Security findings
        - Coverage report

        Next: PM release approval gate
    '''.stripIndent())

    logRotator {
        daysToKeepStr('60')
        numToKeepStr('20')
        artifactDaysToKeepStr('30')
        artifactNumToKeepStr('10')
    }

    properties {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr('20')))
    }

    // Trigger: Upstream job (pr-test-pipeline) success
    triggers {
        upstreamTrigger {
            upstreamProjects('pr-test-pipeline')
            triggerStrategy('ALWAYS')
        }

        // Also trigger on manual branch push to qual
        githubPush()
    }

    parameters {
        stringParam('BRANCH', 'qual', 'Git branch for qualification')
        stringParam('QUAL_TIMEOUT', '1500', 'Qual timeout in seconds (25 min)')
        booleanParam('RUN_ISABELLE', true, 'Run Isabelle formal verification')
        booleanParam('RUN_SONARQUBE', true, 'Run SonarQube analysis')
    }

    definition {
        cpsScm {
            scm {
                git {
                    remote {
                        url('https://github.com/rajames440/StarForth.git')
                        credentials('github-token')
                    }
                    branches('origin/qual')
                }
            }
            scriptPath('jenkinsfiles/qual/Jenkinsfile')
        }
    }

    // Post-build actions
    publishers {
        // Archive qual logs
        archiveArtifacts {
            artifacts('logs/**')
            allowEmpty(true)
        }

        // Archive isabelle proofs
        archiveArtifacts {
            artifacts('docs/isabelle/**')
            allowEmpty(true)
        }

        // Publish SonarQube results
        sonarPublisher {
            installationName('SonarQube')
            sonarSources('src/**')
            sonarInclusions('src/**/*.c')
            sonarExclusions('src/test_runner/**')
            exclusions('src/test_runner/**')
        }

        // Publish code coverage
        publishHTML {
            reportDir('coverage')
            reportFiles('index.html')
            reportName('Code Coverage')
            keepAll(true)
            alwaysLinkToLastBuild(true)
        }

        // Publish Isabelle proof report
        publishHTML {
            reportDir('docs/isabelle')
            reportFiles('proof-report.html')
            reportName('Isabelle Proof Status')
            keepAll(true)
            alwaysLinkToLastBuild(true)
        }

        // Email notification on failure
        mailer {
            recipients('${BUILD_LOG_REGEX_MATCH}')
            dontNotifyEveryUnstableBuild(false)
            sendToIndividuals(true)
        }

        // Trigger PM release approval on success
        downstreamParameterized {
            trigger('pr-prod-release') {
                condition('SUCCESS')
                parameters {
                    currentBuild()
                }
            }
        }
    }

    // Quality gates
    properties {
        buildNamer {
            template('${BUILD_NUMBER}-qual-${GIT_BRANCH}')
        }
    }
}

// Job Description
println("✅ Created: pr-qual-pipeline")
println("   Trigger: Upstream (pr-test-pipeline success) OR manual qual branch push")
println("   Duration: ~25 minutes")
println("   Jenkinsfile: jenkinsfiles/qual/Jenkinsfile")
println("   Includes: Isabelle verification, SonarQube, code coverage")
println("   On Success: Triggers pr-prod-release (PM approval gate)")