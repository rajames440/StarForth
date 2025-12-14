# Backlog Gatekeeper Procedures

**Last Updated:** November 3, 2025
**Status:** Active (Phase 1B - Gatekeeper Enforcement)

---

## Overview

This document defines the procedures for enforcing **Rule 1 of the governance system**:

> **Rule 1:** Only approved governance workflows or the project owner may add items to the development backlog.

The gatekeeper system prevents unauthorized backlog access through automated enforcement and audit logging.

---

## Rule 1: Backlog Access Control

### Who Can Add to the Backlog?

**✅ ALLOWED:**

1. **Project Owner** (rajames440)
   - May manually add items directly to backlog
   - No approval required
   - Full discretionary authority

2. **Automated Workflows** (Approved governance processes)
   - ECO (Engineering Change Order) workflow
     - Always routes to backlog
     - Triggered by: ECO template + approval
   - CAPA (Corrective/Preventive Action) workflow
     - Routes to backlog if PM decides
     - Triggered by: CAPA template + approval + PM decision (`route:backlog` label)

**❌ NOT ALLOWED:**

- Direct label application by non-owners
- Manual backlog entry without governance template
- Bypassing ECO/CAPA workflows
- Creating backlog issues without proper approval chain
- Applying `route:backlog` label outside workflow context

---

## Enforcement Mechanism

### Gatekeeper Workflow

**File:** `.github/workflows/gatekeeper-backlog-enforcement.yml`

**Trigger:** Monitors for `route:backlog` label application

**Detection Logic:**

```
IF route:backlog label applied THEN:
  IF actor is owner (rajames440) THEN:
    ✓ ALLOW (owner has full discretionary authority)
  ELSE IF issue has approval labels (approved-by-projm, approved-by-qualm, etc.) THEN:
    ✓ ALLOW (went through proper workflow)
  ELSE IF issue is ECO (type:eco) THEN:
    ✓ ALLOW (ECO always goes to backlog)
  ELSE:
    ✗ BYPASS DETECTED → Trigger enforcement actions
```

### Bypass Detection & Response

When an unauthorized backlog access attempt is detected:

1. **Security Warning Comment** (Posted to original issue)
   - Explains the violation
   - References proper procedures
   - Instructs user on correct process
   - Links to governance documentation

2. **PM Notification** (Automatic)
   - PM assigned to issue
   - Ensures management awareness
   - Allows PM to follow up with user

3. **Incident Report Creation** (Automatic)
   - New IR issue created with evidence
   - Severity: MAJOR
   - Full audit trail documentation
   - Links to original issue

4. **Security Log Entry** (Automatic)
   - Event logged to `in_basket/SEC_LOG.adoc`
   - Immutable audit trail in governance repository
   - Timestamp, actor, method, and response recorded

---

## GitHub Permission Configuration

### Recommended Setup

Since GitHub's native permissions are limited, we use a layered approach:

#### Layer 1: Repository Access Control

**Team Structure:**
- `Core Contributors` - PM, QA Lead, Engineering Manager
  - Can create and modify any issue
  - Can apply workflow labels
- `Development Team` - Developers, Engineers
  - Can create issues (governance templates)
  - Cannot directly modify backlog-related labels
  - Must use proper workflows

#### Layer 2: Branch Protection (if using branches for backlog)

If backlog is tracked via git branch:
- Restrict direct commits to backlog branch
- Require PR and approval
- Enforce workflow checks before merge

#### Layer 3: Automated Enforcement (Currently Active)

**Gatekeeper Workflow** (primary enforcement)
- Detects label application violations
- Creates incidents for violations
- Logs to SEC_LOG.adoc
- Notifies PM
- **This is the active enforcement layer**

### GitHub Repository Settings

To support gatekeeper enforcement:

1. **GitHub Token Permissions**
   - Ensure `GITHUB_TOKEN` can:
     - Write to issues (add comments, labels)
     - Create new issues (for IR)
   - Current: ✓ Configured in workflow

2. **Workflow Permissions**
   - Enable "Allow GitHub Actions to create and approve pull requests"
   - Ensure workflow `contents: write` for SEC_LOG updates
   - Current: ✓ Configured in workflow

