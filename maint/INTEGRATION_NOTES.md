# Integration Scripts Documentation

## Overview

Two complementary sync scripts for bidirectional integration between StarForth and StarshipOS.

## Scripts

### 1. `integrator.sh` (StarForth → StarshipOS) ✅ PRODUCTION READY

- **Location**: `StarForth/maint/integrator.sh`
- **Direction**: StarForth → StarshipOS
- **Source**: StarForth root
- **Destination**: `$STARSHIPOS_ROOT/l4/pkg/starforth/server`
- **Quarantine**: `StarForth/maint/quarantine`
- **Path stripping**: Strips `server/` prefix from files
- **Status**: ✅ PRODUCTION READY (2025-10-19)

### 2. `integrator.sh` (StarshipOS → StarForth) 🆕 NEW

- **Final Location**: `StarshipOS/maint/integrator.sh`
- **Direction**: StarshipOS → StarForth (reverse)
- **Source**: StarshipOS root
- **Destination**: `$STARFORTH_ROOT`
- **Quarantine**: `StarshipOS/maint/quarantine`
- **Path stripping**: Strips `l4/pkg/starforth/server/` prefix from files
- **Status**: Generated based on working integrator.sh, needs testing

## The Rules (Cap't Bob Sez)

1. **Never mkdir into target** — both repos must already exist
2. **Blacklist = Law** — anything matching it is ignored
3. **If file exists at same relative path in destination** → overwrite (green)
4. **Otherwise** → quarantine for review (orange)

## How They Work

Both scripts:

1. Read files from `maint/mergefiles.txt` (or generate from `git diff --name-only HEAD~1`)
2. Check each file against `maint/blacklist.txt`
3. Strip appropriate path prefix to compute destination path
4. If destination exists → overwrite (safe)
5. If destination doesn't exist → quarantine for Captain's review
6. Refresh git index when done

## Next Steps

- [ ] Move `integrator.sh` to `StarshipOS/maint/`
- [ ] Test `integrator.sh` in StarshipOS
- [ ] Verify blacklist.txt and mergefiles.txt exist in StarshipOS/maint/
- [ ] Test round-trip sync (both directions)

## Notes

- Both scripts are meant to be run from their respective repository roots
- Quarantine prevents accidental file creation in unexpected locations
- The path stripping logic is the key difference between the two scripts

---

## Test & Repair Cycle (StarshipOS → StarForth)

### Date: 2025-10-19

### Status: ✅ PRODUCTION READY

This section documents the complete debugging process for `StarshipOS/maint/integrator.sh`. Use this methodology to test
and debug the reverse script (`StarForth/maint/integrator.sh`).

---

### 🐛 Bugs Found & Fixed

#### Bug #1: Blacklist Regex Matching (Line 43) - CRITICAL

**Problem**: Used `grep -Fqx` which treats patterns as **fixed strings**, but `blacklist.txt` contains **regex patterns
** like `(^|/)?Makefile($|[.].*)`.

**Symptom**: Makefiles and blacklisted files were NOT being blocked.

**Original Code**:

```bash
if grep -Fqx -f "$BLACKLIST" <<< "$FILE"; then
```

**Fix**:

```bash
if grep -Eq -f <(sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d' "$BLACKLIST") <<< "$FILE"; then
```

**Changes**:

