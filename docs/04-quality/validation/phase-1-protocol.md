# Phase 1 Governance Validation Protocol

**Date Created:** November 3, 2025
**Protocol Version:** 1.0
**Status:** Ready for Execution
**Type:** Comprehensive System Validation

---

## Protocol Objective

Validate that the Phase 1 GitHub governance workflow system functions correctly end-to-end, with all document types properly routed, approval chains working, and backlog access control enforced.

This protocol tests the complete governance system to ensure it is production-ready before proceeding to Phase 2 vault infrastructure.

---

## Scope

This protocol covers validation of:

1. **Template Recognition** - All 16 document types
2. **Submission Workflows** - Document intake and validation
3. **Approval Chains** - QA/PM approvals trigger correctly
4. **Routing Decisions** - PM decision gate works
5. **Backlog Entry** - Approved documents enter backlog correctly
6. **Vault Routing** - Approved documents route to in_basket
7. **Gatekeeper Enforcement** - Unauthorized access blocked
8. **Security Logging** - All events logged to SEC_LOG.adoc
9. **Label Management** - Labels apply and track correctly
10. **Error Handling** - System provides clear feedback on problems

**Out of Scope:**
- Vault organization and archival (Phase 2)
- Jenkins CI/CD integration (Phase 2)
- Digital signature collection (Phase 2)

---

## Test Environment Setup

### Prerequisites
- [ ] StarForth GitHub repository access
- [ ] StarForth-Governance repository access (read/write)
- [ ] Test user accounts (non-owner) for bypass testing
- [ ] GitHub Actions access logs visible
- [ ] Repository labels created (verify in GitHub settings)
- [ ] Workflows deployed and active (check Actions tab)

### Cleanup from Previous Testing
- [ ] Archive or delete test issues #147-153
- [ ] Clear any test data from in_basket
- [ ] Verify SEC_LOG.adoc is clean

### Documentation Ready
- [ ] BACKLOG_GATEKEEPER_PROCEDURES.md available
- [ ] GOVERNANCE_WORKFLOW_IMPLEMENTATION.md available
- [ ] Template instructions reviewed

---

## Validation Test Cases

### TEST SUITE 1: Template Recognition & Classification

#### Test 1.1: ECO Template Recognition
```
Test ID: VAL-1.1
Document Type: ECO (Engineering Change Order)
Precondition: No issues with type:eco label exist

Steps:
1. Create new GitHub issue using eco.yml template
2. Fill in all required fields:
   - Related ECR reference
   - Acceptance criteria (checklist)
   - Implementation constraints
   - FMEA decision
   - Priority level
   - Effort estimate
   - Success criteria
3. Submit issue

Expected Result:
- Issue created successfully
- Labels automatically applied: type:eco, status:approved
- Issue appears in repository
- No validation errors

Success Criteria:
- ✓ Issue visible in GitHub
- ✓ type:eco label present
- ✓ status:approved label present
- ✓ Issue number recorded for audit trail
```

#### Test 1.2: CAPA Template Recognition
```
Test ID: VAL-1.2
Document Type: CAPA (Corrective/Preventive Action)
Precondition: No CAPA issues exist

Steps:
1. Create new GitHub issue using capa.yml template
2. Fill in all required fields:
   - What is the Problem? (substantive description)
   - When did it happen? (timing)
   - Why did it happen? (root cause)
   - What is the impact? (severity assessment)
   - What fix do we recommend?
   - Evidence/Attachments
   - Regression potential
3. Submit issue

Expected Result:
- Issue created successfully
- Labels automatically applied: type:capa, status:submitted
- Workflow comment posted with validation feedback
- No placeholder text accepted

Success Criteria:
- ✓ Issue visible in GitHub
- ✓ type:capa label present
- ✓ status:submitted label present
- ✓ CAPA submission workflow comment present
```

