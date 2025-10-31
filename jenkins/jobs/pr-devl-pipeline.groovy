// ============================================================================
// PR DEVL PIPELINE JOB
// ============================================================================
// Purpose: Quick feedback for developers on PR updates
// Trigger: GitHub webhook (PR opened, updated, reopened)
// Duration: ~5 minutes
// Status: Version controlled for audit compliance
// Closes: #33 (CAPA-032: Groovy Job DSL Implementation)
// ============================================================================

pipelineJob('pr-devl-pipeline') {
    description('''
        ⚡ StarForth PR DevL Pipeline

        Quick feedback loop for developers.
        Triggered on: PR opened, updated, reopened
        Branch: PR feature branch
        Duration: ~5 minutes

        Stages:
        1. Cleanup & Build (make fastest)
        2. Smoke Test (quick validation)

        Result: Auto-merge to devL branch on success
        Next: test pipeline triggered automatically
    '''.stripIndent())

    logRotator {
        daysToKeepStr('7')
        numToKeepStr('20')
        artifactDaysToKeepStr('7')
        artifactNumToKeepStr('10')
    }

    properties {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr('20')))
    }

    // Trigger: GitHub webhook for pull requests
    triggers {
        githubPullRequest {
            admins(['rajames440'])
            useGitHubAppCredentials(false)

            // Trigger on:
            // - PR opened
            // - PR updated (new commits)
            // - PR reopened
            triggerPhrase('.*@jenkins.*devl.*')

            // Status context
            statusContext('StarForth/devL-pipeline')

            // Auto-update commit status
            gitHubAuthenticationToken('${GITHUB_TOKEN}')
        }
    }

    parameters {
        stringParam('ghprbPullId', '', 'GitHub PR number')
        stringParam('ghprbSourceBranch', 'master', 'Source branch')
        stringParam('ghprbTargetBranch', 'master', 'Target branch')
    }

    definition {
        cpsScm {
            scm {
                git {
                    remote {
                        url('https://github.com/rajames440/StarForth.git')
                        credentials('github-token')
                        refspec('+refs/pull/*/head:refs/remotes/origin/pr/*')
                    }
                    branches('origin/pr/${ghprbPullId}/merge')
                    userRemoteConfigs {
                        url('https://github.com/rajames440/StarForth.git')
                        credentialsId('github-token')
                        refspec('+refs/pull/*/head:refs/remotes/origin/pr/*')
                    }
                }
            }
            scriptPath('jenkinsfiles/devl/Jenkinsfile')
        }
    }

    // Post-build actions
    publishers {
        archiveArtifacts {
            artifacts('logs/**')
            allowEmpty(true)
        }

        // Publish GitHub status
        githubCommitStatus {
            contextSource(defaultCommitContextSource())
            statusMessage {
                addTestResults(true)
            }
        }

        // On success: Auto-merge to devL branch
        conditionalPublisher {
            condition {
                statusEquals(0)
            }
            action {
                shell('''
                    echo "✅ DevL pipeline passed"
                    git checkout devL || git checkout -b devL origin/devL
                    git merge --no-ff origin/pr/${ghprbPullId}/merge -m "Merge PR #${ghprbPullId} devL pipeline passed"
                    git push origin devL
                    echo "Auto-merged to devL branch"
                ''')
            }
        }
    }
}

// Job Description
println("✅ Created: pr-devl-pipeline")
println("   Trigger: GitHub webhook (PR opened/updated)")
println("   Duration: ~5 minutes")
println("   Jenkinsfile: jenkinsfiles/devl/Jenkinsfile")
println("   On Success: Auto-merge PR → devL branch")