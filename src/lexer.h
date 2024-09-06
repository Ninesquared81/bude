#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

#include "location.h"
#include "string_view.h"

#define TOKENS                                  \
    /* Literals. */                             \
    X(TOKEN_INT_LIT)                            \
    X(TOKEN_FLOAT_LIT)                          \
    X(TOKEN_CHAR_LIT)                           \
    X(TOKEN_STRING_LIT)                         \
    X(TOKEN_SYMBOL)                             \
    /* Keywords. */                             \
    X(TOKEN_AND)                                \
    X(TOKEN_AS)                                 \
    X(TOKEN_BOOL)                               \
    X(TOKEN_BYTE)                               \
    X(TOKEN_CHAR)                               \
    X(TOKEN_CHAR16)                             \
    X(TOKEN_CHAR32)                             \
    X(TOKEN_COMP)                               \
    X(TOKEN_DECOMP)                             \
    X(TOKEN_DEF)                                \
    X(TOKEN_DEREF)                              \
    X(TOKEN_DIVMOD)                             \
    X(TOKEN_DO)                                 \
    X(TOKEN_DUPE)                               \
    X(TOKEN_EDIVMOD)                            \
    X(TOKEN_ELIF)                               \
    X(TOKEN_ELSE)                               \
    X(TOKEN_END)                                \
    X(TOKEN_EQUALS)                             \
    X(TOKEN_EXIT)                               \
    X(TOKEN_FALSE)                              \
    X(TOKEN_FOR)                                \
    X(TOKEN_FROM)                               \
    X(TOKEN_FUNC)                               \
    X(TOKEN_F32)                                \
    X(TOKEN_F64)                                \
    X(TOKEN_GREATER_EQUALS)                     \
    X(TOKEN_GREATER_THAN)                       \
    X(TOKEN_IDIVMOD)                            \
    X(TOKEN_IF)                                 \
    X(TOKEN_IMPORT)                             \
    X(TOKEN_INT)                                \
    X(TOKEN_LEFT_ARROW)                         \
    X(TOKEN_LESS_EQUALS)                        \
    X(TOKEN_LESS_THAN)                          \
    X(TOKEN_MINUS)                              \
    X(TOKEN_NOT)                                \
    X(TOKEN_OR)                                 \
    X(TOKEN_OVER)                               \
    X(TOKEN_PACK)                               \
    X(TOKEN_PERCENT)                            \
    X(TOKEN_PLUS)                               \
    X(TOKEN_POP)                                \
    X(TOKEN_PRINT)                              \
    X(TOKEN_PRINT_CHAR)                         \
    X(TOKEN_PTR)                                \
    X(TOKEN_RET)                                \
    X(TOKEN_RIGHT_ARROW)                        \
    X(TOKEN_ROT)                                \
    X(TOKEN_S8)                                 \
    X(TOKEN_S16)                                \
    X(TOKEN_S32)                                \
    X(TOKEN_SLASH)                              \
    X(TOKEN_SLASH_EQUALS)                       \
    X(TOKEN_STAR)                               \
    X(TOKEN_STRING)                             \
    X(TOKEN_SWAP)                               \
    X(TOKEN_THEN)                               \
    X(TOKEN_TILDE)                              \
    X(TOKEN_TO)                                 \
    X(TOKEN_TRUE)                               \
    X(TOKEN_U8)                                 \
    X(TOKEN_U16)                                \
    X(TOKEN_U32)                                \
    X(TOKEN_UNPACK)                             \
    X(TOKEN_VAR)                                \
    X(TOKEN_WHILE)                              \
    X(TOKEN_WITH)                               \
    X(TOKEN_WORD)                               \
    /* Special. */                              \
    X(TOKEN_EOT)

enum token_type {
#define X(token) token,
    TOKENS
#undef X
};

struct token {
    enum token_type type;
    struct string_view value;
    struct location location;
    const char *subscript_start;
    const char *subscript_end;
    struct location subscript_location;
};

struct lexer {
    const char *start;
    const char *end;  // If NULL, lex until EOF.
    const char *current;
    struct location position;
    struct location start_position;
    const char *filename;
};

#define HAS_SUBSCRIPT(token) ((token).start != (token).end)

void init_lexer(struct lexer *lexer, const char *src, const char *src_end, const char *filename);
struct token next_token(struct lexer *lexer);
struct lexer get_subscript_lexer(struct token token, const char *filename);
const char *token_type_name(enum token_type type);
void print_token(struct token token);

#endif