#### Test 1.3: CER Template Recognition (Sequential Approval)
```
Test ID: VAL-1.3
Document Type: CER (Continuous Engineering Report)
Precondition: No CER issues exist

Steps:
1. Create new GitHub issue using cer.yml template
2. Fill in Section 1 only (Engineering Protocol):
   - Research Objective
   - Methodology & Approach
   - Acceptance Criteria
3. Submit issue (leave Sections 2 & 3 empty)

Expected Result:
- Issue created successfully
- Labels: type:cer, status:submitted, cer-awaiting-protocol-approval
- Workflow comment shows current stage: "Protocol ⏳ → Results ⏳ → Report ⏳"
- System awaits protocol approval before allowing results

Success Criteria:
- ✓ Issue visible in GitHub
- ✓ type:cer label present
- ✓ cer-awaiting-protocol-approval label present
- ✓ Sequential approval structure detected
- ✓ Workflow comment shows progress tracking
```

#### Test 1.4: DHR Template Recognition (Reference Document)
```
Test ID: VAL-1.4
Document Type: DHR (Design History Record - Reference)
Precondition: No DHR issues exist

Steps:
1. Create new GitHub issue using dhr.yml template
2. Fill in required fields:
   - Release version
   - Design snapshot content
   - Architecture overview
   - Key components list
3. Submit issue

Expected Result:
- Issue created successfully
- Labels automatically applied: type:dhr, status:approved
- Auto-vault workflow should trigger (may need slight delay)
- Reference document treated as already approved

Success Criteria:
- ✓ Issue visible in GitHub
- ✓ type:dhr label present
- ✓ status:approved label present (auto-applied)
- ✓ Auto-vault comment posted (or ready to post)
```

#### Test 1.5: Additional Document Types
```
Test ID: VAL-1.5
Document Types: DWG, ENG, SEC, IR, VAL, DTA

For each document type:
Steps:
1. Create GitHub issue using corresponding template
2. Fill in required fields specific to type
3. Submit issue

Expected Result (for each):
- Issue created with correct type:* label
- status:submitted label applied
- Document type correctly classified

Success Criteria:
- ✓ All document types can be submitted
- ✓ Correct labels applied for each type
- ✓ No submission errors or rejections
```

---

### TEST SUITE 2: Approval Workflows

#### Test 2.1: CAPA QA Approval Workflow
```
Test ID: VAL-2.1
Starting State: CAPA issue with status:submitted label
Role: QA Approver

Steps:
1. Using QA/PM account, post approval comment on CAPA issue:
   "✅ APPROVED
   Stakeholder: Quality Lead
   Role: QA Manager
   Date: YYYY-MM-DD
   Notes: Issue well documented and reproducible"

2. Apply label: approved-by-qualm

Expected Result:
- Approval label applied successfully
- Workflow detects approval
- Next stage triggers (PM decision gate)
- Issue transitions from submitted → approved state

Success Criteria:
- ✓ Approval comment visible
- ✓ approved-by-qualm label present
- ✓ Workflow comment indicates next step: PM routing decision
- ✓ No validation errors
```

#### Test 2.2: PM Approval & Routing Decision
```
Test ID: VAL-2.2
Starting State: CAPA with approved-by-qualm label
Role: PM

Steps:
1. Review CAPA issue
2. Apply one of:
   - route:backlog (if this creates implementation work)
   - route:vault (if this is reference/documentation)

Expected Result (for route:backlog):
- Label applied successfully
- Gatekeeper accepts it (approved item → allowed)
- Issue moved to development backlog
- Item available for developer pickup

Expected Result (for route:vault):
- Label applied successfully
- Vault routing workflow triggers
- Document prepared for in_basket
- Metadata file created
- Confirmation comment posted

Success Criteria:
- ✓ Routing label applied
- ✓ No gatekeeper blocking
- ✓ Appropriate workflow triggered
- ✓ Document status updated
```

