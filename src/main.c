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
    }
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("Could not open file");
        exit(1);
    }
    fread(inbuf, sizeof *inbuf, INPUT_BUFFER_SIZE, file);
    if (ferror(file)) {
        perror("Error reading file");
        exit(1);
    }
    if (!feof(file)) {
        perror("File too large");
        exit(1);
    }
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
    /*
    write_immediate(block, OP_PUSH, 34);
    write_immediate(block, OP_PUSH, 35);
    write_simple(block, OP_ADD);
    write_simple(block, OP_PRINT);
    */
    compile(inbuf, &block);
    free(inbuf);

    interpret(&block);
    free_block(&block);

    return 0;
}
