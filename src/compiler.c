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
#include "type_punning.h"


struct compiler {
    struct lexer lexer;
    struct ir_block *block;
    struct token current_token;
    struct token previous_token;
};

static void init_compiler(struct compiler *compiler, const char *src, struct ir_block *block) {
    init_lexer(&compiler->lexer, src);
    compiler->block = block;
    compiler->current_token = next_token(&compiler->lexer);
}

static struct token advance(struct compiler *compiler) {
    compiler->previous_token = compiler->current_token;
    compiler->current_token = next_token(&compiler->lexer);
    return compiler->current_token;
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

static struct token peek(struct compiler *compiler) {
    return compiler->current_token;
}

static void compile_expr(struct compiler *compiler);

#define IN_RANGE(x, lower, upper) ((lower) <= (x) && (x) <= (upper))

static void compile_integer(struct compiler *compiler) {
    int64_t integer = strtoll(peek(compiler).start, NULL, 0);
    if (INT32_MIN <= integer && integer <= INT32_MAX) {
        write_immediate_sv(compiler->block, OP_PUSH8, integer);
    }
    else {
        int index = write_constant(compiler->block, s64_to_u64(integer));
        write_immediate_uv(compiler->block, OP_LOAD8, index);
    }

}

static int start_jump(struct compiler *compiler, enum opcode jump_instruction) {
    assert(IS_JUMP(jump_instruction));
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
    advance(compiler);  // Consume the `if`/`elif` token.
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

    if (check(compiler, TOKEN_ELIF)) {
        start_jump(compiler, OP_JUMP);
        else_start = compiler->block->count;
        end_addr = compile_conditional(compiler);
    }
    else if (match(compiler, TOKEN_ELSE)) {
        start_jump(compiler, OP_JUMP);
        else_start = compiler->block->count;
        compile_expr(compiler);  // Else body.
        end_addr = compiler->block->count;
    }
    else {
        expect_keep(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }

    int jump = else_start - start - 1;
    patch_jump(compiler, start, jump);
    write_jump(compiler->block, else_start);

    if (else_start != end_addr) {
        // We only emit a jump at the end of the `then` clause if we need to.
        int jump_addr = else_start - 3;
        int else_jump = end_addr - jump_addr - 1;
        patch_jump(compiler, jump_addr, else_jump);
        write_jump(compiler->block, end_addr);
    }
    
    return end_addr;
}

static void compile_loop(struct compiler *compiler) {
    advance(compiler);  // Consume `while` token.
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
    write_jump(compiler->block, condition_start);

    int exit_jump = compiler->block->count - body_start - 1;  // Positive.
    patch_jump(compiler, body_start, exit_jump);
    write_jump(compiler->block, compiler->block->count);

    expect_keep(compiler, TOKEN_END, "Expect `end` after `while` body.");
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
    struct token token = peek(compiler);
    struct string_builder builder = {0};
    struct string_builder *current = &builder;
    const char *start = token.start + 1;
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
    int length = sb_length(&builder);
    char *string = region_alloc(compiler->block->static_memory, length + 1);
    build_string(&builder, string);
    int index = write_constant(compiler->block, (uintptr_t)string);
    write_immediate_uv(compiler->block, OP_LOAD8, index);
    write_immediate_sv(compiler->block, OP_PUSH8, length);
    kill_region(temp_region);
}

static void compile_expr(struct compiler *compiler) {
    struct ir_block *block = compiler->block;
    for (struct token token = peek(compiler);
         token.type != TOKEN_EOT;
         token = advance(compiler)) {
        switch (token.type) {
        case TOKEN_AND:
            write_simple(compiler->block, OP_AND);
            break;
        case TOKEN_DEREF:
            write_simple(compiler->block, OP_DEREF);
            break;
        case TOKEN_DUPE:
            write_simple(compiler->block, OP_DUPE);
            break;
        case TOKEN_EXIT:
            write_simple(compiler->block, OP_EXIT);
            break;
        case TOKEN_IF:
            compile_conditional(compiler);
            break;
        case TOKEN_INT:
            compile_integer(compiler);
            break;
        case TOKEN_MINUS:
            write_simple(block, OP_SUB);
            break;
        case TOKEN_NOT:
            write_simple(block, OP_NOT);
            break;
        case TOKEN_OR:
            write_simple(block, OP_OR);
            break;
        case TOKEN_PLUS:
            write_simple(block, OP_ADD);
            break;
        case TOKEN_POP:
            write_simple(block, OP_POP);
            break;
        case TOKEN_PRINT:
            write_simple(block, OP_PRINT);
            break;
        case TOKEN_PRINT_CHAR:
            write_simple(block, OP_PRINT_CHAR);
            break;
        case TOKEN_SLASH_PERCENT:
            write_simple(block, OP_DIVMOD);
            break;
        case TOKEN_STAR:
            write_simple(block, OP_MULT);
            break;
        case TOKEN_STRING:
            compile_string(compiler);
            break;
        case TOKEN_SWAP:
            write_simple(block, OP_SWAP);
            break;
        case TOKEN_SYMBOL:
            assert(0 && "Not implemented");
            break;
        case TOKEN_WHILE:
            compile_loop(compiler);
            break;
        default:
            /* All other tokens fall through. */
            return;
        }
    }
}

void compile(const char *src, struct ir_block *block) {
    struct compiler compiler;
    init_compiler(&compiler, src, block);
    compile_expr(&compiler);
    write_simple(compiler.block, OP_NOP);
}