#### Test 2.3: CER Sequential Approval
```
Test ID: VAL-2.3
Starting State: CER with Section 1 only
Role: PM/Approver

Steps (Phase 1 - Protocol):
1. Review Engineering Protocol section
2. Post approval comment
3. Apply label: cer-protocol-approved

Expected Result:
- Workflow detects protocol approval
- Progress updates to: "Protocol ✅ → Results ⏳ → Report ⏳"
- System waits for Section 2 (Experimental Data)
- Blocks report submission until results approved

Steps (Phase 2 - Results):
1. Add Section 2: Experimental Data & Results
2. Post approval comment
3. Apply label: cer-results-approved

Expected Result:
- Workflow detects results approval
- Progress updates to: "Protocol ✅ → Results ✅ → Report ⏳"
- System waits for Section 3 (Final Report)

Steps (Phase 3 - Report):
1. Add Section 3: Final Engineering Report
2. Post approval comment
3. Apply label: cer-report-approved

Expected Result:
- Workflow detects report approval
- All stages complete: "Protocol ✅ → Results ✅ → Report ✅"
- Status changes to cer-complete, approved-by-projm
- Issue ready for routing decision

Success Criteria:
- ✓ Each stage requires previous approval
- ✓ Progress feedback accurate
- ✓ System blocks advancement without approval
- ✓ Final state triggers routing workflow
```

---

### TEST SUITE 3: Routing & Backlog Entry

#### Test 3.1: Document Routes to Development Backlog
```
Test ID: VAL-3.1
Starting State: Approved CAPA with route:backlog label applied
Expected Destination: Development backlog

Steps:
1. Apply route:backlog label to approved CAPA/CER/DWG issue
2. Observe workflow execution
3. Verify document appears in backlog

Expected Result:
- Label applied successfully
- Workflow processes routing
- Document available for developer assignment
- Issue remains open for tracking
- GitHub issue linked from backlog item

Success Criteria:
- ✓ route:backlog label present
- ✓ Document visible in backlog context
- ✓ Developer can pick up work
- ✓ Audit trail shows routing decision
```

#### Test 3.2: Document Routes to Governance Vault
```
Test ID: VAL-3.2
Starting State: Approved document with route:vault label
Expected Destination: StarForth-Governance/in_basket/

Steps:
1. Apply route:vault label to approved document
2. Observe vault-routing workflow execution
3. Check in_basket directory
4. Verify metadata file created

Expected Result:
- route:vault label applied
- Vault routing workflow triggers
- Document metadata file created in in_basket
- Confirmation comment posted to GitHub issue
- status:in-vault label applied to issue

Success Criteria:
- ✓ route:vault label present
- ✓ in_basket contains new metadata
- ✓ Confirmation comment visible
- ✓ status:in-vault label applied
- ✓ GitHub issue linked in metadata
```

#### Test 3.3: Reference Documents Auto-Vault
```
Test ID: VAL-3.3
Document Types: DHR, DMR, ART, MIN, REL, RMP
Expected Behavior: Auto-approval and auto-vault without review

Steps:
1. Create DHR/DMR/ART/MIN/REL/RMP issue
2. Observe auto-vault workflow
3. Verify automatic routing

Expected Result:
- Reference document detected
- Auto-approved with status:approved label
- Auto-routed with route:vault label
- Confirmation comment: "No review required for reference material"
- Document ready for vault processing

Success Criteria:
- ✓ status:approved applied automatically
- ✓ route:vault applied automatically
- ✓ governance-routed label applied
- ✓ Confirmation comment present
- ✓ No manual approval needed
```

---

### TEST SUITE 4: Gatekeeper Enforcement

