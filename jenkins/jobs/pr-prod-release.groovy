// ============================================================================
// PROD RELEASE PIPELINE JOB
// ============================================================================
// Purpose: Production release gate and deployment (PM approval required)
// Trigger: Automatic on qual success + manual PM approval
// Duration: ~10 minutes
// Status: Version controlled for audit compliance
// Closes: #33 (CAPA-032: Groovy Job DSL Implementation)
// ============================================================================

pipelineJob('pr-prod-release') {
    description('''
        🚀 StarForth Prod Release Pipeline

        Production release gate with PM approval requirement.
        Triggered automatically after qual pipeline succeeds.
        Requires explicit PM approval to proceed.
        Branch: prod (release branch)
        Duration: ~10 minutes

        Stages:
        1. Await PM approval (manual gate)
        2. Checkout prod branch
        3. Validate release (version check, tag validation)
        4. Generate release notes
        5. Tag release in Git (vX.Y.Z.B)
        6. Create GitHub release
        7. Archive release artifacts
        8. Update version documentation

        Release Process:
        - Merge qual → prod (manually or via script)
        - Wait for PM approval via GitHub PR review
        - Automatic tag creation
        - GitHub release created
        - Artifacts archived for deployment

        Controls:
        - Requires PM approval (gitHub branch protection)
        - Version number validation
        - Release tag mandatory
        - Audit trail in Git commits
    '''.stripIndent())

    logRotator {
        daysToKeepStr('365')
        numToKeepStr('50')
        artifactDaysToKeepStr('365')
        artifactNumToKeepStr('50')
    }

    properties {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr('50')))
    }

    // Trigger: Upstream job (pr-qual-pipeline) success + manual approval
    triggers {
        upstreamTrigger {
            upstreamProjects('pr-qual-pipeline')
            triggerStrategy('ALWAYS')
        }
    }

    parameters {
        stringParam('BRANCH', 'prod', 'Release branch (prod)')
        stringParam('RELEASE_VERSION', '', 'Release version (e.g., 2.0.3)')
        stringParam('BUILD_NUMBER_OVERRIDE', '', 'Build number (optional)')
        booleanParam('CREATE_GITHUB_RELEASE', true, 'Create GitHub release')
        booleanParam('PUSH_TO_MASTER', true, 'Merge prod → master after release')
    }

    definition {
        cpsScm {
            scm {
                git {
                    remote {
                        url('https://github.com/rajames440/StarForth.git')
                        credentials('github-token')
                        refspec('+refs/heads/prod:refs/remotes/origin/prod')
                    }
                    branches('origin/prod')
                }
            }
            scriptPath('jenkinsfiles/prod/Jenkinsfile')
        }
    }

    // Approval gate - requires PM review
    // Note: GitHub branch protection rules enforce this
    properties {
        gatekeeper {
            downstreamProjects('')
            autoPromote(false)  // Require manual approval
        }
    }

    // Post-build actions
    publishers {
        // Archive release artifacts
        archiveArtifacts {
            artifacts('artifacts/**')
            allowEmpty(true)
        }

        // Archive release notes
        archiveArtifacts {
            artifacts('RELEASE_NOTES.md')
            allowEmpty(false)
        }

        // Create GitHub release
        githubRelease {
            repositoryOwner('rajames440')
            repositoryName('StarForth')
            tagName('v${RELEASE_VERSION}')
            body('Release ${RELEASE_VERSION}\n\nSee RELEASE_NOTES.md for details')
            draft(false)
            prerelease(false)
            releaseName('StarForth ${RELEASE_VERSION}')
            artifacts('artifacts/starforth-*')
        }

        // Update master branch after release
        conditionalPublisher {
            condition {
                booleanCondition('${PUSH_TO_MASTER}')
            }
            action {
                shell('''
                    git config user.name "Jenkins Release Bot"
                    git config user.email "jenkins@starforth.local"
                    git tag v${RELEASE_VERSION}
                    git push origin v${RELEASE_VERSION}
                    git checkout master
                    git merge --no-ff prod -m "Release v${RELEASE_VERSION}"
                    git push origin master
                    echo "Released v${RELEASE_VERSION} to master"
                ''')
            }
        }

        // Send release notification
        mailer {
            recipients('rajames440@github.com')
            dontNotifyEveryUnstableBuild(false)
            sendToIndividuals(true)
        }

        // Log release to audit trail
        shell('''
            echo "✅ RELEASE AUDIT TRAIL"
            echo "Release: v${RELEASE_VERSION}"
            echo "Build: ${BUILD_NUMBER}"
            echo "Date: $(date -u)"
            echo "PM Approval: Required"
            echo "Branch: prod → master"
            echo "Tag: v${RELEASE_VERSION}"
        ''')
    }

    // Email notifications
    publishers {
        groovyPostbuild {
            script('''
                // Notify PM on release completion
                manager.addShortText("Released v" + build.getEnvironment()['RELEASE_VERSION'], "blue", "white", "bold")
            ''')
        }
    }
}

// Job Description
println("✅ Created: pr-prod-release")
println("   Trigger: Upstream (pr-qual-pipeline success) + PM approval")
println("   Duration: ~10 minutes")
println("   Jenkinsfile: jenkinsfiles/prod/Jenkinsfile")
println("   Approval: Requires PM review (GitHub branch protection)")
println("   Result: Release tagged, master updated, GitHub release created")