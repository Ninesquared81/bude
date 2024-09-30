#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "string_builder.h"
#include "string_view.h"

static void set_position(struct lexer *lexer, struct location position) {
    lexer->position = position;
    lexer->start_position = position;
}

void init_lexer(struct lexer *lexer, const char *src, const char *src_end, const char *filename) {
    lexer->start = src;
    lexer->end = src_end;
    lexer->current = src;
    set_position(lexer, (struct location) {LINE_START, COLUMN_START});
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
        pos->column = COLUMN_START;
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
    if (lexer->end != NULL) return lexer->current == lexer->end;
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

static void lex_string(struct lexer *lexer) {
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
}

static void lex_subscript(struct lexer *lexer) {
    while (!match(lexer, ']')) {
        if (is_at_end(lexer)) {
            lex_error(lexer, "unexpected EOF in token subscript.");
            exit(1);
        }
        if (match(lexer, '[')) {
            lex_subscript(lexer);
        }
        else if (match(lexer, '"')) {
            lex_string(lexer);
        }
        else if (check(lexer, '#')) {
            return;  // Comment; stop lexing.
        }
        else {
            advance(lexer);
        }
    }
}

static struct token make_token(struct lexer *lexer, enum token_type type, bool allow_subscript) {
    int length = lexer->current - lexer->start;
    const char *subscript_start = lexer->current;
    struct location subscript_location = lexer->position;
    if (allow_subscript && match(lexer, '[')) {
        lex_subscript(lexer);
    }
    const char *subscript_end = lexer->current;
    return (struct token) {
        .type = type,
        .value = {
            .start = lexer->start,
            .length = length
        },
        .location = lexer->start_position,
        .subscript_start = subscript_start,
        .subscript_end = subscript_end,
        .subscript_location = subscript_location,
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
    case '~': return check_terminal(lexer, 1, TOKEN_TILDE);
    case 'a':
        switch (lexer->start[1]) {
        case 'n': return check_keyword(lexer, 2, 1, "d", TOKEN_AND);
        case 'r': return check_keyword(lexer, 2, 3, "ray", TOKEN_ARRAY);
        case 's': return check_terminal(lexer, 2, TOKEN_AS);
        }
        break;
    case 'b':
        switch (lexer->start[1]) {
        case 'o': return check_keyword(lexer, 2, 2, "ol", TOKEN_BOOL);
        case 'y': return check_keyword(lexer, 2, 2, "te", TOKEN_BYTE);
        }
        break;
    case 'c':
        switch (lexer->start[1]) {
        case 'h':
            if (check_middle(lexer, 2, 2, "ar")) {
                switch (lexer->start[4]) {
                case '1': return check_keyword(lexer, 5, 1, "6", TOKEN_CHAR16);
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
        case 'a': return check_keyword(lexer, 2, 3, "lse", TOKEN_FALSE);
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
        case 'm': return check_keyword(lexer, 2, 4, "port", TOKEN_IMPORT);
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
                case 'l': return check_keyword(lexer, 6, 1, "n", TOKEN_PRINTLN);
                case 's': return check_keyword(lexer, 6, 1, "p", TOKEN_PRINTSP);
                case 't': return check_keyword(lexer, 6, 1, "b", TOKEN_PRINTTB);
                default: return check_terminal(lexer, 5, TOKEN_PRINT);
                }
            }
            break;
        case 't': return check_keyword(lexer, 2, 1, "r", TOKEN_PTR);
        }
        break;
    case 'o':
        switch (lexer->start[1]) {
        case 'r': return check_terminal(lexer, 2, TOKEN_OR);
        case 'v': return check_keyword(lexer, 2, 2, "er", TOKEN_OVER);
        }
        break;
    case 'r':
        switch (lexer->start[1]) {
        case 'e': return check_keyword(lexer, 2, 1, "t", TOKEN_RET);
        case 'o': return check_keyword(lexer, 2, 1, "t", TOKEN_ROT);
        }
        break;
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
        case 'r': return check_keyword(lexer, 2, 2, "ue", TOKEN_TRUE);
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
    case 'v': return check_keyword(lexer, 1, 2, "ar", TOKEN_VAR);
    case 'w':
        switch (lexer->start[1]) {
        case 'h': return check_keyword(lexer, 2, 3, "ile", TOKEN_WHILE);
        case 'i': return check_keyword(lexer, 2, 2, "th", TOKEN_WITH);
        case 'o': return check_keyword(lexer, 2, 2, "rd", TOKEN_WORD);
        }
        break;
    }
    return TOKEN_SYMBOL;
}

static bool is_special(char c) {
    const char *const special_chars = "#[]";
    return strchr(special_chars, c) != NULL;
}

static bool is_symbolic(char c) {
    return !isspace(c) && !is_special(c);
}

static struct token symbol(struct lexer *lexer) {
    while (!is_at_end(lexer) && is_symbolic(peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, symbol_type(lexer), true);
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
    return is_at_end(lexer) || !is_symbolic(peek(lexer));
}

static bool lex_float_suffix(struct lexer *lexer) {
    if (match(lexer, 'f')) {
        switch (advance(lexer)) {
        case '3': if (!match(lexer, '2')) return false; break;
        case '6': if (!match(lexer, '4')) return false; break;
        default: return false;
        }
    }
    return is_at_end(lexer) || !is_symbolic(peek(lexer));
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
        return make_token(lexer, TOKEN_FLOAT_LIT, false);
    }

    if (check(lexer, 'f')) {
        // We allow floats to have no '.' or 'e' if followed by a suffix.
        // NOTE: if we lexed it as an integer, then we know we had a non-zero
        // number of digits in the mantissa.
        if (!lex_float_suffix(lexer)) return symbol(lexer);
        return make_token(lexer, TOKEN_FLOAT_LIT, false);
    }

    if (!lex_int_suffix(lexer)) return symbol(lexer);
    return make_token(lexer, TOKEN_INT_LIT, false);
}

static struct token hexadecimal_lit(struct lexer *lexer) {
    if (!lex_hexadecimal(lexer) || !lex_int_suffix(lexer)) {
        return symbol(lexer);
    }
    return make_token(lexer, TOKEN_INT_LIT, false);
}

static struct token binary_lit(struct lexer *lexer) {
    if (!lex_binary(lexer) || !lex_int_suffix(lexer)) {
        return symbol(lexer);
    }
    return make_token(lexer, TOKEN_INT_LIT, false);
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
    lex_string(lexer);
    return make_token(lexer, TOKEN_STRING_LIT, true);
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

    return make_token(lexer, TOKEN_CHAR_LIT, true);
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

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOT, false);

    if (match(lexer, '"')) {
        return string(lexer);
    }
    if (match(lexer, '\'')) {
        return character(lexer);
    }
    if (match(lexer, '[')) {
        return make_token(lexer, TOKEN_SQUARE_BRACKET_LEFT, false);
    }
    if (match(lexer, ']')) {
        return make_token(lexer, TOKEN_SQUARE_BRACKET_RIGHT, false);
    }
    // NOTE: This consumes any `+` or `-` characters, even if not in a number.
    if (is_number(lexer)) {
        return number(lexer);
    }

    return symbol(lexer);
}