#### Test 4.1: Unauthorized Backlog Access Attempt
```
Test ID: VAL-4.1
Test User: Non-owner account (not rajames440)
Starting State: Regular issue without approval labels
Expected Behavior: Bypass detected and blocked

Steps:
1. Create test issue (no template, no type label)
2. Apply route:backlog label (unauthorized attempt)
3. Observe gatekeeper response

Expected Result:
- Gatekeeper workflow detects bypass
- Security warning comment posted
- IR (Incident Report) issue created
- PM assigned for review
- Event logged to SEC_LOG.adoc
- No backlog entry permitted

Success Criteria:
- ✓ Security warning comment present
- ✓ IR issue created with severity:major
- ✓ PM notified (assigned)
- ✓ SEC_LOG.adoc updated
- ✓ Document NOT in backlog
```

#### Test 4.2: Authorized Access - Approved Document
```
Test ID: VAL-4.2
Test User: Non-owner account
Starting State: Approved CAPA issue
Expected Behavior: Access permitted (approved path)

Steps:
1. CAPA issue with approval-by-qualm label exists
2. Apply route:backlog label (approved path)
3. Observe gatekeeper response

Expected Result:
- Gatekeeper validates approval evidence
- route:backlog label accepted
- No security warning posted
- No IR created
- Document enters backlog normally
- No SEC_LOG entry

Success Criteria:
- ✓ No security warning posted
- ✓ No IR created
- ✓ route:backlog label accepted
- ✓ Document in backlog
- ✓ SEC_LOG clean (no false positive)
```

#### Test 4.3: Owner Direct Backlog Access
```
Test ID: VAL-4.3
Test User: Owner account (rajames440)
Starting State: Any issue
Expected Behavior: Owner has direct backlog access

Steps:
1. Owner account applies route:backlog label to any issue
2. Observe gatekeeper response

Expected Result:
- Gatekeeper recognizes owner
- route:backlog label accepted
- No security warning posted
- No IR created
- Document enters backlog

Success Criteria:
- ✓ No security warning posted
- ✓ route:backlog accepted
- ✓ Document enters backlog
- ✓ Owner discretionary authority respected
```

#### Test 4.4: ECO Always Routes to Backlog
```
Test ID: VAL-4.4
Document Type: ECO
Expected Behavior: ECO always permitted to backlog without approval

Steps:
1. Create ECO issue
2. System should recognize type:eco
3. route:backlog should be automatically applied
4. No approval labels needed

Expected Result:
- ECO created with type:eco label
- route:backlog applied automatically or accepted without approval
- No gatekeeper blocking
- Document enters backlog

Success Criteria:
- ✓ type:eco recognized
- ✓ Backlog routing permitted without approval
- ✓ No security blocking
- ✓ ECO in backlog as expected
```

---

### TEST SUITE 5: Security Audit Logging

#### Test 5.1: SEC_LOG.adoc Records All Events
```
Test ID: VAL-5.1
Expected Behavior: All security events logged immutably

Steps:
1. Execute tests 4.1-4.4 (gatekeeper tests)
2. Review SEC_LOG.adoc in governance repo
3. Verify all bypass attempts recorded

Expected Result:
- Unauthorized attempt from Test 4.1 logged
- Timestamp recorded (UTC)
- Actor username recorded
- Event type: "Unauthorized Backlog Access"
- Reference to created IR
- Severity: MAJOR

Success Criteria:
- ✓ SEC_LOG.adoc contains entry for each bypass
- ✓ Format: timestamp | actor | issue | severity | action
- ✓ All unauthorized attempts logged
- ✓ Authorized attempts not logged (no false positives)
- ✓ SEC_LOG is git-versioned (immutable)
```

#### Test 5.2: Incident Report Creation
```
Test ID: VAL-5.2
Starting State: Gatekeeper detected bypass (from Test 4.1)
Expected Behavior: IR issue created with full details

Steps:
1. Find created IR issue
2. Review IR content
3. Verify completeness

Expected Result:
- IR issue created with title: "IR: Unauthorized Backlog Access"
- Body contains:
  - Report Type: Security / Governance Violation
  - Severity: MAJOR
  - Unauthorized User: @testuser
  - Original Issue: #NNN
  - Violation Method: description
  - Expected Process: governance procedure
- Labels: type:ir, status:submitted, severity:major, security-violation
- Linked to original issue

Success Criteria:
- ✓ IR created automatically
- ✓ All required fields present
- ✓ severity:major applied
- ✓ PM can review and take action
- ✓ Full audit trail maintained
```

