# Test CAPA Issue Template

## Copy This Entire Text and Paste into GitHub Issues

Use this to create a test CAPA that will trigger the GitHub Actions workflow.

---

```markdown
## CAPA: Integer Overflow in ADD Word - Test Issue

### 1. What is the Problem?

When calling the ADD word (arithmetic addition) with very large positive integers near the maximum value of a 64-bit signed integer, the operation produces incorrect results due to integer overflow.

**Observed Behavior:**
- Adding `9223372036854775807 + 1` produces incorrect value instead of proper overflow handling
- No error message or warning is displayed
- Application continues execution with corrupted data

**Expected Behavior:**
- ADD word should detect overflow and either:
  - Return error flag on stack, or
  - Provide overflow indication to calling code

**Impact:** MAJOR - Core arithmetic operation produces wrong results for large numbers

### 2. How to Reproduce

**Step 1: Build StarForth**
```bash
make clean
make fast
```

**Step 2: Start REPL**
```bash
./build/starforth
```

**Step 3: Execute test code**
```forth
9223372036854775807 1 + . CR
```

**Step 4: Observe result**
- Expected: Error or overflow indication
- Actual: Incorrect integer value (wraparound)

**Environment:**
- StarForth version: v2.0.2
- Platform: x86_64 (Linux)
- Build configuration: fast
- Compiler: GCC 11.4
- Test date: 2025-11-02

### 3. Expected Behavior

The ADD word should properly handle addition of large numbers. For values approaching the maximum 64-bit signed integer limit, the implementation should:

1. Detect when addition would overflow
2. Either clamp to maximum value or signal error
3. Provide feedback to calling code about overflow condition

Current behavior: Silently produces incorrect result (two's complement wraparound)

### 4. Code Location (if known)

Likely affected files:
- `src/word_source/arithmetic_words.c` - ADD word implementation (estimated line 50-80)
- `src/vm.c` - May need addition validation during word execution
- Related: FORTH-79 specification for numeric word semantics

### 5. Severity Assessment

**SEVERITY: MAJOR**

- [x] Major (feature broken)
- [ ] Critical (crash/data loss)
- [ ] Minor (incorrect behavior, workaround exists)
- [ ] Low (cosmetic/documentation)

**Justification:** Core arithmetic operation is fundamental to all FORTH programs. Incorrect results propagate through any code using ADD with large numbers.

### 6. Is this a Regression?

- [x] Unknown (need historical testing)
- [ ] Yes, worked before
- [ ] No, new bug

**Notes:** Need to verify if this is regression from recent changes or pre-existing issue in current version.

### 7. Workaround

**Temporary Workaround:**
Avoid adding numbers near the maximum int64 value. Use smaller number ranges or implement manual overflow checking at FORTH level before calling ADD.

**Not a permanent solution** - core arithmetic must work for valid ranges.

### 8. Test Case

**Minimal Test Case:**
```forth
: TEST-ADD-OVERFLOW
  9223372036854775807 DUP . CR
  1 + . CR ;

TEST-ADD-OVERFLOW
```

**Expected Output:**
```
9223372036854775807
[overflow error or clamped value]
```

**Actual Output:**
```
9223372036854775807
-9223372036854775808
```

---

**Notes for Testing:**
This is a test CAPA issue for CAPA-032 workflow validation.
GitHub Actions should validate template, auto-classify severity, and assign to QA.
```

---

## Instructions

1. **Go to:** https://github.com/rajames440/StarForth/issues/new

2. **Paste the template** above (everything in the ```markdown block)

3. **Click "Submit new issue"**

4. **Watch GitHub Actions:**
   - Go to **Actions** tab
   - Look for `capa-submission` workflow run
   - Watch it validate and label the issue

5. **Expected Results:**
   - Labels applied: `type:capa`, `severity:major`, possibly `regression`
   - GitHub Actions comment with validation results
   - Issue assigned to QA team
   - Added to Kanban board (if configured)

---

## What Will Happen

When you submit this CAPA issue:

```
GitHub Issue Created (#NNN)
         ↓
capa-submission.yml workflow triggers
         ↓
Workflow validates CAPA template
  ✓ Checks for: "What is the Problem?"
  ✓ Checks for: "How to Reproduce"
  ✓ Checks for: "Expected Behavior"
         ↓
Auto-classifies severity
  ✓ Detects "overflow", "broken", "major"
  ✓ Sets label: severity:major
         ↓
Detects regression keywords
  ✓ Looks for "regression", "worked before"
  ✓ May set label: regression
         ↓
Adds labels & assigns to QA
  ✓ type:capa
  ✓ status:submitted
  ✓ severity:major
         ↓
Posts GitHub Actions comment
  ✓ Shows validation results
  ✓ Confirms CAPA received
         ↓
QA Team Notified (if Slack integration enabled)
```

---

## Success Criteria

✅ **Issue Created Successfully** - You can see it on GitHub
✅ **Workflow Triggered** - Actions tab shows `capa-submission` running
✅ **Template Validated** - No template errors in workflow output
✅ **Labels Applied** - Issue has `type:capa` label
✅ **Severity Classified** - Issue has `severity:major` label
✅ **Comment Posted** - GitHub Actions posts validation comment
✅ **Issue Assigned** - If configured, assigned to QA team

---

## Next Steps (After Issue Created)

1. **Monitor Actions tab** to see workflow execute
2. **Check issue** for labels and comments
3. **Review workflow output** to see validation results
4. **Verify labels** applied correctly
5. **Check if assigned** to QA (if Slack/assignment configured)

---

**Ready? Create the issue now and let's see the workflow in action!** 🚀