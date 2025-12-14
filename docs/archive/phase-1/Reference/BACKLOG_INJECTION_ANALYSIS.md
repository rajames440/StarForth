# Backlog Injection Points Analysis

**Analysis Date:** 2025-11-03
**Rule 1:** Governance-first. Only injection points identified in governance model are authorized.

---

## Governance Model Summary

**Authorized backlog items:**
1. **ECO** - Created by PM when ECR approved. Goes to backlog.
2. **CAPA** - Created when defect reported. Goes to backlog after QA validation + FMEA decision.

**Rule 1 Enforcement:** No GitHub issue may be created without a template.

**Disposition:** Always to `StarForth-Governance/in_basket/` (filesystem), not GitHub.

---

## Current State: Templates

| Template | Status | File | Required Field |
|----------|--------|------|---|
| ECR | ✅ Exists | `.github/ISSUE_TEMPLATES/ecr.yml` | `## ECR:` |
| CAPA | ❌ MISSING | N/A | N/A |
| FMEA | ✅ Exists | `.github/ISSUE_TEMPLATES/fmea.yml` | `Related Issue` |
| Generic | ❌ MISSING | N/A | N/A |

**FINDING #1: CAPA template missing.** Workflow validates on keywords, not structured template.

---

## Injection Point Map

### **INJECTION POINT 1: Direct GitHub Issue Creation**

**How it works:**
- User navigates to GitHub repo → Issues → New Issue
- GitHub shows "Choose issue type" dialog
- Options: Pick template OR click "blank issue"

**Current State:**
- ECR template available → routed to ecr-submission.yml
- CAPA validation loose (keyword-based) → bypasses template enforcer
- **BLANK ISSUE allowed** → no validation, no template

**Authorization per Rule 1:** ❌ REJECTED
- Blank issues are NOT authorized
- Must use template

**Gate Needed:**
- Enforce template-only creation
- Reject or auto-close issues not from templates
- Provide workflow that detects and enforces

---

### **INJECTION POINT 2: ECR Workflow → ECO Creation**

**Flow:**
1. ECR created (from ecr.yml template)
2. ecr-submission.yml validates template
3. ecr-submission.yml adds labels: `type:ecr`, `status:submitted`
4. **AUTHORIZED:** PM reviews (7-day SLA)
5. **AUTHORIZED:** PM creates ECO issue manually OR via workflow
6. **AUTHORIZED:** eco-creation.yml adds to Kanban backlog

**Current State:**
- eco-creation.yml exists but may not trigger automatically
- PM manually creates ECO (no automation)

**Authorization per Rule 1:** ✅ AUTHORIZED (if PM manually creates)
- ECO created from approved ECR ✓
- ECO added to backlog ✓

**Gate Needed:**
- Ensure ONLY approved ECR → ECO
- Validate ECO links back to ECR
- Confirm ECO has proper FMEA decision before backlog entry

---

### **INJECTION POINT 3: CAPA Workflow → Backlog**

**Flow:**
1. CAPA created (NO TEMPLATE, keyword-based validation)
2. capa-submission.yml validates loosely
3. capa-submission.yml classifies severity
4. capa-submission.yml checks FMEA requirement
5. capa-to-kanban.yml adds to backlog

**Current State:**
- capa.yml template MISSING
- CAPA validation is regex/keyword-based, not template-enforced
- CAPA can be created as blank issue

**Authorization per Rule 1:** ⚠️ VIOLATION
- CAPA should require strict template
- Currently allows blank/informal issues

**Gate Needed:**
- Create capa.yml template (structured, required fields)
- Update capa-submission.yml to validate template structure
- Reject CAPAs created without template
- Enforce template validation before backlog injection

---

### **INJECTION POINT 4: GitHub API / Programmatic Issue Creation**

**How it works:**
- External tools (Jenkins, Slack bots, email-to-issue)
- GitHub CLI: `gh issue create --title "..."` (no template)
- REST API: `POST /repos/{owner}/{repo}/issues` (no template)

**Current State:**
- No validation on API submissions
- Jenkins could create issues without template
- Slack integration could bypass template

**Authorization per Rule 1:** ❌ REJECTED
- API calls must still honor template requirement
- Cannot create issues without template validation

**Gate Needed:**
- Webhook validation on issue creation
- Reject API submissions not matching template structure
- Require API callers to use template-compatible format

---

### **INJECTION POINT 5: GitHub UI Kanban Manual Drag-Drop**

