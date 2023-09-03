#include <assert.h>
#include <stdlib.h>

#include "compiler.h"
#include "ir.h"
#include "lexer.h"


void compile(const char *restrict src, struct ir_block *block) {
    struct lexer lexer;
    init_lexer(&lexer, src);
    struct token token;
    while ((token = next_token(&lexer)).type != TOKEN_EOT) {
        switch (token.type) {
        case TOKEN_INT: {
            uint64_t integer = strtoull(token.start, NULL, 0);
            if (integer <= UINT8_MAX) {
                write_immediate(block, OP_PUSH, integer);
            }
            break;
        }
        case TOKEN_PLUS:
            write_simple(block, OP_ADD);
            break;
        case TOKEN_PRINT:
            write_simple(block, OP_PRINT);
            break;
        case TOKEN_SYMBOL:
            assert(0 && "Not implemented");
            break;
        default:
            assert(0 && "Unrecognized token");
            break;
        }
    }
}

