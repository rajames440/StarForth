STRICT_PTR ?= 1
CC = gcc
CFLAGS = -std=c99 -g -O0 -Wall -Werror -Iinclude -Isrc/word_source -Isrc/test_runner/include
SRC = $(wildcard src/*.c src/word_source/*.c src/test_runner/*.c src/test_runner/modules/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
TARGET = build/starforth

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

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