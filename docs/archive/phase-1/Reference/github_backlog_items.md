# GitHub Project Backlog Prep

Use these entries to populate https://github.com/users/rajames440/projects/5. Each item references the CAPA it closes so
tracking stays aligned with the governance audit.

## Issues To Create

1. **Resolve Tier I Report Placeholders (CAPA-35)**
    - Link: codex_audit_report.md / CAPA-35
    - Summary: Replace placeholder metrics, attach actual coverage & deficiency stats, reissue signed Tier I report.
    - Acceptance: Updated report checked in, signatures captured, placeholders eliminated.

2. **Archive ARM64 Validation Log (CAPA-36 & CAPA-09)**
    - Summary: Run ARM64 suite, capture log per protocol format, store under evidence dir and update references.
    - Acceptance: `starforth-test-arm64-*.log` present, test log references updated.

3. **Standardize Test Log Metadata (CAPA-37 & CAPA-06)**
    - Summary: Implement logging template (platform/commit/tester/timestamp/outcome) and regenerate existing logs.
    - Acceptance: `Reference/run.log` compliant; template tooling documented.

4. **Populate Deficiency Ledger (CAPA-38 & CAPA-14)**
    - Summary: Enter current deficiency counts/statuses; cross-link from Tier I report.
    - Acceptance: `Validation/SHARED/DEFICIENCY_LOG.adoc` reflects reality; Tier I sections updated.

5. **Secure Policy & Process Signatures (CAPA-39)**
    - Summary: Route signature blocks for governance policies/process chapters; update tracker.
    - Acceptance: Signature table in `SIGNATURE_TRACKING.adoc` shows completed entries.

6. **Assign Governance Owners & Status (CAPA-40)**
    - Summary: Fill `[TBD]` roles in validation roadmap, defect logs, refinement CAPA; ensure status fields set.
    - Acceptance: Documents list named owners and current states.

7. **Document Continuous Validation Evidence (CAPA-41 & CAPA-05)**
    - Summary: Capture CI/Jenkins run outputs, schedules, and place under evidence tree; update README references.
    - Acceptance: CI logs accessible; governance docs link to them.

8. **Integrate SonarQube Quality Gate (CAPA-42)**
    - Summary: Configure SonarQube analysis, archive reports, embed results in Tier II quality docs.
    - Acceptance: CI runs Sonar scan; reports cited in CODE_QUALITY_BASELINE_REPORT.

## Notes

- Map each GitHub issue to its CAPA ID in the description (e.g., “Closes CAPA-35”).
- After creation, add issues to the “Backlog” column of Project 5 for triage.
