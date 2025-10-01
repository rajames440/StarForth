STRICT_PTR ?= 1
CC = gcc

# Architecture detection
ARCH := $(shell uname -m)

# Detect if we're on x86_64 or ARM64
ifeq ($(ARCH),x86_64)
    ARCH_NAME = x86_64
    ARCH_FLAGS = -march=native
    ARCH_DEFINES = -DARCH_X86_64=1
    ASM_SYNTAX = -masm=intel
else ifeq ($(ARCH),amd64)
    ARCH_NAME = x86_64
    ARCH_FLAGS = -march=native
    ARCH_DEFINES = -DARCH_X86_64=1
    ASM_SYNTAX = -masm=intel
else ifeq ($(ARCH),aarch64)
    ARCH_NAME = ARM64
    ARCH_FLAGS = -march=armv8-a+crc+simd -mtune=cortex-a72
    ARCH_DEFINES = -DARCH_ARM64=1
    ASM_SYNTAX =
else ifeq ($(ARCH),arm64)
    ARCH_NAME = ARM64
    ARCH_FLAGS = -march=armv8-a+crc+simd -mtune=cortex-a72
    ARCH_DEFINES = -DARCH_ARM64=1
    ASM_SYNTAX =
else
    $(warning Unknown architecture: $(ARCH), using generic flags)
    ARCH_NAME = generic
    ARCH_FLAGS =
    ARCH_DEFINES =
    ASM_SYNTAX =
endif

# Base compiler flags
BASE_CFLAGS = -std=c99 -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include -DSTRICT_PTR=$(STRICT_PTR)

# Fast defaults; override CFLAGS on the command line if you need a debug build.
CFLAGS ?= $(BASE_CFLAGS) -O2 -march=native -flto=auto -fuse-linker-plugin -DNDEBUG \
          -ffunction-sections -fdata-sections -fomit-frame-pointer \
          -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-strict-aliasing

LDFLAGS ?= -Wl,--gc-sections -s -flto=auto -fuse-linker-plugin -static

# Platform support
ifdef MINIMAL
CFLAGS += -DSTARFORTH_MINIMAL=1 -nostdlib -ffreestanding
LDFLAGS += -nostdlib
PLATFORM_SRC = src/platform/starforth_minimal.c
else
PLATFORM_SRC =
endif