- `-F` removed (enables regex)
- `-x` removed (allows partial line matching)
- `-E` added (extended regex)
- `sed` filter added (see Bug #2)

---

#### Bug #2: Empty Lines in Blacklist - CRITICAL

**Problem**: Line 5 in `blacklist.txt` is empty. Empty patterns in `grep -f` match **everything**.

**Symptom**: ALL files were being blocked when blacklist was enabled.

**Test**:

```bash
# This blocks EVERYTHING because of empty line:
echo "any_file.txt" | grep -Eq -f blacklist.txt && echo "BLOCKED"

# This works correctly:
echo "any_file.txt" | grep -Eq -f <(sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d' blacklist.txt)
```

**Fix**: Filter out empty lines and comments using `sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d'`

---

#### Bug #3: Case Sensitivity in mergefiles.txt - MINOR

**Problem**: `mergefiles.txt` listed `testing.md` but actual file is `TESTING.md`.

**Symptom**: File skipped by line 40 (`[[ -f "$SRC" ]] || continue`).

**Fix**: Corrected filename in `mergefiles.txt`.

---

### 📋 Testing Methodology

#### Phase 1: Basic Execution Test

```bash
# Run with existing mergefiles.txt
bash maint/integrator.sh
```

**Expected**: Script runs without errors, processes files listed in `mergefiles.txt`.

---

#### Phase 2: Blacklist Enforcement Test

Create comprehensive test file:

```bash
cat > maint/mergefiles_blacklist_test.txt << 'EOF'
l4/pkg/starforth/server/Makefile
l4/pkg/starforth/server/Makefile.inc
l4/pkg/starforth/server/README.md
l4/pkg/starforth/server/src/panic.c
l4/pkg/starforth/server/src/core.c
l4/pkg/starforth/server/Control
maint/integrator.sh
EOF

# Create test file that should be quarantined
touch l4/pkg/starforth/server/src/core.c

# Clear quarantine
rm -rf maint/quarantine/*

# Swap mergefiles and run test
cp maint/mergefiles.txt maint/mergefiles.txt.backup
cp maint/mergefiles_blacklist_test.txt maint/mergefiles.txt
bash maint/integrator.sh
```

**Expected Results**:
| File | Expected Action | Reason |
|------|----------------|---------|
| `Makefile` | 🔴 **BLOCKED** | Matches `(^|/)?Makefile($|[.].*)` |
| `Makefile.inc` | 🔴 **BLOCKED** | Matches `(^|/)?Makefile($|[.].*)` |
| `README.md` | 🟢 **OVERWRITE** | Not blacklisted, exists in StarForth |
| `panic.c` | 🔴 **BLOCKED** | Matches `server/src/panic\.c$` |
| `core.c` | 🟠 **QUARANTINE** | Not blacklisted, doesn't exist in StarForth |
| `Control` | 🔴 **BLOCKED** | Matches `(^|/)?Control($|/)` |
| `integrator.sh` | 🟢 **OVERWRITE** | Not blacklisted, exists in StarForth |

**Verify Blacklist**:

```bash
# Test individual patterns
echo "l4/pkg/starforth/server/Makefile" | grep -Eq -f <(sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d' maint/blacklist.txt) && echo "BLOCKED" || echo "NOT BLOCKED"

echo "l4/pkg/starforth/server/src/panic.c" | grep -Eq -f <(sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d' maint/blacklist.txt) && echo "BLOCKED" || echo "NOT BLOCKED"
```

---

#### Phase 3: Path Stripping Verification

**Test**: Verify that `l4/pkg/starforth/server/` prefix is correctly stripped.

```bash
# Check where files land in destination
FILE="l4/pkg/starforth/server/README.md"
RELATIVE="${FILE#l4/pkg/starforth/server/}"
echo "Source: $FILE"
echo "Destination: $STARFORTH_ROOT/$RELATIVE"
```

**Expected**:

- Source: `l4/pkg/starforth/server/README.md`
- Destination: `/home/rajames/CLionProjects/StarForth/README.md`

**Verify**:

```bash
diff l4/pkg/starforth/server/README.md $STARFORTH_ROOT/README.md && echo "✓ Synced correctly"
```

---

#### Phase 4: Overwrite vs Quarantine Logic

**Rule 3**: If file exists at destination → **overwrite** (green)
**Rule 4**: If file doesn't exist → **quarantine** (orange)

**Test Overwrite**:

```bash
# README.md exists in StarForth root
test -f $STARFORTH_ROOT/README.md && echo "Exists → should OVERWRITE"
```

**Test Quarantine**:

```bash
# core.c doesn't exist in StarForth
test -f $STARFORTH_ROOT/src/core.c || echo "Doesn't exist → should QUARANTINE"

# Verify it's quarantined
test -f maint/quarantine/l4/pkg/starforth/server/src/core.c && echo "✓ Correctly quarantined"
```

---

#### Phase 5: Auto-Generation from Git (Webhook Mode)

**Test**: Script generates `mergefiles.txt` from `git diff --name-only HEAD~1` when file doesn't exist.

```bash
# Remove mergefiles.txt to trigger auto-generation
mv maint/mergefiles.txt maint/mergefiles.txt.manual

# Run script - should auto-generate
bash maint/integrator.sh

# Check what was generated
cat maint/mergefiles.txt
```

**Expected Output**:

```
[INFO] No mergefiles list found — generating diff from last commit.
```

**Verify**:

```bash
git diff --name-only HEAD~1  # Should match mergefiles.txt content
```

---

### ✅ Final Production Test

```bash
# 1. Restore original mergefiles.txt
cp maint/mergefiles.txt.backup maint/mergefiles.txt

# 2. Clear quarantine
rm -rf maint/quarantine/*

# 3. Run production test
bash maint/integrator.sh

# 4. Verify sync
diff maint/integrator.sh $STARFORTH_ROOT/maint/integrator.sh && echo "✓ Synced"
```

**Expected Output**:

```
🛰️  Quark webhook engaged — StarshipOS → StarForth (Reverse Sync)

[Quarantining] l4/pkg/starforth/server/TESTING.md
[Overwriting] maint/integrator.sh

🧾 First Officer's Reverse Report:
  • Destination base: /home/rajames/CLionProjects/StarForth
  • Quarantine:       /home/rajames/CLionProjects/StarshipOS/maint/quarantine
  • Merge list:       2 files processed

Awaiting Cappy's signature: Captain Bob ✍️
```

---

### 🔧 Debugging Commands Reference

#### Check Blacklist Patterns

```bash
# View blacklist with line numbers
cat -n maint/blacklist.txt

# Test specific file against blacklist
FILE="l4/pkg/starforth/server/Makefile"
grep -Eq -f <(sed '/^[[:space:]]*$/d; /^[[:space:]]*#/d' maint/blacklist.txt) <<< "$FILE" && echo "BLOCKED" || echo "NOT BLOCKED"
```

#### Verify Path Stripping

```bash
# For StarshipOS → StarForth
FILE="l4/pkg/starforth/server/src/vm.c"
RELATIVE="${FILE#l4/pkg/starforth/server/}"
echo "$RELATIVE"  # Should output: src/vm.c

# For StarForth → StarshipOS (reverse)
FILE="server/src/vm.c"
RELATIVE="${FILE#server/}"
echo "$RELATIVE"  # Should output: src/vm.c
```

#### Check File Existence

```bash
# Check source
ls -la l4/pkg/starforth/server/TESTING.md

# Check destination
ls -la $STARFORTH_ROOT/TESTING.md

# Check quarantine
ls -la maint/quarantine/l4/pkg/starforth/server/TESTING.md
```

#### Monitor Git Index Refresh

```bash
# Manual refresh (what script does)
cd $STARFORTH_ROOT && git update-index --really-refresh
```

---

### 🎯 Apply to StarForth → StarshipOS Script

Use the same methodology to test `StarForth/maint/integrator.sh`:

**Key Differences**:

1. **Path stripping**: `server/` → `l4/pkg/starforth/server/`
2. **Direction**: StarForth → StarshipOS
3. **Blacklist patterns**: Different (StarForth-specific patterns)

**Same Fixes Needed**:

- Change `grep -Fqx` → `grep -Eq`
- Add `sed` filter for empty lines and comments
- Verify case sensitivity in mergefiles.txt

**Test Pattern**:

```bash
cd /home/rajames/CLionProjects/StarForth

# Phase 1: Basic test
bash maint/integrator.sh

# Phase 2: Blacklist test (create test mergefiles)
# Phase 3: Path stripping test
# Phase 4: Overwrite vs quarantine test
# Phase 5: Auto-generation test
```

---

### 📊 Test Results Summary

| Test Category         | Status | Notes                                        |
|-----------------------|--------|----------------------------------------------|
| Basic Execution       | ✅ PASS | Script runs without errors                   |
| Blacklist Enforcement | ✅ PASS | Regex patterns work, empty lines filtered    |
| Path Stripping        | ✅ PASS | `l4/pkg/starforth/server/` removed correctly |
| Overwrite Logic       | ✅ PASS | Existing files overwritten (README.md, vm.h) |
| Quarantine Logic      | ✅ PASS | New files quarantined (TESTING.md, core.c)   |
| Auto-Generation       | ✅ PASS | Generates from `git diff HEAD~1`             |
| Git Index Refresh     | ✅ PASS | StarForth index updated                      |

**Final Status**: 🚀 **PRODUCTION READY** for git push webhook

---

*Debugged: 2025-10-19*
*Captain Bob ✍️*
---

## Test & Repair Cycle (StarForth → StarshipOS)

### Date: 2025-10-19

### Status: ✅ PRODUCTION READY

This section documents the complete debugging and testing process for `StarForth/maint/integrator.sh`.

---

### 🐛 Bugs Found & Fixed

#### Bug #1: Blacklist "..." Pattern - CRITICAL

**Problem**: Line 12 in `blacklist.txt` contained `...` which is a regex pattern matching any 3 characters.

**Symptom**: ALL files with 3 or more characters were being blocked (basically everything).

**Original Code (blacklist.txt line 12)**:

```
...
```

**Fix**: Removed the `...` line from blacklist.txt

**Impact**: Files like `include/vm.h` were incorrectly blocked before the fix.

---

### 📋 Testing Methodology Applied

All 5 phases from the StarshipOS testing methodology were applied successfully:

#### Phase 1: Basic Execution Test ✅

```bash
bash maint/integrator.sh
```

**Result**: Script runs without errors, processes `include/vm.h`

---

#### Phase 2: Blacklist Enforcement Test ✅

**Test File Created**:

```
Makefile
Makefile.inc
README.md
maint/integrator.sh
Control
LICENSE.spdx
include/vm.h
src/forth.c
```

**Results**:
| File | Action | Status |
|------|--------|---------|
| `Makefile` | 🔴 BLOCKED | ✅ Correct |
| `README.md` | 🟢 OVERWRITE | ✅ Correct |
| `maint/integrator.sh` | 🔴 BLOCKED | ✅ Correct |
| `include/vm.h` | 🟢 OVERWRITE | ✅ Correct |

**Blacklist patterns verified**:

- `(^|/)?Makefile($|[.].*)` - blocks Makefile and Makefile.inc
- `(^|/)?maint($|/)` - blocks maint/integrator.sh
- `(^|/)?Control($|/)` - blocks Control files
- `(^|/)?LICENSE\.spdx$` - blocks LICENSE.spdx

---

#### Phase 3: Path Stripping Verification ✅

**Test**:

```bash
FILE="include/vm.h"
RELATIVE="${FILE#server/}"  # Strips server/ prefix
DEST="$STARSHIPOS_ROOT/l4/pkg/starforth/server/$RELATIVE"
```

**Result**:

- Source: `StarForth/include/vm.h`
- Destination: `StarshipOS/l4/pkg/starforth/server/include/vm.h`
- Files identical: ✅ YES

---

#### Phase 4: Overwrite vs Quarantine Logic Test ✅

**Overwrite Test**:

- Files existing in both repos: `include/vm.h`, `README.md`
- Action: 🟢 Overwritten correctly

**Quarantine Test**:

- Created: `test_new.c` (doesn't exist in StarshipOS)
- Action: 🟠 Quarantined to `maint/quarantine/test_new.c`
- Verified NOT copied to StarshipOS: ✅ Correct

---

#### Phase 5: Auto-Generation from Git Test ✅

**Test**: Removed `mergefiles.txt`, script auto-generated it from `git diff --name-only HEAD~1`

**Output**:

```
[INFO] No mergefiles list found — generating diff from last commit.
```

**Generated content matched git diff**: ✅ YES

---

### ✅ Final Production Test

```bash
# Restored original mergefiles.txt
# Cleared quarantine
bash maint/integrator.sh
```

**Output**:

```
🛰️  Quark webhook engaged — StarForth → StarshipOS (Final Cut)

[Overwriting] include/vm.h

🧾 First Officer's Report:
  • Destination base: /home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server
  • Quarantine:       /home/rajames/CLionProjects/StarForth/maint/quarantine
  • Merge list:       1 files processed

Awaiting Cappy's signature: Captain Bob ✍️
```

**Verification**:

```bash
diff StarForth/include/vm.h StarshipOS/l4/pkg/starforth/server/include/vm.h
# ✅ Files identical
```

---

### 📊 Test Results Summary (StarForth → StarshipOS)

| Test Category         | Status | Notes                                |
|-----------------------|--------|--------------------------------------|
| Basic Execution       | ✅ PASS | Script runs without errors           |
| Blacklist Enforcement | ✅ PASS | All patterns working correctly       |
| Path Stripping        | ✅ PASS | `server/` prefix handled correctly   |
| Overwrite Logic       | ✅ PASS | Existing files overwritten correctly |
| Quarantine Logic      | ✅ PASS | New files quarantined correctly      |
| Auto-Generation       | ✅ PASS | Generates from `git diff HEAD~1`     |
| Git Index Refresh     | ✅ PASS | StarshipOS index updated             |

**Final Status**: 🚀 **PRODUCTION READY** for git pre-push webhook

---

### 🔧 Key Differences from StarshipOS→StarForth Script

| Aspect             | StarForth → StarshipOS                                | StarshipOS → StarForth                  |
|--------------------|-------------------------------------------------------|-----------------------------------------|
| **Path Stripping** | `server/`                                             | `l4/pkg/starforth/server/`              |
| **Destination**    | `$STARSHIPOS_ROOT/l4/pkg/starforth/server`            | `$STARFORTH_ROOT`                       |
| **Quarantine**     | `StarForth/maint/quarantine`                          | `StarshipOS/maint/quarantine`           |
| **Example**        | `include/vm.h` → `StarshipOS/.../server/include/vm.h` | `l4/.../server/vm.h` → `StarForth/vm.h` |

---

*Tested & Verified: 2025-10-19*
*Captain Bob ✍️*
