# StarForth Standards Quick Reference

**One-page reference for standards validation decisions**

---

## Which Standards Apply? Quick Decision Tree

### Question 1: What validation level do you need?

| If you need... | Target Layer | Standards | Effort | Cost |
|----------------|--------------|-----------|--------|------|
| Basic commercial credibility | Layer 1 | ISO 12207, 25010, 29119 | 4-8 weeks | $10K-30K |
| Production-grade quality | Layer 1-2 | Add: ISO 5055, CERT C, NIST SSDF | 10-20 weeks | $30K-110K |
| Safety certification | Layer 3 | IEC 61508, domain-specific | 6-24 months | $50K-500K+ |
| Security certification | Layer 4 | Common Criteria (CC) | 12-24 months | $100K-1M |
| Highest assurance (formal proof) | Layer 5 | seL4 platform or formal verification | 2-36 months | $50K-2M |

**Recommendation: Start with Layers 1-2 for v1.0**

---

## Standards Applicability Matrix

### ✅ YES - Directly Applicable (Do These)

| Standard | What It Proves | When to Do | Tool/Cost |
|----------|---------------|------------|-----------|
| **ISO/IEC/IEEE 12207** | Professional lifecycle | v1.0 Layer 1 | Documentation ($10K) |
| **ISO/IEC 25010** | Quality characteristics | v1.0 Layer 1 | Documentation ($5K) |
| **ISO/IEC/IEEE 29119** | Testing standards | v1.0 Layer 1 | Documentation ($10K) |
| **ISO/IEC 5055** | Code quality metrics | v1.0 Layer 2 | Static analysis ($5K-30K) |
| **SEI CERT C** | Secure coding | v1.0 Layer 2 | Checker ($5K-20K) |
| **CWE Top 25** | Vulnerability analysis | v1.0 Layer 2 | Static analysis ($5K) |
| **NIST SP 800-218** | Secure development | v1.0 Layer 2 | Documentation ($10K) |
| **ISO/IEC 27034** | Application security | v1.0 Layer 4 | Documentation ($10K-30K) |

**Total v1.0 (Layers 1-2): $30K-110K, 10-20 weeks**

---

### ⚠️ CONDITIONAL - Only If Customer Requires

| Standard | What It Proves | When to Do | Trigger |
|----------|---------------|------------|---------|
| **MISRA C:2023** | Safety-critical C coding | v1.0 if budget | Customer requires OR targeting safety |
| **IEC 61508** | Functional safety (SIL) | When needed | Safety-critical deployment |
| **ISO 26262** | Automotive safety (ASIL) | When needed | Automotive deployment |
| **DO-178C** | Aviation safety (DAL) | When needed | Avionics deployment |
| **IEC 62304** | Medical device software | When needed | Medical device deployment |
| **ISO/IEC 15408** | Common Criteria (EAL) | When needed | Government/defense customer |
| **NIST SP 800-53** | Federal security controls | When needed | Federal system integration |
| **NIST SP 800-171** | DoD CUI protection | When needed | DoD contractor requirement |

**Don't pursue these speculatively - wait for customer requirement**

---

### ❌ NO - Not Applicable

| Standard | Why Not |
|----------|---------|
| **ISO 9001** | Organizational QMS, not product certification |
| **ISO/IEC 27001** | Organizational ISMS, not product certification |
| **CMMI** | Organizational maturity, not product validation |

---

## v1.0 Recommended Package

**Goal:** Strong commercial defensibility with professional engineering validation

### Phase 1: Layer 1 Documentation (4-8 weeks, $10K-30K)

**What to Create:**
1. Requirements traceability matrix (FORTH-79 → code → tests)
2. Software development process documentation
3. Design documentation (consolidate from code)
4. Test documentation per ISO 29119
5. Quality characteristics per ISO 25010
6. Configuration management plan

**Deliverables:**
- Software Requirements Specification (SRS)
- Software Design Document (SDD)
- Test Plan and Test Results Report
- Quality Assurance Plan
- Configuration Management Plan

---

### Phase 2: Layer 2 Analysis (6-12 weeks, $20K-80K)

**What to Do:**
1. Run ISO 5055 static analysis (SonarQube, Coverity, etc.)
2. Run CERT C checker (Rosecheckers or commercial)
3. Run CWE Top 25 scanner
4. Document security practices (NIST SSDF attestation)
5. Remediate findings (expected: minimal)
6. Create compliance reports

