# StarForth on Raspberry Pi 4 - Build Guide

## Overview

This guide covers building and optimizing StarForth for Raspberry Pi 4 with ARM64 assembly optimizations.

## Hardware Specifications

| Component    | Specification                                    |
|--------------|--------------------------------------------------|
| CPU          | Broadcom BCM2711 (Quad-core Cortex-A72 @ 1.5GHz) |
| Architecture | ARMv8-A (64-bit)                                 |
| L1 Cache     | 32KB I + 32KB D per core                         |
| L2 Cache     | 1MB shared                                       |
| RAM          | 1GB/2GB/4GB/8GB LPDDR4-3200                      |
| NEON         | Yes (Advanced SIMD)                              |
| CRC32        | Yes                                              |

## Quick Start

### Option 1: Build Natively on Raspberry Pi

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential git

# Clone repository
git clone <your-repo-url> starforth
cd starforth

# Build with ARM64 optimizations
make clean
make CFLAGS="-std=c99 -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -Iinclude -Isrc/word_source -Isrc/test_runner/include" LDFLAGS=""

# Test
./build/starforth
```

### Option 2: Cross-Compile from x86_64 Linux

```bash
# Install cross-compiler
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu

# Build
make clean
make CC=aarch64-linux-gnu-gcc \
     CFLAGS="-std=c99 -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -Iinclude -Isrc/word_source -Isrc/test_runner/include" \
     LDFLAGS="-static"

# Copy to Raspberry Pi
scp build/starforth pi@raspberrypi.local:~/
```

## Makefile Integration

Add ARM64-specific targets to your Makefile:

```makefile
# Raspberry Pi 4 native build
rpi4:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 -DUSE_ASM_OPT=1 -DARCH_ARM64=1" LDFLAGS=""

# Cross-compile for Raspberry Pi 4
rpi4-cross:
	$(MAKE) CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 -DUSE_ASM_OPT=1 -DARCH_ARM64=1 -static" \
	        LDFLAGS="-static"

# Performance build with direct threading
rpi4-perf:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DARCH_ARM64=1 -flto" LDFLAGS="-flto"
```

## Code Integration

### 1. Architecture Detection Header

Create `include/arch_detect.h`:

```c
#ifndef ARCH_DETECT_H
#define ARCH_DETECT_H

/* Detect architecture at compile time */
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X86_64 1
    #define ARCH_ARM64 0
    #include "vm_asm_opt.h"
    #include "vm_inner_interp_asm.h"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_X86_64 0
    #define ARCH_ARM64 1
    #include "vm_asm_opt_arm64.h"
    #include "vm_inner_interp_arm64.h"
#else
    #define ARCH_X86_64 0
    #define ARCH_ARM64 0
    #warning "Unknown architecture, assembly optimizations disabled"
#endif

/* Architecture-agnostic wrapper macros */
#if ARCH_X86_64
    #define vm_push_opt(vm, val) vm_push_asm(vm, val)
    #define vm_pop_opt(vm) vm_pop_asm(vm)
    #define NEXT_OPT() NEXT_ASM()
    #define PRIM_DUP_OPT() PRIM_DUP()
    #define PRIM_DROP_OPT() PRIM_DROP()
    #define PRIM_SWAP_OPT() PRIM_SWAP()
    #define PRIM_PLUS_OPT() PRIM_PLUS()
    /* ... other primitives ... */
#elif ARCH_ARM64
    #define vm_push_opt(vm, val) vm_push_asm(vm, val)
    #define vm_pop_opt(vm) vm_pop_asm(vm)
    #define NEXT_OPT() NEXT_ARM64()
    #define PRIM_DUP_OPT() PRIM_DUP_ARM64()
    #define PRIM_DROP_OPT() PRIM_DROP_ARM64()
    #define PRIM_SWAP_OPT() PRIM_SWAP_ARM64()
    #define PRIM_PLUS_OPT() PRIM_PLUS_ARM64()
    /* ... other primitives ... */
#else
    #define vm_push_opt(vm, val) vm_push(vm, val)
    #define vm_pop_opt(vm) vm_pop(vm)
    #define NEXT_OPT() do {} while(0)
    /* ... fallback to C implementations ... */
#endif