---

### TEST SUITE 6: Label Management

#### Test 6.1: Type Label Exclusivity
```
Test ID: VAL-6.1
Expected Behavior: Each issue has exactly ONE type:* label

Steps:
1. Attempt to create issue with multiple type labels
   (e.g., apply both type:capa and type:eco)
2. Verify enforcement

Expected Result:
- System detects multiple type labels
- Warning comment posted
- User instructed to use only one type
- Validation workflow enforces exclusivity

Success Criteria:
- ✓ Multiple type labels detected
- ✓ Warning comment posted
- ✓ Guidance provided to correct
```

#### Test 6.2: Status Label Progression
```
Test ID: VAL-6.2
Expected Behavior: Status labels track workflow progression

Steps:
1. Create issue (status:submitted)
2. Approve issue (add approval label)
3. Route issue (add route:* label)
4. Verify status progression

Expected Result:
- status:submitted applied on creation
- Approval labels track review status
- route:backlog or route:vault applied
- status:in-vault applied if routed to vault
- Status progression visible in label history

Success Criteria:
- ✓ Each stage has appropriate status label
- ✓ Labels document progression
- ✓ Audit trail shows status changes
```

---

### TEST SUITE 7: Error Handling & User Feedback

#### Test 7.1: Incomplete Template Detection
```
Test ID: VAL-7.1
Expected Behavior: System detects incomplete submissions

Steps:
1. Create CAPA issue with minimal/placeholder text
2. Verify validation feedback

Expected Result:
- Submission workflow detects incomplete template
- Comment posted: "Template Incomplete"
- Guidance provided on required fields
- status:needs-info label applied
- User can resubmit after fixing

Success Criteria:
- ✓ Validation feedback clear and helpful
- ✓ Specific fields identified as missing
- ✓ Template example provided
- ✓ User can fix and retry
```

#### Test 7.2: Workflow Feedback Comments
```
Test ID: VAL-7.2
Expected Behavior: Workflows provide clear status feedback

Steps:
1. Create various document types
2. Apply labels and move through workflow
3. Review workflow comment feedback

Expected Result:
- Each workflow stage posts status comment
- Comments include:
  - Current status
  - What's needed next
  - Who needs to act
  - Timeline expectations
- Comments are clear and actionable

Success Criteria:
- ✓ Workflow comments visible
- ✓ Feedback is clear and specific
- ✓ Next steps explained
- ✓ Users understand status
```

---

## Execution Plan

### Phase A: Basic Functionality (Day 1)
- [ ] Test Suites 1-2 (Templates and Approvals)
- [ ] Verify all document types can be submitted
- [ ] Verify approval chains work
- Estimated Time: 2-3 hours

### Phase B: Routing & Backlog (Day 2)
- [ ] Test Suite 3 (Routing and Backlog Entry)
- [ ] Verify documents route correctly
- [ ] Verify vault routing works
- [ ] Verify in_basket receives documents
- Estimated Time: 2-3 hours

### Phase C: Security & Enforcement (Day 3)
- [ ] Test Suites 4-5 (Gatekeeper and Audit Logging)
- [ ] Verify bypass attempts blocked
- [ ] Verify authorized access allowed
- [ ] Verify SEC_LOG.adoc updated
- Estimated Time: 2-3 hours

### Phase D: Labels & Feedback (Day 4)
- [ ] Test Suites 6-7 (Label Management and Feedback)
- [ ] Verify label enforcement
- [ ] Verify error handling
- [ ] Verify user feedback quality
- Estimated Time: 1-2 hours

