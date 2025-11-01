# StarForth Workflow & Governance Plan (DRAFT - UNREGULATED)

**Date:** 2025-10-31
**Status:** Draft - Under Development
**Author:** Captain Bob (Robert A. James)
**Audience:** Development team, Quality management, Engineering leadership

---

## PART 1: DECIDED PRINCIPLES & PATTERNS

### 1.1 Roles & Responsibilities

```
PROJECT_MANAGER = "rajames440@gmail.com, Captain Bob, R. A. James"
  Authority: Final approval for major version bumps (X increment)
  Authority: qual → prod promotion decision

QUALITY_MANAGER = [Captain Bob - TBD for long-term]
  Gatekeeper: All generated and manually introduced CAPAs
  Authority: CAPA assignment and approval
  Authority: test → qual promotion decision
  Agent Partner: QUAL_AGENT (Jenkins CI/CD)

ENGINEERING_MANAGER = [Captain Bob - TBD for long-term]
  Gatekeeper: All manually introduced features
  Authority: Feature assignment and approval
  Agent Partner: ENG_AGENT (Jenkins CI/CD)
```

### 1.2 Version Scheme

```
VERSION FORMAT: X.y.z [Production Timestamp] [LTS Status]

X = Major version (PM discretion only)
y = Minor version (increments with every closed feature)
z = Patch version (increments with every closed CAPA)

Example: "StarForth 2.1.47 Production timestamp: 2025-10-31T18:23:45Z LTS"

Embedding:
  - include/version.h (compile-time constants)
  - src/main.c (--version flag)
  - Binary metadata (at runtime)
```

### 1.3 Dual-Stamp Pattern

**Every CI/CD action requires:**

1. **Human Stamp** - Decision maker (QUAL_MGR, PM, ENG_MGR) approves
2. **Agent Stamp** - CI/CD agent (QUAL_AGENT, ENG_AGENT) executes

**Logged in:**

- Git commits (merge author shows agent action)
- Jenkins logs (approval step visible)
- GitHub PR history (approver visible)

**Extensible for multiple teams:**

- Pattern repeats for ENGINEERING_AGENT, TRAINING_AGENT, etc.
- Each team has own gatekeeper + agent
- All share same backlog/kanban board

### 1.4 Document Injection Pattern

**No staging directory. Validated workflows only.**

**Model:**

1. **Generated documents** (from QUAL_AGENT, ENG_AGENT, etc.)
    - Automatically created during workflow execution
    - Directly injected to StarForth-Governance/in_basket/
    - No manual intervention

2. **Human-created documents**
    - Must go through a **validated workflow**
    - Workflow applies validation gates
    - Workflow approval gates before injection
    - Injected to StarForth-Governance/in_basket/ upon approval

3. **Disposition** (in StarForth-Governance)
    - Documents await disposition in in_basket/
    - Moved to Reference/ when ready
    - Never edited; only created and moved

**Key principle:** Humans never directly edit vault. Only validated workflows (human-approved + agent-executed) inject
documents to in_basket.

### 1.5 Shared Backlog & Kanban

- Single GitHub Project board (shared by all teams)
- All teams (QUAL, ENG, TRAINING, etc.) pull from same backlog
- Ticket moves through multiple team workflows
- Accumulates stamps from each team's gatekeeper
- Full audit trail of all approvals

---

## PART 2: GAPS REQUIRING DECISION (To Be Filled)

### 2.1 Feature vs CAPA Distinction

**Question:** When is something a Feature vs a Corrective/Preventive Action?

**Criteria to define:**

- Feature = new functionality, enhancement (y increment)
- CAPA = defect fix, process improvement (z increment)
- Overlap cases? (enhancement that fixes a gap?)

**Decision needed:**

- [ ] Clear definition/criteria
- [ ] Examples of each
- [ ] Who makes the distinction? (Dev? QUAL_MGR?)

---

### 2.2 Closure & Promotion Criteria

**Question:** What constitutes "closed" for CAPA/Feature? What gates each stage?

**Unknowns:**

- Test stage gate: What must pass? (all tests? coverage %?)
- Qual stage gate: What must pass? (compliance check? performance bench?)
- Prod stage gate: What approval? (QUAL_MGR? PM?)
- Rollback triggers: What fails cause rollback?

**Decision needed:**

- [ ] Test stage success criteria
- [ ] Qual stage success criteria
- [ ] Prod stage approval path
- [ ] Failure handling / rollback procedures

---

### 2.3 CAPA vs Feature Workflow Differences

**Question:** Do CAPAs and Features follow the same test → qual → prod path, or different paths?

**Decision needed:**

- [ ] Same workflow path or different?
- [ ] If different, what are the differences?
- [ ] Different approval timelines?
- [ ] Different test requirements?

---

### 2.4 Emergency/Hotfix Flow

**Question:** How do critical production issues bypass normal workflow?

**Decision needed:**

- [ ] Can hotfixes skip qual stage? When?
- [ ] Who authorizes emergency merges?
- [ ] Rollback procedure if hotfix causes new issues?
- [ ] Post-incident CAPA required after hotfix?

---

### 2.5 Release & Versioning Mechanics

**Question:** When and how are versions bumped? Who triggers it? How is it tracked?

**Decision needed:**

- [ ] Auto-increment z/y on CAPA/Feature closure, or manual PM action?
- [ ] Git tag creation: automatic or manual?
- [ ] Release notes: generated or manual? When?
- [ ] Release timing: every CAPA? Weekly? PM discretion?
- [ ] When is version embedded in binary? (every build, every release, only prod builds?)

---

### 2.6 BUILD_MANIFEST Schema

**Question:** What metadata must be captured in each build?

**Schema to define:**