#endif /* ARCH_DETECT_H */
```

### 2. Update Stack Management

In `src/stack_management.c`:

```c
#include "../include/vm.h"
#include "../include/arch_detect.h"
#include "../include/log.h"
#include "../include/profiler.h"

void vm_push(VM *vm, cell_t value) {
    PROFILE_INC_STACK_OP();

#if USE_ASM_OPT
    vm_push_opt(vm, value);
    if (vm->error) {
        log_message(LOG_ERROR, "Stack overflow");
    }
#else
    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[++vm->dsp] = value;
    log_message(LOG_DEBUG, "PUSH: %ld (dsp=%d)", (long)value, vm->dsp);
#endif
}

cell_t vm_pop(VM *vm) {
    PROFILE_INC_STACK_OP();

#if USE_ASM_OPT
    cell_t value = vm_pop_opt(vm);
    if (vm->error) {
        log_message(LOG_ERROR, "Stack underflow");
    }
    return value;
#else
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Stack underflow (dsp=%d)", vm->dsp);
        vm->error = 1;
        return 0;
    }
    cell_t value = vm->data_stack[vm->dsp--];
    log_message(LOG_DEBUG, "POP: %ld (dsp=%d)", (long)value, vm->dsp);
    return value;
#endif
}
```

### 3. Update Arithmetic Words

In `src/word_source/arithmetic_words.c`:

```c
#include "../../include/arch_detect.h"

void arithmetic_word_plus(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "+: Stack underflow");
        vm->error = 1;
        return;
    }

#if USE_ASM_OPT
    cell_t n2 = vm_pop_opt(vm);
    cell_t n1 = vm_pop_opt(vm);
    cell_t result;

    if (vm_add_check_overflow(n1, n2, &result)) {
        log_message(LOG_ERROR, "+: Overflow");
        vm->error = 1;
        return;
    }

    vm_push_opt(vm, result);
#else
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    vm_push(vm, n1 + n2);
#endif
    log_message(LOG_DEBUG, "+: %ld + %ld = %ld", (long)n1, (long)n2, (long)result);
}

void arithmetic_word_star_slash_mod(VM *vm) {
    // n1 n2 n3 -- rem quot   (n1*n2/n3, remainder and quotient)
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

#if USE_ASM_OPT
    cell_t n3 = vm_pop_opt(vm);
    cell_t n2 = vm_pop_opt(vm);
    cell_t n1 = vm_pop_opt(vm);

    if (n3 == 0) {
        vm->error = 1;
        return;
    }

    // Use double-width multiplication
    cell_t hi, lo;
    vm_mul_double(n1, n2, &hi, &lo);

    // Divide by n3 (simplified - assumes result fits in 64 bits)
    cell_t quot, rem;
    vm_divmod(lo, n3, &quot, &rem);

    vm_push_opt(vm, rem);
    vm_push_opt(vm, quot);
#else
    cell_t n3 = vm_pop(vm);
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    // ... C implementation ...
#endif
}
```

## Performance Optimization

### 1. Cache Line Alignment

Align VM structure to cache line (64 bytes on Cortex-A72):

```c
typedef struct VM {
    /* Hot data - frequently accessed */
    cell_t data_stack[STACK_SIZE];
    cell_t return_stack[STACK_SIZE];
    int dsp;
    int rsp;

    /* Align to cache line */
    char _padding[64 - ((sizeof(cell_t) * STACK_SIZE * 2 + sizeof(int) * 2) % 64)];

    /* Cold data - infrequently accessed */
    int error;
    int halted;
    // ...
} __attribute__((aligned(64))) VM;
```

### 2. Prefetching

Add prefetch hints in hot loops:

```c
#if ARCH_ARM64
#include "vm_asm_opt_arm64.h"

DictEntry *vm_find_word(VM *vm, const char *name, size_t len) {
    DictEntry *entry = vm->latest;

    while (entry) {
        // Prefetch next entry while processing current
        if (entry->link) {
            vm_prefetch(entry->link);
        }

        if (entry->name_len == len) {
            if (vm_strcmp_asm(entry->name, name, len) == 0) {
                return entry;
            }
        }
        entry = entry->link;
    }
    return NULL;
}
#endif
```

### 3. NEON Optimizations

For block copy operations:

```c
#if ARCH_ARM64 && defined(__ARM_NEON)
#include <arm_neon.h>

