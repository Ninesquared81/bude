#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "region.h"
#include "string_builder.h"
#include "symbol.h"
#include "type_punning.h"


struct compiler {
    struct lexer lexer;
    struct ir_block *block;
    struct token current_token;
    struct token previous_token;
    struct symbol_dictionary symbols;
    size_t for_loop_level;
};

static void init_compiler(struct compiler *compiler, const char *src, struct ir_block *block) {
    init_lexer(&compiler->lexer, src);
    compiler->block = block;
    compiler->current_token = next_token(&compiler->lexer);
    compiler->previous_token = (struct token){0};
    init_symbol_dictionary(&compiler->symbols);
    compiler->for_loop_level = 0;
}

static struct token advance(struct compiler *compiler) {
    compiler->previous_token = compiler->current_token;
    compiler->current_token = next_token(&compiler->lexer);
    return compiler->previous_token;
}

static bool check(struct compiler *compiler, enum token_type type) {
    return compiler->current_token.type == type;
}

static bool match(struct compiler *compiler, enum token_type type) {
    if (check(compiler, type)) {
        advance(compiler);
        return true;
    }
    return false;
}

static void expect_consume(struct compiler *compiler, enum token_type type, const char *message) {
    if (!match(compiler, type)) {
        fprintf(stderr, "Error: %s\n", message);
        exit(1);
    }
}

static void expect_keep(struct compiler *compiler, enum token_type type, const char *message) {
    if (!check(compiler, type)) {
        fprintf(stderr, "Error: %s\n", message);
        exit(1);
    }
}

static bool is_at_end(struct compiler *compiler) {
    return compiler->current_token.type == TOKEN_EOT;
}

static struct token peek(struct compiler *compiler) {
    return compiler->current_token;
}

static struct token peek_previous(struct compiler *compiler) {
    return compiler->previous_token;
}

static void compile_expr(struct compiler *compiler);
static void compile_symbol(struct compiler *compiler);

#define IN_RANGE(x, lower, upper) ((lower) <= (x) && (x) <= (upper))

enum integer_type {
    INT_WORD, INT_BYTE, INT_INT,
    INT_S8, INT_S16, INT_S32,
    INT_U8, INT_U16, INT_U32,
};

int parse_integer_prefix(struct token *token) {
    struct string_view *value = &token->value;
    if (value->start[0] != '0') return 10;

    if (value->length >= 3) {
        switch (value->start[1]) {
        case 'b': return 2;
        case 'x': return 16;
        }
    }

    return 10;
}

static enum integer_type parse_integer_suffix(struct token *token) {
    struct string_view *value = &token->value;
    if (value->length <= 1) return INT_INT;

    const char *end = &value->start[value->length];
    switch (end[-1]) {
    case 'W':
    case 'w':
        return INT_WORD;
    case 'T':
    case 't':
        return INT_BYTE;
    case '2':
        if (end[-2] == '3' && value->length > 3) {
            switch (end[-3]) {
            case 'S':
            case 's':
                return INT_S32;
            case 'U':
            case 'u':
                return INT_U32;
            }
        }
        break;
    case '6':
        if (end[-2] == '1' && value->length > 3) {
            switch (end[-3]) {
            case 'S':
            case 's':
                return INT_S16;
            case 'U':
            case 'u':
                return INT_U16;
            }
        }
        break;
    case '8':
        switch (end[-2]) {
        case 'S':
        case 's':
            return INT_S8;
        case 'U':
        case 'u':
            return INT_U8;
        }
        break;
    }
    return INT_INT;
}

