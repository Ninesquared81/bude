#ifndef LEXER_H
#define LEXER_H

enum token_type {
    // Literals.
    TOKEN_INT,
    TOKEN_SYMBOL,

    // Keywords.
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_END,
    TOKEN_IF,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_POP,
    TOKEN_PRINT,
    TOKEN_SLASH_PERCENT,
    TOKEN_STAR,
    TOKEN_SWAP,
    TOKEN_THEN,

    // Special.
    TOKEN_EOT,
};

struct token {
    enum token_type type;
    const char *start;
    int length;
};

struct lexer {
    const char *start;
    const char *current;
};

void init_lexer(struct lexer *lexer, const char *src);
struct token next_token(struct lexer *lexer);

#endif
