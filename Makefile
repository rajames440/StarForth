STRICT_PTR ?= 1
CC = gcc

# Base compiler flags
BASE_CFLAGS = -std=c99 -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include -DSTRICT_PTR=$(STRICT_PTR)

# Fast defaults; override CFLAGS on the command line if you need a debug build.
CFLAGS ?= $(BASE_CFLAGS) -O2 -march=native -flto=auto -fuse-linker-plugin -DNDEBUG \
          -ffunction-sections -fdata-sections -fomit-frame-pointer \
          -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-strict-aliasing

LDFLAGS ?= -Wl,--gc-sections -s -flto=auto -fuse-linker-plugin

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
TARGET = build/starforth

# Default target
all: $(TARGET)

# Platform-specific targets
minimal: 
	$(MAKE) MINIMAL=1

l4re:
	$(MAKE) MINIMAL=1 CC=l4-gcc CFLAGS="$(BASE_CFLAGS) -DL4RE_TARGET=1"

profile:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -DPROFILE_ENABLED=1 -g -O1"

performance:
	$(MAKE) CFLAGS="$(BASE_CFLAGS) -O3 -DSTARFORTH_PERFORMANCE -DNDEBUG -march=native -flto -funroll-loops -finline-functions -fomit-frame-pointer" LDFLAGS="-flto -s"

# Main executable
$(TARGET): $(OBJ) | build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Build rules
build/%.o: src/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/platform/%.o: src/platform/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/word_source/%.o: src/word_source/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/test_runner/%.o: src/test_runner/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/test_runner/modules/%.o: src/test_runner/modules/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directories
build:
	@mkdir -p build
	@mkdir -p build/platform
	@mkdir -p build/word_source
	@mkdir -p build/test_runner
	@mkdir -p build/test_runner/modules

# Clean
clean:
	rm -rf build/*

# Help
help:
	@echo "StarForth Build System"
	@echo "======================"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build the main executable (default)"
	@echo "  minimal     - Build for minimal/embedded environments"
	@echo "  l4re        - Build for L4Re microkernel"
	@echo "  profile     - Build with profiling support enabled"
	@echo "  performance - Build with maximum performance optimizations"
	@echo "  clean       - Remove build artifacts"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  CC        - Compiler (default: gcc)"
	@echo "  CFLAGS    - Compiler flags"
	@echo "  LDFLAGS   - Linker flags"
	@echo "  MINIMAL   - Set to 1 for minimal builds"
	@echo ""
	@echo "Examples:"
	@echo "  make clean && make                    # Standard build"
	@echo "  make clean && make minimal            # Minimal build (no glibc)"
	@echo "  make clean && make profile            # With profiling"
	@echo "  make CFLAGS='-g -O0' clean all       # Debug build"

.PHONY: all minimal l4re profile performance clean help