struct lexer get_subscript_lexer(struct token token, const char *filename) {
    const char *start = token.subscript_start;
    const char *end = token.subscript_end;
    struct location location = token.subscript_location;
    if (HAS_SUBSCRIPT(token)) {
        ++start;
        --end;
        ++location.column;
    }
    return (struct lexer) {
        .start = start,
        .end = end,
        .current = start,
        .position = location,
        .start_position = location,
        .filename = filename,
    };
}

const char *token_type_name(enum token_type type) {
    static const char *const type_names[] = {
#define X(token) #token,
        TOKENS
#undef X
    };
    static_assert(sizeof type_names / sizeof type_names[0] == TOKEN_EOT + 1);
    assert(0 <= type && type <= TOKEN_EOT);
    return type_names[type];
}

static struct string_builder token_to_sb(struct token token, struct region *region) {
    struct string_builder sb = SB_FROM_SV(token.value);
    if (!HAS_SUBSCRIPT(token)) return sb;
    ++sb.view.length;  // Include '['.
    struct lexer sublexer = get_subscript_lexer(token, NULL);  // There shouldn't be any errors.
    struct token subtoken = next_token(&sublexer);
    while (subtoken.type != TOKEN_EOT) {
        sb_append(&sb, token_to_sb(subtoken, region), region);
        subtoken = next_token(&sublexer);
    }
    sb_append(&sb, SB_FROM_SV(SV_LIT("]")), region);
    return sb;
}

struct string_view token_to_sv(struct token token, struct region *region) {
    struct string_builder sb = token_to_sb(token, region);
    return join_string_in_region(&sb, " ", region);  // Join tokens with a space.
}

void print_token(struct token token) {
    printf("%s: %"PRI_SV"%.*s\n", token_type_name(token.type),
           SV_FMT(token.value),
           (int)(token.subscript_end - token.subscript_start), token.subscript_start);
}
