#ifndef LEXER_H
#define LEXER_H

enum token_type {
    TOKEN_INT,
    TOKEN_SYMBOL,
    TOKEN_PLUS,
    TOKEN_PRINT,

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
