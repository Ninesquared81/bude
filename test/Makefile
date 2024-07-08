CC = gcc

CFLAGS = -Wall -g -D__USE_MINGW_STDIO=1

.PHONY: all

sources = $(wildcard *.c)
exes = $(patsubst %.c,%.exe,$(sources))
bude_objs = $(filter-out ../bin/main.o,$(wildcard ../bin/*.o))

all: $(exes)

%.exe : %.c $(bude_objs)
	$(CC) $(CFLAGS) -o $@ $(bude_objs) $<
