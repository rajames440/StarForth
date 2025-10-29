# ==============================================================================
# StarForth Build System - The Fastest Forth in the West!
# ==============================================================================
#
# Quick Start:
#   make                  - Standard optimized build
#   make fastest          - Maximum performance build
#   make test             - Run test suite
#   make help             - Show all available targets
#
# ==============================================================================

# ==============================================================================
# CONFIGURATION
# ==============================================================================

VERSION ?= 2.0.0
STRICT_PTR ?= 1
CC = gcc

# Architecture detection
ARCH := $(shell uname -m)

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

# Compiler flags
BASE_CFLAGS = -std=c99 -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include -DSTRICT_PTR=$(STRICT_PTR)

CFLAGS ?= $(BASE_CFLAGS) -O2 -march=native -flto=auto -fuse-linker-plugin -DNDEBUG \
          -DUSE_ASM_OPT=1 \
          -ffunction-sections -fdata-sections -fomit-frame-pointer \
          -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-strict-aliasing

LDFLAGS ?= -Wl,--gc-sections -s -flto=auto -fuse-linker-plugin -static

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

# Source and object files
SRC = $(wildcard src/*.c src/word_source/*.c src/test_runner/*.c src/test_runner/modules/*.c) $(PLATFORM_SRC) $(PLATFORM_TIME_SRC)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
ASM_FILES = $(patsubst src/%.c,build/%.s,$(SRC))
TARGET = build/starforth

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
.PHONY: fastest fast turbo pgo pgo-perf pgo-valgrind bench-compare
.PHONY: rpi4 rpi4-cross rpi4-fastest
.PHONY: minimal fake-l4re debug profile performance
.PHONY: test smoke bench benchmark
.PHONY: asm sbom
.PHONY: docs api-docs latex info
.PHONY: install uninstall package deb rpm

# ==============================================================================
# MAIN TARGETS
# ==============================================================================

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
# OPTIMIZATION BUILDS - The Fastest in the West
# ==============================================================================

# Maximum performance - no compromises
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

# Fast without LTO (easier debugging)
fast: banner
	@echo "⚡ Building FAST configuration (no LTO for debugging)..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG" LDFLAGS="-s" $(TARGET)

# Assembly optimizations only (no direct threading)
turbo: banner
	@echo "🚀 Building TURBO configuration (ASM only)..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DNDEBUG -flto" LDFLAGS="-flto -s" $(TARGET)

# Profile-guided optimization - 6-stage build
pgo: banner
	@echo "📊 Building with Profile-Guided Optimization..."
	@echo "   Stage 1: Clean build environment..."
	$(MAKE) clean
	@rm -f *.gcda */*.gcda */*/*.gcda src/*.gcda src/*/*.gcda build/*.gcda 2>/dev/null || true
	@rm -f *.gcno */*.gcno */*/*.gcno src/*.gcno src/*/*.gcno build/*.gcno 2>/dev/null || true
	@echo "   Stage 2: Instrumentation build..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -DUSE_ASM_OPT=1 -fprofile-generate" LDFLAGS="-fprofile-generate -lgcov" $(TARGET)
	@echo "   Stage 3: Running comprehensive profiling workload..."
	@echo "     (This exercises all code paths: tests, benchmarks, REPL, blocks, etc.)"
	@./scripts/pgo-workload.sh $(TARGET)
	@echo "   Stage 4: Collecting profile data..."
	@find . -name "*.gcda" -type f | wc -l | xargs -I {} echo "     Found {} profile data files"
	@echo "   Stage 5: Optimized build with profile data..."
	$(MAKE) clean-obj
	@rm -f *.gcno */*.gcno */*/*.gcno src/*.gcno src/*/*.gcno build/*.gcno 2>/dev/null || true
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction -Wno-error=coverage-mismatch" LDFLAGS="-fprofile-use" $(TARGET)
	@echo "   Stage 6: Cleaning up profile data..."
	@rm -f *.gcda */*.gcda */*/*.gcda src/*.gcda src/*/*.gcda build/*.gcda 2>/dev/null || true
	@rm -f *.gcno */*.gcno */*/*.gcno src/*.gcno src/*/*.gcno build/*.gcno 2>/dev/null || true
	@echo "✓ PGO build complete!"
	@echo ""
	@echo "Compare performance with: make bench-compare"

# PGO with perf analysis
pgo-perf: banner
	@echo "📊 Building with PGO + perf analysis..."
	@if ! command -v perf &> /dev/null; then \
		echo "Error: perf not found. Install with: sudo apt-get install linux-tools-generic"; \
		exit 1; \
	fi
	@echo "   Stage 1: Clean build..."
	$(MAKE) clean
	@echo "   Stage 2: Building instrumented binary..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -DUSE_ASM_OPT=1 -fprofile-generate -fno-omit-frame-pointer" LDFLAGS="-fprofile-generate -lgcov" $(TARGET)
	@echo "   Stage 3: Running workload with perf profiling..."
	@sudo perf record -g --call-graph dwarf -o pgo-perf.data -- ./scripts/pgo-workload.sh $(TARGET) 2>&1 | head -20
	@echo "   Stage 4: Optimized build with profile data..."
	$(MAKE) clean-obj
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction -fno-omit-frame-pointer -Wno-error=coverage-mismatch" LDFLAGS="-fprofile-use" $(TARGET)
	@echo "   Stage 5: Analyzing perf data..."
	@sudo perf report --stdio -i pgo-perf.data --sort comm,dso,symbol --percent-limit 1 | head -50
	@echo ""
	@echo "✓ PGO + perf build complete!"
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
	$(MAKE) clean
	@echo "   Stage 2: Building instrumented binary with debug symbols..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O2 -g -DUSE_ASM_OPT=1 -fprofile-generate" LDFLAGS="-fprofile-generate -lgcov" $(TARGET)
	@echo "   Stage 3: Running workload with callgrind profiling..."
	@echo "     (This will be slow - callgrind adds significant overhead)"
	@valgrind --tool=callgrind --callgrind-out-file=pgo-callgrind.out --dump-instr=yes --collect-jumps=yes \
		./$(TARGET) --benchmark 100 --log-none || true
	@echo "   Stage 4: Optimized build with profile data..."
	$(MAKE) clean-obj
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -g -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -fprofile-use -fprofile-correction -Wno-error=coverage-mismatch" LDFLAGS="-fprofile-use" $(TARGET)
	@echo ""
	@echo "✓ PGO + valgrind build complete!"
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
	@/usr/bin/time -f "%E elapsed, %U user" ./$(TARGET) --benchmark 10000 --log-none 2>&1 | tail -1
	@echo ""
	@echo "Building PGO optimized binary..."
	@$(MAKE) pgo > /dev/null 2>&1
	@echo "Running benchmark (10000 iterations)..."
	@echo -n "  PGO build:     "
	@/usr/bin/time -f "%E elapsed, %U user" ./$(TARGET) --benchmark 10000 --log-none 2>&1 | tail -1
	@echo ""
	@echo "✓ Comparison complete!"

# ==============================================================================
# PLATFORM-SPECIFIC BUILDS
# ==============================================================================

# Raspberry Pi 4 - native build
rpi4: banner
	@echo "🥧 Building for Raspberry Pi 4 (native)..."
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto" LDFLAGS="-flto -s" $(TARGET)

# Raspberry Pi 4 - cross-compile from x86_64
rpi4-cross: banner
	@echo "🥧 Cross-compiling for Raspberry Pi 4..."
	$(MAKE) CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -static" \
	        LDFLAGS="-flto -s -static" \
	        $(TARGET)
	@echo "✓ Cross-compiled binary ready: $(TARGET)"
	@echo "  Copy to RPi4: scp $(TARGET) pi@raspberrypi.local:~/"

# Raspberry Pi 4 - maximum optimization
rpi4-fastest: banner
	@echo "🥧⚡ Building FASTEST for Raspberry Pi 4..."
	$(MAKE) CC=aarch64-linux-gnu-gcc \
	        CFLAGS="$(BASE_CFLAGS) -march=armv8-a+crc+simd+crypto -mtune=cortex-a72 -DARCH_ARM64=1 -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 -DNDEBUG -flto -funroll-loops -finline-functions" \
	        LDFLAGS="-flto -s -static" \
	        $(TARGET)

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
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O0 -g -DDEBUG" LDFLAGS="" $(TARGET)

# Build with profiler support
profile:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -DPROFILE_ENABLED=1 -g -O1"

# Performance build (legacy - use 'fastest' instead)
performance:
	@echo "Note: Use 'make fastest' for maximum performance"
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O3 -DSTARFORTH_PERFORMANCE -DNDEBUG -march=native -flto -funroll-loops -finline-functions -fomit-frame-pointer" LDFLAGS="-flto -s"

# ==============================================================================
# BUILD RULES
# ==============================================================================

# Main executable
$(TARGET): $(OBJ) | build
	@echo "🔗 Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Pattern rule for all C files (consolidates 5 repetitive rules)
build/%.o: src/%.c | build
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@
ifdef ASM
	@$(CC) $(CFLAGS) -S $(ASM_SYNTAX) $< -o $(patsubst %.o,%.s,$@)
endif

# Create build directories
build:
	@mkdir -p build
	@mkdir -p build/platform/linux
	@mkdir -p build/platform/l4re
	@mkdir -p build/word_source
	@mkdir -p build/test_runner/modules

# ==============================================================================
# TESTING & BENCHMARKING
# ==============================================================================

# Run full test suite
test: $(TARGET)
	@echo "🧪 Running test suite..."
	@printf 'BYE\n' | ./$(TARGET) --run-tests $(PROFILE_ARGS)

# Quick smoke test
smoke: $(TARGET)
	@echo ""
	@echo "🚦 Running smoke test (1 2 + .)"
	@out=$$(printf '1 2 + . BYE\n' | ./$(TARGET) --log-none); \
	printf '%s\n' "$$out"; \
	echo "$$out" | grep -q '^ok> 3 Goodbye!' || (echo "Smoke test failed" >&2; exit 1)
	@echo "✓ Smoke test passed"

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

# ==============================================================================
# ASSEMBLY OUTPUT & ANALYSIS
# ==============================================================================

# Generate assembly files for inspection
asm: banner
	@echo "📝 Generating assembly output..."
	$(MAKE) ASM=1 CFLAGS="$(BASE_CFLAGS) $(ARCH_FLAGS) $(ARCH_DEFINES) -O3 -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1" LDFLAGS="" $(TARGET)
	@echo "✓ Assembly files generated in build/"
	@echo "  Example: less build/stack_management.s"

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

# Convert all AsciiDoc to LaTeX
latex:
	@echo "📄 Converting AsciiDoc to LaTeX..."
	@if [ ! -f scripts/asciidoc-to-latex.sh ]; then \
		echo "Error: scripts/asciidoc-to-latex.sh not found"; \
		exit 1; \
	fi
	@./scripts/asciidoc-to-latex.sh
	@echo "✅ LaTeX files generated: docs/latex/"

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

info: docs/starforth.info

# Generate all documentation
docs: api-docs latex
	@echo "✅ All documentation generated!"
	@echo ""
	@echo "📂 Documentation locations:"
	@echo "  • API docs: docs/src/appendix/"
	@echo "  • LaTeX:    docs/latex/"

# Clean generated documentation
clean-docs:
	@echo "🗑️  Cleaning generated documentation..."
	@rm -rf docs/src/appendix/ docs/latex/
	@echo "✅ Documentation cleaned"

# ==============================================================================
# INSTALLATION
# ==============================================================================

install: build/starforth
	@echo "📦 Installing StarForth to $(PREFIX)..."
	@install -d $(BINDIR)
	@install -m 755 build/starforth $(BINDIR)/starforth
	@install -d $(CONFDIR)
	@install -m 644 conf/init.4th $(CONFDIR)/init.4th
	@install -d $(MANDIR)
	@install -m 644 man/starforth.1 $(MANDIR)/starforth.1 2>/dev/null || true
	@install -d $(INFODIR)
	@install -m 644 docs/starforth.info $(INFODIR)/starforth.info 2>/dev/null || true
	@install-info --info-dir=$(INFODIR) $(INFODIR)/starforth.info 2>/dev/null || true
	@install -d $(DOCDIR)
	@install -m 644 README.md $(DOCDIR)/
	@install -m 644 QUICKSTART.md $(DOCDIR)/
	@install -m 644 docs/*.md $(DOCDIR)/ 2>/dev/null || true
	@echo "✅ Installation complete!"
	@echo "   Binary: $(BINDIR)/starforth"
	@echo "   Config: $(CONFDIR)/init.4th"
	@echo "   Man page: $(MANDIR)/starforth.1"
	@echo "   Docs: $(DOCDIR)/"

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
deb: build/starforth man/starforth.1
	@echo "📦 Building Debian package..."
	@if ! command -v dpkg-buildpackage >/dev/null 2>&1; then \
		echo "Error: dpkg-buildpackage not found. Install with: sudo apt-get install dpkg-dev"; \
		exit 1; \
	fi
	@dpkg-buildpackage -us -uc
	@echo "✅ Debian package built: ../starforth_*.deb"

# Build RPM package
rpm: build/starforth man/starforth.1
	@echo "📦 Building RPM package..."
	@if ! command -v rpmbuild >/dev/null 2>&1; then \
		echo "Error: rpmbuild not found. Install with: sudo dnf install rpm-build"; \
		exit 1; \
	fi
	@mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
	@tar czf ~/rpmbuild/SOURCES/starforth-$(VERSION).tar.gz \
		--transform 's,^,starforth-$(VERSION)/,' \
		--exclude=build --exclude=.git --exclude=debian \
		.
	@cp starforth.spec ~/rpmbuild/SPECS/
	@rpmbuild -ba ~/rpmbuild/SPECS/starforth.spec
	@echo "✅ RPM package built: ~/rpmbuild/RPMS/*/starforth-*.rpm"

# Build all packages
package: deb rpm info
	@echo "✅ All packages built successfully!"

# ==============================================================================
# CLEANUP
# ==============================================================================

clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rfv build/*
	@rm -f src/*.gcda src/word_source/*.gcda src/*.gcno src/word_source/*.gcno
	@echo "✓ Clean complete"

clean-obj:
	@rm -rf build/*.o build/**/*.o

# ==============================================================================
# HELP
# ==============================================================================

help:
	@echo "════════════════════════════════════════════════════════════"
	@echo "   ⚡ StarForth Build System - Fastest in the West! ⚡"
	@echo "════════════════════════════════════════════════════════════"
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
	@echo "  latex           - Convert AsciiDoc to LaTeX"
	@echo "  info            - Build GNU info documentation"
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