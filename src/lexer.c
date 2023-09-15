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

static bool check(struct lexer *lexer, char c) {
    return *lexer->current == c;
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

static bool check_middle(struct lexer *lexer, int start, int length, const char *middle) {
    if (lexer->current - lexer->start < start + length) return false;
    return memcmp(&lexer->start[start], middle, length) == 0;
}

static enum token_type check_terminal(struct lexer *lexer, int length, enum token_type type) {
    return (lexer->current - lexer->start == length) ? type : TOKEN_SYMBOL;
}

static enum token_type symbol_type(struct lexer *lexer) {
    switch (lexer->start[0]) {
    case '+': return check_terminal(lexer, 1, TOKEN_PLUS);
    case '-': return check_terminal(lexer, 1, TOKEN_MINUS);
    case '*': return check_terminal(lexer, 1, TOKEN_STAR);
    case '/': return check_keyword(lexer, 1, 1, "%", TOKEN_SLASH_PERCENT);
    case 'd':
        switch (lexer->start[1]) {
        case 'e': return check_keyword(lexer, 2, 3, "ref", TOKEN_DEREF);
        case 'o': return check_terminal(lexer, 2, TOKEN_DO);
        case 'u': return check_keyword(lexer, 2, 2, "pe", TOKEN_DUPE);
        }
        break;
    case 'e':
        switch (lexer->start[1]) {
        case 'n': return check_keyword(lexer, 2, 1, "d", TOKEN_END);
        case 'l':
            if (lexer->current - lexer->start > 2) {
                switch (lexer->start[2]) {
                case 'i': return check_keyword(lexer, 3, 1, "f", TOKEN_ELIF);
                case 's': return check_keyword(lexer, 3, 1, "e", TOKEN_ELSE);
                }
            }
        }
        break;
    case 'i': return check_keyword(lexer, 1, 1, "f", TOKEN_IF);
    case 'n': return check_keyword(lexer, 1, 2, "ot", TOKEN_NOT);
    case 'p':
        switch (lexer->start[1]) {
        case 'r':
            if (check_middle(lexer, 2, 3, "int")) {
                switch (lexer->start[5]) {
                case '-': return check_keyword(lexer, 6, 4, "char", TOKEN_PRINT_CHAR);
                default: return check_terminal(lexer, 5, TOKEN_PRINT);
                }
            }
            break;
        case 'o': return check_keyword(lexer, 2, 1, "p", TOKEN_POP);
        }
        break;
    case 's': return check_keyword(lexer, 1, 3, "wap", TOKEN_SWAP);
    case 't': return check_keyword(lexer, 1, 3, "hen", TOKEN_THEN);
    case 'w': return check_keyword(lexer, 1, 4, "hile", TOKEN_WHILE);
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

static struct token string(struct lexer *lexer) {
    while (!check(lexer, '"')) {
        advance(lexer);
    }
    // Consume the closing '"'.
    advance(lexer);
    return make_token(lexer, TOKEN_STRING);
}

struct token next_token(struct lexer *lexer) {
    consume_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOT);
    
    char c = advance(lexer);

    if (isdigit(c) || (c == '-' && isdigit(peek(lexer)))) {
        return integer(lexer);
    }
    if (c == '"') {
        return string(lexer);
    }

    return symbol(lexer);
}
