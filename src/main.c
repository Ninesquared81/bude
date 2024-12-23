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
#include "lexer.h"
#include "memory.h"
//#include "optimiser.h"
#include "reader.h"
#include "stack.h"
#include "symbol.h"
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
    bool show_tokens;
    // Parameterised options.
    const char *output_filename;
    // Positional args.
    const char *filename;
    // Private fields.
    bool _had_i;
    bool _had_a;
    bool _should_exit;
    bool _should_help;
    bool _should_explain;
    int _exit_code;
    enum link_type _default_linking;
};

static void print_usage(FILE *file, const char *name) {
    fprintf(file, "Usage: %s [options] <file>\n", name);
}

static void print_summary(FILE *file) {
    fprintf(file, "bude -- the Bude language compiler\n\n");
}

static void print_description(FILE *file, const char *name) {
    print_summary(file);
    print_usage(file, name);
    fprintf(file,
            "Common arguments/options:\n"
            "  file         name of the source code file\n"
            "  -a           generate assembly code\n"
            "  -i           interpret ir code (enabled by default)\n"
            "  -o <file>    write the output to the specified file. This option can be omitted,\n"
            "               in which case, the filename is based on the input filename.\n"
            "  -h, --help   display help message and exit\n"
            "\n"
            "NOTE: Whenever a file is required for input/output, you can use `-` to specify stdin/stdout.\n"
            "\n"
            "Example invocations:\n"
            "\n"
            "  Compile `hello_world.bude` to assembly and save it in the default output file\n"
            "\n"
            "    bude hello_world.bude -a\n"
            "\n"
            "  Interpret `hello_world.bude`\n"
            "\n"
            "    bude hello_world.bude -i\n"
            "\n"
            "  Compile `hello_world.bude` to assembly and save it in another directoy\n"
            "\n"
            "    bude hello_world.bude -a -o output/hello_world.asm\n"
            "\n"
            "  Compile and assemble `hello_world.bude` (using FASM)\n"
            "\n"
            "    bude hello_word.bude -a\n"
            "    fasm hello_world.asm\n"
            "\n"
            "For more information on options, use `bude --help`.\n"
            "For more information on a specific command, use `bude [options] <file> --explain`.\n"
        );
}

enum filetype {FILE_FILE, FILE_STDSTREAM};

enum filetype get_filetype(const char *restrict filename) {
    return (filename != NULL && strcmp(filename, "-") != 0) ? FILE_FILE : FILE_STDSTREAM;
}

static enum filetype fixup_outfile(struct cmdopts *opts, struct module *module) {
    if (opts->output_filename != NULL) {
        return get_filetype(opts->output_filename);
    }
    assert(opts->filename != NULL);
    enum filetype filetype = get_filetype(opts->filename);
    if (filetype == FILE_STDSTREAM) {
        // If we read from stdin, write to stdout.
        opts->output_filename = "-";
        return FILE_STDSTREAM;
    }
    assert(filetype == FILE_FILE);  // Any other filetype not covered.
    size_t required_length = strlen(opts->filename);
    const char *ext = strrchr(opts->filename, '.');
    if (ext != NULL && strcmp(ext, ".bude") == 0) {
        required_length -= 5;  // Length of `.bude` extension.
    }
    size_t original_length = required_length;
    char *filename = NULL;
    if (opts->generate_asm) {
        required_length += 4;  // "`.asm` extension."
        filename = region_alloc(module->region, required_length + 1);
        memcpy(filename, opts->filename, original_length);
        char *new_ext = filename + original_length;
        strcpy(new_ext, ".asm");
    }
    else if (opts->generate_bytecode) {
        required_length += 5;  // "`.bbwf` extension."
        filename = region_alloc(module->region, required_length + 1);
        memcpy(filename, opts->filename, original_length);
        char *new_ext = filename + original_length;
        strcpy(new_ext, ".bbwf");
    }
    opts->output_filename = filename;
    return FILE_FILE;
}

static void print_output_file(FILE* file, struct cmdopts *opts, const char *restrict output_type,
                              struct module *module) {
    enum filetype filetype = fixup_outfile(opts, module);
    if (filetype == FILE_FILE) {
        fprintf(file, ", save the %s to %s,", output_type, opts->output_filename);
    }
    else {
        fprintf(file, ", print the %s to stdout,", output_type);
    }
}