void vm_block_copy_neon(void *dest, const void *src, size_t len) {
    // Copy 64 bytes at a time using NEON
    while (len >= 64) {
        uint8x16_t v0 = vld1q_u8((const uint8_t*)src);
        uint8x16_t v1 = vld1q_u8((const uint8_t*)src + 16);
        uint8x16_t v2 = vld1q_u8((const uint8_t*)src + 32);
        uint8x16_t v3 = vld1q_u8((const uint8_t*)src + 48);

        vst1q_u8((uint8_t*)dest, v0);
        vst1q_u8((uint8_t*)dest + 16, v1);
        vst1q_u8((uint8_t*)dest + 32, v2);
        vst1q_u8((uint8_t*)dest + 48, v3);

        src += 64;
        dest += 64;
        len -= 64;
    }

    // Handle remainder
    vm_memcpy_asm(dest, src, len);
}
#endif
```

## Benchmarking

### 1. CPU Frequency Scaling

```bash
# Check current frequency
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq

# Set to performance mode
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Or set fixed frequency
echo 1500000 | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
```

### 2. Temperature Monitoring

```bash
# Monitor temperature while benchmarking
watch -n 1 vcgencmd measure_temp

# Check throttling
vcgencmd get_throttled
# 0x0 = no throttling
# Other values indicate thermal throttling occurred
```

### 3. Benchmark Script

Create `benchmarks/rpi4_bench.sh`:

```bash
#!/bin/bash

echo "StarForth Raspberry Pi 4 Benchmark"
echo "==================================="
echo ""

# System info
echo "CPU: $(cat /proc/cpuinfo | grep 'Model' | head -1)"
echo "Frequency: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq) kHz"
echo "Temperature: $(vcgencmd measure_temp)"
echo ""

# Set performance mode
echo "Setting CPU to performance mode..."
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null

# Build baseline
echo "Building baseline (no optimizations)..."
make clean > /dev/null 2>&1
make CFLAGS="-std=c99 -O2 -Iinclude -Isrc/word_source -Isrc/test_runner/include" LDFLAGS="" > /dev/null 2>&1
cp build/starforth build/starforth_baseline

# Build optimized
echo "Building optimized (ARM64 ASM)..."
make clean > /dev/null 2>&1
make rpi4-perf > /dev/null 2>&1
cp build/starforth build/starforth_opt

# Create benchmark
cat > /tmp/bench.fth <<'EOF'
: BENCH-STACK 1000000 0 DO 1 2 3 DROP DROP DROP LOOP ;
: BENCH-MATH  1000000 0 DO 10 20 + 5 * 100 / DROP LOOP ;
: BENCH-LOGIC 1000000 0 DO 255 DUP AND DUP OR XOR DROP LOOP ;

." Running benchmarks..." CR
." Stack operations: " BENCH-STACK ." Done" CR
." Math operations:  " BENCH-MATH  ." Done" CR
." Logic operations: " BENCH-LOGIC ." Done" CR
BYE
EOF

# Run benchmarks
echo ""
echo "Baseline performance:"
time build/starforth_baseline /tmp/bench.fth 2>&1 | grep -v "^$"

echo ""
echo "Optimized performance:"
time build/starforth_opt /tmp/bench.fth 2>&1 | grep -v "^$"

echo ""
echo "Final temperature: $(vcgencmd measure_temp)"
echo "Throttling status: $(vcgencmd get_throttled)"
```

Run it:

```bash
chmod +x benchmarks/rpi4_bench.sh
./benchmarks/rpi4_bench.sh
```

### 4. Detailed Profiling with perf

```bash
# Install perf (if not already installed)
sudo apt install linux-perf

# Profile with perf
sudo perf record -g ./build/starforth benchmarks/test.fth
sudo perf report

# Cache analysis
sudo perf stat -e cache-references,cache-misses,instructions,cycles ./build/starforth benchmarks/test.fth