**How it works:**
- User opens GitHub Project (Kanban board)
- Drags issue from "Backlog" to "In Progress"
- Or manually adds issue to board without validation

**Current State:**
- Kanban allows manual drag-drop without validation
- No gate on which issues can be added to backlog column

**Authorization per Rule 1:** ⚠️ VIOLATION
- Only ECO and CAPA allowed in backlog
- Other types (FMEA, generic) should NOT be backlog items

**Gate Needed:**
- Workflow monitors issue additions to backlog column
- Reject issues missing backlog-ready labels
- Only allow `type:eco` or `type:capa` in backlog column
- Reject FMEA, ECR, etc. if incorrectly added

---

### **INJECTION POINT 6: FMEA Auto-Creation (Blocking Gate)**

**How it works:**
1. CAPA created with high severity
2. capa-submission.yml auto-creates blocking FMEA issue
3. FMEA issue could be incorrectly added to backlog

**Current State:**
- FMEA is blocking gate, NOT backlog item
- FMEA should stay in "Awaiting Approval" status
- Should NOT appear in backlog column

**Authorization per Rule 1:** ⚠️ VIOLATION
- FMEA must NOT be added to backlog
- It's a side trip, not a backlog task

**Gate Needed:**
- Workflow validates issue type before backlog addition
- Reject `type:fmea` from backlog column
- Add explicit label: `do-not-backlog` for FMEA
- Alert if FMEA accidentally moved to backlog

---

### **INJECTION POINT 7: ECR in Backlog (Accidental)**

**How it works:**
- ECR created and labeled `type:ecr`
- User might manually add ECR to backlog (confusion)
- ECR should NEVER be in backlog (only ECO should be)

**Current State:**
- No gate preventing ECR from backlog

**Authorization per Rule 1:** ❌ REJECTED
- ECR is for PM review only
- ECO (not ECR) goes to backlog

**Gate Needed:**
- Workflow rejects `type:ecr` additions to backlog
- Only `type:eco` allowed in backlog
- Auto-move ECR out if accidentally added

---

## Summary: Injection Points & Gates Needed

| Injection Point | Current State | Authorization | Gate Priority |
|---|---|---|---|
| 1. Direct GitHub blank issue | ❌ Allowed | ❌ Rejected | CRITICAL |
| 2. ECR → ECO → Backlog | ⚠️ Manual PM | ✅ Authorized | HIGH |
| 3. CAPA → Backlog | ❌ Loose validation | ⚠️ Partial | CRITICAL |
| 4. GitHub API issue creation | ❌ Unvalidated | ❌ Rejected | HIGH |
| 5. Manual Kanban drag-drop | ⚠️ No label check | ⚠️ Needs validation | HIGH |
| 6. FMEA in backlog | ⚠️ No prevention | ❌ Rejected | HIGH |
| 7. ECR in backlog | ⚠️ No prevention | ❌ Rejected | HIGH |

---

## Gates to Implement

### Gate 1: Template Enforcement (CRITICAL)
- Create `capa.yml` template (matching ECR/FMEA rigor)
- Create workflow to reject non-template issues
- Block blank issue creation

### Gate 2: Backlog Item Type Validation (CRITICAL)
- Only `type:eco` and `type:capa` allowed in backlog column
- Reject FMEA, ECR, generic issues
- Auto-label backlog items: `backlog-item`

### Gate 3: FMEA Blocking Prevention (HIGH)
- Add label: `do-not-backlog` to FMEA
- Workflow rejects if moved to backlog
- Auto-move back to "Awaiting Approval"

### Gate 4: ECR to ECO Transition Validation (HIGH)
- ECR closed → ECO created (automated or manual confirmation)
- ECO must link back to ECR
- Only approved ECR → ECO pathway allowed

### Gate 5: API & External Tool Validation (HIGH)
- Webhook on issue creation validates template structure
- Reject API calls that don't match template format
- Log attempted violations

### Gate 6: Kanban Column Automation (HIGH)
- Monitor backlog column additions
- Validate label: `backlog-ready` or `backlog-item`
- Reject issues missing required labels

---

## Next Action

Which gates would you like to implement first? Recommend order:

1. **Create capa.yml template** (prerequisite for Gate 1)
2. **Template enforcement workflow** (blocks unauthorized issues)
3. **Backlog type validation** (prevents wrong types in backlog)
4. **FMEA blocking prevention** (keeps FMEA out of backlog)