static void print_help_prompt(FILE *file, struct cmdopts *opts) {
    fprintf(file, "For more information on options, ");
    if (!opts->_should_help) {
        fprintf(file, "use `bude --help`.\n");
    }
    else {
        fprintf(file,
                "see the help message above,\n"
                "which was specified by the `--help` option (aka `-h`, `-?`).\n"
            );
    }
}

static void print_explanation(FILE *file, struct cmdopts *opts, struct module *module) {
    if (opts->filename != NULL) {
        fprintf(file, "Explanation of command entered:\n\n");
    }
    else {
        fprintf(file, "Specify a file to compile.\n");
        return;
    }
    const char *input_filename = opts->filename;
    if (get_filetype(input_filename) == FILE_STDSTREAM) input_filename = "text from stdin";
    if (!opts->show_tokens) {
        fprintf(file, "  Compile %s to IR code", input_filename);
    }
    else {
        fprintf(file, "Lex %s, print the tokens to stdout, compile it to IR code", input_filename);
    }
    if (opts->dump_ir) {
        fprintf(file, ", print the IR code to stdout");
    }
    if (opts->generate_asm) {
        fprintf(file, ", assemble the IR code");
        print_output_file(file, opts, "assembly", module);
    }
    else if (opts->generate_bytecode) {
        print_output_file(file, opts, "IR code (in BudeBWF format)", module);
    }
    fprintf(file, " and %s.\n", (opts->interpret) ? "interpret it" : "exit");
    for (int i = 0; i < module->ext_libraries.count; ++i) {
        struct ext_library *library = &module->ext_libraries.items[i];
        const char *linking_adverb = (library->link_type == LINK_STATIC) ? "statically" : "dynamically";
        fprintf(file, "  Link %s with library %"PRI_SV".\n", linking_adverb, SV_FMT(library->filename));
    }
    fprintf(file, "\n");
    print_help_prompt(file, opts);
}

static void print_help(FILE *file, const char *name) {
    print_summary(file);
    print_usage(file, name);
    fprintf(file,
            "Positional arguments:\n"
            "  file              name of the source code file. Use `-` for stdin.\n"
            "Options:\n"
            "  -a                generate assembly code\n"
            "  -b                generate bytecode only\n"
            "  -B                load bytecode from a BudeBWF file instead of "
                                       "a Bude source code file.\n"
            "  -d, --dump        dump the generated ir code and exit "
                                       "unless -i or -a are specified\n"
            "  -o <file>         write the output to the specified file. This option can be omitted,\n"
            "                    in which case, the filename is based on the input filename. "
                                       "Use `-` for stdout.\n"
            "  -h, -?, --help    display this help message and exit\n"
            "  --explain         explain the meaning of the arguments parsed up until `--explain` is used\n"
            "  -i, --interpret   interpret ir code (enabled by default)\n"
            "  --lib[:st|:dy] <libname>=<path> link with a STatic or DYnamic library. "
                                       "If neither :st nor :dy\n"
            "                    are specified, the default linking strategy is used. "
                                       "This option can be used\n"
            "                    multiple times to link multiple libraries.\n"
            "  --lib-type:<st|dy> set the default library linking strategy to STatic or "
                                       "DYnamic.\n"
            "                    This option can be used multiple times and affects "
                                       "subsequent uses of --lib.\n"
            "  -O, --optimise    optimise ir code\n"
            "  -t                print the token stream and exit "
                                       "unless -i or -a are specified\n"
            "  -v, --version     display the version number and exit\n"
            "  --                treat all following arguments as positional\n"
        );
}

static void print_version(FILE *file) {
    fprintf(file, "Bude version %s\n", version_number);
}

static struct cmdopts new_cmdopts() {
    return (struct cmdopts) {
        .interpret = true,
        ._default_linking = LINK_DYNAMIC,
        // All other fields set to zero.
    };
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

#define BAD_OPTION(opts, arg) do {                              \
        if (!(opts)._should_exit || (opts)._exit_code != 0) {   \
            fprintf(stderr, "Unknown option '%s'.\n", arg);     \
        }                                                       \
    } while (0)

#define DEFER_EXIT(opts, exit_code) do {                \
        if (!(opts)._should_exit || exit_code == 0) {   \
            (opts)._exit_code = exit_code;              \
        }                                               \
        (opts)._should_exit = true;                     \
    } while (0)

