#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "disassembler.h"
#include "interpreter.h"
#include "ir.h"


#define INPUT_BUFFER_SIZE 4 * 1024 * 1024

struct cmdopts {
    // Options.
    bool dump_ir;
    // Positional Args
    const char *filename;
};

static void print_usage(FILE *file, const char *name) {
    fprintf(file, "Usage: %s [options] <file>\n", name);
}

static void print_help(FILE *file, const char *name) {
    print_usage(file, name);
    fprintf(file,
            "Positional arguments:\n"
            "  file:           name of the source code file\n"
            "Options:\n"
            "  -d, --dump:     dump the generated ir code and exit\n"
            "  -h, -?, --help: display this help message and exit\n"
        );
}

static void init_cmdopts(struct cmdopts *opts) {
    opts->filename = NULL;
    opts->dump_ir = false;
}

static void parse_args(int argc, char *argv[], struct cmdopts *opts) {
    init_cmdopts(opts);
    const char *name = argv[0];
    if (argc == 1) {
        fprintf(stderr, "Error: missing positional argument.\n");
        print_usage(stderr, name);
        exit(1);
    }
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        switch (arg[0]) {
        case '-':
            switch (arg[1]) {
            case 'd':
                opts->dump_ir = true;
                break;
            case 'h': case '?':
                print_help(stderr, name);
                exit(0);
            case '-':
                // Long options.
                if (strcmp(&arg[2], "dump") == 0) {
                    opts->dump_ir = true;
                }
                if (strcmp(&arg[2], "help") == 0) {
                    print_help(stderr, name);
                    exit(0);
                }
                break;
            default:
                fprintf(stderr, "Unknown option '%s'.", arg);
                print_usage(stderr, name);
                exit(1);
            }
            break;
        default:
            if (opts->filename == NULL) {
                opts->filename = arg;
            }
            else {
                fprintf(stderr, "Warning: extraneous positional argument '%s' ignored.\n", arg);
                print_usage(stderr, name);
            }
            break;
        }
    }
}

void load_source(const char *restrict filename, char *restrict inbuf) {
    FILE *file = fopen(filename, "r");
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
    struct cmdopts opts;
    parse_args(argc, argv, &opts);
    load_source(opts.filename, inbuf);
    
    struct ir_block block;
    init_block(&block);
    compile(inbuf, &block);
    free(inbuf);
    if (!opts.dump_ir) {
        interpret(&block);
    }
    else {
        disassemble_block(&block);
    }
    free_block(&block);

    return 0;
}