**Total Estimated Time:** 7-11 hours across 4 days

---

## Pass/Fail Criteria

### PASS Conditions (All Must Be Met)
- ✅ All 16 document types can be submitted
- ✅ All approval workflows trigger correctly
- ✅ Documents route correctly to backlog/vault
- ✅ Gatekeeper blocks unauthorized access
- ✅ Authorized access is permitted
- ✅ All events logged to SEC_LOG.adoc
- ✅ Users receive clear feedback
- ✅ No critical errors or crashes
- ✅ Audit trail is complete

### FAIL Conditions (Any One Fails)
- ❌ Document type not recognized
- ❌ Approval workflow doesn't trigger
- ❌ Document doesn't route correctly
- ❌ Gatekeeper allows unauthorized access
- ❌ Gatekeeper blocks authorized access
- ❌ Events not logged to SEC_LOG
- ❌ User feedback unclear or missing
- ❌ System errors or crashes
- ❌ Audit trail incomplete or corrupted

---

## Success Metrics

| Metric | Target | How Measured |
|--------|--------|--------------|
| Template Recognition | 16/16 (100%) | All document types submit successfully |
| Approval Accuracy | 100% | All approvals trigger correct workflows |
| Routing Accuracy | 100% | Documents reach correct destination |
| Gatekeeper Effectiveness | 100% | 0 unauthorized bypasses, all authorizations accepted |
| Audit Logging | 100% | All events logged, no gaps |
| User Feedback | Clear & Actionable | Users understand status without questions |
| System Stability | No crashes | All tests complete without errors |

---

## Reporting & Documentation

### During Execution
- [ ] Record test results in execution log
- [ ] Screenshot successful workflows
- [ ] Document any issues encountered
- [ ] Note timing of each test
- [ ] Capture workflow comments

### After Execution
- [ ] Complete test report (pass/fail for each test)
- [ ] Document any defects found
- [ ] Note any workflow improvements needed
- [ ] Collect metrics and statistics
- [ ] Get sign-off on results

### Deliverables
1. Test Execution Report
2. Defect Report (if any)
3. Metrics Summary
4. Recommendations for Phase 2
5. Sign-off from validator(s)

---

## Rollback Plan

If critical issues are found during validation:

1. **Stop** - Pause Phase 2 preparations
2. **Isolate** - Identify failing workflows
3. **Analyze** - Determine root cause
4. **Fix** - Implement corrections
5. **Re-test** - Validate fix
6. **Resume** - Continue once critical issues resolved

---

## Appendix: Test Data Templates

### Test User Accounts Needed
- [ ] QA Approver account (for approval tests)
- [ ] PM account (for routing decisions)
- [ ] Test Developer account (for bypass attempts)
- [ ] Owner account (rajames440)

### Test Documents to Create
- [ ] Sample CAPA issue
- [ ] Sample ECR issue
- [ ] Sample CER issue
- [ ] Sample DWG issue
- [ ] Sample SEC issue
- [ ] Sample IR issue

### Test Checklists
- [ ] GitHub Actions logs accessible
- [ ] SEC_LOG.adoc writable by workflow
- [ ] in_basket directory accessible
- [ ] All workflows deployed (no syntax errors)
- [ ] All labels created in GitHub
- [ ] Test issues can be created

---

## Sign-Off

**Validation Protocol Prepared By:** Claude Code
**Date:** November 3, 2025
**Version:** 1.0
**Status:** Ready for Execution

**To Execute This Protocol:**
1. Schedule 4-day validation window
2. Prepare test environment (see Prerequisites)
3. Follow test cases in order
4. Document all results
5. Report findings

**Expected Outcome:** Phase 1 system validated and confirmed production-ready for Phase 2 vault infrastructure setup.

---

*This protocol is comprehensive and self-contained. It can be executed by anyone with GitHub/governance repo access and can be repeated as needed for regression testing.*