# Branch prediction
sudo perf stat -e branches,branch-misses ./build/starforth benchmarks/test.fth
```

## Thermal Management

### 1. Passive Cooling

Recommended heatsink specs:

- Material: Aluminum or copper
- Size: Covers entire CPU
- Thermal pad: 1mm thick

### 2. Active Cooling

Fan recommendations:

- Size: 30mm x 30mm x 7mm
- Voltage: 5V
- CFM: 3-5 CFM
- Connector: 2-pin JST or GPIO

GPIO fan control:

```bash
# Install fan control
sudo apt install fancontrol

# Auto-configure
sudo pwmconfig

# Or manual control
echo 255 > /sys/class/hwmon/hwmon0/pwm1  # Full speed
echo 128 > /sys/class/hwmon/hwmon0/pwm1  # Half speed
```

### 3. Overclocking (Optional)

⚠️ **Warning**: Requires adequate cooling and may void warranty

Add to `/boot/config.txt`:

```
# Raspberry Pi 4 overclock
over_voltage=6
arm_freq=2000
gpu_freq=750

# Additional cooling required!
```

Reboot and verify:

```bash
sudo reboot
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
```

## Power Consumption

Typical power usage:

- Idle: 2-3W
- Light load: 4-5W
- Full load (all cores): 7-8W
- With peripherals: +2-3W

Recommendations:

- Use official 5V/3A power supply
- For battery operation, use 5V/3A+ battery bank
- Add power meter for monitoring

## SD Card Performance

Recommended SD cards:

- Class: UHS-I or UHS-II
- Speed: A2 or higher (Application Performance Class 2)
- Size: 16GB minimum, 32GB+ recommended

Brands with good random I/O:

- Samsung EVO Plus
- SanDisk Extreme
- Kingston Canvas React

Test SD card speed:

```bash
# Write test
sudo dd if=/dev/zero of=/tmp/test bs=1M count=1024 conv=fdatasync

# Read test
sudo dd if=/tmp/test of=/dev/null bs=1M count=1024
```

## Troubleshooting

### Build Errors

**Problem**: `Illegal instruction` when running

```bash
# Check CPU features
cat /proc/cpuinfo | grep Features

# Rebuild with more conservative flags
make rpi4 CFLAGS="$(BASE_CFLAGS) -O2 -march=armv8-a -DUSE_ASM_OPT=1"
```

**Problem**: Cross-compiled binary doesn't run

```bash
# Check if binary is static
file build/starforth
# Should show "statically linked"

# If dynamic, rebuild with -static
make rpi4-cross LDFLAGS="-static"
```

### Performance Issues

**Problem**: Slower than expected

```bash
# Check CPU governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Should be "performance", not "ondemand"

# Check for thermal throttling
vcgencmd get_throttled
# 0x0 = no throttling

# Monitor while running
watch -n 1 'vcgencmd measure_clock arm; vcgencmd measure_temp'
```

### Memory Issues

**Problem**: Out of memory

```bash
# Check available memory
free -h

# Reduce VM memory size in vm.h
#define VM_MEMORY_SIZE (2 * 1024 * 1024)  // 2MB instead of 5MB

# Or increase swap
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile
# CONF_SWAPSIZE=2048
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
```

## Production Deployment

### 1. Systemd Service

Create `/etc/systemd/system/starforth.service`:

```ini
[Unit]
Description=StarForth VM
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/starforth
ExecStart=/home/pi/starforth/build/starforth
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable starforth
sudo systemctl start starforth
sudo systemctl status starforth
```

### 2. Boot Optimization

Reduce boot time in `/boot/config.txt`:

```
# Disable unused features
dtparam=audio=off
camera_auto_detect=0
display_auto_detect=0

# Faster boot
boot_delay=0
disable_splash=1
```

### 3. Security Hardening

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Remove unnecessary packages
sudo apt autoremove -y

# Configure firewall
sudo apt install ufw
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw enable

# Disable password auth (use keys)
sudo nano /etc/ssh/sshd_config
# PasswordAuthentication no
sudo systemctl restart sshd
```

## Further Reading

- [ARM Cortex-A72 Technical Reference Manual](https://developer.arm.com/documentation/100095/latest)
- [ARM NEON Programmer's Guide](https://developer.arm.com/architectures/instruction-sets/simd-isas/neon)
- [Raspberry Pi Documentation](https://www.raspberrypi.org/documentation/)
- [ARMv8-A Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest)

## License

Public domain / CC0. No warranty. Use at your own risk.
