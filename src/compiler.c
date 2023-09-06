#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "ir.h"
#include "lexer.h"


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

static bool match(struct compiler *compiler, enum token_type type) {
    if (compiler->current_token.type == type) {
        advance(compiler);
        return true;
    }
    return false;
}

static void expect(struct compiler *compiler, enum token_type type, const char *message) {
    if (!match(compiler, type)) {
        fprintf(stderr, "Error: %s\n", message);
        exit(1);
    }
}

static struct token peek(struct compiler *compiler) {
    return compiler->current_token;
}

static void compile_expr(struct compiler *compiler);

static void patch_jump(struct compiler *compiler, int offset, int jump) {
    if (jump < INT16_MIN || jump > INT16_MAX) {
        fprintf(stderr, "Jump too big.\n");
        exit(1);
    }
    overwrite_s16(compiler->block, offset, jump);
}

static void compile_conditional(struct compiler *compiler) {
    advance(compiler);  // Consume the `if` token.
    compile_expr(compiler);  // Condition.
    expect(compiler, TOKEN_THEN, "Expect `then` after `if` condition.");

    int start = compiler->block->count;  // Offset of the jump instruction.
    write_immediate_s16(compiler->block, OP_JUMP_NCOND, 0);  // Jump if false (i.e. zero).
    compile_expr(compiler);  // Then body.

    int else_start = compiler->block->count;
    bool has_else = match(compiler, TOKEN_ELSE);
    if (has_else) {
        write_immediate_s16(compiler->block, OP_JUMP, 0);
    }

    int jump = compiler->block->count - start - 1;  // Point to instruction just before.
    patch_jump(compiler, start + 1, jump);

    if (has_else) {
        compile_expr(compiler);  // Else body.
        int else_jump = compiler->block->count - else_start - 1;
        patch_jump(compiler, else_start + 1, else_jump);
    }
}

static void compile_loop(struct compiler *compiler) {
    advance(compiler);  // Consume `while` token.
    int condition_start = compiler->block->count;
    compile_expr(compiler);  // Condition.
    expect(compiler, TOKEN_DO, "Expect `do` after `while` condition.");

    int body_start = compiler->block->count;
    write_immediate_s16(compiler->block, OP_JUMP_NCOND, 0);
    compile_expr(compiler);  // Loop body.

    /*   [if]
     * +-> condition
     * | [then]
     * |   OP_JUMP_NCOND -+
     * |   body           |
     * +-- OP_JUMP        |
     *   [end]            |
     *     ... <----------+
     */

    int loop_jump = condition_start - compiler->block->count - 1;  // Negative.
    write_immediate_s16(compiler->block, OP_JUMP, loop_jump);

    int exit_jump = compiler->block->count - body_start - 1;  // Positive.
    patch_jump(compiler, body_start + 1, exit_jump);
}

static void compile_expr(struct compiler *compiler) {
    struct ir_block *block = compiler->block;
    for (struct token token = peek(compiler); token.type != TOKEN_EOT; token = advance(compiler)) {
        switch (token.type) {
        case TOKEN_DUPE:
            write_simple(compiler->block, OP_DUPE);
            break;
        case TOKEN_IF:
            compile_conditional(compiler);
            break;
        case TOKEN_INT: {
            int64_t integer = strtoll(token.start, NULL, 0);
            if (INT8_MIN <= integer && integer <= INT8_MAX) {
                write_immediate_s8(block, OP_PUSH, integer);
            }
            break;
        }
        case TOKEN_MINUS:
            write_simple(block, OP_SUB);
            break;
        case TOKEN_NOT:
            write_simple(block, OP_NOT);
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
        case TOKEN_SLASH_PERCENT:
            write_simple(block, OP_DIVMOD);
            break;
        case TOKEN_STAR:
            write_simple(block, OP_MULT);
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
}

