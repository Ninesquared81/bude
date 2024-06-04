CC = gcc

CFLAGS = -Wall -Wextra -Werror -pedantic -g -std=c2x -D__USE_MINGW_ANSI_STDIO=1
DEPDIR = .deps
BINDIR = bin
SRCDIR = src
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) -c

sources = $(wildcard $(SRCDIR)/*.c)
objects = $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%.o,$(sources))
deps = $(patsubst bin/%.o,$(DEPDIR)/%.d,$(objects))

out = bin/bude

.PHONY: all

all: $(out)

bin/%.o : src/%.c $(DEPDIR)/%.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

-include $(deps)

$(objects): | bin

bin:
	mkdir -p bin

$(DEPDIR): ; @mkdir -p $@

$(out): $(objects)
	$(CC) $(CFLAGS) -o $(out) $(objects)
$(deps):
