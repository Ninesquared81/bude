#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "region.h"
#include "string_builder.h"
#include "symbol.h"
#include "type_punning.h"


#define TEMP_REGION_SIZE 4096

struct compiler {
    struct lexer lexer;
    struct ir_block *block;
    struct token current_token;
    struct token previous_token;
    struct symbol_dictionary symbols;
    size_t for_loop_level;
    struct type_table *types;
};

static void init_compiler(struct compiler *compiler, const char *src,
                          struct ir_block *block, const char *filename,
                          struct type_table *types) {
    init_lexer(&compiler->lexer, src, filename);
    compiler->block = block;
    compiler->current_token = next_token(&compiler->lexer);
    compiler->previous_token = (struct token){0};
    init_symbol_dictionary(&compiler->symbols);
    compiler->for_loop_level = 0;
    compiler->types = types;
}

static void free_compiler(struct compiler *compiler) {
    free_symbol_dictionary(&compiler->symbols);
}

static void parse_error(struct compiler *compiler, const char *restrict message, ...) {
    report_location(compiler->lexer.filename, &compiler->previous_token.location);
    fprintf(stderr, "Parse error: ");
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void compile_error(struct compiler *compiler, const char *restrict message, ...) {
    report_location(compiler->lexer.filename, &compiler->previous_token.location);
    fprintf(stderr, "Compile error: ");
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static struct token advance(struct compiler *compiler) {
    compiler->previous_token = compiler->current_token;
    compiler->current_token = next_token(&compiler->lexer);
    return compiler->previous_token;
}

static bool check(struct compiler *compiler, enum token_type type) {
    return compiler->current_token.type == type;
}

static bool match(struct compiler *compiler, enum token_type type) {
    if (check(compiler, type)) {
        advance(compiler);
        return true;
    }
    return false;
}

static void expect_consume(struct compiler *compiler, enum token_type type,
                           const char *message) {
    if (!match(compiler, type)) {
        parse_error(compiler, "%s", message);
        exit(1);
    }
}

static bool is_at_end(struct compiler *compiler) {
    return compiler->current_token.type == TOKEN_EOT;
}

static struct token peek(struct compiler *compiler) {
    return compiler->current_token;
}

static struct token peek_previous(struct compiler *compiler) {
    return compiler->previous_token;
}

static void emit_simple(struct compiler *compiler, enum t_opcode instruction) {
    write_simple(compiler->block, instruction, &compiler->previous_token.location);
}

static void emit_immediate_u8(struct compiler *compiler, enum t_opcode instruction,
                              uint8_t operand) {
    write_immediate_u8(compiler->block, instruction, operand,
                       &compiler->previous_token.location);
}

static void emit_immediate_u16(struct compiler *compiler, enum t_opcode instruction,
                               uint16_t operand) {
    write_immediate_u16(compiler->block, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_immediate_s16(struct compiler *compiler, enum t_opcode instruction,
                               int16_t operand) {
    write_immediate_s16(compiler->block, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_immediate_uv(struct compiler *compiler, enum t_opcode instruction8,
                              uint64_t operand) {
    enum t_opcode instruction16 = instruction8 + 1;
    enum t_opcode instruction32 = instruction8 + 2;
    enum t_opcode instruction64 = instruction8 + 3;
    if (operand <= UINT8_MAX) {
        write_immediate_u8(compiler->block, instruction8, operand,
                           &compiler->previous_token.location);
    }
    else if (operand <= UINT16_MAX) {
        write_immediate_u16(compiler->block, instruction16, operand,
                            &compiler->previous_token.location);
    }
    else if (operand <= UINT32_MAX) {
        write_immediate_u32(compiler->block, instruction32, operand,
                            &compiler->previous_token.location);
    }
    else {
        write_immediate_u64(compiler->block, instruction64, operand,
                            &compiler->previous_token.location);
    }
}

#define IN_RANGE(x, lower, upper) ((lower) <= (x) && (x) <= (upper))

static void emit_immediate_sv(struct compiler *compiler, enum t_opcode instruction8,
                              int64_t operand) {
    enum t_opcode instruction16 = instruction8 + 1;
    enum t_opcode instruction32 = instruction8 + 2;
    enum t_opcode instruction64 = instruction8 + 3;
    if (IN_RANGE(operand, INT8_MIN, INT8_MAX)) {
        write_immediate_s8(compiler->block, instruction8, operand,
                           &compiler->previous_token.location);
    }
    else if (IN_RANGE(operand, INT16_MIN, INT16_MAX)) {
        write_immediate_s16(compiler->block, instruction16, operand,
                            &compiler->previous_token.location);
    }
    else if (IN_RANGE(operand, INT32_MIN, INT32_MAX)) {
        write_immediate_s32(compiler->block, instruction32, operand,
                            &compiler->previous_token.location);
    }
    else {
        write_immediate_s64(compiler->block, instruction64, operand,
                            &compiler->previous_token.location);
    }
}

static void compile_expr(struct compiler *compiler);
static void compile_symbol(struct compiler *compiler);

struct integer_prefix {
    char sign;  // '\0', '-', '+'.
    int base;   // 10, 16, 2.
};

enum integer_type {
    INT_WORD, INT_BYTE, INT_INT,
    INT_S8, INT_S16, INT_S32,
    INT_U8, INT_U16, INT_U32,
};

struct integer_prefix parse_integer_prefix(struct string_view *value) {
    // Start by assuming base 10 and no sign (e.g. `42`). This is the most common form.
    struct integer_prefix prefix = {.sign = '\0', .base = 10};
    char first = value->start[0];
    if (first == '-' || first == '+') {
        prefix.sign = first;
        // Consume the sign. This mutates the passed string view.
        ++value->start;
        --value->length;
    }

    if (value->start[0] == '0' && value->length >= 3) {
        switch (value->start[1]) {
        case 'B':
        case 'b':
            prefix.base = 2;
            value->start += 2;
            value->length -= 2;
            break;
        case 'X':
        case 'x':
            prefix.base = 16;
            value->start += 2;
            value->length -= 2;
            break;
        }
    }

    return prefix;
}

static enum integer_type parse_integer_suffix(struct string_view *value) {
    if (value->length <= 1) return INT_INT;

    const char *end = &value->start[value->length];
    switch (end[-1]) {
    case 'W':
    case 'w':
        return INT_WORD;
    case 'T':
    case 't':
        return INT_BYTE;
    case '2':
        if (end[-2] == '3' && value->length > 3) {
            switch (end[-3]) {
            case 'S':
            case 's':
                return INT_S32;
            case 'U':
            case 'u':
                return INT_U32;
            }
        }
        break;
    case '6':
        if (end[-2] == '1' && value->length > 3) {
            switch (end[-3]) {
            case 'S':
            case 's':
                return INT_S16;
            case 'U':
            case 'u':
                return INT_U16;
            }
        }
        break;
    case '8':
        switch (end[-2]) {
        case 'S':
        case 's':
            return INT_S8;
        case 'U':
        case 'u':
            return INT_U8;
        }
        break;
    }
    return INT_INT;
}

static const char *integer_type_name(enum integer_type type) {
    switch (type) {
    case INT_WORD: return "word";
    case INT_BYTE: return "byte";
    case INT_INT:  return "int";
    case INT_S8:   return "s8";
    case INT_S16:  return "s16";
    case INT_S32:  return "s32";
    case INT_U8:   return "u8";
    case INT_U16:  return "u16";
    case INT_U32:  return "u32";
    }
    return "<Unknown type>";
}

static bool check_range(uint64_t magnitude, char sign, enum integer_type type) {
    uint64_t maximum;
    switch (type) {
    case INT_WORD:
    case INT_INT:
        // This has special handling.
        return true;
    case INT_BYTE:
        maximum = UINT8_MAX;
        break;
    case INT_U8:
        maximum = UINT8_MAX;
        break;
    case INT_U16:
        maximum = UINT16_MAX;
        break;
    case INT_U32:
        maximum = UINT32_MAX;
        break;
    case INT_S8:
        maximum = (sign == '-') ? -INT8_MIN : INT8_MAX;
        break;
    case INT_S16:
        maximum = (sign == '-') ? -INT16_MIN : INT16_MAX;
        break;
    case INT_S32:
        maximum = (sign == '-') ? -(int64_t)INT32_MIN : INT32_MAX;
        break;
    }
    return magnitude <= maximum;
}

static void compile_integer(struct compiler *compiler) {
    struct string_view value = peek_previous(compiler).value;
    struct integer_prefix prefix = parse_integer_prefix(&value);
    enum integer_type type = parse_integer_suffix(&value);
    uint64_t magnitude = strtoull(value.start, NULL, prefix.base);
    if (magnitude >= UINT64_MAX && errno == ERANGE) {
        parse_error(compiler, "integer literal not in representable range.");
        exit(1);
    }
    if (!check_range(magnitude, prefix.sign, type)) {
        parse_error(compiler,
                    "integer literal not in representable range for type '%s'.\n",
                    integer_type_name(type));
        exit(1);
    }
    switch (type) {
    case INT_INT: {
        int64_t integer = 0;  // Zero-initialized in case we want to continue after errors.
        // NOTE: we get one extra value for negative literals.
        if (prefix.sign == '-' && magnitude <= -s64_to_u64(INT64_MIN)) {
            integer = -(int64_t)magnitude;
        }
        else if (magnitude <= INT64_MAX) {
            integer = magnitude;
        }
        else {
            parse_error(compiler, "magnitude of signed integer literal too large.");
            exit(1);
        }
        emit_immediate_sv(compiler, T_OP_PUSH_INT8, integer);
        break;
    }
    case INT_WORD: {
        uint64_t integer = (prefix.sign == '-') ? -magnitude : magnitude;
        emit_immediate_uv(compiler, T_OP_PUSH8, integer);
        break;
    }
    case INT_BYTE: {
        uint8_t integer = (prefix.sign == '-') ? -magnitude : magnitude;
        emit_immediate_uv(compiler, T_OP_PUSH8, integer);
        emit_simple(compiler, T_OP_AS_BYTE);
        break;
    }
    case INT_U8: {
        uint8_t integer = (prefix.sign == '-') ? -magnitude : magnitude;
        emit_immediate_uv(compiler, T_OP_PUSH8, integer);
        emit_simple(compiler, T_OP_AS_U8);
        break;
    }
    case INT_U16: {
        uint16_t integer = (prefix.sign == '-') ? -magnitude : magnitude;
        emit_immediate_uv(compiler, T_OP_PUSH8, integer);
        emit_simple(compiler, T_OP_AS_U16);
        break;
    }
    case INT_U32: {
        uint32_t integer = (prefix.sign == '-') ? -magnitude : magnitude;
        emit_immediate_uv(compiler, T_OP_PUSH8, integer);
        emit_simple(compiler, T_OP_AS_U32);
        break;
    }
    case INT_S8: {
        int8_t integer = (prefix.sign == '-') ? -(int64_t)magnitude : (int64_t)magnitude;
        emit_immediate_sv(compiler, T_OP_PUSH_INT8, integer);
        emit_simple(compiler, T_OP_AS_S8);
        break;
    }
    case INT_S16: {
        int16_t integer = (prefix.sign == '-') ? -(int64_t)magnitude : (int64_t)magnitude;
        emit_immediate_sv(compiler, T_OP_PUSH_INT8, integer);
        emit_simple(compiler, T_OP_AS_S16);
        break;
    }
    case INT_S32: {
        int32_t integer = (prefix.sign == '-') ? -(int64_t)magnitude : (int64_t)magnitude;
        emit_immediate_sv(compiler, T_OP_PUSH_INT8, integer);
        emit_simple(compiler, T_OP_AS_S32);
        break;
    }
    }
}

static int start_jump(struct compiler *compiler, enum t_opcode jump_instruction) {
    assert(is_jump(jump_instruction));
    int jump_offset = compiler->block->count;
    emit_immediate_s16(compiler, jump_instruction, 0);
    return jump_offset;
}

static void patch_jump(struct compiler *compiler, int instruction_offset, int jump) {
    if (jump < INT16_MIN || jump > INT16_MAX) {
        compile_error(compiler, "Jump too big.");
        exit(1);
    }
    overwrite_s16(compiler->block, instruction_offset + 1, jump);
}

static int compile_conditional(struct compiler *compiler) {
    compile_expr(compiler);  // Condition.
    expect_consume(compiler, TOKEN_THEN, "Expect `then` after condition in `if` block.");

    /*
     *   [if]
     *     condition -+
     *   [then]       | `then_jump`
     * +-- body  +----+
     * | [elif]  v
     * |   condition -+
     * | [then]       | `then_jump`
     * +-- body  +----+
     * | [elif]  v
     * |   condition -+
     * | [then]       | `then_jump`
     * +-- body       |
     * | [else]       |
     * |   body <-----+
     * | [end]
     * +-> ...
     * `else_jump`
     *
     * if and elif are basically the same
     *  - compile condition
     *  - write placeholder jump
     *  - compile then clause
     *  - if next token is `elif`, then compile that
     *  - if the next token is `else`, then compile the else clause
     *  - expect `end`
     *  - patch if-jump
     */

    int start = start_jump(compiler, T_OP_JUMP_NCOND);  // Offset of the jump instruction.

    compile_expr(compiler);  // Then body.

    // Note: these initial values assume no `else` or `elif` clauses.
    int end_addr = compiler->block->count;
    int else_start = end_addr;

    if (match(compiler, TOKEN_ELIF)) {
        start_jump(compiler, T_OP_JUMP);
        else_start = compiler->block->count;
        end_addr = compile_conditional(compiler);
    }
    else if (match(compiler, TOKEN_ELSE)) {
        start_jump(compiler, T_OP_JUMP);
        else_start = compiler->block->count;
        compile_expr(compiler);  // Else body.
        end_addr = compiler->block->count;
        expect_consume(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }
    else {
        expect_consume(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }

    int jump = else_start - start - 1;
    patch_jump(compiler, start, jump);
    add_jump(compiler->block, else_start);

    if (else_start != end_addr) {
        // We only emit a jump at the end of the `then` clause if we need to.
        int jump_addr = else_start - 3;
        int else_jump = end_addr - jump_addr - 1;
        patch_jump(compiler, jump_addr, else_jump);
        add_jump(compiler->block, end_addr);
    }
    
    return end_addr;
}

static void compile_for_loop(struct compiler *compiler) {
    enum t_opcode start_instruction = T_OP_FOR_DEC_START;
    enum t_opcode update_instruction = T_OP_FOR_DEC;
    if (match(compiler, TOKEN_SYMBOL)) {
        struct symbol symbol = {
            .name = peek_previous(compiler).value,
            .type = SYM_LOOP_VAR,
            .loop_var.level = compiler->for_loop_level + 1
        };
        if (match(compiler, TOKEN_FROM)) {
            insert_symbol(&compiler->symbols, &symbol);
        } else if (match(compiler, TOKEN_TO)) {
            start_instruction = T_OP_FOR_INC_START;
            update_instruction = T_OP_FOR_INC;
            insert_symbol(&compiler->symbols, &symbol);
        }
        else {
            // Symbol was part of count.
            compile_symbol(compiler);
        }
    }
    compile_expr(compiler);  // Count.
    expect_consume(compiler, TOKEN_DO, "Expect `do` after `for` start.");
    ++compiler->for_loop_level;
    if (compiler->for_loop_level > compiler->block->max_for_loop_level) {
        compiler->block->max_for_loop_level = compiler->for_loop_level;
    }
    
    int offset = start_jump(compiler, start_instruction);
    int body_start = compiler->block->count;
    compile_expr(compiler);  // Loop body.

    int loop_jump = body_start - compiler->block->count - 1;
    emit_immediate_s16(compiler, update_instruction, loop_jump);
    add_jump(compiler->block, body_start);

    int skip_jump = compiler->block->count - offset - 1;
    patch_jump(compiler, offset, skip_jump);
    add_jump(compiler->block, compiler->block->count);

    --compiler->for_loop_level;
    expect_consume(compiler, TOKEN_END, "Expect `end` after `for` loop.");
}

static void compile_loop(struct compiler *compiler) {
    int condition_start = compiler->block->count;
    compile_expr(compiler);  // Condition.
    expect_consume(compiler, TOKEN_DO, "Expect `do` after `while` condition.");

    int body_start = compiler->block->count;
    emit_immediate_s16(compiler, T_OP_JUMP_NCOND, 0);
    compile_expr(compiler);  // Loop body.

    /*   [while]
     * +-> condition
     * | [do]
     * |   T_OP_JUMP_NCOND -+
     * |   body           |
     * +-- T_OP_JUMP        |
     *   [end]            |
     *     ... <----------+
     */

    int loop_jump = condition_start - compiler->block->count - 1;  // Negative.
    emit_immediate_s16(compiler, T_OP_JUMP, loop_jump);
    add_jump(compiler->block, condition_start);

    int exit_jump = compiler->block->count - body_start - 1;  // Positive.
    patch_jump(compiler, body_start, exit_jump);
    add_jump(compiler->block, compiler->block->count);

    expect_consume(compiler, TOKEN_END, "Expect `end` after `while` body.");
}

static int escape_character(char ch) {
    switch (ch) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '\\':
    case '"':
    case '\'':
        return ch;
    }

    // `\{ch}` is not an escape sequence.
    return -1;
}

static void compile_string(struct compiler *compiler) {
    struct token token = peek_previous(compiler);
    struct string_builder builder = {0};
    struct string_builder *current = &builder;
    const char *start = token.value.start + 1;
    struct region *temp_region = new_region(TEMP_REGION_SIZE);
    start_view(current, start, temp_region);
    for (const char *c = start; *c != '"'; ++c) {
        if (*c == '\\') {
            int escaped = escape_character(c[1]);
            if (escaped == -1) {
                parse_error(compiler, "invalid escape sequence '\\%c'.\n", c[1]);
                exit(1);
            }
            current = store_char(current, escaped, temp_region);
            ++c;  // Consume the escaped character.
        }
        else {
            if (!SB_IS_VIEW(current)) {
                current = start_view(current, c, temp_region);
            }
            ++current->view.length;
        }
    }
    uint32_t index = write_string(compiler->block, &builder);
    emit_immediate_uv(compiler, T_OP_LOAD_STRING8, index);
    kill_region(temp_region);
}

static void compile_character(struct compiler *compiler) {
    struct string_view value = peek_previous(compiler).value;
    char character = value.start[1];
    if (character == '\\') {
        int escaped = escape_character(value.start[2]);
        if (escaped == -1) {
            parse_error(compiler, "invalid escape sequence '\\%c'.\n", value.start[2]);
            exit(1);
        }
        character = escaped;
    }
    emit_immediate_u8(compiler, T_OP_PUSH_CHAR8, character);
}

static void compile_pack(struct compiler *compiler) {
    expect_consume(compiler, TOKEN_SYMBOL, "Expect pack name after `pack`.");
    struct symbol symbol = {
        .name = peek_previous(compiler).value,
        .type = SYM_PACK,
        .pack.index = -1  // -1 to indicate unitiliazed.
    };
    expect_consume(compiler, TOKEN_DEF, "Expect `def` after pack name.");
    struct type_info info = {.kind = KIND_PACK};
    int field_count = 0;
    for (; field_count < 8 && !check(compiler, TOKEN_END); ++field_count) {
        if (is_at_end(compiler)) {
            parse_error(compiler, "unexpected EOF parsing pack definition.\n");
            exit(1);
        }
        switch (advance(compiler).type) {
        case TOKEN_BYTE:
            info.pack.fields[field_count] = TYPE_BYTE;
            break;
        case TOKEN_INT:
            info.pack.fields[field_count] = TYPE_INT;
            break;
        case TOKEN_PTR:
            info.pack.fields[field_count] = TYPE_PTR;
            break;
        case TOKEN_S8:
            info.pack.fields[field_count] = TYPE_S8;
            break;
        case TOKEN_S16:
            info.pack.fields[field_count] = TYPE_S16;
            break;
        case TOKEN_S32:
            info.pack.fields[field_count] = TYPE_S32;
            break;
        case TOKEN_U8:
            info.pack.fields[field_count] = TYPE_U8;
            break;
        case TOKEN_U16:
            info.pack.fields[field_count] = TYPE_U16;
            break;
        case TOKEN_U32:
            info.pack.fields[field_count] = TYPE_U32;
            break;
        case TOKEN_WORD:
            info.pack.fields[field_count] = TYPE_WORD;
            break;
        default:
            parse_error(compiler, "unexpected token while parsing pack definition.\n");
            exit(1);
        }
    }
    expect_consume(compiler, TOKEN_END, "Expect `end` after pack definition.");
    info.pack.field_count = field_count;
    symbol.pack.index = new_type(compiler->types, &info);
    insert_symbol(&compiler->symbols, &symbol);
}

static void compile_comp(struct compiler *compiler) {
    expect_consume(compiler, TOKEN_SYMBOL, "Expect symbol after `comp`.");
    struct symbol symbol = {
        .name = peek_previous(compiler).value,
        .type = SYM_COMP,
        .comp.index = -1,
    };
    expect_consume(compiler, TOKEN_DEF, "Expect `def` after comp name.");
    struct type_info info = {.kind = KIND_COMP};
    int field_count = 0;
    struct region *temp_region = new_region(TEMP_REGION_SIZE);
    struct type_node {struct type_node *next; type_index type;} *head = NULL;
    while (!check(compiler, TOKEN_END)) {
        if (is_at_end(compiler)) {
            parse_error(compiler, "unexpected EOF parsing comp definition.\n");
            exit(1);
        }
        type_index type = TYPE_ERROR;
        switch (advance(compiler).type) {
        case TOKEN_BYTE:
            type = TYPE_BYTE;
            break;
        case TOKEN_INT:
            type = TYPE_INT;
            break;
        case TOKEN_PTR:
            type = TYPE_PTR;
            break;
        case TOKEN_S8:
            type = TYPE_S8;
            break;
        case TOKEN_S16:
            type = TYPE_S16;
            break;
        case TOKEN_S32:
            type = TYPE_S32;
            break;
        case TOKEN_U8:
            type = TYPE_U8;
            break;
        case TOKEN_U16:
            type = TYPE_U16;
            break;
        case TOKEN_U32:
            type = TYPE_U32;
            break;
        case TOKEN_WORD:
            type = TYPE_WORD;
            break;
        default:
            parse_error(compiler, "unexpected token while parsing comp definition.\n");
            exit(1);
        }
        struct type_node *next = head;
        head = region_alloc(temp_region, sizeof *head);
        head->next = next;
        head->type = type;
        ++field_count;
    }
    expect_consume(compiler, TOKEN_END, "Expect `end` after comp definition");
    info.comp.field_count = field_count;
    type_index *fields;
    if (field_count <= 8) {
        // Compact form; stored in struct itself.
        fields = &info.comp.compact.fields[0];
    }
    else {
        // Expanded form; dynamically allocated.
        fields = alloc_extra(compiler->types, field_count * sizeof *fields);
        info.comp.expanded.fields = fields;
    }
    struct type_node *current = head;
    for (int i = field_count - 1; i >= 0; --i) {
        assert(current != NULL);  // this should be true, but it doesn't hurt to assert.
        fields[i] = current->type;
        current = current->next;
    }
    symbol.comp.index = new_type(compiler->types, &info);
    insert_symbol(&compiler->symbols, &symbol);
}

static void compile_loop_var_symbol(struct compiler *compiler, struct symbol *symbol) {
    size_t level = symbol->loop_var.level;
    if (level > compiler->for_loop_level) {
        // TODO: protect against too-long symbol names.
        assert(symbol->name.length <= INT_MAX);
        compile_error(compiler, "loop variable '%.*s' referenced outside defining loop.\n",
                      (int)symbol->name.length, symbol->name.start);
        exit(1);
    }
    uint16_t offset = compiler->for_loop_level - level;  // Offset from top of aux.
    emit_immediate_u16(compiler, T_OP_GET_LOOP_VAR, offset);
}

static void compile_pack_symbol(struct compiler *compiler, struct symbol *symbol) {
    emit_immediate_sv(compiler, T_OP_PACK8, symbol->pack.index);
}

static void compile_comp_symbol(struct compiler *compiler, struct symbol *symbol) {
    emit_immediate_sv(compiler, T_OP_COMP8, symbol->pack.index);
}

static void compile_symbol(struct compiler *compiler) {
    struct string_view symbol_text = peek_previous(compiler).value;
    struct symbol *symbol = lookup_symbol(&compiler->symbols, &symbol_text);
    if (symbol == NULL) {
        // TODO: protect against really long symbol names.
        assert(symbol_text.length <= INT_MAX);
        compile_error(compiler, "unknown symbol '%.*s'.\n",
                      (int)symbol_text.length, symbol_text.start);
        exit(1);
    }
    switch (symbol->type) {
    case SYM_LOOP_VAR:
        compile_loop_var_symbol(compiler, symbol);
        break;
    case SYM_PACK:
        compile_pack_symbol(compiler, symbol);
        break;
    case SYM_COMP:
        compile_comp_symbol(compiler, symbol);
        break;
    }
}

static bool compile_simple(struct compiler *compiler) {
    switch (peek(compiler).type) {
    case TOKEN_AND:
        emit_simple(compiler, T_OP_AND);
        break;
    case TOKEN_DEREF:
        emit_simple(compiler, T_OP_DEREF);
        break;
    case TOKEN_DUPE:
        emit_simple(compiler, T_OP_DUPE);
        break;
    case TOKEN_EXIT:
        emit_simple(compiler, T_OP_EXIT);
        break;
    case TOKEN_MINUS:
        emit_simple(compiler, T_OP_SUB);
        break;
    case TOKEN_NOT:
        emit_simple(compiler, T_OP_NOT);
        break;
    case TOKEN_OR:
        emit_simple(compiler, T_OP_OR);
        break;
    case TOKEN_PLUS:
        emit_simple(compiler, T_OP_ADD);
        break;
    case TOKEN_POP:
        emit_simple(compiler, T_OP_POP);
        break;
    case TOKEN_PRINT:
        emit_simple(compiler, T_OP_PRINT);
        break;
    case TOKEN_PRINT_CHAR:
        emit_simple(compiler, T_OP_PRINT_CHAR);
        break;
    case TOKEN_SLASH_PERCENT:
        emit_simple(compiler, T_OP_DIVMOD);
        break;
    case TOKEN_STAR:
        emit_simple(compiler, T_OP_MULT);
        break;
    case TOKEN_SWAP:
        emit_simple(compiler, T_OP_SWAP);
        break;
    case TOKEN_UNPACK:
        emit_simple(compiler, T_OP_UNPACK);
        break;
    default:
        /* All other tokens fall through. */
        return false;
    }
    advance(compiler);
    return true;
}

static void compile_expr(struct compiler *compiler) {
    while (!is_at_end(compiler)) {
        if (match(compiler, TOKEN_CHAR_LIT)) {
            compile_character(compiler);
        }
        else if (match(compiler, TOKEN_COMP)) {
            compile_comp(compiler);
        }
        else if (match(compiler, TOKEN_FOR)) {
            compile_for_loop(compiler);
        }
        else if (match(compiler, TOKEN_IF)) {
            compile_conditional(compiler);
        }
        else if (match(compiler, TOKEN_INT_LIT)) {
            compile_integer(compiler);
        }
        else if (match(compiler, TOKEN_PACK)) {
            compile_pack(compiler);
        }
        else if (match(compiler, TOKEN_STRING_LIT)) {
            compile_string(compiler);
        }
        else if (match(compiler, TOKEN_SYMBOL)) {
            compile_symbol(compiler);
        }
        else if (match(compiler, TOKEN_WHILE)) {
            compile_loop(compiler);
        }
        else {
            // Treat any other token as a simple token.
            // If it's not simple, stop compiling.
            if (!compile_simple(compiler)) return;
        }
    }
}

void compile(const char *src, struct ir_block *block, const char *filename,
             struct type_table *types) {
    struct compiler compiler;
    init_compiler(&compiler, src, block, filename, types);
    compile_expr(&compiler);
    emit_simple(&compiler, T_OP_NOP);
    free_compiler(&compiler);
}