**Tools Needed:**
- Static analyzer with ISO 5055 support ($0-30K)
  - Free: SonarQube Community, cppcheck, Clang Static Analyzer
  - Commercial: SonarQube Enterprise, Coverity, CodeSonar
- CERT C checker ($0-20K)
  - Free: Rosecheckers (SEI)
  - Commercial: LDRA, CodeSonar, Klocwork

**Deliverables:**
- Code Quality Baseline Report (ISO 5055)
- CERT C Compliance Report
- CWE Top 25 Analysis
- NIST SSDF Attestation
- Remediation Report

---

## Tool Recommendations by Budget

### Budget: $0-10K (Open Source + Documentation)

**Tools:**
- SonarQube Community Edition (free)
- cppcheck (free)
- Clang Static Analyzer (free)
- Rosecheckers for CERT C (free)

**Limitations:**
- Partial ISO 5055 coverage
- No MISRA C:2023 support
- Manual correlation of results

**Outcome:** Basic Layer 2 coverage, good for v1.0

---

### Budget: $10K-50K (Commercial Tools)

**Tools:**
- SonarQube Developer/Enterprise ($5K-15K)
- Coverity Scan commercial ($10K-30K)
- CERT C commercial checker ($5K-10K)

**Benefits:**
- Better ISO 5055 coverage
- Integrated reporting
- Better CWE mapping

**Outcome:** Strong Layer 2 coverage

---

### Budget: $50K-100K (Full Suite)

**Tools:**
- Above plus:
- MISRA C:2023 checker (LDRA, QA-C, Polyspace) ($20K-50K)
- Advanced static analysis (CodeSonar, Klocwork) ($30K-50K)

**Benefits:**
- Full MISRA C:2023 compliance
- Comprehensive security and quality analysis
- Certification-grade reports

**Outcome:** Strong Layer 2 + optional MISRA compliance

---

## Certification Decision Matrix

### When to Pursue Safety Certification (Layer 3)

**Pursue IEC 61508 if:**
- [ ] Customer explicitly requires SIL certification
- [ ] Deploying in industrial control system
- [ ] Deploying in safety-related embedded system
- [ ] You have system context for hazard analysis
- [ ] Budget available: $50K-500K+
- [ ] Timeline allows: 6-24 months

**Pursue ISO 26262 if:**
- [ ] Targeting automotive deployment
- [ ] OEM partnership established
- [ ] ASIL level determined by system analysis
- [ ] Budget available: $200K-2M
- [ ] Timeline allows: 12-24 months

**Pursue DO-178C if:**
- [ ] Targeting avionics deployment
- [ ] Aerospace partnership established
- [ ] DAL level determined
- [ ] Budget available: $500K-5M
- [ ] Timeline allows: 24-36+ months

**Pursue IEC 62304 if:**
- [ ] Targeting medical device deployment
- [ ] Regulatory pathway defined (FDA/MDR)
- [ ] Safety class determined
- [ ] Budget available: $100K-1M
- [ ] Timeline allows: 12-24 months

**If NONE of the above are checked: DON'T PURSUE - Focus on Layers 1-2**

---

### When to Pursue Security Certification (Layer 4)

**Pursue Common Criteria if:**
- [ ] Government customer requires CC certification
- [ ] Defense/intelligence deployment
- [ ] International market requires CC
- [ ] Budget available: $100K-1M
- [ ] Timeline allows: 12-24 months

**Instead, do ISO 27034 documentation if:**
- [ ] Want to demonstrate security-conscious design
- [ ] Need application security framework
- [ ] Budget: $10K-30K
- [ ] Timeline: 4-8 weeks

**If customer doesn't require CC: Skip certification, do ISO 27034 documentation only**

---

## Phase 2 (v2.0) - seL4 Strategy

**Target:** Layer 5 platform assurance

**What seL4 Provides:**
- Formally verified microkernel
- Proven memory safety and isolation
- Proven security properties
- Platform-level mathematical correctness

**What to Do:**
1. Port StarForth to seL4 (already planned Phase 2)
2. Validate functionality on seL4
3. Document platform assurance claims
4. Position as "verified platform execution"

**Effort:** 2-6 months
**Cost:** $50K-150K
**Timeline:** v2.0 (2026)

**Claim:** "StarForth executes on seL4 - the world's first formally verified microkernel"

---

## Common Mistakes to Avoid

### ❌ DON'T Do These:

1. **Don't pursue safety certification without system context**
   - Safety standards require system-level hazard analysis
   - Can't certify a component in isolation

2. **Don't pursue Common Criteria speculatively**
   - Very expensive ($100K-1M)
   - Wait for customer requirement

