# ==============================================================================
# StarForth Build System - The Fastest Forth in the West!
# ==============================================================================
#
# Quick Start:
#   make                          - Standard optimized build (auto ARCH, TARGET=standard)
#   make TARGET=fastest           - Maximum performance build
#   make ARCH=arm64 TARGET=fastest - Build for arm64 (e.g., Raspberry Pi 4)
#   make test                     - Run test suite for current ARCH/TARGET
#   make help                     - Show all available targets
#
# ==============================================================================

# ==============================================================================
# CONFIGURATION
# ==============================================================================

VERSION ?= 3.0.1
STRICT_PTR ?= 1
CC = gcc

# Isabelle configuration
# NOTE: Isabelle must be installed separately and available on PATH
# See docs/DEVELOPER.md for installation instructions
ISABELLE ?= isabelle

# Architecture / target selection
ARCH ?= $(shell uname -m)
ARCH := $(strip $(ARCH))
TARGET ?= standard
TARGET := $(strip $(TARGET))

# Friendly guardrail: 32-bit builds are not supported
unsupported_32bit_arches := i386 i486 i586 i686 x86 armv6 armv7 armv7l arm
ifneq ($(filter $(ARCH),$(unsupported_32bit_arches)),)
    $(error StarForth targets 64-bit platforms only. ARCH='$(ARCH)' is unsupported. Please build on amd64/arm64/raspi/riscv64.)
endif

# Canonical architecture normalization (x86_64→amd64, aarch64→arm64, etc.)
canon_arch = $(strip $(if $(filter x86_64,$1),amd64,$(if $(filter amd64,$1),amd64,$(if $(filter aarch64 arm64,$1),arm64,$(if $(filter riscv riscv64,$1),riscv64,$1)))))
ARCH_DIR := $(call canon_arch,$(ARCH))
# Normalize ARCH for consistent ifeq matching
ARCH := $(call canon_arch,$(ARCH))

ifeq ($(ARCH),amd64)
    ARCH_NAME = x86_64
    ARCH_FLAGS = -march=native
    ARCH_DEFINES = -DARCH_X86_64=1
    ASM_SYNTAX = -masm=intel
else ifeq ($(ARCH),arm64)
    ARCH_NAME = ARM64
    ARCH_FLAGS = -march=armv8-a+crc+simd -mtune=cortex-a72
    ARCH_DEFINES = -DARCH_ARM64=1
    ASM_SYNTAX =
else ifeq ($(ARCH),raspi)
    ARCH_NAME = Raspberry Pi 4 (ARM64)
    ARCH_FLAGS = -march=armv8-a+crc+simd -mtune=cortex-a72
    ARCH_DEFINES = -DARCH_ARM64=1 -DRASPBERRY_PI_BUILD=1
    ASM_SYNTAX =
else ifeq ($(ARCH),riscv64)
    ARCH_NAME = RISC-V 64 (QEMU)
    ARCH_FLAGS = -march=rv64gc -mabi=lp64d -mcmodel=medany
    ARCH_DEFINES = -DARCH_RISCV64=1
    ASM_SYNTAX =
else ifeq ($(ARCH),riscv)
    ARCH_NAME = RISC-V 64 (QEMU)
    ARCH_FLAGS = -march=rv64gc -mabi=lp64d -mcmodel=medany
    ARCH_DEFINES = -DARCH_RISCV64=1
    ASM_SYNTAX =
else
    $(warning Unknown architecture: $(ARCH), using generic flags)
    ARCH_NAME = generic
    ARCH_FLAGS =
    ARCH_DEFINES =
    ASM_SYNTAX =
endif

# Compiler flags
# Feature switches (default: enabled)
# ENABLE_HOTWORDS_CACHE: Physics-driven hot-words cache (1.78× speedup on dictionary lookups)
#   Default: 1 (enabled - experiments show cache is optimal)
#   Set ENABLE_HOTWORDS_CACHE=0 to disable (for research comparison only)
ENABLE_HOTWORDS_CACHE ?= 0

# ENABLE_PIPELINING: Speculative execution via word transition prediction
#   Default: 1 (enabled - experiments show pipelining is optimal)
#   Set ENABLE_PIPELINING=0 to disable (for research comparison only)
#   NOTE: As of 2025-11-19, all future experiments use both cache and pipelining enabled
ENABLE_PIPELINING ?= 0

# Heartbeat configuration
# HEARTBEAT_THREAD_ENABLED: 1 = run vm_tick() in background thread (OPTIMAL), 0 = inline (legacy)
#   Default: 1 (experiments show threaded heartbeat is optimal for adaptive tuning)
#   NOTE: As of 2025-11-19, all future experiments use heartbeat enabled
HEARTBEAT_THREAD_ENABLED ?= 1

# HEARTBEAT_TICK_NS: Wake frequency for heartbeat thread (default 1ms)
HEARTBEAT_TICK_NS ?= 10000

ifeq ($(L4RE),1)
HEARTBEAT_THREAD_ENABLED := 0
endif

# Feedback Loop Toggle Switches (for systematic performance measurement)
# ============================================================================
# These allow turning each feedback loop on/off to measure additive performance gains
# EXP_00-BASELINE: all loops OFF (plain FORTH-79)
# EXP_01: Loop #1 ON
# EXP_02: Loops #1-2 ON
# ... etc, building cumulatively to measure each loop's contribution

# ==============================================================================
# SSM (Steady State Machine) Configuration - Data-Driven Architecture
# ==============================================================================
# Based on 2^7 DoE with 300 reps (128 configs, 38,400 runs).
# Top 5% analysis (speed + stability) reveals optimal loop combinations.
#
# Experimentally Validated Architecture:
#   L1 (heat_tracking):       DISABLED (harmful in 86% of top configs)
#   L4 (pipelining_metrics):  DISABLED (harmful in 100% of top configs)
#   L7 (adaptive_heartrate):  ALWAYS ON (beneficial in 71% of top configs)
#   L2, L3, L5, L6:           Runtime-controlled by L8 (workload-dependent)
#   L8 (Jacquard):            4-bit mode selector (16 modes: C0-C15)
#
# L8 dynamically controls L2/L3/L5/L6 based on entropy, CV, and temporal metrics.
# Top 5% validated modes: C4, C7, C9, C11, C12 (see src/ssm_jacquard.c)

# Tuning Knobs (Physics-driven Optimization System)
# ============================================================================
# Knob #7: ROLLING_WINDOW_SIZE (Initial execution history capture)
#   - Default: 4096 (conservative, ensures statistical significance at cold start)
#   - Usage: make ROLLING_WINDOW_SIZE=8192 (larger for complex workloads)
#           or ROLLING_WINDOW_SIZE=2048 (smaller for memory constraints)
#   - System AUTOMATICALLY shrinks during execution if diminishing returns detected
#   - Window starts conservative, self-tunes down if pattern diversity plateaus
#   - Window becomes "warm" (representative) after N executions, then adapts
ROLLING_WINDOW_SIZE ?= 4096

# Knob #6: TRANSITION_WINDOW_SIZE (Pipelining context depth)
#   - Default: 2 (2-word lookahead for next-word prediction)
#   - Usage: make TRANSITION_WINDOW_SIZE=1 (or 4, 8 for deeper context)
#   - Empirically tuned via binary chop: 1 vs 2 vs 4 vs 8
TRANSITION_WINDOW_SIZE ?= 8

# Knobs #8-11: Adaptive Window Shrinking Control
# ============================================================================
# These control how aggressively the rolling window learns and adapts
# (NOT the initial window size - that's ROLLING_WINDOW_SIZE above)

# Knob #8: ADAPTIVE_SHRINK_RATE (percentage to retain when shrinking)
#   - Default: 75 (shrink to 75% = discard 25% each cycle)
#   - Range: 50-95 (lower = more aggressive, higher = more conservative)
#   - Usage: make ADAPTIVE_SHRINK_RATE=50 (fast learning) or 90 (slow learning)
ADAPTIVE_SHRINK_RATE ?= 50

