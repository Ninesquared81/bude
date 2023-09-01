CC = gcc

CFLAGS = -Wall -Wextra -Werror -pedantic -g -std=c23 -D__USE_MINGW_ANSI_STDIO=1

sources = $(wildcard src/*.c)
objects = $(patsubst src/%.c,bin/%.o,$(sources))

out = bin/main

.PHONY: all

all: $(out)

bin/%.o : src/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(objects): | bin

bin:
	mkdir bin

$(out): $(objects)
	$(CC) $(CFLAGS) -o $(out) $(objects)