static void parse_short_opt(const char *arg, struct cmdopts *opts) {
    for (const char *opt = &arg[1]; *opt != '\0'; ++opt) {
        switch (*opt) {
        case 'a':
            opts->generate_asm = true;
            opts->interpret = opts->_had_i;
            if (opts->generate_bytecode) {
                fprintf(stderr, "Warning: `-a` option takes precedence over previous usage of `-b`.\n");
            }
            opts->generate_bytecode = false;
            opts->_had_a = true;
            break;
        case 'b':
            opts->generate_bytecode = true;
            opts->interpret = opts->_had_i;
            if (opts->generate_asm) {
                fprintf(stderr, "Warning: `-b` option takes precedence over previous usage of `-a`.\n");
            }
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
            opts->_should_help = true;
            DEFER_EXIT(*opts, 0);
            return;
        case 'i':
            opts->interpret = true;
            opts->_had_i = true;
            break;
        case 'O':
            opts->optimise = true;
            break;
        case 't':
            opts->show_tokens = true;
            opts->interpret = opts->_had_i;
            opts->generate_asm = opts->_had_a;
            break;
        case 'v':
            print_version(stderr);
            DEFER_EXIT(*opts, 0);
            return;
        default:
            BAD_OPTION(*opts, arg);
            DEFER_EXIT(*opts, 1);
            return;
        }
    }
}

static enum link_type parse_link_type(const char *rest, const char *arg,
                                      struct cmdopts *opts) {
    if (strcmp(rest, "st") == 0) {
        return LINK_STATIC;
    }
    if (strcmp(rest, "dy") == 0) {
        return LINK_DYNAMIC;
    }
    BAD_OPTION(*opts, arg);
    DEFER_EXIT(*opts, 1);
    return opts->_default_linking;
}

static struct cmdopts parse_args(int argc, char *argv[], struct symbol_dictionary *symbols,
                                 struct module *module) {
    assert(argc >= 1);

    struct cmdopts opts = new_cmdopts();
    const char *name = argv[0];

    if (argc == 1) {
        // Print help message when no arguments passed.
        print_description(stderr, name);
        DEFER_EXIT(opts, 0);
    }

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        switch (arg[0]) {
        case '-':
            // Options.
            switch (arg[1]) {
            case '\0':
                // '-' refers to stdin/out.
                handle_positional_arg(name, &opts, arg);
                break;
            case 'a':
            case 'b':
            case 'B':
            case 'd':
            case 'h': case '?':
            case 'i':
            case 'O':
            case 't':
            case 'v':
                parse_short_opt(arg, &opts);
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
                    DEFER_EXIT(opts, 1);
                }
                opts.output_filename = filename;
                break;
            }
            case '-':
                if (arg[2] == '\0') {
                    // End of options.
                    while (++i < argc) {
                        arg = argv[i];
                        handle_positional_arg(name, &opts, arg);
                    }
                    // Note: goto needed due to switch statement.
                    goto check_filename;
                }
                // Long options.
                if (strcmp(&arg[2], "dump") == 0) {
                    opts.dump_ir = true;
                    opts.interpret = opts._had_i;
                    opts.generate_asm = opts._had_a;
                }
                else if (strcmp(&arg[2], "help") == 0) {
                    opts._should_help = true;
                    DEFER_EXIT(opts, 0);
                }
                else if (strcmp(&arg[2], "explain") == 0) {
                    opts._should_explain = true;
                    DEFER_EXIT(opts, 0);
                }
                else if (strcmp(&arg[2], "interpret") == 0) {
                    opts.interpret = true;
                    opts._had_i = true;
                }
                else if (strcmp(&arg[2], "lib-type:") == 0) {
                    // NOTE: this must come BEFORE the check for `--lib`.
                    const char *rest = &arg[2 + sizeof "lib-type:" - 1];
                    opts._default_linking = parse_link_type(rest, arg, &opts);
                }
                else if (strncmp(&arg[2], "lib", 3) == 0) {
                    // NOTE: this must come AFTER the check for `--lib-type`.
                    const char *rest = &arg[2 + 3];
                    enum link_type linking = opts._default_linking;
                    if (*rest == ':') {
                        ++rest;
                        linking = parse_link_type(rest, arg, &opts);
                    }
                    else if (*rest != '\0') {
                        BAD_OPTION(opts, arg);
                        DEFER_EXIT(opts, 1);
                    }
                    arg = argv[++i];
                    int sep = 0;
                    while (arg[sep] != '=') {
                        if (arg[sep] == '\0') {
                            BAD_OPTION(opts, arg);
                            DEFER_EXIT(opts, 1);
                        }
                        ++sep;
                    }
                    struct string_view libname = {.start = arg, .length = sep};
                    const char *path = &arg[sep + 1];
                    struct ext_library library = {
                        .filename = {.start = path, .length = strlen(path)},
                        .link_type = linking,
                    };
                    int index = add_ext_library(&module->ext_libraries, library);
                    insert_symbol(symbols, &(struct symbol) {
                            .name = libname,
                            .type = SYM_EXT_LIBRARY,
                            .ext_library.index = index,
                    });
                }
                else if (strcmp(&arg[2], "optimise") == 0) {
                    opts.optimise = true;
                }
                else if (strcmp(&arg[2], "version") == 0) {
                    print_version(stderr);
                    DEFER_EXIT(opts, 0);
                }
                else {
                    BAD_OPTION(opts, arg);
                    DEFER_EXIT(opts, 1);
                }
                break;
            default:
                BAD_OPTION(opts, arg);
                DEFER_EXIT(opts, 1);
            }
            break;
        default:
            handle_positional_arg(name, &opts, arg);
            break;
        }
    }