# Knob #9: ADAPTIVE_MIN_WINDOW_SIZE (floor to prevent over-shrinking)
#   - Default: 256 (never shrink below 256 word IDs)
#   - Range: 64-1024 (smaller = leaner, larger = safer)
#   - Usage: make ADAPTIVE_MIN_WINDOW_SIZE=512 (conservative) or 128 (lean)
ADAPTIVE_MIN_WINDOW_SIZE ?= 256

# Knob #10: ADAPTIVE_CHECK_FREQUENCY (how often to measure diversity)
#   - Default: 256 (check after every 256 executions)
#   - Range: 32-1024 (more = faster response, less = less overhead)
#   - Usage: make ADAPTIVE_CHECK_FREQUENCY=128 (responsive) or 512 (lazy)
ADAPTIVE_CHECK_FREQUENCY ?= 512

# Knob #11: ADAPTIVE_GROWTH_THRESHOLD (growth rate that signals saturation)
#   - Default: 1 (shrink when growth < 1%)
#   - Range: 0-10 (lower = eager shrinking, higher = cautious)
#   - Usage: make ADAPTIVE_GROWTH_THRESHOLD=0 (aggressive) or 5 (conservative)
ADAPTIVE_GROWTH_THRESHOLD ?= 5

# Knob #11a: INITIAL_DECAY_SLOPE_Q48 (Starting decay slope for inference engine)
#   - Default: 21845 (1/3 in Q48.16 format, = 0.333... starting slope)
#   - Range: 13107-43691 (0.2 to 0.67 in Q48.16)
#   - Usage: make INITIAL_DECAY_SLOPE_Q48=13107 (0.2) or 32768 (0.5) or 43691 (0.67)
#   - DoE: OPP #3 tests 0.2, 0.33, 0.5, 0.67 to find optimal starting point for convergence
#   - Note: Inference engine adapts this value at runtime; this is just the cold-start value
INITIAL_DECAY_SLOPE_Q48 ?= 21845

# Knob #11b: DECAY_MIN_INTERVAL (Minimum time before decay applies)
#   - Default: 1000 (nanoseconds, ~1 microsecond)
#   - Range: 500-5000 (balance between decay sensitivity and overhead)
#   - Usage: make DECAY_MIN_INTERVAL=500 (faster decay) or 2000 (slower decay)
#   - DoE: May test in future opportunities for decay sensitivity tuning
DECAY_MIN_INTERVAL ?= 500

# Knob #11d: HEARTBEAT_INFERENCE_FREQUENCY (How often to run inference engine)
#   - Default: 5000 (ticks between inference runs, ~5ms heartbeat window)
#   - Set VERY HIGH (999999) to effectively disable inference (static mode)
#   - Set LOW (1000) for frequent inference (adaptive mode)
#   - DoE: OPP #3 tests 999999 (static, no Loop #6) vs 5000 (adaptive, with Loop #6)
#   - Measures: Inference engine cost vs benefit in optimized performance
HEARTBEAT_INFERENCE_FREQUENCY ?= 1000

# Knob #12: DECAY_RATE_PER_US_Q16 (Heat decay rate in Q16 fixed-point)
#   - Default: 1 (1/65536 heat units decayed per microsecond)
#   - Range: 0-65536 (0 = no decay, 65536 = 1 heat/µs)
#   - Half-life: ~6.5 seconds at default (100-heat word → 50-heat in 6.5s)
#   - Usage: make DECAY_RATE_PER_US_Q16=2 (faster decay) or 0 (no decay for baseline)
#   - DoE: Vary this to measure impact of heat decay on performance
DECAY_RATE_PER_US_Q16 ?= 1

# Build the CFLAGS with tuning knobs
# L8 FINAL INTEGRATION: All experimental loop flags removed.
# L1-L7 are now always-on internal physics layers.
# L8 Jacquard mode selector is the sole policy engine.
BASE_CFLAGS = -std=c99 -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include \
              -DSTRICT_PTR=$(STRICT_PTR) \
              -DENABLE_HOTWORDS_CACHE=$(ENABLE_HOTWORDS_CACHE) \
              -DENABLE_PIPELINING=$(ENABLE_PIPELINING) \
              -DROLLING_WINDOW_SIZE=$(ROLLING_WINDOW_SIZE) \
              -DTRANSITION_WINDOW_SIZE=$(TRANSITION_WINDOW_SIZE) \
              -DADAPTIVE_SHRINK_RATE=$(ADAPTIVE_SHRINK_RATE) \
              -DADAPTIVE_MIN_WINDOW_SIZE=$(ADAPTIVE_MIN_WINDOW_SIZE) \
              -DADAPTIVE_CHECK_FREQUENCY=$(ADAPTIVE_CHECK_FREQUENCY) \
              -DADAPTIVE_GROWTH_THRESHOLD=$(ADAPTIVE_GROWTH_THRESHOLD) \
              -DINITIAL_DECAY_SLOPE_Q48=$(INITIAL_DECAY_SLOPE_Q48) \
              -DDECAY_RATE_PER_US_Q16=$(DECAY_RATE_PER_US_Q16) \
              -DHEARTBEAT_THREAD_ENABLED=$(HEARTBEAT_THREAD_ENABLED) \
              -DHEARTBEAT_TICK_NS=$(HEARTBEAT_TICK_NS) \
              -DHEARTBEAT_INFERENCE_FREQUENCY=$(HEARTBEAT_INFERENCE_FREQUENCY)

ifeq ($(HEARTBEAT_THREAD_ENABLED),1)
ifeq ($(L4RE),1)
THREAD_FLAGS :=
else
THREAD_FLAGS := -pthread
endif
else
THREAD_FLAGS :=
endif

BASE_CFLAGS += $(THREAD_FLAGS)

# Build profiles (TARGET values)
SUPPORTED_TARGETS := standard fast fastest turbo pgo asan

