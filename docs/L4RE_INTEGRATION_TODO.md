# L4Re Integration TODO

## Overview

The platform abstraction layer is complete and working for standalone POSIX builds. This document outlines the specific
steps needed to integrate into the StarshipOS L4Re BID build system.

---

## Pre-Integration Checklist

- [x] Platform abstraction layer implemented
- [x] POSIX backend tested and working
- [x] L4Re backend code written
- [x] Standalone builds work with `make L4RE=1`
- [ ] Platform files copied to StarshipOS tree
- [ ] BID Makefile updated
- [ ] L4Re build tested in StarshipOS tree

---

## Integration Steps

### 1. Copy Platform Files to StarshipOS

**Source Location:** `/home/rajames/CLionProjects/StarForth/src/platform/`

**Destination:** `/home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server/src/platform/`

**Files to Copy:**

```bash
cd /home/rajames/CLionProjects/StarForth
mkdir -p /home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server/src/platform

cp src/platform/platform_init.c \
   /home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server/src/platform/

cp src/platform/time_l4re.c \
   /home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server/src/platform/

# Note: Do NOT copy time_posix.c - not needed in L4Re build
```

**Header File:**

```bash
cp include/platform_time.h \
   /home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server/include/
```

---

### 2. Update BID Makefile

**File:** `/home/rajames/CLionProjects/StarshipOS/l4/pkg/starforth/server/src/Makefile`

**Changes Needed:**

```makefile
# Add platform sources to SRC_C list
SRC_C = \
  main.c \
  log.c \
  io.c \
  vm.c \
  vm_debug.c \
  word_registry.c \
  device_map.c \
  stack_management.c \
  panic.c \
  repl_io.c \
  memory_management.c \
  dictionary_management.c \
  vm_api.c \
  bam.c \
  platform/platform_init.c \     # ← ADD THIS
  platform/time_l4re.c \         # ← ADD THIS
  word_source/arithmetic_words.c \
  # ... rest of existing files ...

# Add librtc to REQUIRES_LIBS
REQUIRES_LIBS = libc_be_l4re libc l4re l4re_c l4sys l4util libsigma0 libblock-device librtc
#                                                                                    ^^^^^^ ADD THIS
```

**Note:** BID already sets `-D__l4__` at line 77-78, so platform selection happens automatically.

---

### 3. Update Core Source Files in StarshipOS

These files need the same changes made to standalone StarForth:

**File:** `server/src/log.c`

- [ ] Add `#include "../include/platform_time.h"` to includes
- [ ] Replace `get_timestamp()` implementation with platform API calls
- [ ] Change: `time(NULL)` → `sf_realtime_ns()`
- [ ] Change: `localtime()` + `strftime()` → `sf_format_timestamp()`

**File:** `server/src/profiler.c` (if it exists in L4Re version)

- [ ] Add `#include "../include/platform_time.h"` to includes
- [ ] Replace `get_time_ns()` implementation
- [ ] Change: `clock_gettime(CLOCK_MONOTONIC)` → `sf_monotonic_ns()`

**File:** `server/src/main.c`

- [ ] Add `#include "platform_time.h"` to includes
- [ ] Add `sf_time_init();` call at start of `main()` (before any logging)

---

### 4. Verify RTC Library Availability

**Check RTC package exists:**

```bash
cd /home/rajames/CLionProjects/StarshipOS
ls -la l4/pkg/rtc/
ls -la l4/build-x86_64/lib/*/librtc.a
```

**Expected files:**

- `l4/pkg/rtc/lib/client/librtc.cc` - RTC client library
- `l4/pkg/rtc/include/rtc` - C++ header
- `l4/pkg/rtc/include/rtc.h` - C header
- `l4/build-x86_64/lib/amd64_gen/l4f/librtc.a` - Compiled library

**If missing:**

```bash
cd l4/pkg/rtc
make
```

---

### 5. Update Loader Configuration

**File:** Choose your loader script (e.g., `l4/conf/modules.list` or a `.lua` file)

**Add RTC server startup:**

```lua
-- Start RTC server (needed for wall-clock time)
ld:start({
  caps = {
    vbus = vbus_l4re,
    icu = L4.default_loader:new_channel(),
  },
  log = L4.Env.log:m("rws", "magenta"),
}, "rom/rtc");

-- StarForth with RTC capability
ld:start({
  caps = {
    rtc = ld:wait("rtc", 1000),  -- Wait for RTC server, timeout 1s
    -- ... other caps ...
  },
  log = L4.Env.log:m("rws", "cyan"),
}, "rom/starforth");
```

