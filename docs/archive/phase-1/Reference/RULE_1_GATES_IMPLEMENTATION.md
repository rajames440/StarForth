# Rule 1: Governance-First Backlog Injection Gates Implementation

**Status:** ✅ COMPLETE
**Implementation Date:** 2025-11-03
**Rule:** Rule 1 - Governance-First, Only identified injection points with respect to governance will be allowed.

---

## Overview

Rule 1 establishes that **only injection points identified in the governance model are authorized** for backlog entry. This document describes the 4 gates implemented to enforce Rule 1.

**Governance Model Summary:**
- **Backlog items:** Only ECO (Engineering Change Order) and CAPA (Corrective/Preventive Action)
- **NOT allowed in backlog:** ECR (stays with PM), FMEA (blocking side trip), generic issues
- **Template enforcement:** All GitHub issues MUST use a template (CAPA, ECR, or FMEA)
- **Disposition:** Always to `StarForth-Governance/in_basket/` (filesystem), not GitHub

---

## The Four Gates

### **Gate 1: CAPA Template Structure Enforcement** ✅

**File:** `.github/ISSUE_TEMPLATES/capa.yml` + `.github/workflows/capa-submission.yml`
**Commit:** `52151a20`

**What it does:**
- Creates structured CAPA template with 7 required sections
- Validates all sections are present AND contain substantive content
- Classifies severity (CRITICAL, MAJOR, MINOR, LOW)
- Auto-detects regressions
- Rejects incomplete CAPAs with Rule 1 error message

**Sections enforced:**
1. Problem Statement (with observed/expected behavior)
2. Reproduction (step-by-step)
3. Environment (version, platform, OS)
4. Defect Classification (bug type)
5. Severity Assessment (CRITICAL/MAJOR/MINOR/LOW)
6. Root Cause & Location (optional)
7. Additional Context (optional)

**Protection:** Ensures CAPA issues entering the system have complete information for QA triage.

---

### **Gate 2: Template Requirement Enforcement** ✅

**File:** `.github/workflows/enforce-template-requirement.yml`
**Commit:** `addf9efc`

**What it does:**
- Monitors EVERY issue creation
- Detects which template was used (CAPA, ECR, FMEA)
- **Rejects issues with NO template** - closes them immediately
- Posts Rule 1 explanation with links to proper templates
- Labels violations with `status:invalid`, `needs-template`, `rule-1-violation`

**Template detection:**
- **CAPA:** `## 1. PROBLEM STATEMENT` marker
- **ECR:** Title starts with `ECR:` OR body contains `Change Description`
- **FMEA:** `## 1. SUBMISSION DETAILS` (new) or `Related Issue` (old template)

**Protection:** Prevents any non-template issues from entering the system. **This is the primary enforcement point.**

---

### **Gate 3: Backlog Item Type Validation** ✅

**File:** `.github/workflows/validate-backlog-item-type.yml`
**Commit:** `ce735726`

**What it does:**
- Monitors when issues get labeled with `backlog-item` or `backlog-ready`
- Validates issue type is ONLY `type:eco` or `type:capa`
- **Rejects ECR, FMEA, and generic issues** from backlog
- Automatically removes backlog labels from invalid items
- Posts type-specific error message explaining why

**Type validation:**
| Type | Backlog? | Why |
|------|----------|-----|
| `type:eco` | ✅ YES | PM-created order for implementation |
| `type:capa` | ✅ YES | Defect fix after QA validation |
| `type:ecr` | ❌ NO | Stays with PM for review/approval |
| `type:fmea` | ❌ NO | Blocking side trip, separate workflow |
| (generic) | ❌ NO | No type label - invalid |

**Protection:** Ensures only authorized issue types appear in backlog column.

---

### **Gate 4: FMEA Blocking Prevention** ✅

**File:** `.github/workflows/prevent-fmea-in-backlog.yml`
**Commit:** `ce735726`

