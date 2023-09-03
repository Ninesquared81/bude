#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "lexer.h"

void init_lexer(struct lexer *lexer, const char *src) {
    lexer->start = src;
    lexer->current = src;
}

static char advance(struct lexer *lexer) {
    return *lexer->current++;
}

static char peek(struct lexer *lexer) {
    return *lexer->current;
}

static bool is_at_end(struct lexer *lexer) {
    return *lexer->current == '\0';
}

static void consume_whitespace(struct lexer *lexer) {
    while (!is_at_end(lexer) && isspace(peek(lexer))) {
        advance(lexer);
    }
}

static struct token make_token(struct lexer *lexer, enum token_type type) {
    return (struct token) {
        .type = type,
        .start = lexer->start,
        .length = lexer->current - lexer->start,
    };
}

static enum token_type check_keyword(struct lexer *lexer, int start, int length,
                                     const char *rest, enum token_type type) {
    if (lexer->current - lexer->start == start + length
        && memcmp(&lexer->start[start], rest, length) == 0) {
        return type;
    }
    return TOKEN_SYMBOL;
}

static enum token_type symbol_type(struct lexer *lexer) {
    switch (lexer->start[0]) {
    case '+': return check_keyword(lexer, 1, 0, "", TOKEN_PLUS);
    case 'p': return check_keyword(lexer, 1, 4, "rint", TOKEN_PRINT);
    }
    return TOKEN_SYMBOL;
}

static struct token symbol(struct lexer *lexer) {
    while (!is_at_end(lexer) && !isspace(peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, symbol_type(lexer));
}

static struct token integer(struct lexer *lexer) {
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
    if (!isspace(peek(lexer)) && !is_at_end(lexer)) {
        return symbol(lexer);
    }
    return make_token(lexer, TOKEN_INT);
}

struct token next_token(struct lexer *lexer) {
    consume_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOT);
    
    char c = advance(lexer);

    if (isdigit(c)) return integer(lexer);

    return symbol(lexer);
}
