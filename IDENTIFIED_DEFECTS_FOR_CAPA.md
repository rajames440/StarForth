# Identified Defects & TODOs → CAPA Issues

**Date Scanned:** 2025-10-30
**Severity:** All are LOW to MEDIUM (no CRITICAL defects found)
**Status:** Ready to create as GitHub CAPA issues

---

## PRODUCTION CODE TODOs (Must Address Before Phase 2)

### CAPA-001: L4Re ROMFS Implementation

**Location:** `src/word_source/starforth_words.c:226`

**Description:** L4Re ROMFS integration not yet implemented. Currently reads init.4th from file system, but needs ROMFS support for embedded deployment.

**Type:** Feature gap / Enhancement

**Severity:** MEDIUM (blocks Phase 2 StarshipOS)

**Details:**
```c
/* TODO: L4Re ROMFS implementation */
// Currently: Reads init.4th from file path
// Needed: Read init.4th from ROMFS filesystem provided by L4Re
```

**Acceptance Criteria:**
- [ ] ROMFS support integrated into INIT word
- [ ] init.4th loadable from ROMFS without file system
- [ ] Fallback to file system if ROMFS unavailable
- [ ] Tests pass with ROMFS backend

**Related:** Blocks phase 2 deployment, StarshipOS integration

**Effort Estimate:** MEDIUM (2-3 days)

**Suggested Implementation:** Create L4Re ROMFS integration layer

---

### CAPA-002: Processor Count Detection from KIP

**Location:** `src/physics_runtime.c` (exact line varies)

**Description:** Currently hardcoded to 1 CPU. Should query L4Re KIP (Kernel Info Page) for actual processor count.

**Type:** Enhancement / Performance

**Severity:** LOW (current hardcoded value works, but prevents multi-core optimization)

**Details:**
```c
out->cpu_count = 1u; /* TODO: query processor count from KIP once available */
```

**Acceptance Criteria:**
- [ ] Query processor count from L4Re KIP
- [ ] Return actual core count (not hardcoded 1)
- [ ] Fallback to 1 if KIP unavailable
- [ ] Tests verify correct count on multi-core systems

**Related:** Phase 2 performance optimization, StarshipOS foundation

**Effort Estimate:** SMALL (1 day)

**Suggested Implementation:** Use L4Re KIP API to detect CPU count

---

## DOCUMENTATION TODOS (Lower Priority, Informational)

### CAPA-003: Formal Refinement Proofs

**Location:** `Makefile` + `docs/REFINEMENT_ANNOTATIONS.adoc`

**Description:** Refinement proofs between Isabelle specification and C implementation not yet complete. Multiple functions marked TODO_PROOF.

**Type:** Formal Verification

**Severity:** MEDIUM (Phase 2 requirement for IEC 62304)

**Details:**
- Currently using `sorry` (Isabelle placeholder) in some proofs
- Need complete refinement proof from *.thy to C code
- Blocks certification for medical device use

**Status:** Scaffolding complete, proofs in progress

**Timeline:** Multi-sprint effort (Phase 2)

---

## PRODUCTION TODO SUMMARY

| ID | File | Line | Type | Severity | Effort | Blocker |
|----|------|------|------|----------|--------|---------|
| CAPA-001 | starforth_words.c | 226 | Feature | MEDIUM | 2-3 days | Phase 2 |
| CAPA-002 | physics_runtime.c | ? | Enhancement | LOW | 1 day | No |
| CAPA-003 | Makefile/docs | Multiple | Formal Proof | MEDIUM | Sprint(s) | Phase 2 |

---

## How to Create These as GitHub Issues

Use the CAPA GitHub issue template. Each CAPA should include:

```markdown
## CAPA-001: L4Re ROMFS Implementation

### What is the Problem?
L4Re ROMFS integration not implemented in INIT word.
Currently reads init.4th from file system, needs ROMFS support.

### How to Reproduce
1. Boot StarForth on L4Re system
2. INIT word attempts to load init.4th
3. Currently requires file system; should use ROMFS

### Expected Behavior
INIT word loads init.4th from ROMFS provided by L4Re.

### Type of Change
- [x] Feature (new ROMFS backend)
- [ ] Bug fix
- [ ] Enhancement

### Severity Assessment
- [ ] CRITICAL
- [x] MAJOR (blocks Phase 2)
- [ ] MINOR
- [ ] DOCUMENTATION

### Related Code
- File: src/word_source/starforth_words.c
- Function: INIT word implementation
- TODO at line 226
```

---

## Next Steps

**To integrate into Kanban:**

1. **Create GitHub Issues** for CAPA-001 and CAPA-002 in this repo
   - Use CAPA template from Chapter 3: CAPA_PROCESS.adoc
   - Label: `type:capa` `severity:medium` etc.

2. **Add to GitHub Projects** (Kanban board)
   - Backlog column → CAPAs ready to schedule
   - Link to QA validation workflow

3. **Assign Priority**
   - CAPA-001 (ROMFS): HIGH (Phase 2 dependency)
   - CAPA-002 (CPU count): MEDIUM (optimization)
   - CAPA-003 (Proofs): HIGH (Phase 2 certification)

4. **Update GOVERNANCE_REFERENCE_MANUAL.adoc** once issues created
   - Add actual GitHub issue numbers (#NNN)
   - Link to cross-reference guide

---

## Verification Checklist

Once CAPAs created:

- [ ] CAPA-001 has GitHub issue number
- [ ] CAPA-002 has GitHub issue number
- [ ] CAPA-003 has GitHub issue number
- [ ] All 3 added to GitHub Projects / Kanban board
- [ ] Backlog column populated with CAPAs
- [ ] Labels applied (type:capa, severity, effort)
- [ ] Ready for developer assignment

---

**Status:** Ready to proceed with CAPA creation
**Updated:** 2025-10-30