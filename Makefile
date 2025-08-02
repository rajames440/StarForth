CC = gcc
CFLAGS = -std=c99 -Wall -Werror -Iinclude -Isrc/word_source
SRC = $(wildcard src/*.c src/word_source/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
TARGET = build/forthvm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/word_source/%.o: src/word_source/%.c | build/word_source
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build
	mkdir -p build/word_source

clean:
	rm -rf build/*

.PHONY: all clean build