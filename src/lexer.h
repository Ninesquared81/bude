#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

#include "location.h"
#include "string_view.h"

enum token_type {
    // Literals.
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_CHAR_LIT,
    TOKEN_STRING_LIT,
    TOKEN_SYMBOL,

    // Keywords.
    TOKEN_AND,
    TOKEN_AS,
    TOKEN_BOOL,
    TOKEN_BYTE,
    TOKEN_CHAR,
    TOKEN_CHAR16,
    TOKEN_CHAR32,
    TOKEN_COMP,
    TOKEN_DECOMP,
    TOKEN_DEF,
    TOKEN_DEREF,
    TOKEN_DIVMOD,
    TOKEN_DO,
    TOKEN_DUPE,
    TOKEN_EDIVMOD,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_END,
    TOKEN_EQUALS,
    TOKEN_EXIT,
    TOKEN_FOR,
    TOKEN_FROM,
    TOKEN_FUNC,
    TOKEN_F32,
    TOKEN_F64,
    TOKEN_GREATER_EQUALS,
    TOKEN_GREATER_THAN,
    TOKEN_IDIVMOD,
    TOKEN_IF,
    TOKEN_IMPORT,
    TOKEN_INT,
    TOKEN_LEFT_ARROW,
    TOKEN_LESS_EQUALS,
    TOKEN_LESS_THAN,
    TOKEN_MINUS,
    TOKEN_NOT,
    TOKEN_OR,
    TOKEN_OVER,
    TOKEN_PACK,
    TOKEN_PERCENT,
    TOKEN_PLUS,
    TOKEN_POP,
    TOKEN_PRINT,
    TOKEN_PRINT_CHAR,
    TOKEN_PTR,
    TOKEN_RET,
    TOKEN_RIGHT_ARROW,
    TOKEN_ROT,
    TOKEN_S8,
    TOKEN_S16,
    TOKEN_S32,
    TOKEN_SLASH,
    TOKEN_SLASH_EQUALS,
    TOKEN_STAR,
    TOKEN_STRING,
    TOKEN_SWAP,
    TOKEN_THEN,
    TOKEN_TILDE,
    TOKEN_TO,
    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_UNPACK,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_WITH,
    TOKEN_WORD,

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
