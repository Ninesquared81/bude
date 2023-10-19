#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "compiler.h"
#include "disassembler.h"
#include "generator.h"
#include "interpreter.h"
#include "ir.h"
#include "optimiser.h"
#include "stack.h"


#define INPUT_BUFFER_SIZE 4 * 1024 * 1024

static const char *const version_number = "0.0.1";

struct cmdopts {
    // Options.
    bool dump_ir;
    bool optimise;
    bool interpret;
    bool generate_asm;
    // Parameterised options.
    const char *output_filename;
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
            "  file              name of the source code file\n"
            "Options:\n"
            "  -a                generate assembly code\n"
            "  -d, --dump        dump the generated ir code and exit, unless -i or -a are specified\n"
            "  -f <filename>     write the output to the specified file\n"
            "  -h, -?, --help    display this help message and exit\n"
            "  -i, --interpret   interpret ir code (enabled by default)\n"
            "  -o, --optimise    optimise ir code\n"
            "  --version         display the version number and exit\n"
            "  --                treat all following arguments as positional\n"
        );
}

static void print_version(FILE *file) {
    fprintf(file, "Bude version %s\n", version_number);
}

static void init_cmdopts(struct cmdopts *opts) {
    opts->filename = NULL;
    opts->output_filename = NULL;
    opts->dump_ir = false;
    opts->optimise = false;
    opts->interpret = true;
    opts->generate_asm = false;
}

static void handle_positional_arg(const char *restrict name, struct cmdopts *opts,
                                  const char *restrict arg) {
    if (opts->filename == NULL) {
        opts->filename = arg;
    }
    else {
        fprintf(stderr, "Warning: extraneous positional argument '%s' ignored.\n", arg);
        print_usage(stderr, name);
    }
}

#define BAD_OPTION(name, arg) do {                      \
        fprintf(stderr, "Unknown option '%s'.\n", arg); \
        print_usage(stderr, name);                      \
    } while (0)

static void parse_short_opt(const char *name, const char *arg,
                            struct cmdopts *opts, bool *had_i, bool *had_a) {
    for (const char *opt = &arg[1]; *opt != '\0'; ++opt) {
        switch (*opt) {
        case 'a':
            opts->generate_asm = true;
            opts->interpret = false;
            *had_a = true;
            break;
        case 'd':
            opts->dump_ir = true;
            opts->interpret = *had_i;
            opts->generate_asm = *had_a;
            break;
        case 'h': case '?':
            print_help(stderr, name);
            exit(0);
        case 'i':
            opts->interpret = true;
            *had_i = true;
            break;
        case 'o':
            opts->optimise = true;
            break;
        default:
            BAD_OPTION(name, arg);
            exit(1);
        }
    }
}

static void parse_args(int argc, char *argv[], struct cmdopts *opts) {
    init_cmdopts(opts);
    const char *name = argv[0];

    bool had_i = false;
    bool had_a = false;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        switch (arg[0]) {
        case '-':
            // Options.
            switch (arg[1]) {
            case 'a':
            case 'd':
            case 'h': case '?':
            case 'i':
            case 'o':
                parse_short_opt(name, arg, opts, &had_i, &had_a);
                break;
            case 'f': {
                const char *filename = NULL;
                if (arg[2] != '\0') {
                    // Argument pasted on the end.
                    filename = &arg[2];
                }
                else if (argc >= i + 1) {
                    filename = argv[++i];  // Consume the next argument as the filename.
                }
                else {
                    fprintf(stderr, "'%s' option missing required argument 'filename'.\n", arg);
                    exit(1);
                }
                opts->output_filename = filename;
                break;
            }
            case '-':
                if (arg[2] == '\0') {
                    // End of options.
                    while (++i < argc) {
                        arg = argv[i];
                        handle_positional_arg(name, opts, arg);
                    }
                    // Note: goto needed due to switch statement.
                    goto check_filename;
                }
                // Long options.
                if (strcmp(&arg[2], "dump") == 0) {
                    opts->dump_ir = true;
                    opts->interpret = had_i;
                    opts->generate_asm = had_a;
                }
                else if (strcmp(&arg[2], "help") == 0) {
                    print_help(stderr, name);
                    exit(0);
                }
                else if (strcmp(&arg[2], "interpret") == 0) {
                    opts->interpret = true;
                    had_i = true;
                }
                else if (strcmp(&arg[2], "optimise") == 0) {
                    opts->optimise = true;
                }
                else if (strcmp(&arg[2], "version") == 0) {
                    print_version(stderr);
                    exit(0);
                }
                else {
                    BAD_OPTION(name, arg);
                    exit(1);
                }
                break;
            default:
                BAD_OPTION(name, arg);
                exit(1);
            }
            break;
        default:
            handle_positional_arg(name, opts, arg);
            break;
        }
    }

check_filename:
    if (opts->filename == NULL) {
        fprintf(stderr, "Error: missing positional argument.\n");
        print_usage(stderr, name);
        exit(1);
    }
}

#undef BAD_OPTION


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
    if (opts.optimise) {
        optimise(&block);
    }
    if (opts.dump_ir) {
        disassemble_block(&block);
        if (opts.interpret) {
            printf("------------------------------------------------\n");
        }
    }
    if (opts.interpret) {
        struct stack *stack = malloc(sizeof *stack);
        interpret(&block, stack);
        free(stack);
    }
    if (opts.generate_asm) {
        FILE *outfile = stdout;
        if (opts.output_filename != NULL) {
            outfile = fopen(opts.output_filename, "w");
            if (outfile == NULL) {
                perror("Failed to open output file");
                exit(1);
            }
        }
        struct asm_block *assembly = malloc(sizeof *assembly);
        init_assembly(assembly);
        if (generate(&block, assembly) != GENERATE_OK) {
            exit(1);
        }
        fprintf(outfile, "%s", assembly->code);
        free(assembly);
    }
    free_block(&block);

    return 0;
}