```
BUILD_MANIFEST requirements:
- Commit hash
- Build timestamp
- Compiler version/flags
- Target architecture
- Test results summary
- Code coverage %
- Performance benchmarks
- Linked CAPAs/Features
- Signatory information
- ???
```

**Decision needed:**

- [ ] Complete manifest schema
- [ ] Who generates it? (Make? Jenkins? Script?)
- [ ] Where stored? (Artifact archive?)
- [ ] Checksum/signature required?

---

### 2.7 Governance Document Integration

**Question:** How do controlled documents flow through the workflow?

**Decision needed:**

- [ ] When does a controlled doc get created? (With CAPA? Before? After?)
- [ ] Who routes docs to in_basket? (Dev? QUAL_MGR?)
- [ ] Approval process for routing? (QUAL_MGR checks before moving?)
- [ ] Disposition timeline? (SLA for moving in_basket → Reference?)
- [ ] Who disposes documents? (QUAL_MGR? Documentation team?)

---

### 2.8 Access Control & Branch Protection

**Question:** Who can do what? Which branches are protected?

**Decision needed:**

- [ ] Protected branches: master, prod, qual, test?
- [ ] Who can push to which branches?
- [ ] Require PR reviews? How many? (1, 2, etc.)
- [ ] Force push allowed anywhere? Never?
- [ ] Admin override procedures?

---

### 2.9 Notifications & Communication

**Question:** How do teams stay informed of workflow progress?

**Decision needed:**

- [ ] Slack? Email? GitHub notifications?
- [ ] Who gets notified at each stage?
- [ ] Escalation notifications? (If stuck > N hours?)
- [ ] Summary reports? (Daily? Weekly?)

---

### 2.10 Concurrent Work Management

**Question:** Can multiple CAPAs/Features be in flight simultaneously?

**Decision needed:**

- [ ] Max concurrent CAPAs in each stage?
- [ ] Dependency management? (Can Feature A start if Feature B not closed?)
- [ ] Conflict resolution? (If two CAPAs conflict?)
- [ ] Queueing? (FIFO or priority-based?)

---

### 2.11 Timeline & SLA

**Question:** How long should workflow stages take? Escalation triggers?

**Decision needed:**

- [ ] Target duration: dev → test (1 day? 3 days?)
- [ ] Target duration: test → qual (2 days? 1 week?)
- [ ] Target duration: qual → prod (1 day? 1 week?)
- [ ] Stuck threshold: escalate after how long?
- [ ] Who escalates to? (ENG_MGR? PM?)

---

### 2.12 Artifact Management & Retention

**Question:** Where do builds live? How long kept? Cleanup policy?

**Decision needed:**

- [ ] Artifact storage location
- [ ] Retention policy (keep last N builds? 30 days?)
- [ ] LTS builds: special handling?
- [ ] Cleanup automation or manual?
- [ ] Signed/checksummed artifacts required?

---

### 2.13 Audit Trail & Compliance

**Question:** How to prove audit trail for external auditors?

**Decision needed:**

- [ ] What must be logged? (Commits? PR history? Jenkins logs?)
- [ ] Retention duration? (1 year? 5 years? Forever?)
- [ ] Read-only archive? (Once released, immutable?)
- [ ] Compliance requirements? (SOC2? ISO? Internal standard?)
- [ ] Audit report generation? (Manual review? Automated?)

---

### 2.14 Extensibility: Adding New Teams

**Question:** Template for adding ENGINEERING_AGENT, TRAINING_AGENT, etc.

**Template needed:**

```
When adding new team (e.g., TRAINING):

1. Define role:
   TRAINING_MANAGER = [name/email]
   Gatekeeper for: ???
   Authority: ???

2. Define agent:
   TRAINING_AGENT = Jenkins job: ???
   Executes: ???

3. Define workflow:
   Trigger point: (qual passes? independent?)
   Success criteria: ???
   Failure handling: ???
   Stamp location: (where logged?)

4. Kanban integration:
   New column names: ???
   State transitions: ???

5. Documentation:
   Workflow diagram
   Decision criteria
   Escalation path
```

**Decision needed:**

- [ ] Formalize extensibility pattern
- [ ] Document for future teams

---

## PART 3: INTEGRATION POINTS (To Be Designed)

### 3.1 Jenkins Integration

- How do QUAL_MGR, ENG_MGR approve in Jenkins UI?
- How does Jenkins execute decisions?
- Credential/auth model?

### 3.2 GitHub Integration

- How are tickets linked to git commits?
- PR auto-creation from tickets?
- Label/milestone strategy?

### 3.3 Make/Build System

- How does Makefile interact with versioning?
- BUILD_MANIFEST generation in Makefile?
- Artifact archival?

### 3.4 Git Hooks

- Pre-commit validation?
- Post-merge notifications?
- Automatic branch cleanup?

---

## PART 4: DECISIONS ALREADY MADE (Reference)

- [x] Two separate repos (StarForth + StarForth-Governance)
- [x] in_basket gateway pattern for document routing
- [x] Dual-stamp pattern for all CI/CD actions
- [x] Shared backlog/kanban board for all teams
- [x] Version scheme: X.y.z with timestamp and LTS status
- [x] QUAL_MGR is gatekeeper for CAPAs
- [x] ENG_MGR is gatekeeper for Features
- [x] PM (always Captain Bob initially) approves X version bumps

---

## NEXT STEPS

1. Fill in gaps (sections 2.1 - 2.14) with decisions
2. Design integration points (section 3)
3. Create workflow diagrams
4. Document procedures for each team
5. Implement in Jenkins/GitHub
6. Create developer training docs
7. Formalize as controlled governance document

---

**STATUS:** Unregulated Draft
**NEXT REVIEW:** [Date user specifies]
**APPROVAL CHAIN:** [TBD - will be controlled doc eventually]