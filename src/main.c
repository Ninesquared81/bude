#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "interpreter.h"
#include "ir.h"


#define INPUT_BUFFER_SIZE 4 * 1024 * 1024

static void print_usage(FILE *file, const char *name) {
    fprintf(file, "%s <file>\n", name);
}

static void parse_args(int argc, char *argv[], char *inbuf) {
    const char *name = argv[0];
    if (argc == 1) {
        fprintf(stderr, "Error: missing positional argument.\nUsage: ");
        print_usage(stderr, name);
        exit(1);
    }
    if (argc > 2) {
        fprintf(stderr, "Warning: excess arguments discarded.\nUsage: ");
        print_usage(stderr, name);
    }
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("Could not open file");
        exit(1);
    }
    size_t length = fread(inbuf, sizeof *inbuf, INPUT_BUFFER_SIZE - 1, file);
    if (ferror(file)) {
        perror("Error reading file");
        exit(1);
    }
    if (!feof(file)) {
        perror("File too large");
        exit(1);
    }
    inbuf[length] = '\0';  // Set null byte.
    fclose(file);
}

int main(int argc, char *argv[]) {
    char *inbuf = calloc(INPUT_BUFFER_SIZE, sizeof *inbuf);
    if (inbuf == NULL) {
        fprintf(stderr, "calloc failed!\n");
        exit(1);
    }
    parse_args(argc, argv, inbuf);

    struct ir_block block;
    init_block(&block);
    compile(inbuf, &block);
    free(inbuf);

    interpret(&block);
    free_block(&block);

    return 0;
}
