CC=gcc
CFLAGS=-std=c90 -Wall -Werror -Iinclude
SRC=$(wildcard src/*.c)
OBJ=$(SRC:src/%.c=build/%.o)
TARGET=build/forthvm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build/*

.PHONY: all clean build