3. **Issue Labels**
   - Ensure these labels exist:
     - `type:ir` - For Incident Reports
     - `severity:major` - For incident severity
     - `security-violation` - For security-related incidents
     - `route:backlog` - For backlog routing
     - Approval labels: `approved-by-projm`, `approved-by-qualm`

---

## Procedures for Different User Types

### Project Owner (rajames440)

**Authority:** Full discretionary control of backlog

**Procedures:**

1. **Manual Backlog Addition** (When Needed)
   ```
   1. Create governance document issue (using template)
   2. Review and approve as needed
   3. Apply `route:backlog` label directly
   4. Gatekeeper recognizes owner → ALLOWS
   ```

2. **Governance Oversight**
   - Review gatekeeper bypass incidents regularly
   - Assess if education or access restrictions needed
   - Update procedures based on patterns

### Project Managers, QA Leads, Engineering Managers

**Authority:** Decision-makers for document routing

**Procedures:**

1. **Receive Approval Request**
   ```
   1. Document submitted using proper template
   2. PM/QA reviews and approves
   3. Apply approval label (approved-by-projm, approved-by-qualm)
   4. Wait for PM backlog vs vault decision request
   ```

2. **Make Routing Decision**
   ```
   1. Evaluate: "Will a developer work on this?"
   2. If YES: Apply `route:backlog` label
   3. If NO: Apply `route:vault` label
   4. Gatekeeper validates approval + label → ALLOWS
   ```

3. **Handle Bypass Incidents**
   - Review IR created by gatekeeper
   - Assess if user education needed
   - Contact violating user with guidance

### Development Team

**Authority:** Submit documents only through proper workflows

**Procedures:**

1. **Submitting Work Request**
   ```
   1. Use appropriate template (ECR, CAPA, etc.)
   2. Provide complete information
   3. Submit via GitHub issue
   4. Wait for approval workflow
   5. DO NOT apply backlog labels directly
   ```

2. **ECO Creation (After ECR Approval)**
   ```
   1. ECR → PM approves → Engineering Manager receives notification
   2. Engineering Manager creates ECO from approved ECR
   3. ECO template → ECO automatically routes to backlog
   4. Developer picks up from backlog
   ```

3. **CAPA Submission**
   ```
   1. CAPA template → Complete all required fields
   2. QA/PM reviews and approves
   3. PM decides: Backlog or Vault?
   4. If backlog: developer picks up
   5. If vault: routed to governance
   ```

---

## Workflow Examples

### Example 1: Authorized Backlog Addition (ECO)

```
Step 1: Engineering Manager receives approved ECR
Step 2: Creates ECO issue using eco.yml template
Step 3: Fills in related ECR reference, FMEA decision, priority
Step 4: ECO workflow automatically applies route:backlog label
Step 5: Gatekeeper sees:
        - Actor: workflow (actions/github-script)
        - Labels: type:eco, status:approved, route:backlog
        - Evidence: ECO has required structure
        → ALLOW (recognized as approved workflow)
Step 6: Item appears in backlog
Step 7: Developer picks up and works on implementation
```

**Result:** ✓ Authorized, backlog entry allowed

---

### Example 2: Unauthorized Backlog Addition (Bypass)

```
Step 1: Developer creates issue (e.g., "Add feature X")
Step 2: Developer applies route:backlog label
        (Trying to skip approval process)
Step 3: Gatekeeper detects:
        - Actor: developer (non-owner)
        - Labels: route:backlog (but no approval evidence)
        - No type:eco or approval labels found
        → BYPASS DETECTED
Step 4: Gatekeeper actions:
        ✓ Posts warning comment: "Use proper ECR/CAPA workflow"
        ✓ Assigns PM for review
        ✓ Creates IR issue: "Unauthorized Backlog Access"
        ✓ Logs to SEC_LOG.adoc: Timestamp, actor, method, action
Step 5: PM reviews incident
Step 6: PM contacts developer with guidance
Step 7: Developer resubmits using proper ECR workflow
```

**Result:** ✗ Unauthorized attempt detected, logged, and corrected

---

### Example 3: Authorized PM Routing Decision