# Source files
SRC = $(wildcard src/*.c src/word_source/*.c src/test_runner/*.c src/test_runner/modules/*.c) $(PLATFORM_SRC)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
ASM_FILES = $(patsubst src/%.c,build/%.s,$(SRC))
TARGET = build/starforth

# Default target
all: banner $(TARGET)
	@echo ""
	@echo "✓ Build complete: $(TARGET)"
	@echo "  Architecture: $(ARCH_NAME)"
	@echo "  Optimizations: $(if $(findstring USE_ASM_OPT,$(CFLAGS)),Enabled,Disabled)"
	@echo "  Direct Threading: $(if $(findstring USE_DIRECT_THREADING,$(CFLAGS)),Enabled,Disabled)"

banner:
	@echo "════════════════════════════════════════════════════════════"
	@echo "   ⚡ Building the Fastest Forth in the West! ⚡"
	@echo "════════════════════════════════════════════════════════════"
	@echo "  Target: $(ARCH_NAME)"
	@echo ""

# ==============================================================================
# 🤠 FASTEST IN THE WEST TARGETS
# ==============================================================================

# The quick-draw champion - maximum speed, no compromises
fastest: banner
	@echo "🏆 Building FASTEST configuration..."
	@echo "   - Architecture: $(ARCH_NAME)"
	@echo "   - Assembly optimizations: ENABLED"
	@echo "   - Direct threading: ENABLED"
	@echo "   - Profile-guided optimization: Ready"
	@echo ""
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -funroll-loops -finline-functions -fomit-frame-pointer -fno-plt -fno-semantic-interposition" LDFLAGS="-flto -s -Wl,--gc-sections" $(TARGET)
	@echo ""
	@echo "🎯 You've built the FASTEST FORTH IN THE WEST!"
	@echo ""
	@echo "Quick-draw benchmark:"
	@time -f "  ⏱️  Time: %E seconds" ./$(TARGET) -c ": BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE" 2>/dev/null || true

# Optimized but without LTO (for readable assembly)
fast: banner
	@echo "⚡ Building FAST configuration (no LTO for debugging)..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG" LDFLAGS="-s" $(TARGET)

# Assembly optimizations only (no direct threading)
turbo: banner
	@echo "🚀 Building TURBO configuration (ASM only)..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DNDEBUG -flto" LDFLAGS="-flto -s" $(TARGET)

# Profile-guided optimization - two-stage build
pgo: banner
	@echo "📊 Building with Profile-Guided Optimization..."
	@echo "   Stage 1: Instrumentation build..."
	$(MAKE) clean
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -DUSE_ASM_OPT=1 -fprofile-generate" LDFLAGS="-fprofile-generate" $(TARGET)
	@echo "   Stage 2: Running profiling workload..."
	@./$(TARGET) -c ": PROFILE-TEST 100000 0 DO 1 2 + 3 * DROP LOOP ; PROFILE-TEST BYE" || true
	@echo "   Stage 3: Optimized build with profile data..."
	$(MAKE) clean-obj
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction" LDFLAGS="-fprofile-use" $(TARGET)
	@echo "   Cleaning up profile data..."
	@rm -f src/*.gcda src/word_source/*.gcda
	@echo "✓ PGO build complete!"

# ==============================================================================
# RASPBERRY PI 4 TARGETS
# ==============================================================================

# Native build on Raspberry Pi 4
rpi4: banner
	@echo "🥧 Building for Raspberry Pi 4 (native)..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto" LDFLAGS="-flto -s" $(TARGET)

# Cross-compile for Raspberry Pi 4 from x86_64
rpi4-cross: banner
	@echo "🥧 Cross-compiling for Raspberry Pi 4..."
	$(MAKE) CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -static" \
	        LDFLAGS="-flto -s -static" \
	        $(TARGET)
	@echo "✓ Cross-compiled binary ready: $(TARGET)"
	@echo "  Copy to RPi4: scp $(TARGET) pi@raspberrypi.local:~/"

# Raspberry Pi 4 with maximum optimization
rpi4-fastest: banner
	@echo "🥧⚡ Building FASTEST for Raspberry Pi 4..."
	$(MAKE) CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd+crypto -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -funroll-loops -finline-functions" \
	        LDFLAGS="-flto -s -static" \
	        $(TARGET)

# ==============================================================================
# EXISTING TARGETS (enhanced)
# ==============================================================================

minimal:
	$(MAKE) MINIMAL=1

l4re:
	$(MAKE) MINIMAL=1 CC=l4-gcc CFLAGS="$(BASE_CFLAGS) -DL4RE_TARGET=1"

profile:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -DPROFILE_ENABLED=1 -g -O1"

performance:
	@echo "Note: Use 'make fastest' for maximum performance"
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O3 -DSTARFORTH_PERFORMANCE -DNDEBUG -march=native -flto -funroll-loops -finline-functions -fomit-frame-pointer" LDFLAGS="-flto -s"

# Debug build with symbols
debug: banner
	@echo "🐛 Building DEBUG configuration..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O0 -g -DDEBUG" LDFLAGS="" $(TARGET)

# ==============================================================================
# BUILD RULES
# ==============================================================================

# Main executable
$(TARGET): $(OBJ) | build
	@echo "🔗 Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Build rules
build/%.o: src/%.c | build
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

build/platform/%.o: src/platform/%.c | build
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

build/word_source/%.o: src/word_source/%.c | build
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

build/test_runner/%.o: src/test_runner/%.c | build
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

build/test_runner/modules/%.o: src/test_runner/modules/%.c | build
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

# Create build directories
build:
	@mkdir -p build
	@mkdir -p build/platform
	@mkdir -p build/word_source
	@mkdir -p build/test_runner
	@mkdir -p build/test_runner/modules

# ==============================================================================
# BENCHMARKING AND TESTING
# ==============================================================================

# Quick benchmark
bench: $(TARGET)
	@echo ""
	@echo "🏇 Running quick-draw benchmark..."
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "Test: 1 million stack operations"
	@time -f "Time: %E seconds" ./$(TARGET) -c ": BENCH 1000000 0 DO 1 2 + DROP LOOP ; BENCH BYE" 2>/dev/null || echo "Benchmark complete"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Full benchmark suite
benchmark: $(TARGET)
	@echo ""
	@echo "🎯 Full Benchmark Suite - Fastest Forth in the West"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "📊 Stack Operations (1M iterations):"
	@time -f "  ⏱️  %E seconds" ./$(TARGET) -c ": BENCH-STACK 1000000 0 DO 1 2 3 DROP DROP DROP LOOP ; BENCH-STACK BYE" 2>/dev/null || true
	@echo ""
	@echo "📊 Arithmetic Operations (1M iterations):"
	@time -f "  ⏱️  %E seconds" ./$(TARGET) -c ": BENCH-MATH 1000000 0 DO 10 20 + 5 * 100 / DROP LOOP ; BENCH-MATH BYE" 2>/dev/null || true
	@echo ""
	@echo "📊 Logic Operations (1M iterations):"
	@time -f "  ⏱️  %E seconds" ./$(TARGET) -c ": BENCH-LOGIC 1000000 0 DO 255 DUP AND DUP OR XOR DROP LOOP ; BENCH-LOGIC BYE" 2>/dev/null || true
	@echo ""
	@echo "════════════════════════════════════════════════════════════"

# Run tests
test: $(TARGET)
	@echo "🧪 Running test suite..."
	./$(TARGET) -t

# ==============================================================================
# CLEANUP
# ==============================================================================

clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf build/*
	@rm -f src/*.gcda src/word_source/*.gcda src/*.gcno src/word_source/*.gcno
	@echo "✓ Clean complete"

clean-obj:
	@rm -rf build/*.o build/**/*.o

# ==============================================================================
# ASSEMBLY OUTPUT
# ==============================================================================

# Generate assembly files for inspection
asm: banner
	@echo "📝 Generating assembly output..."
	$(MAKE) ASM=1 CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1" LDFLAGS="" $(TARGET)
	@echo "✓ Assembly files generated in build/"
	@echo "  Example: less build/stack_management.s"

# ==============================================================================
# DOCUMENTATION GENERATION
# ==============================================================================

# Generate all documentation formats
docs:
	@echo "📚 Generating comprehensive API documentation..."
	@./scripts/generate_docs.sh

# Generate HTML documentation only (fast)
docs-html:
	@echo "📚 Generating HTML API documentation..."
	@doxygen Doxyfile 2>&1 | grep -v "warning:" || true
	@echo "✓ HTML documentation generated in docs/api/html/"

# Generate PDF documentation (requires pdflatex)
docs-pdf: docs-html
	@echo "📄 Generating PDF documentation..."
	@cd docs/api/latex && $(MAKE) pdf > /dev/null 2>&1 && cp refman.pdf ../StarForth-API-Reference.pdf && echo "✓ PDF: docs/api/StarForth-API-Reference.pdf"

# Open documentation in browser
docs-open: docs-html
	@echo "🌐 Opening documentation in browser..."
	@if command -v xdg-open > /dev/null; then \
		xdg-open docs/api/html/index.html; \
	elif command -v open > /dev/null; then \
		open docs/api/html/index.html; \
	elif command -v start > /dev/null; then \
		start docs/api/html/index.html; \
	else \
		echo "Please open docs/api/html/index.html in your browser"; \
	fi

# Clean documentation
docs-clean:
	@echo "🧹 Cleaning documentation artifacts..."
	@rm -rf docs/api
	@echo "✓ Documentation cleaned"

# ==============================================================================
# HELP
# ==============================================================================

help:
	@echo "════════════════════════════════════════════════════════════"
	@echo "   ⚡ StarForth Build System - Fastest in the West! ⚡"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "🏆 FASTEST TARGETS:"
	@echo "  fastest         - Maximum performance (ASM + direct threading + LTO)"
	@echo "  fast            - Fast without LTO (easier debugging)"
	@echo "  turbo           - Assembly optimizations only"
	@echo "  pgo             - Profile-guided optimization (2-stage build)"
	@echo ""
	@echo "🥧 RASPBERRY PI 4:"
	@echo "  rpi4            - Native build on Raspberry Pi 4"
	@echo "  rpi4-cross      - Cross-compile from x86_64"
	@echo "  rpi4-fastest    - Maximum optimization for RPi4"
	@echo ""
	@echo "🔧 STANDARD TARGETS:"
	@echo "  all             - Standard build (default)"
	@echo "  debug           - Debug build with symbols (-g -O0)"
	@echo "  minimal         - Minimal/embedded build"
	@echo "  l4re            - L4Re microkernel build"
	@echo "  profile         - Profiling support"
	@echo ""
	@echo "🧪 TESTING & BENCHMARKING:"
	@echo "  bench           - Quick benchmark"
	@echo "  benchmark       - Full benchmark suite"
	@echo "  test            - Run test suite"
	@echo ""
	@echo "📝 UTILITIES:"
	@echo "  asm             - Generate assembly output"
	@echo "  clean           - Remove build artifacts"
	@echo "  help            - Show this help"
	@echo ""
	@echo "📚 DOCUMENTATION:"
	@echo "  docs            - Generate all documentation (HTML, PDF, AsciiDoc, MD)"
	@echo "  docs-html       - Generate HTML documentation only"
	@echo "  docs-pdf        - Generate PDF documentation"
	@echo "  docs-open       - Open HTML documentation in browser"
	@echo "  docs-clean      - Clean documentation artifacts"
	@echo ""
	@echo "⚙️  CONFIGURATION:"
	@echo "  CC              - Compiler (default: gcc)"
	@echo "  CFLAGS          - Compiler flags"
	@echo "  LDFLAGS         - Linker flags"
	@echo "  MINIMAL=1       - Minimal build"
	@echo "  ASM=1           - Generate assembly files"
	@echo ""
	@echo "📋 EXAMPLES:"
	@echo "  make fastest                    # Fastest build for current platform"
	@echo "  make rpi4-cross                 # Cross-compile for Raspberry Pi 4"
	@echo "  make pgo                        # Profile-guided optimization"
	@echo "  make benchmark                  # Run full benchmark suite"
	@echo "  make asm                        # Generate assembly for inspection"
	@echo "  make debug                      # Debug build"
	@echo ""
	@echo "🌟 CURRENT PLATFORM: $(ARCH_NAME)"
	@echo "════════════════════════════════════════════════════════════"

.PHONY: all banner fastest fast turbo pgo rpi4 rpi4-cross rpi4-fastest minimal l4re profile performance debug bench benchmark test asm clean clean-obj help