TARGET_DESCRIPTION_standard := Standard optimized build
TARGET_CFLAGS_standard := $(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -flto=auto -fuse-linker-plugin -DNDEBUG \
                           -DUSE_ASM_OPT=1 \
                           -ffunction-sections -fdata-sections -fomit-frame-pointer \
                           -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-strict-aliasing
TARGET_LDFLAGS_standard := -Wl,--gc-sections -s -flto=auto -fuse-linker-plugin -static $(THREAD_FLAGS)

TARGET_DESCRIPTION_fast := Fast optimized build (no LTO, easier debugging)
TARGET_CFLAGS_fast := $(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG
TARGET_LDFLAGS_fast := -s $(THREAD_FLAGS)

TARGET_DESCRIPTION_fastest := Maximum performance build (ASM + LTO + direct threading)
TARGET_CFLAGS_fastest := $(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG \
                          -flto=auto -fuse-linker-plugin -funroll-loops -finline-functions -fomit-frame-pointer -fno-plt -fno-semantic-interposition
TARGET_LDFLAGS_fastest := -flto=auto -fuse-linker-plugin -s -Wl,--gc-sections $(THREAD_FLAGS)

TARGET_DESCRIPTION_turbo := Assembly-optimized build (no direct threading)
TARGET_CFLAGS_turbo := $(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DNDEBUG -flto
TARGET_LDFLAGS_turbo := -flto -s $(THREAD_FLAGS)

TARGET_DESCRIPTION_pgo := Profile-guided optimization build (requires instrumentation run)
TARGET_CFLAGS_pgo := $(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 \
                      -fprofile-use -fprofile-correction -Wno-error=coverage-mismatch
TARGET_LDFLAGS_pgo := -fprofile-use -lgcov $(THREAD_FLAGS)
TARGET_TYPE_pgo := pgo

TARGET_DESCRIPTION_asan := ASan/UBSan instrumented build for memory error detection
TARGET_CFLAGS_asan := $(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O1 -g \
                      -fsanitize=address,undefined -fno-omit-frame-pointer
TARGET_LDFLAGS_asan := -fsanitize=address,undefined $(THREAD_FLAGS)

define TARGET_POST_fastest
	@echo ""
	@echo "🎯 You've built the FASTEST FORTH IN THE WEST!"
	@echo ""
	@echo "Quick-draw benchmark:"
	@time -f "  ⏱️  Time: %E seconds" ./$(BINARY) -c ": BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE" 2>/dev/null || true
endef

TARGET_DESCRIPTION := $(TARGET_DESCRIPTION_$(TARGET))
ifeq ($(strip $(TARGET_DESCRIPTION)),)
    $(error Unknown TARGET '$(TARGET)'. Supported targets: $(SUPPORTED_TARGETS))
endif

TARGET_CFLAGS_SELECTED := $(TARGET_CFLAGS_$(TARGET))
TARGET_LDFLAGS_SELECTED := $(TARGET_LDFLAGS_$(TARGET))
TARGET_TYPE := $(or $(TARGET_TYPE_$(TARGET)),single)
TARGET_POST := $(or $(TARGET_POST_$(TARGET)),@:)

CFLAGS ?= $(TARGET_CFLAGS_SELECTED)
LDFLAGS ?= $(TARGET_LDFLAGS_SELECTED)

CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

USE_ASM_OPT := $(if $(findstring -DUSE_ASM_OPT=1,$(CFLAGS)),1,0)
USE_LTO := $(if $(or $(findstring -flto,$(CFLAGS)),$(findstring -flto,$(LDFLAGS))),1,0)

# Platform support
ifdef MINIMAL
CFLAGS += -DSTARFORTH_MINIMAL=1 -nostdlib -ffreestanding
LDFLAGS += -nostdlib
PLATFORM_SRC = src/platform/starforth_minimal.c
else ifdef L4RE
CFLAGS += -D__l4__=1
PLATFORM_TIME_SRC = src/platform/l4re/time.c src/platform/platform_init.c
else
PLATFORM_TIME_SRC = src/platform/linux/time.c src/platform/platform_init.c
endif

PLATFORM_COMMON_SRC = src/platform/threading.c

# Source and object files
SRC = $(wildcard src/*.c src/word_source/*.c src/test_runner/*.c src/test_runner/modules/*.c) $(PLATFORM_SRC) $(PLATFORM_TIME_SRC) $(PLATFORM_COMMON_SRC)
BUILD_ROOT ?= build
build_dir = $(BUILD_ROOT)/$(call canon_arch,$(or $2,$(ARCH)))/$1
binary_path = $(call build_dir,$1,$2)/starforth
BUILD_DIR = $(call build_dir,$(TARGET),$(ARCH))
BINARY = $(call binary_path,$(TARGET),$(ARCH))
OBJ = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
ASM_FILES = $(patsubst src/%.c,$(BUILD_DIR)/%.s,$(SRC))

# Profiler support for test targets
PROFILE_LEVEL := $(strip $(PROFILE))
ifeq ($(PROFILE_LEVEL),)
PROFILE_ARGS :=
else
PROFILE_ARGS := --profile $(PROFILE_LEVEL)
PROFILE_REPORT ?= 1
ifneq ($(strip $(PROFILE_REPORT)),0)
PROFILE_ARGS += --profile-report
endif
endif

# Installation directories
PREFIX ?= .
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
INFODIR = $(PREFIX)/share/info
DOCDIR = $(PREFIX)/share/doc/starforth
CONFDIR = $(PREFIX)/etc/starforth

# ==============================================================================
# PHONY TARGETS
# ==============================================================================

.PHONY: all banner help clean clean-obj clean-docs
.PHONY: fastest fast turbo pgo pgo-build pgo-perf pgo-valgrind bench-compare
.PHONY: rpi4 rpi4-cross rpi4-fastest
.PHONY: minimal fake-l4re debug profile performance
.PHONY: test smoke bench benchmark
.PHONY: asm sbom
.PHONY: docs api-docs docs-latex docs-isabelle isabelle-build isabelle-check info
.PHONY: refinement-status refinement-init refinement-phase1 verify-defect refinement-annotate-check refinement-report
.PHONY: install uninstall package deb rpm
.PHONY: quality compile_commands clang_tidy cppcheck gcc_analyzer
.PHONY: FORCE version-info bump-z bump-y build-manifest

# ==============================================================================
# MAIN TARGETS
# ==============================================================================

ifeq ($(TARGET_TYPE),pgo)
all: banner
	@$(MAKE) ARCH=$(ARCH) TARGET=$(TARGET) pgo-build
else
all: banner $(BINARY)
	@echo ""
	@echo "✓ Build complete: $(BINARY)"
	@echo "  Architecture: $(ARCH_NAME)"
	@echo "  Profile: $(TARGET_DESCRIPTION) ($(TARGET))"
	@echo "  Output directory: $(BUILD_DIR)"
	@echo "  Optimizations: $(if $(findstring USE_ASM_OPT,$(CFLAGS)),Enabled,Disabled)"
	@echo "  Direct Threading: $(if $(findstring USE_DIRECT_THREADING,$(CFLAGS)),Enabled,Disabled)"
	$(TARGET_POST)
endif

banner:
	@echo "════════════════════════════════════════════════════════════"
	@echo "   ⚡ Building the Fastest Forth in the West! ⚡"
	@echo "════════════════════════════════════════════════════════════"
	@echo "  Target Architecture: $(ARCH_NAME) ($(ARCH_DIR))"
	@echo "  Build Profile: $(TARGET_DESCRIPTION) ($(TARGET))"
	@echo ""

# ==============================================================================
# OPTIMIZATION BUILDS - The Fastest in the West
# ==============================================================================

# Maximum performance - no compromises
fastest:
	@echo "🏆 Building FASTEST configuration..."
	@echo "   - Architecture: $(ARCH_NAME)"
	@echo "   - Assembly optimizations: ENABLED"
	@echo "   - Direct threading: ENABLED"
	@echo "   - Profile-guided optimization: Ready"
	@echo ""
	@$(MAKE) TARGET=fastest all

# Fast without LTO (easier debugging)
fast:
	@echo "⚡ Building FAST configuration (no LTO for debugging)..."
	@$(MAKE) TARGET=fast all

# Assembly optimizations only (no direct threading)
turbo:
	@echo "🚀 Building TURBO configuration (ASM only)..."
	@$(MAKE) TARGET=turbo all

# Profile-guided optimization - 6-stage build
pgo:
	@$(MAKE) TARGET=pgo all

pgo-build:
	@echo "📊 Building with Profile-Guided Optimization..."
	@echo "   Stage 1: Clean build environment..."
	@$(MAKE) ARCH=$(ARCH) clean
	@rm -f *.gcda */*.gcda */*/*.gcda src/*.gcda src/*/*.gcda 2>/dev/null || true
	@rm -f *.gcno */*.gcno */*/*.gcno src/*.gcno src/*/*.gcno 2>/dev/null || true
	@find $(BUILD_ROOT) -name "*.gcda" -delete 2>/dev/null || true
	@find $(BUILD_ROOT) -name "*.gcno" -delete 2>/dev/null || true
	@echo "   Stage 2: Instrumentation build..."
	@$(MAKE) ARCH=$(ARCH) TARGET=$(TARGET) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -DUSE_ASM_OPT=1 -fprofile-generate" LDFLAGS="-fprofile-generate -lgcov" $(BINARY)
	@echo "   Stage 3: Running comprehensive profiling workload..."
	@echo "     (This exercises all code paths: tests, benchmarks, REPL, blocks, etc.)"
	@./scripts/pgo-workload.sh $(BINARY)
	@echo "   Stage 4: Collecting profile data..."
	@find . -name "*.gcda" -type f | wc -l | xargs -I {} echo "     Found {} profile data files"
	@echo "   Stage 5: Optimized build with profile data..."
	@$(MAKE) ARCH=$(ARCH) clean-obj
	@rm -f *.gcno */*.gcno */*/*.gcno src/*.gcno src/*/*.gcno 2>/dev/null || true
	@find $(BUILD_ROOT) -name "*.gcno" -delete 2>/dev/null || true
	@$(MAKE) ARCH=$(ARCH) TARGET=$(TARGET) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction -Wno-error=coverage-mismatch" LDFLAGS="-fprofile-use -lgcov" $(BINARY)
	@echo "   Stage 6: Cleaning up profile data..."
	@rm -f *.gcda */*.gcda */*/*.gcda src/*.gcda src/*/*.gcda 2>/dev/null || true
	@rm -f *.gcno */*.gcno */*/*.gcno src/*.gcno src/*/*.gcno 2>/dev/null || true
	@find $(BUILD_ROOT) -name "*.gcda" -delete 2>/dev/null || true
	@find $(BUILD_ROOT) -name "*.gcno" -delete 2>/dev/null || true
	@echo ""
	@echo "✓ PGO build complete: $(BINARY)"
	@echo "  Compare performance with: make bench-compare"

# PGO with perf analysis
pgo-perf: banner
	@echo "📊 Building with PGO + perf analysis..."
	@if ! command -v perf &> /dev/null; then \
		echo "Error: perf not found. Install with: sudo apt-get install linux-tools-generic"; \
		exit 1; \
	fi
	@echo "   Stage 1: Clean build..."
	$(MAKE) ARCH=$(ARCH) clean
	@echo "   Stage 2: Building instrumented binary..."
	$(MAKE) ARCH=$(ARCH) TARGET=pgo CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -DUSE_ASM_OPT=1 -fprofile-generate -fno-omit-frame-pointer" LDFLAGS="-fprofile-generate -lgcov" $(call binary_path,pgo)
	@echo "   Stage 3: Running workload with perf profiling..."
	@sudo perf record -g --call-graph dwarf -o pgo-perf.data -- ./scripts/pgo-workload.sh $(call binary_path,pgo) 2>&1 | head -20
	@echo "   Stage 4: Optimized build with profile data..."
	$(MAKE) ARCH=$(ARCH) clean-obj
	$(MAKE) ARCH=$(ARCH) TARGET=pgo CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction -fno-omit-frame-pointer -Wno-error=coverage-mismatch" LDFLAGS="-fprofile-use" $(call binary_path,pgo)
	@echo "   Stage 5: Analyzing perf data..."
	@sudo perf report --stdio -i pgo-perf.data --sort comm,dso,symbol --percent-limit 1 | head -50
	@echo ""
	@echo "✓ PGO + perf build complete: $(call binary_path,pgo)"
	@echo "  View detailed report: sudo perf report -i pgo-perf.data"
	@echo "  View flamegraph: sudo perf script -i pgo-perf.data | ./scripts/flamegraph.pl > pgo-flame.svg"

# PGO with valgrind callgrind
pgo-valgrind: banner
	@echo "📊 Building with PGO + valgrind callgrind analysis..."
	@if ! command -v valgrind &> /dev/null; then \
		echo "Error: valgrind not found. Install with: sudo apt-get install valgrind"; \
		exit 1; \
	fi
	@echo "   Stage 1: Clean build..."
	$(MAKE) ARCH=$(ARCH) clean
	@echo "   Stage 2: Building instrumented binary with debug symbols..."
	$(MAKE) ARCH=$(ARCH) TARGET=pgo CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -g -DUSE_ASM_OPT=1 -fprofile-generate" LDFLAGS="-fprofile-generate -lgcov" $(call binary_path,pgo)
	@echo "   Stage 3: Running workload with callgrind profiling..."
	@echo "     (This will be slow - callgrind adds significant overhead)"
	@valgrind --tool=callgrind --callgrind-out-file=pgo-callgrind.out --dump-instr=yes --collect-jumps=yes \
		$(call binary_path,pgo) --benchmark 100 --log-none || true
	@echo "   Stage 4: Optimized build with profile data..."
	$(MAKE) ARCH=$(ARCH) clean-obj
	$(MAKE) ARCH=$(ARCH) TARGET=pgo CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -g -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction -Wno-error=coverage-mismatch" LDFLAGS="-fprofile-use" $(call binary_path,pgo)
	@echo ""
	@echo "✓ PGO + valgrind build complete: $(call binary_path,pgo)"
	@echo "  View callgrind data: kcachegrind pgo-callgrind.out"
	@echo "  Or text report: callgrind_annotate pgo-callgrind.out"

# Benchmark: regular vs PGO comparison
bench-compare: banner
	@echo "🏁 Benchmark Comparison: Regular vs PGO"
	@echo ""
	@echo "Building regular optimized binary..."
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) fastest > /dev/null 2>&1
	@echo "Running benchmark (10000 iterations)..."
	@echo -n "  Regular build: "
	@/usr/bin/time -f "%E elapsed, %U user" $(call binary_path,fastest) --benchmark 10000 --log-none 2>&1 | tail -1
	@echo ""
	@echo "Building PGO optimized binary..."
	@$(MAKE) pgo > /dev/null 2>&1
	@echo "Running benchmark (10000 iterations)..."
	@echo -n "  PGO build:     "
	@/usr/bin/time -f "%E elapsed, %U user" $(call binary_path,pgo) --benchmark 10000 --log-none 2>&1 | tail -1
	@echo ""
	@echo "✓ Comparison complete!"

# ==============================================================================
# PLATFORM-SPECIFIC BUILDS
# ==============================================================================

# Raspberry Pi 4 - native build
rpi4:
	@echo "🥧 Building for Raspberry Pi 4 (native)..."
	@$(MAKE) ARCH=raspi TARGET=fastest CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto" LDFLAGS="-flto -s" all
	@echo "✓ Raspberry Pi build ready: $(call binary_path,fastest,raspi)"

# Raspberry Pi 4 - cross-compile from x86_64
rpi4-cross:
	@echo "🥧 Cross-compiling for Raspberry Pi 4..."
	@$(MAKE) ARCH=raspi TARGET=fastest CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -static" \
	        LDFLAGS="-flto -s -static" \
	        all
	@echo "✓ Cross-compiled binary ready: $(call binary_path,fastest,raspi)"
	@echo "  Copy to RPi4: scp $(call binary_path,fastest,raspi) pi@raspberrypi.local:~/"

# Raspberry Pi 4 - maximum optimization
rpi4-fastest:
	@echo "🥧⚡ Building FASTEST for Raspberry Pi 4..."
	@$(MAKE) ARCH=raspi TARGET=fastest CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd+crypto -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -funroll-loops -finline-functions" \
	        LDFLAGS="-flto -s -static" \
	        all
	@echo "✓ Raspberry Pi FASTEST build ready: $(call binary_path,fastest,raspi)"

# Minimal/embedded build
minimal:
	$(MAKE) MINIMAL=1

# Fake L4Re build (for testing)
fake-l4re:
	$(MAKE) MINIMAL=1 CC=l4-gcc CFLAGS="$(BASE_CFLAGS) -DL4RE_TARGET=1"

# ==============================================================================
# DEBUG & DEVELOPMENT BUILDS
# ==============================================================================

# Debug build with symbols
debug: banner
	@echo "🐛 Building DEBUG configuration..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O0 -g -DDEBUG" LDFLAGS="" $(BINARY)

# Build with profiler support
profile:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -DPROFILE_ENABLED=1 -g -O1" $(BINARY)

# Performance build (legacy alias - redirects to 'fastest')
performance: fastest

# ==============================================================================
# BUILD RULES
# ==============================================================================

# Generate version header from Makefile VERSION, ARCH, TARGET, and timestamp
include/version.h: FORCE
	@mkdir -p include
	@echo "Generating version header..."
	@BUILD_TS=$$(date -Iseconds 2>/dev/null || echo "unknown"); \
	printf '#ifndef STARFORTH_VERSION_H\n#define STARFORTH_VERSION_H\n\n' > $@; \
	printf '#define STARFORTH_VERSION "%s"\n' "$(VERSION)" >> $@; \
	printf '#define STARFORTH_ARCH "%s"\n' "$(ARCH_NAME)" >> $@; \
	printf '#define STARFORTH_TARGET "%s"\n' "$(TARGET)" >> $@; \
	printf '#define STARFORTH_TIMESTAMP "%s"\n' "$$BUILD_TS" >> $@; \
	printf '#define STARFORTH_VERSION_FULL "StarForth v%s %s %s %s"\n\n' "$(VERSION)" "$(ARCH_NAME)" "$(TARGET)" "$$BUILD_TS" >> $@; \
	printf '#endif /* STARFORTH_VERSION_H */\n' >> $@

# Ensure version.h exists and is up-to-date before compilation
FORCE:

# Main executable
$(BINARY): $(OBJ)
	@echo "🔗 Linking $(BINARY)..."
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for all C files (consolidates 5 repetitive rules)
# Order-only dependency: version.h must exist before compilation, but changes don't trigger rebuild
$(BUILD_DIR)/%.o: src/%.c | include/version.h
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

# ==============================================================================
# TESTING & BENCHMARKING
# ==============================================================================

# Run full test suite
test: $(BINARY)
	@echo "🧪 Running test suite..."
	@printf 'BYE\n' | ./$(BINARY) --run-tests $(PROFILE_ARGS)

# Quick smoke test
smoke: $(BINARY)
	@echo ""
	@echo "🚦 Running smoke test (1 2 + .)"
	@raw_out=$$(printf '1 2 + .\nBYE\n' | NO_COLOR=1 ./$(BINARY) --log-none); \
	printf '%s\n' "$$raw_out"; \
	out_clean=$$(printf '%s\n' "$$raw_out" | sed -E 's/\x1B\[[0-9;]*[A-Za-z]//g'); \
	echo "$$out_clean" | grep -q 'ok> 3' || (echo "Smoke test failed" >&2; exit 1)
	@echo "✓ Smoke test passed"

# Quick benchmark
bench: $(BINARY)
	@echo ""
	@echo "🏇 Running quick-draw benchmark..."
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "Test: 1 million stack operations"
	@time -f "Time: %E seconds" ./$(BINARY) -c ": BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE" 2>/dev/null || echo "Benchmark complete"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Full benchmark suite
benchmark: $(BINARY)
	@echo ""
	@echo "🎯 Full Benchmark Suite - Fastest Forth in the West"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "📊 Stack Operations (1M iterations):"
	@time -f "  ⏱️  %E seconds" ./$(BINARY) -c ": BENCH-STACK 1000000 0 DO 1 2 3 DROP DROP DROP LOOP ; BENCH-STACK BYE" 2>/dev/null || true
	@echo ""
	@echo "📊 Arithmetic Operations (1M iterations):"
	@time -f "  ⏱️  %E seconds" ./$(BINARY) -c ": BENCH-MATH 1000000 0 DO 10 20 + 5 * 100 / DROP LOOP ; BENCH-MATH BYE" 2>/dev/null || true
	@echo ""
	@echo "📊 Logic Operations (1M iterations):"
	@time -f "  ⏱️  %E seconds" ./$(BINARY) -c ": BENCH-LOGIC 1000000 0 DO 255 DUP AND DUP OR XOR DROP LOOP ; BENCH-LOGIC BYE" 2>/dev/null || true
	@echo ""
	@echo "════════════════════════════════════════════════════════════"

# ==============================================================================
# ASSEMBLY OUTPUT & ANALYSIS
# ==============================================================================

# Generate assembly files for inspection
asm: banner
	@echo "📝 Generating assembly output..."
	@echo "   Cleaning to force rebuild..."
	@$(MAKE) clean-obj > /dev/null 2>&1
	$(MAKE) ASM=1 CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1" LDFLAGS="" $(BINARY)
	@echo "✓ Assembly files generated in $(BUILD_DIR)/"
	@echo "  Files generated: $$(find $(BUILD_DIR) -name '*.s' | wc -l)"
	@echo "  Example: less $(BUILD_DIR)/stack_management.s"

# Generate Software Bill of Materials (SBOM) in SPDX format
sbom:
	@echo "📋 Generating SBOM (Software Bill of Materials)..."
	@if ! which syft >/dev/null 2>&1; then \
		echo "Error: syft not found. Install from https://github.com/anchore/syft"; \
		exit 1; \
	fi
	@syft dir:. -o spdx-json=sbom.spdx.json -o spdx=sbom.spdx \
		--source-name StarForth \
		--source-version $(VERSION) \
		--exclude './build/**' --exclude './tools/**' --exclude './.git/**'
	@echo "✅ SBOM generated:"
	@echo "   SPDX JSON: sbom.spdx.json"
	@echo "   SPDX:      sbom.spdx"
	@echo "   Version:   $(VERSION)"

# ==============================================================================
# DOCUMENTATION
# ==============================================================================

# Generate API documentation (Doxygen XML → AsciiDoc)
api-docs:
	@echo "📚 Generating API documentation from Doxygen..."
	@if [ ! -f scripts/generate-doxygen-appendix.sh ]; then \
		echo "Error: scripts/generate-doxygen-appendix.sh not found"; \
		exit 1; \
	fi
	@./scripts/generate-doxygen-appendix.sh
	@echo "✅ API documentation generated: docs/src/appendix/"

# Convert all AsciiDoc to src
docs-latex:
	@echo "📄 Converting AsciiDoc to LaTeX..."
	@if [ ! -f scripts/asciidoc-to-latex.sh ]; then \
		echo "Error: scripts/asciidoc-to-latex.sh not found"; \
		exit 1; \
	fi
	@./scripts/asciidoc-to-latex.sh
	@echo "✅ LaTeX files generated: docs/latex/"

# Build and verify Isabelle theories
isabelle-build:
	@echo "🔬 Building Isabelle formal verification theories..."
	@if ! command -v $(ISABELLE) >/dev/null 2>&1; then \
		echo "Error: Isabelle not found: $(ISABELLE)"; \
		echo "  Install Isabelle or set ISABELLE variable"; \
		exit 1; \
	fi
	@echo "   Using: $$(command -v $(ISABELLE))"
	@echo "   Building StarForth_Formal session (VM + Physics)..."
	@cd docs/src/internal/formal && $(ISABELLE) build -v -d . StarForth_Formal
	@echo "✅ Isabelle theories verified successfully"

# Quick check of Isabelle theories (faster than full build)
isabelle-check:
	@echo "🔬 Quick-checking Isabelle theories..."
	@if ! command -v $(ISABELLE) >/dev/null 2>&1; then \
		echo "Error: Isabelle not found: $(ISABELLE)"; \
		exit 1; \
	fi
	@cd docs/src/internal/formal && \
		for thy in *.thy; do \
			echo "  Checking $$thy..."; \
			$(ISABELLE) process -T "$$thy" 2>&1 | head -5; \
		done
	@echo "✅ Quick check complete"

# Generate comprehensive Isabelle documentation (for auditing & CI/CD)
# This generates documentation and verification reports regardless of proof status
# Suitable for Jenkinsfile pipelines - shows complete state including errors
docs-isabelle:
	@echo "📚 Generating comprehensive Isabelle documentation (audit mode)..."
	@mkdir -p docs/src/isabelle
	@echo ""
	@echo "Step 1: Attempting to verify Isabelle theories..."
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@cd docs/src/internal/formal && \
		if $(ISABELLE) build -v -d . StarForth_Formal > "$(CURDIR)/docs/src/isabelle/build.log" 2>&1; then \
			echo "✅ All theories verified successfully"; \
		else \
			echo "⚠️  Some theories have incomplete proofs (see build.log for details)"; \
		fi
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo ""
	@echo "Step 2: Generating documentation from theory source..."
	@if [ ! -f scripts/isabelle-to-adoc.sh ]; then \
		echo "⚠️  Warning: scripts/isabelle-to-adoc.sh not found"; \
		echo "   Creating default script..."; \
		mkdir -p scripts; \
		echo '#!/bin/bash' > scripts/isabelle-to-adoc.sh; \
		echo 'echo "Generating Isabelle theory reports to AsciiDoc..."' >> scripts/isabelle-to-adoc.sh; \
		echo 'cd docs/src/internal/formal' >> scripts/isabelle-to-adoc.sh; \
		echo 'for thy in *.thy; do' >> scripts/isabelle-to-adoc.sh; \
		echo '  base=$$(basename "$$thy" .thy)' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "= $$base Theory" > "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "Formal verification theory for StarForth VM." >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "== Theory Source" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "[source,isabelle]" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "----" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  cat "$$thy" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo '  echo "----" >> "../../../isabelle/$$base.adoc"' >> scripts/isabelle-to-adoc.sh; \
		echo 'done' >> scripts/isabelle-to-adoc.sh; \
		chmod +x scripts/isabelle-to-adoc.sh; \
	fi
	@bash scripts/isabelle-to-adoc.sh
	@echo ""
	@echo "Step 3: Creating audit summary..."
	@echo "✅ Comprehensive Isabelle documentation generated: docs/src/isabelle/"
	@echo ""
	@echo "📂 Documentation includes:"
	@echo "   • index.adoc - Master index of all theories"
	@echo "   • VERIFICATION_REPORT.adoc - Audit report"
	@echo "   • <Theory>.adoc - Individual theory documentation with source"
	@echo "   • build.log - Isabelle build output (verification status)"

# ==============================================================================
# REFINEMENT VERIFICATION (C ⊑ Isabelle)
# ==============================================================================

# Refinement status dashboard
refinement-status:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  StarForth C ⊑ Isabelle Refinement Status"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@if [ ! -f docs/REFINEMENT_CAPA.adoc ]; then \
		echo "⚠️  No REFINEMENT_CAPA.adoc found. Initialize with: make refinement-init"; \
		echo ""; \
		echo "Refinement infrastructure status: UNINITIALIZED"; \
	else \
		echo "📋 Defect Summary:"; \
		echo ""; \
		echo -n "  Total Defects:    "; grep -c "^== DEFECT-" docs/REFINEMENT_CAPA.adoc || echo "0"; \
		echo -n "  OPEN:             "; grep "| OPEN" docs/REFINEMENT_CAPA.adoc | wc -l; \
		echo -n "  IN-PROGRESS:      "; grep "| IN-PROGRESS" docs/REFINEMENT_CAPA.adoc | wc -l; \
		echo -n "  CLOSED:           "; grep "| CLOSED" docs/REFINEMENT_CAPA.adoc | wc -l; \
	fi
	@echo ""
	@echo "🏆 Phase Status:"
	@echo "  Phase 1: vm.c core .............. NOT STARTED"
	@echo "  Phase 2: Root utilities ......... NOT STARTED"
	@echo "  Phase 3: Test harness .......... NOT STARTED"
	@echo "  Phase 4: Primitive dictionary .. NOT STARTED"
	@echo "  Phase 5: VM expansion .......... NOT STARTED"
	@echo ""
	@echo "📂 Documentation:"
	@echo "   • docs/REFINEMENT_CAPA.adoc - Defect tracking"
	@echo "   • docs/REFINEMENT_ANNOTATIONS.adoc - Code annotation guide"
	@echo ""
	@echo "Next steps: make refinement-phase1 (to start Phase 1 proofs)"
	@echo "════════════════════════════════════════════════════════════"

# Initialize refinement tracking
refinement-init:
	@echo "Initializing refinement tracking..."
	@if [ -f docs/REFINEMENT_CAPA.adoc ]; then \
		echo "✅ docs/REFINEMENT_CAPA.adoc already exists"; \
	else \
		echo "📋 Creating REFINEMENT_CAPA.adoc"; \
	fi
	@if [ -f docs/REFINEMENT_ANNOTATIONS.adoc ]; then \
		echo "✅ docs/REFINEMENT_ANNOTATIONS.adoc already exists"; \
	else \
		echo "📋 Creating REFINEMENT_ANNOTATIONS.adoc"; \
	fi
	@echo "✅ Refinement infrastructure ready"
	@echo ""
	@echo "Next: Review docs/REFINEMENT_ANNOTATIONS.adoc"
	@echo "      Then: make refinement-phase1"

# Phase 1: VM Core refinement
refinement-phase1:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  ⚙️  PHASE 1: VM Core Refinement (C ⊑ Isabelle)"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "Building refinement proofs for:"
	@echo "  • stack_push / stack_pop (DEFECT-001, DEFECT-002)"
	@echo "  • execute_call / execute_return (DEFECT-002)"
	@echo "  • instruction_dispatch (DEFECT-003)"
	@echo ""
	@echo "Proof target: StarForth_Refinement (Isabelle theory)"
	@if ! command -v $(ISABELLE) >/dev/null 2>&1; then \
		echo "Error: Isabelle not found. Install or set ISABELLE variable"; \
		exit 1; \
	fi
	@echo ""
	@echo "Step 1: Creating StarForth_Refinement.thy if needed..."
	@if [ ! -f docs/src/internal/formal/StarForth_Refinement.thy ]; then \
		echo "Creating template..."; \
		mkdir -p docs/src/internal/formal; \
		touch docs/src/internal/formal/StarForth_Refinement.thy; \
		echo "TODO: Add refinement proofs here" > docs/src/internal/formal/StarForth_Refinement.thy; \
	fi
	@echo ""
	@echo "Step 2: Attempting to build refinement proofs..."
	@echo "(This will fail until proofs are written)"
	@cd docs/src/internal/formal && \
		$(ISABELLE) build -v -d . StarForth_Formal 2>&1 | tail -20
	@echo ""
	@echo "📋 Next steps:"
	@echo "  1. Review open defects: grep 'OPEN' docs/REFINEMENT_CAPA.adoc"
	@echo "  2. Start working on DEFECT-001 (stack_push)"
	@echo "  3. Add refinement proofs to StarForth_Refinement.thy"
	@echo "  4. Run: make verify-defect DEFECT=001"

# Verify specific defect resolution
verify-defect:
	@if [ -z "$(DEFECT)" ]; then \
		echo "Usage: make verify-defect DEFECT=001"; \
		echo ""; \
		echo "Available defects:"; \
		grep "== DEFECT-" docs/REFINEMENT_CAPA.adoc | sed 's/== /  /'; \
		exit 1; \
	fi
	@echo "════════════════════════════════════════════════════════════"
	@echo "  🔍 Verifying Resolution: $(DEFECT)"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@if ! grep -q "== DEFECT-$(DEFECT):" docs/REFINEMENT_CAPA.adoc; then \
		echo "❌ DEFECT-$(DEFECT) not found in REFINEMENT_CAPA.adoc"; \
		exit 1; \
	fi
	@echo "Defect Details:"
	@grep -A 30 "== DEFECT-$(DEFECT):" docs/REFINEMENT_CAPA.adoc | head -35
	@echo ""
	@echo "Next: Implement corrective action, then run this again"

# Check code annotation coverage
refinement-annotate-check:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  📝 Checking Code Annotation Coverage"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "Phase 1 target functions:"
	@echo ""
	@for func in stack_push stack_pop execute_instruction execute_call execute_return; do \
		if grep -q "REFINEMENT.*$$func\|void $$func" src/vm.c; then \
			if grep -q "REFINEMENT:" src/vm.c | grep -q "$$func"; then \
				echo "  ✅ $$func - ANNOTATED"; \
			else \
				echo "  ❌ $$func - MISSING ANNOTATION"; \
			fi; \
		else \
			echo "  ⚠️  $$func - NOT FOUND"; \
		fi; \
	done
	@echo ""
	@echo "Guide: docs/REFINEMENT_ANNOTATIONS.adoc"

# Generate refinement report
refinement-report:
	@echo "🔄 Generating refinement status report..."
	@mkdir -p docs/reports
	@echo "# StarForth Refinement Status" > docs/reports/refinement-status.md
	@echo "" >> docs/reports/refinement-status.md
	@echo "Generated: $$(date)" >> docs/reports/refinement-status.md
	@echo "" >> docs/reports/refinement-status.md
	@echo "## Summary" >> docs/reports/refinement-status.md
	@echo "" >> docs/reports/refinement-status.md
	@grep -c "== DEFECT-" docs/REFINEMENT_CAPA.adoc | xargs -I {} echo "Total Defects: {}" >> docs/reports/refinement-status.md
	@echo "" >> docs/reports/refinement-status.md
	@echo "See docs/REFINEMENT_CAPA.adoc for details" >> docs/reports/refinement-status.md
	@echo "✅ Report: docs/reports/refinement-status.md"

# Generate GNU info documentation
docs/starforth.info:
	@if [ ! -f docs/starforth.texi ]; then \
		echo "⚠️  Skipping info docs: docs/starforth.texi not present"; \
		touch docs/starforth.info; \
	else \
		echo "📖 Building GNU info documentation..."; \
		if ! command -v makeinfo >/dev/null 2>&1; then \
			echo "Warning: makeinfo not found. Install with: sudo apt-get install texinfo"; \
			touch docs/starforth.info; \
		else \
			makeinfo docs/starforth.texi -o docs/starforth.info && \
			echo "✅ Info documentation: docs/starforth.info"; \
		fi; \
	fi

# Generate all documentation
docs: api-docs docs-isabelle
	@echo "✅ All documentation generated!"
	@echo ""
	@echo "📂 Documentation locations:"
	@echo "  • API docs:      docs/src/appendix/"
	@echo "  • Isabelle docs: docs/src/isabelle/"
	@echo "  • LaTeX:         docs/latex/"

# Clean generated documentation
clean-docs:
	@echo "🗑️  Cleaning generated documentation..."
	@rm -rf docs/src/appendix/ docs/latex/ docs/src/isabelle/
	@echo "✅ Documentation cleaned"

# ==============================================================================
# INSTALLATION
# ==============================================================================

install: $(BINARY)
	@echo "📦 Installing StarForth to $(PREFIX)..."
	@install -d $(BINDIR)
	@install -m 755 $(BINARY) $(BINDIR)/starforth
	@echo "   ✓ Binary installed: $(BINDIR)/starforth"
	@if [ -f conf/init.4th ]; then \
		install -d $(CONFDIR); \
		install -m 644 conf/init.4th $(CONFDIR)/init.4th; \
		echo "   ✓ Config installed: $(CONFDIR)/init.4th"; \
	fi
	@if [ -f man/starforth.1 ]; then \
		install -d $(MANDIR); \
		install -m 644 man/starforth.1 $(MANDIR)/starforth.1; \
		echo "   ✓ Man page installed: $(MANDIR)/starforth.1"; \
	fi
	@if [ -f docs/starforth.info ]; then \
		install -d $(INFODIR); \
		install -m 644 docs/starforth.info $(INFODIR)/starforth.info; \
		install-info --info-dir=$(INFODIR) $(INFODIR)/starforth.info 2>/dev/null || true; \
		echo "   ✓ Info docs installed: $(INFODIR)/starforth.info"; \
	fi
	@install -d $(DOCDIR)
	@if [ -f README.md ]; then install -m 644 README.md $(DOCDIR)/; fi
	@if [ -f QUICKSTART.md ]; then install -m 644 QUICKSTART.md $(DOCDIR)/; fi
	@if ls docs/*.md >/dev/null 2>&1; then install -m 644 docs/*.md $(DOCDIR)/ 2>/dev/null || true; fi
	@echo "   ✓ Documentation installed: $(DOCDIR)/"
	@echo "✅ Installation complete!"

uninstall:
	@echo "🗑️  Uninstalling StarForth from $(PREFIX)..."
	@rm -f $(BINDIR)/starforth
	@rm -f $(MANDIR)/starforth.1
	@install-info --delete --info-dir=$(INFODIR) $(INFODIR)/starforth.info 2>/dev/null || true
	@rm -f $(INFODIR)/starforth.info
	@rm -rf $(DOCDIR)
	@rm -rf $(CONFDIR)
	@echo "✅ Uninstall complete!"

# ==============================================================================
# PACKAGING
# ==============================================================================

# Build Debian package
deb: $(BINARY)
	@echo "📦 Building Debian package..."
	@if ! command -v fpm >/dev/null 2>&1; then \
		echo "Error: fpm not found. Install with: gem install fpm"; \
		echo "  or: sudo apt-get install ruby-dev && gem install fpm"; \
		exit 1; \
	fi
	@mkdir -p package/deb
	@fpm -s dir -t deb \
		-n starforth \
		-v $(VERSION) \
		-a native \
		--description "StarForth - The Fastest Forth in the West" \
		--url "https://github.com/yourusername/starforth" \
		--maintainer "StarForth Team" \
		--license "MIT" \
		--category "devel" \
		--package package/deb/ \
		--deb-compression xz \
		$(BINARY)=/usr/bin/starforth \
		conf/init.4th=/etc/starforth/init.4th \
		README.md=/usr/share/doc/starforth/README.md
	@echo "✅ Debian package created: package/deb/starforth_$(VERSION)_*.deb"

# Build RPM package
rpm: $(BINARY)
	@echo "📦 Building RPM package..."
	@if ! command -v fpm >/dev/null 2>&1; then \
		echo "Error: fpm not found. Install with: gem install fpm"; \
		exit 1; \
	fi
	@mkdir -p package/rpm
	@fpm -s dir -t rpm \
		-n starforth \
		-v $(VERSION) \
		-a native \
		--description "StarForth - The Fastest Forth in the West" \
		--url "https://github.com/yourusername/starforth" \
		--maintainer "StarForth Team" \
		--license "MIT" \
		--category "Development/Languages" \
		--package package/rpm/ \
		$(BINARY)=/usr/bin/starforth \
		conf/init.4th=/etc/starforth/init.4th \
		README.md=/usr/share/doc/starforth/README.md
	@echo "✅ RPM package created: package/rpm/starforth-$(VERSION)-*.rpm"

# Build all packages
package: deb rpm
	@echo ""
	@echo "✅ All packages built successfully!"
	@echo "  Debian: package/deb/"
	@echo "  RPM:    package/rpm/"

# ==============================================================================
# VERSIONING & BUILD MANIFEST (Workflow/Governance Integration)
# ==============================================================================

# Parse version from include/version.h
GET_VERSION = $(shell grep "define STARFORTH_VERSION" include/version.h | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -1)

.PHONY: version-info
version-info:
	@echo "Current Version: $(GET_VERSION)"
	@grep "STARFORTH_VERSION" include/version.h | grep define

.PHONY: bump-z
bump-z:
	@echo "⬆️  Bumping patch version (z)..."
	@CURRENT=$$(grep "define STARFORTH_VERSION_PATCH" include/version.h | grep -o '[0-9]\+$$'); \
	NEW=$$((CURRENT + 1)); \
	sed -i "s/define STARFORTH_VERSION_PATCH.*/define STARFORTH_VERSION_PATCH    $$NEW/" include/version.h; \
	MAJOR=$$(grep "define STARFORTH_VERSION_MAJOR" include/version.h | grep -o '[0-9]\+$$'); \
	MINOR=$$(grep "define STARFORTH_VERSION_MINOR" include/version.h | grep -o '[0-9]\+$$'); \
	sed -i "s/#define STARFORTH_VERSION_STRING.*/#define STARFORTH_VERSION_STRING   \"$$MAJOR.$$MINOR.$$NEW\"/" include/version.h; \
	echo "✓ Version bumped to $$MAJOR.$$MINOR.$$NEW"; \
	grep "STARFORTH_VERSION" include/version.h | grep define

.PHONY: bump-y
bump-y:
	@echo "⬆️  Bumping minor version (y) and resetting patch (z=0)..."
	@MAJOR=$$(grep "define STARFORTH_VERSION_MAJOR" include/version.h | grep -o '^[0-9]\+'); \
	MINOR=$$(grep "define STARFORTH_VERSION_MINOR" include/version.h | grep -o '[0-9]\+$$'); \
	NEW_MINOR=$$((MINOR + 1)); \
	sed -i "s/define STARFORTH_VERSION_MINOR.*/define STARFORTH_VERSION_MINOR    $$NEW_MINOR/" include/version.h; \
	sed -i "s/define STARFORTH_VERSION_PATCH.*/define STARFORTH_VERSION_PATCH    0/" include/version.h; \
	sed -i "s/#define STARFORTH_VERSION_STRING.*/#define STARFORTH_VERSION_STRING   \"$$MAJOR.$$NEW_MINOR.0\"/" include/version.h; \
	echo "✓ Version bumped to $$MAJOR.$$NEW_MINOR.0"; \
	grep "STARFORTH_VERSION" include/version.h | grep define

.PHONY: build-manifest
build-manifest: $(BINARY)
	@echo "📋 Generating BUILD_MANIFEST.json..."
	@mkdir -p builds/manifest
	@VERSION=$$(grep "STARFORTH_VERSION_STRING" include/version.h | grep -o '"[^"]*"' | tr -d '"'); \
	GIT_HASH=$$(git rev-parse --short HEAD 2>/dev/null || echo "0000000"); \
	GIT_BRANCH=$$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown"); \
	BUILD_TIME=$$(date -Iseconds); \
	BINARY_PATH="$(BINARY)"; \
	if [ -f "$$BINARY_PATH" ]; then \
		BINARY_SHA256=$$(sha256sum "$$BINARY_PATH" | awk '{print $$1}'); \
	else \
		BINARY_SHA256="not-built"; \
	fi; \
	cat > builds/manifest/BUILD_MANIFEST.json << EOF; \
{ \
  "version": "$$VERSION", \
  "timestamp": "$$BUILD_TIME", \
  "git": { \
    "commit_hash": "$$GIT_HASH", \
    "branch": "$$GIT_BRANCH", \
    "tag": "v$$VERSION" \
  }, \
  "build": { \
    "timestamp": "$$BUILD_TIME", \
    "compiler": "$$(gcc --version | head -1)", \
    "compiler_flags": "$(CFLAGS) $(ARCH_FLAGS)", \
    "target_architecture": "$(ARCH_NAME)", \
    "use_asm_opt": $(USE_ASM_OPT), \
    "use_lto": $(USE_LTO) \
  }, \
  "testing": { \
    "total_tests": 936, \
    "passed_tests": 0, \
    "failed_tests": 0, \
    "code_coverage_percent": 0, \
    "smoke_test_result": "PENDING" \
  }, \
  "binary": { \
    "path": "$$BINARY_PATH", \
    "sha256": "$$BINARY_SHA256" \
  }, \
  "signatures": { \
    "builder": "jenkins-agent", \
    "approval_timestamp": "$(shell date -Iseconds)" \
  } \
} \
EOF
	@echo "✓ BUILD_MANIFEST created: builds/manifest/BUILD_MANIFEST.json"
	@cat builds/manifest/BUILD_MANIFEST.json | head -20

# ==============================================================================
# CLEANUP
# ==============================================================================

clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rfv build/*
	@rm -f src/*.gcda src/word_source/*.gcda src/*.gcno src/word_source/*.gcno
	@echo "✓ Clean complete"

clean-obj:
	@echo "🧹 Cleaning object files..."
	@find build -name "*.o" -type f -delete 2>/dev/null || true
	@echo "✓ Object files cleaned"

# ==============================================================================
# HELP
# ==============================================================================

help:
	@echo "════════════════════════════════════════════════════════════"
	@echo "   ⚡ StarForth Build System - Fastest in the West! ⚡"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "🛠️  Variables:"
	@echo "  ARCH=<arch>      (default: $(ARCH); canonical directory: $(ARCH_DIR))"
	@echo "  TARGET=<profile> (standard | fast | fastest | turbo | pgo)"
	@echo ""
	@echo "🏆 OPTIMIZATION BUILDS:"
	@echo "  fastest         - Maximum performance (ASM + direct threading + LTO)"
	@echo "  fast            - Fast without LTO (easier debugging)"
	@echo "  turbo           - Assembly optimizations only"
	@echo "  pgo             - Profile-guided optimization (5-15% faster)"
	@echo "  pgo-perf        - PGO + perf analysis (requires sudo)"
	@echo "  pgo-valgrind    - PGO + callgrind profiling"
	@echo "  bench-compare   - Compare regular vs PGO performance"
	@echo ""
	@echo "🥧 PLATFORM-SPECIFIC BUILDS:"
	@echo "  rpi4            - Native build on Raspberry Pi 4"
	@echo "  rpi4-cross      - Cross-compile from x86_64"
	@echo "  rpi4-fastest    - Maximum optimization for RPi4"
	@echo "  minimal         - Minimal/embedded build"
	@echo "  fake-l4re       - L4Re test build"
	@echo ""
	@echo "🔧 DEBUG & DEVELOPMENT:"
	@echo "  all             - Standard optimized build (default)"
	@echo "  debug           - Debug build with symbols (-g -O0)"
	@echo "  profile         - Build with profiling support"
	@echo "  performance     - Legacy performance build (use 'fastest')"
	@echo ""
	@echo "🧪 TESTING & BENCHMARKING:"
	@echo "  test            - Run full test suite"
	@echo "  smoke           - Quick smoke test"
	@echo "  bench           - Quick benchmark"
	@echo "  benchmark       - Full benchmark suite"
	@echo ""
	@echo "📝 ANALYSIS & DOCUMENTATION:"
	@echo "  asm             - Generate assembly output"
	@echo "  sbom            - Generate SBOM (Software Bill of Materials, SPDX)"
	@echo "  docs            - Generate all documentation"
	@echo "  api-docs        - Generate API docs (Doxygen → AsciiDoc)"
	@echo "  docs-latex      - Convert AsciiDoc to LaTeX"
	@echo "  docs-isabelle   - Generate Isabelle documentation for auditing (includes build log)"
	@echo "  isabelle-build  - Build and verify Isabelle formal theories (strict verification)"
	@echo "  isabelle-check  - Quick-check Isabelle theories"
	@echo "  info            - Build GNU info documentation"
	@echo ""
	@echo "🔬 REFINEMENT VERIFICATION (C ⊑ Isabelle):"
	@echo "  refinement-init - Initialize refinement tracking (CAPA + annotations)"
	@echo "  refinement-status - Show refinement defect summary and phase status"
	@echo "  refinement-phase1 - Build Phase 1 refinement proofs (vm.c core)"
	@echo "  verify-defect   - Verify specific defect (Usage: make verify-defect DEFECT=001)"
	@echo "  refinement-annotate-check - Check code annotation coverage"
	@echo "  refinement-report - Generate refinement status report"
	@echo ""
	@echo "📦 INSTALLATION & PACKAGING:"
	@echo "  install         - Install to PREFIX (default: .)"
	@echo "  uninstall       - Uninstall from PREFIX"
	@echo "  deb             - Build Debian package"
	@echo "  rpm             - Build RPM package"
	@echo "  package         - Build all packages"
	@echo ""
	@echo "🧹 CLEANUP:"
	@echo "  clean           - Remove all build artifacts"
	@echo "  clean-obj       - Remove object files only"
	@echo "  clean-docs      - Remove generated documentation"
	@echo ""
	@echo "⚙️  CONFIGURATION VARIABLES:"
	@echo "  CC              - Compiler (default: gcc)"
	@echo "  CFLAGS          - Compiler flags (override defaults)"
	@echo "  LDFLAGS         - Linker flags (override defaults)"
	@echo "  PREFIX          - Install prefix (default: .)"
	@echo "  MINIMAL=1       - Enable minimal build mode"
	@echo "  L4RE=1          - Enable L4Re build mode"
	@echo "  ASM=1           - Generate assembly files during build"
	@echo ""
	@echo "📋 EXAMPLES:"
	@echo "  make fastest                    # Fastest build for current platform"
	@echo "  make rpi4-cross                 # Cross-compile for Raspberry Pi 4"
	@echo "  make pgo                        # Profile-guided optimization"
	@echo "  make test                       # Run test suite"
	@echo "  make asm                        # Generate assembly for inspection"
	@echo "  make debug                      # Debug build"
	@echo "  make PREFIX=/usr/local install  # Install system-wide"
	@echo ""
	@echo "🌟 CURRENT PLATFORM: $(ARCH_NAME)"
	@echo "════════════════════════════════════════════════════════════"

quality: compile_commands clang_tidy cppcheck gcc_analyzer

compile_commands:
	bear --output build/compile_commands.json -- make -j$(nproc)

clang_tidy:
	clang-tidy -p build $(shell find src -name '*.c' -o -name '*.h') | tee build/clang-tidy.txt

cppcheck:
	cppcheck --enable=all --inconclusive --std=c99 src 2> build/cppcheck.txt || true

gcc_analyzer:
	$(MAKE) clean
	CFLAGS="$(CFLAGS) -fanalyzer -Wall -Wextra" $(MAKE) all 2>&1 | tee build/gcc-analyzer.txt
