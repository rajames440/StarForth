# StarForth Platform Abstraction Layer

StarForth now includes a clean platform abstraction layer that allows building for multiple platforms with a simple
Makefile switch.

## Overview

The platform abstraction provides portable timing/clock functionality across:

- **POSIX** systems (Linux, macOS, BSD, etc.)
- **L4Re/StarshipOS** microkernel

## Architecture

Similar to the `blkio` subsystem, the platform layer uses a **vtable pattern** for zero runtime overhead while
maintaining clean separation between core VM logic and platform-specific code.

```
StarForth Core (platform-agnostic)
    ↓
include/platform_time.h (abstraction API)
    ↓
Runtime platform selection (compile-time)
    ├── src/platform/time_posix.c   (POSIX: clock_gettime)
    └── src/platform/time_l4re.c    (L4Re: RTC server + KIP clock)
```

## Usage

### Building for Different Platforms

```bash
# POSIX build (default - Linux/macOS/BSD)
make

# L4Re/StarshipOS build
make L4RE=1

# Minimal embedded build (no platform layer)
make MINIMAL=1
```

### API Reference

All timing functions are accessed through `include/platform_time.h`:

```c
#include "platform_time.h"

// Initialize platform (call once at startup)
sf_time_init();

// Get monotonic time (for performance measurement)
sf_time_ns_t uptime = sf_monotonic_ns();  // nanoseconds since boot

// Get wall-clock time (Unix epoch)
sf_time_ns_t now = sf_realtime_ns();      // nanoseconds since 1970-01-01

// Set system time (requires privileges on most platforms)
sf_set_realtime_ns(new_time_ns);

// Format timestamp as human-readable string
char buf[SF_TIME_STAMP_SIZE];
sf_format_timestamp(now, buf, 1);  // 1 = 24-hour format
printf("Current time: %s\n", buf);

// Check if RTC is available
if (sf_has_rtc()) {
    printf("RTC available\n");
}
```

### Helper Functions

```c
// Time conversion helpers
uint64_t seconds = 1234567890;
sf_time_ns_t ns = sf_seconds_to_ns(seconds);

uint64_t secs = sf_ns_to_seconds(ns);
uint64_t ms = sf_ns_to_ms(ns);
uint64_t us = sf_ns_to_us(ns);
```

## Implementation Details

### POSIX Backend (`time_posix.c`)

- Uses `clock_gettime(CLOCK_MONOTONIC)` for high-precision monotonic time
- Uses `clock_gettime(CLOCK_REALTIME)` for wall-clock time
- Provides `localtime()` and `strftime()` for timestamp formatting
- Always reports RTC as available (standard POSIX behavior)

### L4Re Backend (`time_l4re.c`)

- **Monotonic time**: L4Re KIP (Kernel Info Page) clock - microsecond counter since boot
  ```c
  l4_kip_clock_ns(l4re_kip())  // Direct CPU timestamp counter access
  ```

- **Real time**: RTC server offset + KIP clock
  ```c
  rtc_offset + l4_kip_clock_ns(l4re_kip())
  ```

- **RTC server integration**:
    - Attempts to get `"rtc"` capability from L4Re environment
    - Queries offset via `get_timer_offset()` RPC
    - Falls back gracefully if RTC server unavailable (reports epoch time)

- **Time setting**: Updates RTC offset via `set_timer_offset()` RPC (requires write capability)

### Platform Selection (`platform_init.c`)

Compile-time platform selection based on `__l4__` define:

```c
void sf_time_init(void) {
#ifdef __l4__
    sf_time_init_l4re();               // L4Re-specific init
    sf_time_backend = &sf_time_backend_l4re;
#else
    sf_time_backend = &sf_time_backend_posix;
#endif
}
```

## Porting to L4Re/StarshipOS

When copying StarForth to `StarshipOS/l4/pkg/starforth`, the build system automatically uses the L4Re backend:

### StarshipOS Makefile Integration

```makefile
# In StarshipOS/l4/pkg/starforth/server/src/Makefile
SRC_C = main.c vm.c log.c profiler.c \
        platform/time_l4re.c platform/platform_init.c

REQUIRES_LIBS = libc_be_l4re libc l4re l4re_c l4sys librtc

CPPFLAGS += -D__l4__=1
```

### Loader Configuration

```lua
-- Start RTC server first
ld:start({
  caps = {
    vbus = vbus_l4re,
  },
}, "rom/rtc");

-- StarForth with RTC capability
ld:start({
  caps = {
    rtc = ld:wait("rtc", 1000),  -- Wait for RTC server
  },
}, "rom/starforth");
```

## Migration from Old Code

The platform abstraction **replaces** direct POSIX calls:

### Before (POSIX-only):

```c
// profiler.c
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// log.c
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%H:%M:%S", tm_info);
}
```

### After (Platform-agnostic):

```c
// profiler.c
#include "platform_time.h"

static uint64_t get_time_ns(void) {
    return sf_monotonic_ns();  // Works on both POSIX and L4Re
}

// log.c
#include "platform_time.h"

static void get_timestamp(char *buffer, size_t size) {
    sf_time_ns_t now = sf_realtime_ns();
    sf_format_timestamp(now, buffer, 1);
}
```

## Benefits

1. **Single codebase**: No more `#ifdef` spaghetti in core files
2. **Zero overhead**: Inline functions compile to direct calls
3. **Type safety**: Strong typing via vtable pattern
4. **Extensibility**: Easy to add new platforms (WASM, baremetal, etc.)
5. **Testability**: Mock implementations possible via vtable swapping

## Future Platforms

The abstraction is designed to support:

- **Baremetal** (direct hardware timer access)
- **WASM** (performance.now() emulation)
- **RTOS** (FreeRTOS, Zephyr, etc.)
- **Custom microkernels** (seL4, etc.)

Adding a new platform requires:

1. Create `src/platform/time_<platform>.c`
2. Implement `sf_time_backend_<platform>` vtable
3. Add platform detection to `platform_init.c`
4. Update Makefile with new platform flag

## Testing

```bash
# Test POSIX build
make clean && make
./build/starforth --run-tests

# Test profiler timing
./build/starforth --profile 2 --run-tests --profile-report

# Test logging timestamps
./build/starforth --log-debug
```

## See Also

- `include/platform_time.h` - Platform API documentation
- `src/platform/time_posix.c` - POSIX implementation
- `src/platform/time_l4re.c` - L4Re implementation
- `src/platform/platform_init.c` - Platform selector
- StarshipOS CLAUDE.md - L4Re RTC architecture details