**What it does:**
- Specifically monitors FMEA issues for backlog entry attempts
- Detects FMEA by `type:fmea` label OR unique body markers
- **Removes backlog labels immediately** if found
- Adds `do-not-backlog` and `awaiting-approval` markers
- Explains FMEA is a blocking gate, not a backlog task

**FMEA Governance Model:**
```
Parent issue (CAPA/ECR/ECO) identified as HIGH RISK
    ↓
FMEA created (blocking gate)
    ↓
Stakeholder approval (separate column: "Awaiting FMEA Approval")
    ↓
All stakeholders approve
    ↓
Parent issue unblocked → goes to backlog for implementation
```

**Protection:** Prevents developers from mistaking FMEA approval work as implementation backlog items.

---

## Backlog Injection Point Coverage

| Injection Point | Gate | Status | Prevention |
|---|---|---|---|
| 1. Direct GitHub blank issue | Gate 2 | ✅ | Closed immediately, no template used |
| 2. ECR → ECO → Backlog | Gate 1,3 | ✅ | ECO gets `type:eco` label, allowed. ECR rejected if in backlog |
| 3. CAPA → Backlog | Gate 1,3 | ✅ | CAPA validated, gets `type:capa` label, allowed |
| 4. GitHub API issue creation | Gate 2 | ✅ | Workflow validates template on all issue creation paths |
| 5. Manual Kanban drag-drop | Gate 3,4 | ✅ | Label-based validation catches unauthorized additions |
| 6. FMEA in backlog | Gate 4 | ✅ | Automatically removed, re-routed to approval status |
| 7. ECR in backlog | Gate 3 | ✅ | Rejected if found with backlog labels |

---

## Rule 1 Enforcement Flow

```
GitHub Issue Created
    ↓
Gate 2: Check Template (enforce-template-requirement.yml)
    ├─ Template found → Continue
    │   ├─ CAPA → Gate 1: Validate CAPA structure (capa-submission.yml)
    │   ├─ ECR  → Gate 1: Validate ECR structure (ecr-submission.yml)
    │   └─ FMEA → Gate 1: Validate FMEA structure (fmea-submission.yml)
    │
    └─ NO Template → CLOSE ISSUE (Rule 1 Violation)
         Post: "Use CAPA / ECR / FMEA template"
         Labels: status:invalid, needs-template, rule-1-violation

Valid CAPA/ECR/FMEA with correct labels
    ↓
Issue labeled with backlog-item/backlog-ready
    ↓
Gate 3: Validate Type (validate-backlog-item-type.yml)
    ├─ type:eco or type:capa → Allowed
    └─ type:ecr, type:fmea, or generic → REJECT
         Remove labels: backlog-item, backlog-ready
         Add labels: backlog-validation-failed, needs-correction

FMEA issues with backlog labels
    ↓
Gate 4: Enforce FMEA Gate (prevent-fmea-in-backlog.yml)
    ├─ If backlog labels found → Remove them
    └─ Add labels: do-not-backlog, awaiting-approval
         Post: "FMEA is blocking gate, not backlog item"
```

---

## Implementation Details

### Workflow Orchestration

| Workflow | Trigger | Purpose | Gate |
|----------|---------|---------|------|
| `enforce-template-requirement.yml` | issue.opened | First line of defense: reject non-template issues | 2 |
| `capa-submission.yml` | issue.opened, issue.edited | Validate CAPA structure, classify severity, check FMEA requirement | 1 |
| `ecr-submission.yml` | issue.opened, issue.edited | Validate ECR structure, extract FMEA decision | 1 |
| `fmea-submission.yml` | issue.opened, issue.edited | Validate FMEA structure, notify stakeholders, block parent | 1 |
| `validate-backlog-item-type.yml` | issue.labeled, issue.unlabeled, issue.edited | Prevent invalid types in backlog | 3 |
| `prevent-fmea-in-backlog.yml` | issue.labeled, issue.unlabeled, issue.edited | Prevent FMEA from backlog column | 4 |