**Note:** StarForth will work without RTC capability, but timestamps will show epoch (1970).

---

### 6. Build and Test

**Clean build:**

```bash
cd /home/rajames/CLionProjects/StarshipOS
make clean-l4
make l4
```

**Check for platform files in build:**

```bash
ls -la l4/build-x86_64/pkg/starforth/server/OBJ-amd64_gen/platform/
# Should see: platform_init.o, time_l4re.o
```

**Launch in QEMU:**

```bash
scripts/runos.sh
```

**Verify in StarForth REPL:**

- Logging should show timestamps (may be 1970-01-01 if no RTC server)
- Profiler should work (uses KIP clock, doesn't need RTC)
- No crashes or missing symbol errors

---

## Verification Tests

### Test 1: Basic Functionality

```forth
\ In StarForth REPL:
1 2 + .           \ Should print 3
: TEST 42 . ;     \ Should work
TEST              \ Should print 42
```

### Test 2: Profiler (Monotonic Time)

```bash
# From shell:
./build-x86_64/bin/amd64_gen/l4f/starforth --profile 2 --run-tests --profile-report
```

Should show timing information without errors.

### Test 3: Logging (Real Time)

```bash
./build-x86_64/bin/amd64_gen/l4f/starforth --log-debug
```

Should show timestamps in log output (may be 1970 epoch if no RTC).

### Test 4: RTC Server Integration

```bash
# In QEMU, after starting with RTC server
# Check for RTC capability
grep "RTC" /proc/l4re/caps
```

If RTC server is running and capability provided, timestamps should be correct.

---

## Troubleshooting

### Build Errors

**Error:** `platform_init.c: No such file or directory`
**Fix:** Check that platform directory was created and files copied correctly

**Error:** `undefined reference to l4_kip_clock_ns`
**Fix:** Make sure linking against `l4re` library (should be in REQUIRES_LIBS)

**Error:** `undefined reference to L4rtc::Rtc::get_timer`
**Fix:** Add `librtc` to REQUIRES_LIBS

**Error:** `fatal error: platform_time.h: No such file or directory`
**Fix:** Copy header to `server/include/` directory

### Runtime Errors

**Warning:** "RTC server not found, assuming 1.1.1970"
**Fix:** Add RTC server to loader configuration and provide `rtc` capability

**Issue:** Timestamps show 1970-01-01
**Fix:** Either RTC server not running, or capability not provided to StarForth

**Issue:** Profiler shows 0 elapsed time
**Fix:** Check that `sf_monotonic_ns()` is being called (shouldn't need RTC)

---

## Rollback Plan

If integration fails, revert by:

1. Remove platform files from `SRC_C` in Makefile
2. Remove `librtc` from `REQUIRES_LIBS`
3. Revert changes to `log.c`, `profiler.c`, `main.c`
4. Delete `platform/` directory
5. Rebuild: `make clean-l4 && make l4`

The L4Re build will work as before (using direct POSIX calls via libc).

---

## Notes

- **BID build system** automatically sets `-D__l4__` flag (line 77-78 in Makefile)
- **Platform selection** happens at compile-time based on `__l4__` define
- **No conditional Makefiles needed** - BID handles everything
- **RTC is optional** - StarForth works without it (uses epoch time)
- **Profiler always works** - uses KIP clock, independent of RTC

---

## Success Criteria

- [ ] StarForth builds cleanly in L4Re tree
- [ ] No linker errors for platform functions
- [ ] REPL starts without crashes
- [ ] Logging shows timestamps (even if 1970)
- [ ] Profiler works with `--profile` flag
- [ ] Tests pass with `--run-tests`
- [ ] If RTC server running: correct wall-clock time
- [ ] If no RTC: graceful fallback to epoch time

---

## Future Enhancements

- [ ] Add RTC FORTH words (GET-TIME, SET-TIME)
- [ ] Expose monotonic timer to FORTH (for benchmarking)
- [ ] Add timezone support (currently UTC only)
- [ ] Support for RTC alarm/wakeup features
- [ ] Performance comparison: POSIX vs L4Re backend