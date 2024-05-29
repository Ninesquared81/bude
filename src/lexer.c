#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

void init_lexer(struct lexer *lexer, const char *src, const char *filename) {
    lexer->start = src;
    lexer->current = src;
    struct location init_location = {1, 1};
    lexer->position = init_location;
    lexer->start_position = init_location;
    lexer->filename = filename;
}

static void lex_error(struct lexer *lexer, const char *restrict message, ...) {
    report_location(lexer->filename, &lexer->start_position);
    fprintf(stderr, "Syntax Error: ");
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static char advance(struct lexer *lexer) {
    struct location *pos = &lexer->position;
    if (*lexer->current == '\n') {
        ++pos->line;
        pos->column = 1;
    }
    else {
        ++pos->column;
    }
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

static bool match(struct lexer *lexer, char c) {
    if (is_at_end(lexer) || !check(lexer, c)) return false;
    advance(lexer);
    return true;
}

static void consume_comment(struct lexer *lexer) {
    while (!is_at_end(lexer) && advance(lexer) != '\n') {
        /* Do nothing. */
    }
}

static void consume_whitespace(struct lexer *lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (isspace(c)) {
            advance(lexer);
        }
        else if (c == '#') {
            consume_comment(lexer);
        }
        else {
            return;  // End of whitespace.
        }
    }
}

static struct token make_token(struct lexer *lexer, enum token_type type) {
    return (struct token) {
        .type = type,
        .value = {
            .start = lexer->start,
            .length = lexer->current - lexer->start
        },
        .location = lexer->start_position,
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
    case '-':
        switch (lexer->start[1]) {
        case '>': return check_terminal(lexer, 2, TOKEN_RIGHT_ARROW);
        default: return check_terminal(lexer, 1, TOKEN_MINUS);
        }
        break;
    case '*': return check_terminal(lexer, 1, TOKEN_STAR);
    case '/':
        switch (lexer->start[1]) {
        case '=': return check_terminal(lexer, 2, TOKEN_SLASH_EQUALS);
        default: return check_terminal(lexer, 1, TOKEN_SLASH);
        }
        break;
    case '%': return check_terminal(lexer, 1, TOKEN_PERCENT);
    case '<':
        switch (lexer->start[1]) {
        case '-': return check_terminal(lexer, 2, TOKEN_LEFT_ARROW);
        case '=': return check_terminal(lexer, 2, TOKEN_LESS_EQUALS);
        default: return check_terminal(lexer, 1, TOKEN_LESS_THAN);
        }
        break;
    case '=': return check_terminal(lexer, 1, TOKEN_EQUALS);
    case '>': switch (lexer->start[1]) {
        case '=': return check_terminal(lexer, 2, TOKEN_GREATER_EQUALS);
        default: return check_terminal(lexer, 1, TOKEN_GREATER_THAN);
        }
        break;
    case 'a':
        switch (lexer->start[1]) {
        case 'n': return check_keyword(lexer, 2, 1, "d", TOKEN_AND);
        case 's': return check_terminal(lexer, 2, TOKEN_AS);
        }
        break;
    case 'b': return check_keyword(lexer, 1, 3, "yte", TOKEN_BYTE);
    case 'c':
        switch (lexer->start[1]) {
        case 'h':
            if (check_middle(lexer, 2, 2, "ar")) {
                switch (lexer->start[4]) {
                case '3': return check_keyword(lexer, 5, 1, "2", TOKEN_CHAR32);
                default: return check_terminal(lexer, 4, TOKEN_CHAR);
                }
            }
            break;
        case 'o': return check_keyword(lexer, 2, 2, "mp", TOKEN_COMP);
        }
        break;
    case 'd':
        switch (lexer->start[1]) {
        case 'e':
            switch (lexer->start[2]) {
            case 'c': return check_keyword(lexer, 3, 3, "omp", TOKEN_DECOMP);
            case 'f': return check_terminal(lexer, 3, TOKEN_DEF);
            case 'r': return check_keyword(lexer, 3, 2, "ef", TOKEN_DEREF);
            }
            break;
        case 'i': return check_keyword(lexer, 2, 4, "vmod", TOKEN_DIVMOD);
        case 'o': return check_terminal(lexer, 2, TOKEN_DO);
        case 'u': return check_keyword(lexer, 2, 2, "pe", TOKEN_DUPE);
        }
        break;
    case 'e':
        switch (lexer->start[1]) {
        case 'd': return check_keyword(lexer, 2, 5, "ivmod", TOKEN_EDIVMOD);
        case 'l':
            if (lexer->current - lexer->start > 2) {
                switch (lexer->start[2]) {
                case 'i': return check_keyword(lexer, 3, 1, "f", TOKEN_ELIF);
                case 's': return check_keyword(lexer, 3, 1, "e", TOKEN_ELSE);
                }
            }
            break;
        case 'n': return check_keyword(lexer, 2, 1, "d", TOKEN_END);
        case 'x': return check_keyword(lexer, 2, 2, "it", TOKEN_EXIT);
        }
        break;
    case 'f':
        switch (lexer->start[1]) {
        case 'o': return check_keyword(lexer, 2, 1, "r", TOKEN_FOR);
        case 'r': return check_keyword(lexer, 2, 2, "om", TOKEN_FROM);
        case 'u': return check_keyword(lexer, 2, 2, "nc", TOKEN_FUNC);
        case '3': return check_keyword(lexer, 2, 1, "2", TOKEN_F32);
        case '6': return check_keyword(lexer, 2, 1, "4", TOKEN_F64);
        }
        break;
    case 'i':
        switch (lexer->start[1]) {
        case 'd': return check_keyword(lexer, 2, 5, "ivmod", TOKEN_IDIVMOD);
        case 'f': return check_terminal(lexer, 2, TOKEN_IF);
        case 'n': return check_keyword(lexer, 2, 1, "t", TOKEN_INT);
        }
        break;
    case 'n': return check_keyword(lexer, 1, 2, "ot", TOKEN_NOT);
    case 'p':
        switch (lexer->start[1]) {
        case 'a':
            return check_keyword(lexer, 2, 2, "ck", TOKEN_PACK);
        case 'o': return check_keyword(lexer, 2, 1, "p", TOKEN_POP);
        case 'r':
            if (check_middle(lexer, 2, 3, "int")) {
                switch (lexer->start[5]) {
                case '-': return check_keyword(lexer, 6, 4, "char", TOKEN_PRINT_CHAR);
                default: return check_terminal(lexer, 5, TOKEN_PRINT);
                }
            }
            break;
        case 't': return check_keyword(lexer, 2, 1, "r", TOKEN_PTR);
        }
        break;
    case 'o': return check_keyword(lexer, 1, 1, "r", TOKEN_OR);
    case 'r': return check_keyword(lexer, 1, 2, "et", TOKEN_RET);
    case 's':
        switch (lexer->start[1]) {
        case 't': return check_keyword(lexer, 2, 4, "ring", TOKEN_STRING);
        case 'w': return check_keyword(lexer, 2, 2, "ap", TOKEN_SWAP);
        case '8': return check_terminal(lexer, 2, TOKEN_S8);
        case '1': return check_keyword(lexer, 2, 1, "6", TOKEN_S16);
        case '3': return check_keyword(lexer, 2, 1, "2", TOKEN_S32);
        }
        break;
    case 't':
        switch (lexer->start[1]) {
        case 'h': return check_keyword(lexer, 2, 2, "en", TOKEN_THEN);
        case 'o': return check_terminal(lexer, 2, TOKEN_TO);
        }
        break;
    case 'u':
        switch (lexer->start[1]) {
        case '8': return check_terminal(lexer, 2, TOKEN_U8);
        case '1': return check_keyword(lexer, 2, 1, "6", TOKEN_U16);
        case '3': return check_keyword(lexer, 2, 1, "2", TOKEN_U32);
        case 'n': return check_keyword(lexer, 2, 4, "pack", TOKEN_UNPACK);
        }
        break;
    case 'w':
        switch (lexer->start[1]) {
        case 'h': return check_keyword(lexer, 2, 3, "ile", TOKEN_WHILE);
        case 'o': return check_keyword(lexer, 2, 2, "rd", TOKEN_WORD);
        }
        break;
    }
    return TOKEN_SYMBOL;
}

static struct token symbol(struct lexer *lexer) {
    while (!is_at_end(lexer) && !isspace(peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, symbol_type(lexer));
}

static int lex_decimal(struct lexer *lexer) {
    int digit_count = 0;
    while (isdigit(peek(lexer))) {
        advance(lexer);
        ++digit_count;
    }
    return digit_count;
}

static int lex_hexadecimal(struct lexer *lexer) {
    int hexit_count = 0;
    while (isxdigit(peek(lexer))) {
        advance(lexer);
        ++hexit_count;
    }
    return hexit_count;
}

static int lex_binary(struct lexer *lexer) {
    int bit_count = 0;
    for (char c = peek(lexer); c == '0' || c == '1'; c = peek(lexer)) {
        advance(lexer);
        ++bit_count;
    }
    return bit_count;
}

static bool lex_int_suffix(struct lexer *lexer) {
    if (match(lexer, 'u') || match(lexer, 's')) {
        if (is_at_end(lexer)) return false;

        switch (advance(lexer)) {
        case '1': if (!match(lexer, '6')) return false; break;
        case '3': if (!match(lexer, '2')) return false; break;
        case '8': break;
        default: return false;
        }
    }
    else {
        (void)(match(lexer, 'w') || match(lexer, 't'));
    }
    return is_at_end(lexer) || isspace(peek(lexer));
}

static bool lex_float_suffix(struct lexer *lexer) {
    if (match(lexer, 'f')) {
        switch (advance(lexer)) {
        case '3': if (!match(lexer, '2')) return false; break;
        case '6': if (!match(lexer, '4')) return false; break;
        default: return false;
        }
    }
    return is_at_end(lexer) || isspace(peek(lexer));
}

static struct token decimal_lit(struct lexer *lexer) {
    enum {NUMBER_INT, NUMBER_FLOAT} num_type = NUMBER_INT;
    bool float_had_mantissa = lex_decimal(lexer);
    bool float_had_frac = false;
    if (match(lexer, '.')) {
        num_type = NUMBER_FLOAT;
        // Fractional part.
        float_had_frac = lex_decimal(lexer);
    }
    if (match(lexer, 'e')) {
        num_type = NUMBER_FLOAT;
        // Exponent part.
        // (Optional) sign.
        (void)(match(lexer, '+') || match(lexer, '-'));
        if (!lex_decimal(lexer)) {
            // Must have at least one digit.
            return symbol(lexer);
        }
    }

    if (num_type == NUMBER_FLOAT) {
        if ((!float_had_mantissa && !float_had_frac) || !lex_float_suffix(lexer)) {
            return symbol(lexer);
        }
        return make_token(lexer, TOKEN_FLOAT_LIT);
    }

    if (check(lexer, 'f')) {
        // We allow floats to have no '.' or 'e' if followed by a suffix.
        // NOTE: if we lexed it as an integer, then we know we had a non-zero
        // number of digits in the mantissa.
        if (!lex_float_suffix(lexer)) return symbol(lexer);
        return make_token(lexer, TOKEN_FLOAT_LIT);
    }

    if (!lex_int_suffix(lexer)) return symbol(lexer);
    return make_token(lexer, TOKEN_INT_LIT);
}

static struct token hexadecimal_lit(struct lexer *lexer) {
    if (!lex_hexadecimal(lexer) || !lex_int_suffix(lexer)) {
        return symbol(lexer);
    }
    return make_token(lexer, TOKEN_INT_LIT);
}

static struct token binary_lit(struct lexer *lexer) {
    if (!lex_binary(lexer) || !lex_int_suffix(lexer)) {
        return symbol(lexer);
    }
    return make_token(lexer, TOKEN_INT_LIT);
}

static struct token number(struct lexer *lexer) {
    if (!match(lexer, '0')) {
        return decimal_lit(lexer);
    }
    // Prefixes `0x`, `0b`.
    if (match(lexer, 'x')) {
        return hexadecimal_lit(lexer);
    }
    if (match(lexer, 'b')) {
        return binary_lit(lexer);
    }
    // Decimal integers can start with '0'.
    return decimal_lit(lexer);
}

static struct token string(struct lexer *lexer) {
    while (!is_at_end(lexer) && !check(lexer, '"')) {
        char c = advance(lexer);
        if (c == '\\') {
            // Escape sequence.
            advance(lexer);
        }
    }
    if (is_at_end(lexer)) {
        lex_error(lexer, "unterminated string literal.");
        exit(1);
    }
    // Consume the closing '"'.
    advance(lexer);
    return make_token(lexer, TOKEN_STRING_LIT);
}

static struct token character(struct lexer *lexer) {
    if (check(lexer, '\'')) {
        lex_error(lexer, "empty character literal");
        exit(1);
    }
    while (!is_at_end(lexer) && !check(lexer, '\'')) {
        if (match(lexer, '\\')) {
            // Consume '\\' and the following character.
            if (is_at_end(lexer)) break;
        }
        advance(lexer);
    }
    if (is_at_end(lexer) || !match(lexer, '\'')) {
        lex_error(lexer, "unterminated character literal.");
        exit(1);
    }

    return make_token(lexer, TOKEN_CHAR_LIT);
}

static bool is_number(struct lexer *lexer) {
    (void)(match(lexer, '-') || match(lexer, '+'));  // NOTE: We only allow one leading sign.
    return isdigit(peek(lexer)) || check(lexer, '.');
}

static void start_token(struct lexer *lexer) {
    lexer->start = lexer->current;
    lexer->start_position = lexer->position;
}

struct token next_token(struct lexer *lexer) {
    consume_whitespace(lexer);
    start_token(lexer);

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOT);

    if (is_number(lexer)) {
        return number(lexer);
    }
    if (match(lexer, '"')) {
        return string(lexer);
    }
    if (match(lexer, '\'')) {
        return character(lexer);
    }

    return symbol(lexer);
}
