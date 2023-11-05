#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

#include "location.h"
#include "string_view.h"

enum token_type {
    // Literals.
    TOKEN_INT,
    TOKEN_CHAR,
    TOKEN_STRING,
    TOKEN_SYMBOL,

    // Keywords.
    TOKEN_AND,
    TOKEN_DEREF,
    TOKEN_DO,
    TOKEN_DUPE,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_END,
    TOKEN_EXIT,
    TOKEN_FOR,
    TOKEN_FROM,
    TOKEN_IF,
    TOKEN_MINUS,
    TOKEN_NOT,
    TOKEN_PLUS,
    TOKEN_POP,
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_PRINT_CHAR,
    TOKEN_SLASH_PERCENT,
    TOKEN_STAR,
    TOKEN_SWAP,
    TOKEN_THEN,
    TOKEN_TO,
    TOKEN_WHILE,

    // Special.
    TOKEN_EOT,
};

struct token {
    enum token_type type;
    struct string_view value;
    struct location location;
};

struct lexer {
    const char *start;
    const char *current;
    struct location position;
    struct location start_position;
    const char *filename;
};

void init_lexer(struct lexer *lexer, const char *src, const char *filename);
struct token next_token(struct lexer *lexer);

#endif
