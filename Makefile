STRICT_PTR ?= 1
CC = gcc

# Fast defaults; override CFLAGS on the command line if you need a debug build.
CFLAGS ?= -std=c99 -O2 -march=native -flto=auto -fuse-linker-plugin -DNDEBUG -Wall -Werror \
          -ffunction-sections -fdata-sections -fomit-frame-pointer \
          -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-strict-aliasing \
          -Iinclude -Isrc/word_source -Isrc/test_runner/include \
          -DSTRICT_PTR=$(STRICT_PTR)

LDFLAGS ?= -Wl,--gc-sections -s -flto=auto -fuse-linker-plugin

SRC = $(wildcard src/*.c src/word_source/*.c src/test_runner/*.c src/test_runner/modules/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
TARGET = build/starforth

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/word_source/%.o: src/word_source/%.c | build/word_source
	$(CC) $(CFLAGS) -c $< -o $@

build/test_runner/%.o: src/test_runner/%.c | build/test_runner
	$(CC) $(CFLAGS) -c $< -o $@

build/test_runner/modules/%.o: src/test_runner/modules/%.c | build/test_runner/modules
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build
	mkdir -p build/word_source
	mkdir -p build/test_runner
	mkdir -p build/test_runner/modules

clean:
	rm -rf build/*

.PHONY: all clean build