```
Step 1: CAPA submitted using capa.yml template
Step 2: QA reviews and approves
        → Adds approved-by-qualm label
Step 3: Gatekeeper rule 2 triggers: "Request PM decision"
Step 4: PM receives decision request comment
        (Backlog = developer work vs Vault = documentation)
Step 5: PM evaluates: "Is this a bug that needs fixing?"
        Yes → PM applies route:backlog label
Step 6: Gatekeeper sees:
        - Actor: PM (rajames440 or delegated)
        - Labels: type:capa, approved-by-qualm, route:backlog
        - Evidence: Approval + PM decision
        → ALLOW
Step 7: Route-to-vault workflow checks:
        route:backlog → Don't route to vault, stays in backlog
Step 8: Developer picks up CAPA from backlog
```

**Result:** ✓ Authorized through approval + PM decision chain

---

## Audit Procedures

### SEC_LOG.adoc

**Location:** `/StarForth-Governance/in_basket/SEC_LOG.adoc`

**Contents:**
- Timestamp (UTC) of security event
- Event type (Unauthorized Backlog Access, etc.)
- User/actor who triggered event
- Reference (Original issue #NNN)
- Detailed description of violation
- Severity level (CRITICAL, MAJOR, MINOR, LOW)
- Action taken (IR created, PM notified)

**Access:**
- Version controlled in StarForth-Governance repo
- Immutable audit trail
- Updated by workflows automatically

### Incident Reports (IR)

**Created By:** Gatekeeper workflow

**Labels:** `type:ir`, `severity:major`, `security-violation`

**Contains:**
- Original violation details
- What the user did
- What the proper process should be
- Governance references
- Links to governance documentation

**Review:** PM reviews IR, contacts user, documents follow-up

---

## Troubleshooting

### Issue: "I applied `route:backlog` but got a warning"

**Likely Cause:** Missing approval labels or not following workflow

**Solution:**
1. Check if your issue has proper approval labels
2. If not, go back and request approval first
3. PM will handle routing decision
4. Let PM apply the `route:backlog` label

**Reference:** See "Procedures for Development Team" above

---

### Issue: "My ECO should have gone to backlog but got a vault warning"

**Likely Cause:** ECO issue missing required fields or type label

**Solution:**
1. Verify you used eco.yml template
2. Check that issue has `type:eco` label
3. Check that issue has `status:approved` label
4. Try re-applying `route:backlog` after template validation

**Contact:** PM (@rajames440) if issue persists

---

### Issue: "I'm the owner but still got a warning"

**Likely Cause:** Workflow logic issue or label timing

**Solution:**
1. Check that your GitHub username is exactly `rajames440`
2. Verify gatekeeper workflow has correct OWNER_USERNAME
3. Check workflow execution logs in GitHub Actions

**Reference:** Gatekeeper workflow: `.github/workflows/gatekeeper-backlog-enforcement.yml`

---

## Governance References

**Related Documents:**
- `GOVERNANCE_WORKFLOW_IMPLEMENTATION.md` - Full workflow details
- `GOVERNANCE_DOCUMENT_TYPE_SPECIFICATION.md` - Document types and routing
- `.github/workflows/gatekeeper-backlog-enforcement.yml` - Enforcement code
- `/StarForth-Governance/in_basket/SEC_LOG.adoc` - Audit trail

**Key Rules:**
- **Rule 1:** Only approved workflows or owner may add to backlog
- **Rule 2:** All governance documents require approval before routing
- **Rule 3:** PM makes backlog vs vault routing decision for conditional types

---

## Summary

| Aspect | Details |
|--------|---------|
| **Rule** | Only owner or approved workflows may add to backlog |
| **Owner Authority** | rajames440 has full discretionary backlog access |
| **Approved Workflows** | ECO (always), CAPA (if PM decides) |
| **Detection** | Gatekeeper monitors route:backlog label application |
| **Bypass Response** | Warning + IR creation + PM notification + Security logging |
| **Audit Trail** | SEC_LOG.adoc in in_basket (immutable) |
| **Access Control** | Layered: GitHub permissions + Gatekeeper enforcement |

**Status:** ✓ Enforcement Active (Ready for Use)