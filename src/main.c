#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "compiler.h"
#include "disassembler.h"
#include "function.h"
#include "generator.h"
#include "interpreter.h"
#include "ir.h"
#include "memory.h"
//#include "optimiser.h"
#include "reader.h"
#include "stack.h"
#include "type_checker.h"
#include "writer.h"


#define INPUT_BUFFER_SIZE 4 * 1024 * 1024

static const char *const version_number = "0.0.1";

struct cmdopts {
    // Options.
    bool dump_ir;
    bool optimise;
    bool interpret;
    bool generate_asm;
    bool generate_bytecode;
    bool from_bytecode;
    // Parameterised options.
    const char *output_filename;
    // Positional args.
    const char *filename;
    // Private fields.
    bool _had_i;
    bool _had_a;
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
            "  -b                generate bytecode only\n"
            "  -B                load bytecode from a BudeBWF file instead of "
                                       "a Bude source code file.\n"
            "  -d, --dump        dump the generated ir code and exit "
                                       "unless -i or -a are specified\n"
            "  -o <file>         write the output to the specified file\n"
            "  -h, -?, --help    display this help message and exit\n"
            "  -i, --interpret   interpret ir code (enabled by default)\n"
            "  -O, --optimise    optimise ir code\n"
            "  -v, --version     display the version number and exit\n"
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
    opts->generate_bytecode = false;
    opts->from_bytecode = false;
    opts->_had_i = false;
    opts->_had_a = false;
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

static void parse_short_opt(const char *name, const char *arg, struct cmdopts *opts) {
    for (const char *opt = &arg[1]; *opt != '\0'; ++opt) {
        switch (*opt) {
        case 'a':
            opts->generate_asm = true;
            opts->interpret = false;
            opts->generate_bytecode = false;
            opts->_had_a = true;
            break;
        case 'b':
            opts->generate_bytecode = true;
            opts->interpret = false;
            opts->generate_asm = false;
            break;
        case 'B':
            opts->from_bytecode = true;
            break;
        case 'd':
            opts->dump_ir = true;
            opts->interpret = opts->_had_i;
            opts->generate_asm = opts->_had_a;
            break;
        case 'h': case '?':
            print_help(stderr, name);
            exit(0);
        case 'i':
            opts->interpret = true;
            opts->_had_i = true;
            break;
        case 'O':
            opts->optimise = true;
            break;
        case 'v':
            print_version(stderr);
            exit(0);
        default:
            BAD_OPTION(name, arg);
            exit(1);
        }
    }
}

static void parse_args(int argc, char *argv[], struct cmdopts *opts) {
    init_cmdopts(opts);
    const char *name = argv[0];

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        switch (arg[0]) {
        case '-':
            // Options.
            switch (arg[1]) {
            case 'a':
            case 'b':
            case 'B':
            case 'd':
            case 'h': case '?':
            case 'i':
            case 'O':
            case 'v':
                parse_short_opt(name, arg, opts);
                break;
            case 'o': {
                const char *filename = NULL;
                if (arg[2] != '\0') {
                    // Argument pasted on the end.
                    filename = &arg[2];
                }
                else if (argc >= i + 1) {
                    filename = argv[++i];  // Consume the next argument as the filename.
                }
                else {
                    fprintf(stderr, "'%s' option missing required argument 'file'.\n", arg);
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
                    opts->interpret = opts->_had_i;
                    opts->generate_asm = opts->_had_a;
                }
                else if (strcmp(&arg[2], "help") == 0) {
                    print_help(stderr, name);
                    exit(0);
                }
                else if (strcmp(&arg[2], "interpret") == 0) {
                    opts->interpret = true;
                    opts->_had_i = true;
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
        fprintf(stderr, "Error: missing positional argument 'file'.\n");
        print_usage(stderr, name);
        exit(1);
    }
}

#undef BAD_OPTION


void load_source(const char *restrict filename, char *restrict inbuf) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open input file '%s': %s.\n", filename, strerror(errno));
        exit(1);
    }
    size_t length = fread(inbuf, sizeof *inbuf, INPUT_BUFFER_SIZE - 1, file);
    if (ferror(file)) {
        fprintf(stderr, "Error reading input file '%s': %s.\n", filename, strerror(errno));
        exit(1);
    }
    if (!feof(file)) {
        fprintf(stderr, "Input file '%s' too large.\n", filename);
        exit(1);
    }
    inbuf[length] = '\0';  // Set null byte.
    if (fclose(file) != 0) {
        fprintf(stderr, "Failed to close input file '%s': %s.\n", filename, strerror(errno));
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    struct cmdopts opts;
    parse_args(argc, argv, &opts);
    struct module module = {0};
    if (!opts.from_bytecode) {
        char *inbuf = calloc(INPUT_BUFFER_SIZE, sizeof *inbuf);
        CHECK_ALLOCATION(inbuf);
        load_source(opts.filename, inbuf);

        init_module(&module, opts.filename);

        compile(inbuf, &module);
        free(inbuf);
        if (opts.optimise) {
            // optimise(&module);
        }
        if (opts.dump_ir) {
            printf("=== Before type checking: ===\n");
            disassemble_tir(&module);
            printf("------------------------------------------------\n");
        }
        struct type_checker checker;
        init_type_checker(&checker, &module);
        if (type_check(&checker) == TYPE_CHECK_ERROR) {
            // Error message(s) already emitted.
            exit(1);
        }
    }
    else {
        module = read_bytecode(opts.filename);
    }
    if (opts.dump_ir) {
        printf("=== After type checking: ===\n");
        disassemble_wir(&module);
        if (opts.interpret) {
            printf("------------------------------------------------\n");
        }
    }
    if (opts.interpret) {
        struct interpreter interpreter;
        init_interpreter(&interpreter, &module);
        interpret(&interpreter);
        free_interpreter(&interpreter);
    }
    if (opts.generate_asm) {
        assert(!opts.generate_bytecode);
        struct asm_block *assembly = malloc(sizeof *assembly);
        CHECK_ALLOCATION(assembly);
        init_assembly(assembly);
        if (generate(&module, assembly) != GENERATE_OK) {
            fprintf(stderr, "Failed to write assembly code.\n");
            exit(1);
        }
        FILE *outfile = stdout;
        if (opts.output_filename != NULL) {
            outfile = fopen(opts.output_filename, "w");
            if (outfile == NULL) {
                fprintf(stderr, "Failed to open output file '%s': %s.\n",
                        opts.output_filename, strerror(errno));
                exit(1);
            }
        }
        fprintf(outfile, "%s", assembly->code);
        if (fclose(outfile) != 0) {
            fprintf(stderr, "Failed to close output file '%s': %s.\n",
                    opts.output_filename, strerror(errno));
            exit(1);
        }
        free(assembly);
    }
    if (opts.generate_bytecode) {
        assert(!opts.generate_asm);
        if (opts.output_filename != NULL) {
            FILE *outfile = fopen(opts.output_filename, "wb");
            if (outfile == NULL) {
                fprintf(stderr, "Failed to open output file '%s': '%s.\n",
                        opts.output_filename, strerror(errno));
                exit(1);
            }
            int error = write_bytecode(&module, outfile);
            fclose(outfile);
            if (error != 0) {
                fprintf(stderr, "Failed to write to file '%s': '%s'.\n",
                        opts.output_filename, strerror(error));
                exit(error);
            }
        } else {
            display_bytecode(&module, stdout);
        }
    }
    free_module(&module);
    return 0;
}