check_filename:
    if (opts.filename == NULL && !opts._should_exit) {
        fprintf(stderr, "Error: missing positional argument 'file'.\n");
        print_usage(stderr, name);
        DEFER_EXIT(opts, 1);
    }
    if (!opts._should_exit) fixup_outfile(&opts, module);
    return opts;
}

#undef BAD_OPTION
#undef DEFER_EXIT


void load_source(const char *restrict filename, char *restrict inbuf) {
    enum filetype filetype = get_filetype(filename);
    FILE *file = (filetype == FILE_FILE) ? fopen(filename, "r") : stdin;
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
    if (filetype == FILE_FILE && fclose(file) != 0) {
        fprintf(stderr, "Failed to close input file '%s': %s.\n", filename, strerror(errno));
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    struct symbol_dictionary symbols;
    struct module module = {0};
    init_symbol_dictionary(&symbols);
    init_module(&module, NULL);
    struct cmdopts opts = parse_args(argc, argv, &symbols, &module);
    if (opts._should_help) {
        print_help(stderr, argv[0]);
    }
    if (opts._should_explain) {
        print_explanation(stderr, &opts, &module);
    }
    if (opts._should_exit) {
        if (opts._exit_code != 0) {
            print_usage(stderr, argv[0]);
        }
        exit(opts._exit_code);
    }
    if (!opts.from_bytecode) {
        char *inbuf = calloc(INPUT_BUFFER_SIZE, sizeof *inbuf);
        CHECK_ALLOCATION(inbuf);
        load_source(opts.filename, inbuf);

        module.filename = opts.filename;
        if (opts.show_tokens) {
            struct lexer lexer = {0};
            init_lexer(&lexer, inbuf, NULL, module.filename);
            struct token token = {0};
            while ((token = next_token(&lexer)).type != TOKEN_EOT) {
                print_token(token);
            }
        }
        compile(inbuf, &module, &symbols);
        free(inbuf);
        inbuf = NULL;
        free_symbol_dictionary(&symbols);
        symbols = (struct symbol_dictionary){0};
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
        free_symbol_dictionary(&symbols);
        free_module(&module);
        symbols = (struct symbol_dictionary){0};
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
        enum filetype filetype = get_filetype(opts.output_filename);
        FILE *outfile = (filetype == FILE_FILE) ? fopen(opts.output_filename, "w") : stdout;
        if (outfile == NULL) {
            fprintf(stderr, "Failed to open output file '%s': %s.\n",
                    opts.output_filename, strerror(errno));
            exit(1);
        }
        fprintf(outfile, "%s", assembly->code);
        if (filetype == FILE_FILE && fclose(outfile) != 0) {
            fprintf(stderr, "Failed to close output file '%s': %s.\n",
                    opts.output_filename, strerror(errno));
            exit(1);
        }
        free(assembly);
    }
    if (opts.generate_bytecode) {
        assert(!opts.generate_asm);
        enum filetype filetype = get_filetype(opts.output_filename);
        if (filetype == FILE_FILE) {
            assert(opts.output_filename != NULL);
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