### Label Strategy

**Type Labels (mutually exclusive):**
- `type:capa` - Defect/fix
- `type:ecr` - Feature request (PM level)
- `type:eco` - Feature order (PM approved, implementation ready)
- `type:fmea` - Risk analysis blocking gate

**Backlog Labels:**
- `backlog-item` - In backlog, ready for assignment
- `backlog-ready` - Ready to move to backlog (pending column transition)
- `do-not-backlog` - Should never appear in backlog (e.g., FMEA)

**Status Labels:**
- `status:invalid` - Does not meet governance requirements
- `status:submitted` - Template-valid, awaiting processing
- `status:needs-info` - Incomplete, requesting clarification
- `backlog-validation-failed` - Added to backlog incorrectly

**Violation Labels:**
- `rule-1-violation` - No template used
- `needs-template` - Template missing or incomplete
- `needs-correction` - Invalid item in backlog

---

## Governance Alignment

These gates enforce the governance model defined in:
- **03-CAPA_PROCESS.adoc** - CAPA lifecycle with QA/FMEA/PM gates
- **01-ECR_PROCESS.adoc** - ECR submission and PM review (7-day SLA)
- **02-ECO_PROCESS.adoc** - ECO creation and backlog entry (PM action)
- **04-FMEA_PROCESS.adoc** - FMEA as blocking gate with stakeholder approval

**Key Governance Rules Enforced:**
1. ✅ Template-first: All issues use CAPA/ECR/FMEA template
2. ✅ Type-enforcement: Only ECO and CAPA in backlog
3. ✅ FMEA-separation: FMEA stays in approval workflow, never in development backlog
4. ✅ ECR-gating: ECR reviewed by PM, only ECO proceeds to backlog
5. ✅ Complete audit trail: All violations logged with governance references

---

## Testing & Validation

To verify gates are working:

1. **Gate 1: Create CAPA with missing section**
   - Create issue without CAPA template
   - Workflow rejects with Rule 1 error
   - Issue labeled: `status:invalid`, `needs-template`, `rule-1-violation`

2. **Gate 2: Try to create blank issue**
   - Click "New Issue" → Choose "Blank issue"
   - Workflow closes it immediately
   - Comment explains templates required

3. **Gate 3: Try to add ECR to backlog**
   - Create ECR using ecr.yml template
   - Manually add `backlog-item` label
   - Workflow removes label, posts error
   - Issue labeled: `backlog-validation-failed`, `needs-correction`

4. **Gate 4: Try to add FMEA to backlog**
   - Create FMEA using fmea.yml template
   - Manually add `backlog-item` label
   - Workflow removes label, adds `do-not-backlog`
   - Issue stays in "Awaiting FMEA Approval" column

---

## Future Enhancements

These gates form the foundation. Future work:

1. **ECR → ECO Automation:** Automate PM creation of ECO when ECR approved
2. **FMEA Approval Tracking:** Auto-unblock parent when all stakeholders approve
3. **Kanban Sync:** Sync GitHub Projects columns with issue status labels
4. **SLA Monitoring:** Track time in each gate, alert if SLA exceeded
5. **Governance Reporting:** Generate audit reports of all backlog entries

---

## Summary

**Rule 1 Implementation: COMPLETE ✅**

Four gates now enforce governance-first backlog injection:
- Gate 1: Template structure validation (CAPA, ECR, FMEA)
- Gate 2: Template requirement enforcement (no blank issues)
- Gate 3: Backlog item type validation (only ECO, CAPA)
- Gate 4: FMEA blocking prevention (no FMEA in backlog)

**Result:** Only governance-authorized items enter the backlog. Full audit trail maintained. Developers cannot bypass governance controls.

---

**Commits:**
- `52151a20` - Gate 1: CAPA template + validation
- `addf9efc` - Gate 2: Template enforcement workflow
- `ce735726` - Gates 3 & 4: Backlog validation + FMEA prevention