static void compile_integer(struct compiler *compiler) {
    struct token token = peek_previous(compiler);
    int base = parse_integer_prefix(&token);
    enum integer_type type = parse_integer_suffix(&token);
    const char *start = (base == 10) ? token.value.start : token.value.start + 2;
    switch (type) {
    case INT_INT: {
        int64_t integer = strtoll(start, NULL, base);
        if (INT32_MIN <= integer && integer <= INT32_MAX) {
            write_immediate_sv(compiler->block, OP_PUSH8, integer);
        }
        else {
            int index = write_constant(compiler->block, s64_to_u64(integer));
            write_immediate_uv(compiler->block, OP_LOAD8, index);
        }
        break;
    }
    default:
        assert(0 && "Not implemented you silly goose.");
    }

}

static int start_jump(struct compiler *compiler, enum opcode jump_instruction) {
    assert(is_jump(jump_instruction));
    int jump_offset = compiler->block->count;
    write_immediate_s16(compiler->block, jump_instruction, 0);
    return jump_offset;
}

static void patch_jump(struct compiler *compiler, int instruction_offset, int jump) {
    if (jump < INT16_MIN || jump > INT16_MAX) {
        fprintf(stderr, "Jump too big.\n");
        exit(1);
    }
    overwrite_s16(compiler->block, instruction_offset + 1, jump);
}