3. **Don't claim organizational certifications for product**
   - ISO 9001 certifies the organization, not StarForth
   - ISO 27001 certifies the organization, not StarForth

4. **Don't attempt full formal verification for v1.0**
   - Research-level effort (12-36 months, $500K-2M)
   - Not necessary for most use cases
   - seL4 (v2.0) provides realistic path to formal assurance

5. **Don't claim domain-specific compliance without targeting that domain**
   - Don't claim ISO 26262 unless targeting automotive
   - Don't claim DO-178C unless targeting aviation
   - Don't claim IEC 62304 unless targeting medical

---

## What to Say to Stakeholders

### General Commercial Customers

**Claim (v1.0 with Layers 1-2):**
"StarForth is developed using professional software engineering practices with documented lifecycle processes (ISO/IEC 12207), quality characteristics validation (ISO 25010), comprehensive testing (ISO 29119), secure coding compliance (CERT C), automated code quality measurement (ISO 5055), and secure development framework (NIST SSDF). Our defensive architecture provides reliability and security suitable for production systems."

---

### Safety-Critical Customers

**Claim (Current):**
"StarForth's deterministic architecture and defensive design are consistent with IEC 61508 safety principles. We can support your system-level safety analysis and pursue domain-specific certification if required for your deployment."

**Don't Claim:** "StarForth is IEC 61508 certified" (not true without system context)

---

### High-Assurance Customers

**Claim (v2.0 with seL4):**
"StarForth Phase 2 executes on seL4 - the world's first formally verified microkernel - providing platform-level mathematical proof of memory safety, isolation, and security properties. This achieves the highest level of platform assurance available today."

---

### Defense/Government Customers

**Claim (v1.0 Layer 2 + ISO 27034):**
"StarForth implements secure development practices per NIST SP 800-218 (SSDF), application security framework per ISO 27034, and can support system integration for NIST SP 800-53 or SP 800-171 compliant systems. Common Criteria evaluation available if required."

---

## Next Steps Checklist

### For v1.0 (Next 3-6 Months)

**Layer 1 (Foundation):**
- [ ] Create requirements traceability matrix
- [ ] Document software development process
- [ ] Formalize design documentation
- [ ] Create test documentation per ISO 29119
- [ ] Document quality characteristics per ISO 25010
- [ ] Create configuration management plan

**Layer 2 (Quality/Security):**
- [ ] Select and procure static analysis tools
- [ ] Run ISO 5055 code quality analysis
- [ ] Run CERT C compliance check
- [ ] Run CWE Top 25 vulnerability scan
- [ ] Document NIST SSDF practices
- [ ] Remediate identified issues
- [ ] Create compliance reports
- [ ] (Optional) Run MISRA C:2023 if budget allows

**Layer 4 (Security Documentation):**
- [ ] Document security architecture per ISO 27034
- [ ] Create application security controls inventory
- [ ] Document threat model
- [ ] Perform security testing (fuzzing)

**Expected Outcome:** Strong commercial validation, defensible claims

---

### For v2.0 (12-18 Months)

**Layer 5 (Platform Assurance):**
- [ ] Complete seL4 port (Phase 2)
- [ ] Validate functionality on seL4
- [ ] Document platform assurance claims
- [ ] Position as formally verified platform execution

**Expected Outcome:** Highest assurance for platform execution

---

### Conditional (As Needed)

**Only if customer requires:**
- [ ] Safety certification (Layer 3) - Engage consultant, determine domain/level
- [ ] Security certification (Layer 4) - Engage CC evaluation lab
- [ ] Formal verification (Layer 5) - Research partnership

---

## Summary: The Practical Path

**v1.0 (Now - Q2 2025):**
- Focus: Layers 1-2
- Effort: 10-20 weeks
- Cost: $30K-110K
- Outcome: Strong commercial defensibility

**v2.0 (2026):**
- Focus: seL4 port (Layer 5 platform)
- Effort: 2-6 months
- Cost: $50K-150K
- Outcome: Formally verified platform execution

**v3.0+ (2027+):**
- Focus: Conditional investments based on customer requirements
- Options: Safety cert, security cert, formal verification
- Trigger: Actual customer requirement, not speculation

**Key Principle: Build strong foundation first, pursue advanced certifications only when customers require them.**

---

**Last Updated:** October 25, 2025
**See Also:**
- `STANDARDS_APPLICABILITY_ANALYSIS.adoc` (comprehensive 77KB analysis)
- `STANDARDS_SUMMARY.md` (executive summary)