static int compile_conditional(struct compiler *compiler) {
    compile_expr(compiler);  // Condition.
    expect_consume(compiler, TOKEN_THEN, "Expect `then` after condition in `if` block.");

    /*
     *   [if]
     *     condition -+
     *   [then]       | `then_jump`
     * +-- body  +----+
     * | [elif]  v
     * |   condition -+
     * | [then]       | `then_jump`
     * +-- body  +----+
     * | [elif]  v
     * |   condition -+
     * | [then]       | `then_jump`
     * +-- body       |
     * | [else]       |
     * |   body <-----+
     * | [end]
     * +-> ...
     * `else_jump`
     *
     * if and elif are basically the same
     *  - compile condition
     *  - write placeholder jump
     *  - compile then clause
     *  - if next token is `elif`, then compile that
     *  - if the next token is `else`, then compile the else clause
     *  - expect `end`
     *  - patch if-jump
     */

    int start = start_jump(compiler, OP_JUMP_NCOND);  // Offset of the jump instruction.

    compile_expr(compiler);  // Then body.

    // Note: these initial values assume no `else` or `elif` clauses.
    int end_addr = compiler->block->count;
    int else_start = end_addr;

    if (match(compiler, TOKEN_ELIF)) {
        start_jump(compiler, OP_JUMP);
        else_start = compiler->block->count;
        end_addr = compile_conditional(compiler);
    }
    else if (match(compiler, TOKEN_ELSE)) {
        start_jump(compiler, OP_JUMP);
        else_start = compiler->block->count;
        compile_expr(compiler);  // Else body.
        end_addr = compiler->block->count;
        expect_consume(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }
    else {
        expect_consume(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }

    int jump = else_start - start - 1;
    patch_jump(compiler, start, jump);
    add_jump(compiler->block, else_start);

    if (else_start != end_addr) {
        // We only emit a jump at the end of the `then` clause if we need to.
        int jump_addr = else_start - 3;
        int else_jump = end_addr - jump_addr - 1;
        patch_jump(compiler, jump_addr, else_jump);
        add_jump(compiler->block, end_addr);
    }
    
    return end_addr;
}

static void compile_for_loop(struct compiler *compiler) {
    enum opcode start_instruction = OP_FOR_DEC_START;
    enum opcode update_instruction = OP_FOR_DEC;
    if (match(compiler, TOKEN_SYMBOL)) {
        struct symbol symbol = {
            .name = peek_previous(compiler).value,
            .type = SYM_LOOP_VAR,
            .loop_var.level = compiler->for_loop_level + 1
        };
        if (match(compiler, TOKEN_FROM)) {
            insert_symbol(&compiler->symbols, &symbol);
        } else if (match(compiler, TOKEN_TO)) {
            start_instruction = OP_FOR_INC_START;
            update_instruction = OP_FOR_INC;
            insert_symbol(&compiler->symbols, &symbol);
        }
        else {
            // Symbol was part of count.
            compile_symbol(compiler);
        }
    }
    compile_expr(compiler);  // Count.
    expect_consume(compiler, TOKEN_DO, "Expect `do` after `for` start.");
    ++compiler->for_loop_level;
    if (compiler->for_loop_level > compiler->block->max_for_loop_level) {
        compiler->block->max_for_loop_level = compiler->for_loop_level;
    }
    
    int offset = start_jump(compiler, start_instruction);
    int body_start = compiler->block->count;
    compile_expr(compiler);  // Loop body.

    int loop_jump = body_start - compiler->block->count - 1;
    write_immediate_s16(compiler->block, update_instruction, loop_jump);
    add_jump(compiler->block, body_start);

    int skip_jump = compiler->block->count - offset - 1;
    patch_jump(compiler, offset, skip_jump);
    add_jump(compiler->block, compiler->block->count);

    --compiler->for_loop_level;
    expect_keep(compiler, TOKEN_END, "Expect `end` after `for` loop.");
}

static void compile_loop(struct compiler *compiler) {
    int condition_start = compiler->block->count;
    compile_expr(compiler);  // Condition.
    expect_consume(compiler, TOKEN_DO, "Expect `do` after `while` condition.");

    int body_start = compiler->block->count;
    write_immediate_s16(compiler->block, OP_JUMP_NCOND, 0);
    compile_expr(compiler);  // Loop body.

    /*   [while]
     * +-> condition
     * | [do]
     * |   OP_JUMP_NCOND -+
     * |   body           |
     * +-- OP_JUMP        |
     *   [end]            |
     *     ... <----------+
     */

    int loop_jump = condition_start - compiler->block->count - 1;  // Negative.
    write_immediate_s16(compiler->block, OP_JUMP, loop_jump);
    add_jump(compiler->block, condition_start);

    int exit_jump = compiler->block->count - body_start - 1;  // Positive.
    patch_jump(compiler, body_start, exit_jump);
    add_jump(compiler->block, compiler->block->count);

    expect_consume(compiler, TOKEN_END, "Expect `end` after `while` body.");
}

static int escape_character(char ch) {
    switch (ch) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '\\':
    case '"':
    case '\'':
        return ch;
    }

    // `\{ch}` is not an escape sequence.
    return -1;
}

static void compile_string(struct compiler *compiler) {
    struct token token = peek_previous(compiler);
    struct string_builder builder = {0};
    struct string_builder *current = &builder;
    const char *start = token.value.start + 1;
    struct region *temp_region = new_region(1024);
    start_view(current, start, temp_region);
    for (const char *c = start; *c != '"'; ++c) {
        if (*c == '\\') {
            if (c[1] == '\0') {
                fprintf(stderr, "Unexpected EOF\n.");
                exit(1);
            }
            int escaped = escape_character(c[1]);
            if (escaped == -1) {
                fprintf(stderr, "Invalid escape sequence '\\%c'.\n", c[1]);
                exit(1);
            }
            current = store_char(current, escaped, temp_region);
            ++c;  // Consume the escaped character.
        }
        else {
            if (!SB_IS_VIEW(current)) {
                current = start_view(current, c, temp_region);
            }
            ++current->view.length;
        }
    }
    uint32_t index = write_string(compiler->block, &builder);
    write_immediate_uv(compiler->block, OP_LOAD_STRING8, index);
    kill_region(temp_region);
}

static void compile_loop_var_symbol(struct compiler *compiler, struct symbol *symbol) {
    size_t level = symbol->loop_var.level;
    if (level > compiler->for_loop_level) {
        // TODO: protect against too-long symbol names.
        assert(symbol->name.length <= INT_MAX);
        fprintf(stderr, "Loop variable '%.*s' referenced outside defining loop.\n",
                (int)symbol->name.length, symbol->name.start);
        exit(1);
    }
    uint16_t offset = compiler->for_loop_level - level;  // Offset from top of aux.
    write_immediate_u16(compiler->block, OP_GET_LOOP_VAR, offset);
}

static void compile_symbol(struct compiler *compiler) {
    struct string_view symbol_text = peek_previous(compiler).value;
    struct symbol *symbol = lookup_symbol(&compiler->symbols, &symbol_text);
    if (symbol == NULL) {
        // TODO: protect against really long symbol names.
        assert(symbol_text.length <= INT_MAX);
        fprintf(stderr, "Unknown symbol '%.*s'.\n", (int)symbol_text.length, symbol_text.start);
        exit(1);
    }
    switch (symbol->type) {
    case SYM_LOOP_VAR:
        compile_loop_var_symbol(compiler, symbol);
        break;
    }
}

static bool compile_simple(struct compiler *compiler) {
#define BIN_OP(block, op)                          \
    do {                                           \
        write_simple(block, OP_NOP);               \
        write_simple(block, OP_NOP);               \
        write_simple(block, op);                   \
        write_simple(block, OP_NOP);               \
    } while (0);

    struct ir_block *block = compiler->block;
    switch (peek(compiler).type) {
    case TOKEN_AND:
        BIN_OP(block, OP_AND);
        break;
    case TOKEN_DEREF:
        write_simple(block, OP_DEREF);
        break;
    case TOKEN_DUPE:
        write_simple(block, OP_DUPE);
        break;
    case TOKEN_EXIT:
        write_simple(block, OP_EXIT);
        break;
    case TOKEN_MINUS:
        BIN_OP(block, OP_SUB);
        break;
    case TOKEN_NOT:
        write_simple(block, OP_NOT);
        break;
    case TOKEN_OR:
        BIN_OP(block, OP_OR);
        break;
    case TOKEN_PLUS:
        BIN_OP(block, OP_ADD);
        break;
    case TOKEN_POP:
        write_simple(block, OP_POP);
        break;
    case TOKEN_PRINT:
        write_simple(block, OP_NOP);  // Conversion.
        write_simple(block, OP_PRINT);
        break;
    case TOKEN_PRINT_CHAR:
        write_simple(block, OP_PRINT_CHAR);
        break;
    case TOKEN_SLASH_PERCENT:
        write_simple(block, OP_NOP);  // LHS conversion.
        write_simple(block, OP_NOP);  // RHS conversion.
        write_simple(block, OP_DIVMOD);
        write_simple(block, OP_NOP);  // Quotient conversion.
        write_simple(block, OP_NOP);  // Remainder conversion.
        break;
    case TOKEN_STAR:
        BIN_OP(block, OP_MULT);
        break;
    case TOKEN_SWAP:
        write_simple(block, OP_SWAP);
        break;
    default:
        /* All other tokens fall through. */
        return false;
    }
    advance(compiler);
    return true;
#undef BIN_OP
}

static void compile_expr(struct compiler *compiler) {
    while (!is_at_end(compiler)) {
        if (match(compiler, TOKEN_FOR)) {
            compile_for_loop(compiler);
        }
        else if (match(compiler, TOKEN_IF)) {
            compile_conditional(compiler);
        }
        else if (match(compiler, TOKEN_INT)) {
            compile_integer(compiler);
        }
        else if (match(compiler, TOKEN_STRING)) {
            compile_string(compiler);
        }
        else if (match(compiler, TOKEN_SYMBOL)) {
            compile_symbol(compiler);
        }
        else if (match(compiler, TOKEN_WHILE)) {
            compile_loop(compiler);
        }
        else {
            // Treat any other token as a simple token.
            // If it's not simple, stop compiling.
            if (!compile_simple(compiler)) return;
        }
    }
}

void compile(const char *src, struct ir_block *block) {
    struct compiler compiler;
    init_compiler(&compiler, src, block);
    compile_expr(&compiler);
    write_simple(compiler.block, OP_NOP);
}

