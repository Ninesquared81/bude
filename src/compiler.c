#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "memory.h"
#include "region.h"
#include "string_builder.h"
#include "symbol.h"
#include "type_punning.h"
#include "unicode.h"


#define TEMP_REGION_SIZE 4096

struct compiler {
    struct lexer lexer;
    struct token current_token;
    struct token previous_token;
    struct symbol_dictionary symbols;
    struct function *function;
    struct module *module;
    struct region *temp;
    int for_loop_level;
};

static void init_compiler(struct compiler *compiler, const char *src, struct module *module) {
    compiler->function = NULL;  // Will be set later.
    init_lexer(&compiler->lexer, src, module->filename);
    compiler->current_token = next_token(&compiler->lexer);
    compiler->previous_token = (struct token){0};
    init_symbol_dictionary(&compiler->symbols);
    compiler->for_loop_level = 0;
    compiler->module = module;
    compiler->temp = new_region(TEMP_REGION_SIZE);
    CHECK_ALLOCATION(compiler->temp);
}

static void free_compiler(struct compiler *compiler) {
    free_symbol_dictionary(&compiler->symbols);
    kill_region(compiler->temp);
    compiler->temp = NULL;
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
    write_simple(&compiler->function->t_code, instruction, &compiler->previous_token.location);
}

[[maybe_unused]]
static void emit_immediate_u8(struct compiler *compiler, enum t_opcode instruction,
                              uint8_t operand) {
    write_immediate_u8(&compiler->function->t_code, instruction, operand,
                       &compiler->previous_token.location);
}

static void emit_immediate_s8(struct compiler *compiler, enum t_opcode instruction,
                              int8_t operand) {
    write_immediate_s8(&compiler->function->t_code, instruction, operand,
                       &compiler->previous_token.location);
}

static void emit_immediate_u16(struct compiler *compiler, enum t_opcode instruction,
                               uint16_t operand) {
    write_immediate_u16(&compiler->function->t_code, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_immediate_s16(struct compiler *compiler, enum t_opcode instruction,
                               int16_t operand) {
    write_immediate_s16(&compiler->function->t_code, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_immediate_u32(struct compiler *compiler, enum t_opcode instruction,
                               uint32_t operand) {
    write_immediate_u32(&compiler->function->t_code, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_immediate_s32(struct compiler *compiler, enum t_opcode instruction,
                               int32_t operand) {
    write_immediate_s32(&compiler->function->t_code, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_immediate_u64(struct compiler *compiler, enum t_opcode instruction,
                               uint64_t operand) {
    write_immediate_u64(&compiler->function->t_code, instruction, operand,
                        &compiler->previous_token.location);
}

static void emit_s8(struct compiler *compiler, int8_t value) {
    write_u8(&compiler->function->t_code, value, &compiler->previous_token.location);
}

static void emit_s16(struct compiler *compiler, int16_t value) {
    write_u16(&compiler->function->t_code, value, &compiler->previous_token.location);
}

static void emit_s32(struct compiler *compiler, int32_t value) {
    write_u32(&compiler->function->t_code, value, &compiler->previous_token.location);
}

static void emit_immediate_uv(struct compiler *compiler, enum t_opcode instruction8,
                              uint64_t operand) {
    enum t_opcode instruction16 = instruction8 + 1;
    enum t_opcode instruction32 = instruction8 + 2;
    enum t_opcode instruction64 = instruction8 + 3;
    if (operand <= UINT8_MAX) {
        write_immediate_u8(&compiler->function->t_code, instruction8, operand,
                           &compiler->previous_token.location);
    }
    else if (operand <= UINT16_MAX) {
        write_immediate_u16(&compiler->function->t_code, instruction16, operand,
                            &compiler->previous_token.location);
    }
    else if (operand <= UINT32_MAX) {
        write_immediate_u32(&compiler->function->t_code, instruction32, operand,
                            &compiler->previous_token.location);
    }
    else {
        write_immediate_u64(&compiler->function->t_code, instruction64, operand,
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
        write_immediate_s8(&compiler->function->t_code, instruction8, operand,
                           &compiler->previous_token.location);
    }
    else if (IN_RANGE(operand, INT16_MIN, INT16_MAX)) {
        write_immediate_s16(&compiler->function->t_code, instruction16, operand,
                            &compiler->previous_token.location);
    }
    else if (IN_RANGE(operand, INT32_MIN, INT32_MAX)) {
        write_immediate_s32(&compiler->function->t_code, instruction32, operand,
                            &compiler->previous_token.location);
    }
    else {
        write_immediate_s64(&compiler->function->t_code, instruction64, operand,
                            &compiler->previous_token.location);
    }
}

static void emit_pack_field(struct compiler *compiler, enum t_opcode instruction8,
                            type_index pack, int offset) {
    emit_immediate_sv(compiler, instruction8, pack);
    emit_s8(compiler, offset);
}

static void emit_comp_field(struct compiler *compiler, enum t_opcode instruction8,
                            type_index comp, int offset) {
    if (IN_RANGE(comp, INT8_MIN, INT8_MAX) && IN_RANGE(offset, INT8_MIN, INT8_MAX)) {
        emit_immediate_s8(compiler, instruction8, comp);
        emit_s8(compiler, offset);
    }
    else if (IN_RANGE(comp, INT16_MIN, INT16_MAX) && IN_RANGE(offset, INT16_MIN, INT16_MAX)) {
        emit_immediate_s16(compiler, instruction8 + 1, comp);
        emit_s16(compiler, offset);
    }
    else {
        emit_immediate_s32(compiler, instruction8 + 2, comp);
        emit_s32(compiler, offset);
    }
}

static bool check_last_instruction(struct compiler *compiler, enum t_opcode instruction) {
    struct ir_block *block = &compiler->function->t_code;
    int count = block->count;
    return count > 0 && block->code[count - 1] == instruction;
}

static void compile_expr(struct compiler *compiler);
static void compile_symbol(struct compiler *compiler);

struct integer_prefix {
    char sign;  // '\0', '-', '+'.
    int base;   // 10, 16, 2.
};

enum floating_point_type {
    FLOAT_F32, FLOAT_F64,
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

static enum floating_point_type parse_floating_point_suffix(struct string_view *value) {
    if (value->length <= 3) return FLOAT_F64;

    const char *end = &value->start[value->length];
    if (strncmp(end - 3, "f32", 3) == 0) return FLOAT_F32;
    if (strncmp(end - 3, "f64", 3) == 0) return FLOAT_F64;
    return FLOAT_F64;
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

static void compile_floating_point(struct compiler *compiler) {
    struct string_view value = peek_previous(compiler).value;
    enum floating_point_type type = parse_floating_point_suffix(&value);
    if (type == FLOAT_F32) {
        // f32 -- single precision floating-point number.
        float f32 = strtof(value.start, NULL);
        emit_immediate_u32(compiler, T_OP_PUSH_FLOAT32, f32_to_u32(f32));
    }
    else {
        // f64 -- double precision floating-point number.
        double f64 = strtod(value.start, NULL);
        emit_immediate_u64(compiler, T_OP_PUSH_FLOAT64, f64_to_u64(f64));
    }
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
    int jump_offset = compiler->function->t_code.count;
    emit_immediate_s16(compiler, jump_instruction, 0);
    return jump_offset;
}

static void patch_jump(struct compiler *compiler, int instruction_offset, int jump) {
    if (jump < INT16_MIN || jump > INT16_MAX) {
        compile_error(compiler, "Jump too big.");
        exit(1);
    }
    overwrite_s16(&compiler->function->t_code, instruction_offset + 1, jump);
}

static int compile_conditional(struct compiler *compiler) {
    struct ir_block *block = &compiler->function->t_code;
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
    int end_addr = block->count;
    int else_start = end_addr;

    if (match(compiler, TOKEN_ELIF)) {
        start_jump(compiler, T_OP_JUMP);
        else_start = block->count;
        end_addr = compile_conditional(compiler);
    }
    else if (match(compiler, TOKEN_ELSE)) {
        start_jump(compiler, T_OP_JUMP);
        else_start = block->count;
        compile_expr(compiler);  // Else body.
        end_addr = block->count;
        expect_consume(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }
    else {
        expect_consume(compiler, TOKEN_END, "Expect `end` after `if` body.");
    }

    int jump = else_start - start - 1;
    patch_jump(compiler, start, jump);
    add_jump(block, else_start);

    if (else_start != end_addr) {
        // We only emit a jump at the end of the `then` clause if we need to.
        int jump_addr = else_start - 3;
        int else_jump = end_addr - jump_addr - 1;
        patch_jump(compiler, jump_addr, else_jump);
        add_jump(block, end_addr);
    }

    return end_addr;
}

static void compile_for_loop(struct compiler *compiler) {
    struct ir_block *block = &compiler->function->t_code;
    enum t_opcode start_instruction = T_OP_FOR_DEC_START;
    enum t_opcode update_instruction = T_OP_FOR_DEC;
    int loop_level_offset = 1;
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
            ++loop_level_offset;  // +1 for loop target.
            ++symbol.loop_var.level;  // Store loop counter above target in loop stack.
            insert_symbol(&compiler->symbols, &symbol);
        }
        else {
            // Symbol was part of count.
            compile_symbol(compiler);
        }
    }
    compile_expr(compiler);  // Count.
    expect_consume(compiler, TOKEN_DO, "Expect `do` after `for` start.");
    compiler->for_loop_level += loop_level_offset;
    if (compiler->for_loop_level > compiler->function->max_for_loop_level) {
        compiler->function->max_for_loop_level = compiler->for_loop_level;
    }

    int offset = start_jump(compiler, start_instruction);
    int body_start = block->count;
    compile_expr(compiler);  // Loop body.

    int loop_jump = body_start - block->count - 1;
    emit_immediate_s16(compiler, update_instruction, loop_jump);
    add_jump(block, body_start);

    int skip_jump = block->count - offset - 1;
    patch_jump(compiler, offset, skip_jump);
    add_jump(block, block->count);

    compiler->for_loop_level -= loop_level_offset;
    expect_consume(compiler, TOKEN_END, "Expect `end` after `for` loop.");
}

static void compile_loop(struct compiler *compiler) {
    struct ir_block *block = &compiler->function->t_code;
    int condition_start = block->count;
    compile_expr(compiler);  // Condition.
    expect_consume(compiler, TOKEN_DO, "Expect `do` after `while` condition.");

    int body_start = block->count;
    emit_immediate_s16(compiler, T_OP_JUMP_NCOND, 0);
    compile_expr(compiler);  // Loop body.

    /*   [while]
     * +-> condition
     * | [do]
     * |   T_OP_JUMP_NCOND -+
     * |   body             |
     * +-- T_OP_JUMP        |
     *   [end]              |
     *     ... <------------+
     */

    int loop_jump = condition_start - block->count - 1;  // Negative.
    emit_immediate_s16(compiler, T_OP_JUMP, loop_jump);
    add_jump(block, condition_start);

    int exit_jump = block->count - body_start - 1;  // Positive.
    patch_jump(compiler, body_start, exit_jump);
    add_jump(block, block->count);

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
    start_view(current, start, compiler->temp);
    for (const char *c = start; *c != '"'; ++c) {
        if (*c == '\\') {
            int escaped = escape_character(c[1]);
            if (escaped == -1) {
                parse_error(compiler, "invalid escape sequence '\\%c'.\n", c[1]);
                exit(1);
            }
            current = store_char(current, escaped, compiler->temp);
            ++c;  // Consume the escaped character.
        }
        else {
            if (!SB_IS_VIEW(current)) {
                current = start_view(current, c, compiler->temp);
            }
            ++current->view.length;
        }
    }
    uint32_t index = write_string(compiler->module, &builder);
    emit_immediate_uv(compiler, T_OP_LOAD_STRING8, index);
    clear_region(compiler->temp);
}

static void compile_character(struct compiler *compiler) {
    struct string_view value = peek_previous(compiler).value;
    uint32_t codepoint = 0;
    const char *c = value.start + 1;
    if (*c != '\\') {
        // Normal character.
        codepoint = decode_utf8(c, &c);
        if (codepoint == UTF8_DECODE_ERROR) {
            parse_error(compiler, "unable to decode UTF-8 character.\n");
        }
    }
    else {
        int escaped = escape_character(*++c);
        ++c;
        if (escaped == -1) {
            parse_error(compiler, "invalid escape sequence '\\%c'.\n", value.start[2]);
            exit(1);
        }
        codepoint = escaped;
    }
    if (*c != '\'') {
        parse_error(compiler, "character literal contains multiple characters.\n");
        exit(1);
    }
    emit_immediate_uv(compiler, T_OP_PUSH_CHAR8, codepoint);
}

static type_index parse_type(struct compiler *compiler, struct token *token) {
    switch (token->type) {
    case TOKEN_BYTE: return TYPE_BYTE;
    case TOKEN_CHAR: return TYPE_CHAR;
    case TOKEN_CHAR16: return TYPE_CHAR16;
    case TOKEN_CHAR32: return TYPE_CHAR32;
    case TOKEN_F32: return TYPE_F32;
    case TOKEN_F64: return TYPE_F64;
    case TOKEN_INT: return TYPE_INT;
    case TOKEN_PTR: return TYPE_PTR;
    case TOKEN_S8: return TYPE_S8;
    case TOKEN_S16: return TYPE_S16;
    case TOKEN_S32: return TYPE_S32;
    case TOKEN_U8: return TYPE_U8;
    case TOKEN_U16: return TYPE_U16;
    case TOKEN_U32: return TYPE_U32;
    case TOKEN_WORD: return TYPE_WORD;
    case TOKEN_STRING: return TYPE_STRING;
    case TOKEN_SYMBOL: {
        struct symbol *symbol = lookup_symbol(&compiler->symbols, &token->value);
        if (symbol == NULL) {
            compile_error(compiler, "Unknown symbol '%"PRI_SV"'", SV_FMT(token->value));
            exit(1);
        }
        switch (symbol->type) {
        case SYM_COMP: return symbol->comp.index;
        case SYM_PACK: return symbol->pack.index;
        default: break;
        }
        break;
    }
    default: break;
    }
    parse_error(compiler, "Invalid type: '%"PRI_SV"'", SV_FMT(token->value));
    exit(1);
}

static void compile_pack(struct compiler *compiler) {
    expect_consume(compiler, TOKEN_SYMBOL, "Expect pack name after `pack`.");
    struct string_view name = peek_previous(compiler).value;
    type_index index = new_type(&compiler->module->types, &name);
    struct symbol symbol = {
        .name = name,
        .type = SYM_PACK,
        .pack.index = index,
    };
    insert_symbol(&compiler->symbols, &symbol);
    expect_consume(compiler, TOKEN_DEF, "Expect `def` after pack name.");
    struct type_info info = {.kind = KIND_PACK};
    int field_count = 0;
    int size = 0;
    for (; field_count < 8 && !check(compiler, TOKEN_END); ++field_count) {
        if (is_at_end(compiler)) {
            parse_error(compiler, "unexpected EOF parsing pack definition.\n");
            exit(1);
        }
        expect_consume(compiler, TOKEN_SYMBOL, "Expect field name.");
        struct symbol field = {
            .name = peek_previous(compiler).value,
            .type = SYM_PACK_FIELD,
            .pack_field = {
                .pack = index,
                .field_offset = field_count,
            },
        };
        insert_symbol(&compiler->symbols, &field);
        expect_consume(compiler, TOKEN_RIGHT_ARROW, "Expect `->` after field name.");
        struct token field_token = advance(compiler);
        type_index field_type = parse_type(compiler, &field_token);
        if (field_type == TYPE_ERROR) {
            parse_error(compiler, "unknown type.");
            exit(1);
        }
        int field_size = type_size(&compiler->module->types, field_type);
        if (field_size > 8) {
            parse_error(compiler, "pack field too large.");
            exit(1);
        }
        info.pack.fields[field_count] = field_type;
        size += field_size;
        if (size > 8) {
            parse_error(compiler, "pack too large.");
            exit(1);
        }
    }
    expect_consume(compiler, TOKEN_END, "Expect `end` after pack definition.");
    info.pack.field_count = field_count;
    info.pack.size = size;
    init_type(&compiler->module->types, index, &info);
}

static void compile_comp(struct compiler *compiler) {
    expect_consume(compiler, TOKEN_SYMBOL, "Expect symbol after `comp`.");
    struct string_view name = peek_previous(compiler).value;
    type_index index = new_type(&compiler->module->types, &name);
    struct symbol symbol = {
        .name = name,
        .type = SYM_COMP,
        .comp.index = index,
    };
    insert_symbol(&compiler->symbols, &symbol);
    expect_consume(compiler, TOKEN_DEF, "Expect `def` after comp name.");
    struct type_info info = {.kind = KIND_COMP};
    int field_count = 0;
    int word_count = 0;
    struct type_node {
        struct type_node *next;
        type_index type;
        int offset;
    } *head = NULL;
    while (!check(compiler, TOKEN_END)) {
        if (is_at_end(compiler)) {
            parse_error(compiler, "unexpected EOF parsing comp definition.");
            exit(1);
        }
        expect_consume(compiler, TOKEN_SYMBOL, "Expect field name.");
        struct symbol field = {
            .name = peek_previous(compiler).value,
            .type = SYM_COMP_FIELD,
            .comp_field = {
                .comp = index,
                .field_offset = field_count,
            },
        };
        insert_symbol(&compiler->symbols, &field);
        expect_consume(compiler, TOKEN_RIGHT_ARROW, "Expect `->` after field name.");
        struct token field_token = advance(compiler);
        type_index type = parse_type(compiler, &field_token);
        int field_word_count = 1;
        if (is_comp(&compiler->module->types, type)) {
            // NOTE: this includes strings.
            const struct type_info *info = lookup_type(&compiler->module->types, type);
            assert(info != NULL);
            assert(info->kind == KIND_COMP);
            field_word_count = info->comp.word_count;
        }
        if (type == TYPE_ERROR) {
            parse_error(compiler, "unknown type while parsing comp definition.\n");
            exit(1);
        }
        struct type_node *next = head;
        head = region_alloc(compiler->temp, sizeof *head);
        CHECK_ALLOCATION(head);
        head->next = next;
        head->type = type;
        head->offset = word_count;
        ++field_count;
        word_count += field_word_count;
    }
    expect_consume(compiler, TOKEN_END, "Expect `end` after comp definition");
    info.comp.field_count = field_count;
    info.comp.word_count = word_count;
    info.comp.fields = alloc_extra(&compiler->module->types,
                                   field_count * sizeof *info.comp.fields);
    info.comp.offsets = alloc_extra(&compiler->module->types,
                                    field_count * sizeof *info.comp.offsets);
    struct type_node *current = head;
    for (int i = field_count - 1; i >= 0; --i) {
        assert(current != NULL);  // this should be true, but it doesn't hurt to assert.
        info.comp.fields[i] = current->type;
        info.comp.offsets[i] = word_count - current->offset;
        current = current->next;
    }
    init_type(&compiler->module->types, index, &info);
    clear_region(compiler->temp);
}

static void compile_as_conversion(struct compiler *compiler) {
    struct token type_token = advance(compiler);
    type_index type = parse_type(compiler, &type_token);
    static enum t_opcode conversions[] = {
        [TYPE_WORD]   = T_OP_AS_WORD,
        [TYPE_BYTE]   = T_OP_AS_BYTE,
        [TYPE_PTR]    = T_OP_AS_PTR,
        [TYPE_INT]    = T_OP_AS_INT,
        [TYPE_U8]     = T_OP_AS_U8,
        [TYPE_U16]    = T_OP_AS_U16,
        [TYPE_U32]    = T_OP_AS_U32,
        [TYPE_S8]     = T_OP_AS_S8,
        [TYPE_S16]    = T_OP_AS_S16,
        [TYPE_S32]    = T_OP_AS_S32,
        [TYPE_F32]    = T_OP_AS_F32,
        [TYPE_F64]    = T_OP_AS_F64,
        [TYPE_CHAR]   = T_OP_AS_CHAR,
        [TYPE_CHAR16] = T_OP_AS_CHAR16,
        [TYPE_CHAR32] = T_OP_AS_CHAR32,
    };
    static_assert(sizeof conversions == sizeof(enum t_opcode[SIMPLE_TYPE_COUNT]));
    assert(type != TYPE_ERROR);
    if (IS_SIMPLE_TYPE(type)) {
        emit_simple(compiler, conversions[type]);
    }
    else {
        // Custom type.
        compile_error(compiler, "Conversion to non-simple types not supported yet");
        exit(1);
    }
}

static void compile_to_conversion(struct compiler *compiler) {
    struct token type_token = advance(compiler);
    type_index type = parse_type(compiler, &type_token);
    static enum t_opcode conversions[] = {
        [TYPE_WORD] = T_OP_TO_WORD,
        [TYPE_BYTE] = T_OP_TO_BYTE,
        [TYPE_PTR]  = T_OP_TO_PTR,
        [TYPE_INT]  = T_OP_TO_INT,
        [TYPE_U8]   = T_OP_TO_U8,
        [TYPE_U16]  = T_OP_TO_U16,
        [TYPE_U32]  = T_OP_TO_U32,
        [TYPE_S8]   = T_OP_TO_S8,
        [TYPE_S16]  = T_OP_TO_S16,
        [TYPE_S32]  = T_OP_TO_S32,
        [TYPE_F32]  = T_OP_TO_F32,
        [TYPE_F64]  = T_OP_TO_F64,
        [TYPE_CHAR] = T_OP_TO_CHAR,
        [TYPE_CHAR16] = T_OP_TO_CHAR16,
        [TYPE_CHAR32] = T_OP_TO_CHAR32,
    };
    static_assert(sizeof conversions == sizeof(enum t_opcode[SIMPLE_TYPE_COUNT]));
    assert(type != TYPE_ERROR);
    if (IS_SIMPLE_TYPE(type)) {
        emit_simple(compiler, conversions[type]);
    }
    else {
        // Custom type.
        compile_error(compiler, "Conversion to non-simple types not supported yet");
        exit(1);
    }
}

static void compile_var(struct compiler *compiler) {
    while (!is_at_end(compiler) && !check(compiler, TOKEN_END)) {
        expect_consume(compiler, TOKEN_SYMBOL, "Expect variable name.");
        struct string_view name = peek_previous(compiler).value;
        expect_consume(compiler, TOKEN_RIGHT_ARROW, "Expect `<-` after variable name.");
        struct token type_token = advance(compiler);
        type_index type = parse_type(compiler, &type_token);
        if (type == TYPE_ERROR) {
            parse_error(compiler, "Invalid type '%"PRI_SV"'.", SV_FMT(type_token.value));
            exit(1);
        }
        int var_index = add_local(compiler->function, type);
        struct symbol symbol = {
            .name = name,
            .type = SYM_VAR,
            .var.var = var_index,
        };
        insert_symbol(&compiler->symbols, &symbol);
    }
    expect_consume(compiler, TOKEN_END, "Expect `end` after `var` block.");
}

static void compile_assignment(struct compiler *compiler) {
    expect_consume(compiler, TOKEN_SYMBOL, "Expect symbol after `<-`");
    struct string_view name = peek_previous(compiler).value;
    struct symbol *symbol = lookup_symbol(&compiler->symbols, &name);
    if (symbol == NULL) {
        compile_error(compiler, "Unknown symbol '"PRI_SV"'", SV_FMT(name));
        exit(1);
    }
    switch (symbol->type) {
    case SYM_PACK_FIELD:
        emit_pack_field(compiler, T_OP_PACK_FIELD_SET8, symbol->pack_field.pack,
                        symbol->pack_field.field_offset);
        break;
    case SYM_COMP_FIELD:
        emit_comp_field(compiler, T_OP_COMP_FIELD_SET8, symbol->comp_field.comp,
                        symbol->comp_field.field_offset);
        break;
    case SYM_VAR:
        emit_immediate_u16(compiler, T_OP_LOCAL_SET, symbol->var.var);
        break;
    default:
        parse_error(compiler, "Incorrect symbol type for `<-`.");
        exit(1);
    }
}

static void compile_function(struct compiler *compiler) {
    /* `func` params... name [`->` rets...] `def` body... `end` */
    struct ir_block *block = &compiler->function->t_code;
    struct token prev = advance(compiler);
    if (prev.type == TOKEN_RIGHT_ARROW || prev.type == TOKEN_DEF || is_at_end(compiler)) {
        parse_error(compiler, "Expect function name.");
        exit(1);
    }
    struct type_list {
        struct type_list *next;
        type_index type;
    };
    struct type_list *param_list = NULL;
    struct type_list *ret_list = NULL;
    int param_count = 0;
    int ret_count = 0;
    while (!check(compiler, TOKEN_RIGHT_ARROW) && !check(compiler, TOKEN_DEF)) {
        // Parameter types.
        type_index param = parse_type(compiler, &prev);
        if (param == TYPE_ERROR) {
            parse_error(compiler, "Expect paramater type.");
            exit(1);
        }
        prev = advance(compiler);
        struct type_list *node = region_alloc(compiler->temp, sizeof *node);
        CHECK_ALLOCATION(node);
        node->next = param_list;
        node->type = param;
        param_list = node;
        ++param_count;
    }
    if (prev.type != TOKEN_SYMBOL) {
        parse_error(compiler, "Expect function name after parameter types.");
        exit(1);
    }
    struct string_view name = prev.value;
    if (match(compiler, TOKEN_RIGHT_ARROW)) {
        // Return values.
        while (!check(compiler, TOKEN_DEF)) {
            struct token token = advance(compiler);
            type_index ret = parse_type(compiler, &token);
            if (ret == TYPE_ERROR) {
                parse_error(compiler, "Expect return type.");
                exit(1);
            }
            struct type_list *node = region_alloc(compiler->temp, sizeof *node);
            CHECK_ALLOCATION(node);
            node->next = ret_list;
            node->type = ret;
            ret_list = node;
            ++ret_count;
        }
    }
    type_index *params = region_calloc(compiler->module->functions.region,
                                       param_count, sizeof *params);
    if (param_count != 0) CHECK_ALLOCATION(params);
    for (int i = param_count - 1; i >= 0; --i) {
        params[i] = param_list->type;
        param_list = param_list->next;  // This isn't a memory leak (because regions).
    }
    type_index *rets = region_calloc(compiler->module->functions.region,
                                     ret_count, sizeof *rets);
    if (ret_count != 0) CHECK_ALLOCATION(rets);
    for (int i = ret_count - 1; i >= 0; --i) {
        rets[i] = ret_list->type;
        ret_list = ret_list->next;  // Again, no memory leak because regions.
    }
    clear_region(compiler->temp);
    int index = add_function(&compiler->module->functions, param_count, ret_count, params, rets);
    struct symbol symbol = {
        .name = name,
        .type = SYM_FUNCTION,
        .function.index = index
    };
    insert_symbol(&compiler->symbols, &symbol);
    expect_consume(compiler, TOKEN_DEF, "Expect `def` after function signature.");
    struct function *previous_function = compiler->function;
    compiler->function = get_function(&compiler->module->functions, index);
    compile_expr(compiler);  // Body.
    if (!check_last_instruction(compiler, T_OP_RET)
        || is_jump_dest(block, block->count)) {
        // Implicit return at end of function. Only emit if we need it.
        emit_simple(compiler, T_OP_RET);
    }
    compiler->function = previous_function;
    expect_consume(compiler, TOKEN_END, "Expect `end` after function body.");
}

static void compile_loop_var_symbol(struct compiler *compiler, struct symbol *symbol) {
    int level = symbol->loop_var.level;
    if (level > compiler->for_loop_level) {
        compile_error(compiler, "loop variable '"PRI_SV"' referenced outside defining loop.\n",
                      SV_FMT(symbol->name));
        exit(1);
    }
    uint16_t offset = compiler->for_loop_level - level;  // Offset from top of aux.
    emit_immediate_u16(compiler, T_OP_GET_LOOP_VAR, offset);
}

static void compile_pack_symbol(struct compiler *compiler, struct symbol *symbol) {
    emit_immediate_sv(compiler, T_OP_PACK8, symbol->pack.index);
}

static void compile_comp_symbol(struct compiler *compiler, struct symbol *symbol) {
    emit_immediate_sv(compiler, T_OP_COMP8, symbol->comp.index);
}

static void compile_pack_field_get_symbol(struct compiler *compiler, struct symbol *symbol) {
    emit_pack_field(compiler, T_OP_PACK_FIELD_GET8,
                    symbol->pack_field.pack, symbol->pack_field.field_offset);
}

static void compile_comp_field_get_symbol(struct compiler *compiler, struct symbol *symbol) {
    int offset = symbol->comp_field.field_offset;
    type_index comp = symbol->comp_field.comp;
    emit_comp_field(compiler, T_OP_COMP_FIELD_GET8, comp, offset);
}

static void compile_function_symbol(struct compiler *compiler, struct symbol *symbol) {
    int index = symbol->function.index;
    emit_immediate_uv(compiler, T_OP_CALL8, index);
}

static void compile_var_symbol(struct compiler *compiler, struct symbol *symbol) {
    int var_index = symbol->var.var;
    emit_immediate_u16(compiler, T_OP_LOCAL_GET, var_index);
}

static void compile_symbol(struct compiler *compiler) {
    struct string_view symbol_text = peek_previous(compiler).value;
    struct symbol *symbol = lookup_symbol(&compiler->symbols, &symbol_text);
    if (symbol == NULL) {
        compile_error(compiler, "unknown symbol '%"PRI_SV"'.\n", SV_FMT(symbol_text));
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
    case SYM_PACK_FIELD:
        compile_pack_field_get_symbol(compiler, symbol);
        break;
    case SYM_COMP_FIELD:
        compile_comp_field_get_symbol(compiler, symbol);
        break;
    case SYM_FUNCTION:
        compile_function_symbol(compiler, symbol);
        break;
    case SYM_VAR:
        compile_var_symbol(compiler, symbol);
        break;
    }
}

static bool compile_simple(struct compiler *compiler) {
    switch (peek(compiler).type) {
    case TOKEN_AND:
        emit_simple(compiler, T_OP_AND);
        break;
    case TOKEN_DECOMP:
        emit_simple(compiler, T_OP_DECOMP);
        break;
    case TOKEN_DEREF:
        emit_simple(compiler, T_OP_DEREF);
        break;
    case TOKEN_DIVMOD:
        emit_simple(compiler, T_OP_DIVMOD);
        break;
    case TOKEN_DUPE:
        emit_simple(compiler, T_OP_DUPE);
        break;
    case TOKEN_EDIVMOD:
        emit_simple(compiler, T_OP_EDIVMOD);
        break;
    case TOKEN_EQUALS:
        emit_simple(compiler, T_OP_EQUALS);
        break;
    case TOKEN_EXIT:
        emit_simple(compiler, T_OP_EXIT);
        break;
    case TOKEN_GREATER_EQUALS:
        emit_simple(compiler, T_OP_GREATER_EQUALS);
        break;
    case TOKEN_GREATER_THAN:
        emit_simple(compiler, T_OP_GREATER_THAN);
        break;
    case TOKEN_IDIVMOD:
        emit_simple(compiler, T_OP_IDIVMOD);
        break;
    case TOKEN_LESS_EQUALS:
        emit_simple(compiler, T_OP_LESS_EQUALS);
        break;
    case TOKEN_LESS_THAN:
        emit_simple(compiler, T_OP_LESS_THAN);
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
    case TOKEN_OVER:
        emit_simple(compiler, T_OP_OVER);
        break;
    case TOKEN_PERCENT:
        emit_simple(compiler, T_OP_DIVMOD);
        emit_simple(compiler, T_OP_SWAP);
        emit_simple(compiler, T_OP_POP);
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
    case TOKEN_RET:
        emit_simple(compiler, T_OP_RET);
        break;
    case TOKEN_ROT:
        emit_simple(compiler, T_OP_ROT);
        break;
    case TOKEN_SLASH:
        emit_simple(compiler, T_OP_DIV);
        break;
    case TOKEN_SLASH_EQUALS:
        emit_simple(compiler, T_OP_NOT_EQUALS);
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
        if (match(compiler, TOKEN_AS)) {
            compile_as_conversion(compiler);
        }
        else if (match(compiler, TOKEN_CHAR_LIT)) {
            compile_character(compiler);
        }
        else if (match(compiler, TOKEN_COMP)) {
            compile_comp(compiler);
        }
        else if (match(compiler, TOKEN_FLOAT_LIT)) {
            compile_floating_point(compiler);
        }
        else if (match(compiler, TOKEN_FOR)) {
            compile_for_loop(compiler);
        }
        else if (match(compiler, TOKEN_FUNC)) {
            compile_function(compiler);
        }
        else if (match(compiler, TOKEN_IF)) {
            compile_conditional(compiler);
        }
        else if (match(compiler, TOKEN_INT_LIT)) {
            compile_integer(compiler);
        }
        else if (match(compiler, TOKEN_LEFT_ARROW)) {
            compile_assignment(compiler);
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
        else if (match(compiler, TOKEN_TO)) {
            compile_to_conversion(compiler);
        }
        else if (match(compiler, TOKEN_VAR)) {
            compile_var(compiler);
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

void compile(const char *src, struct module *module) {
    struct compiler compiler;
    init_compiler(&compiler, src, module);
    init_builtins(&compiler.symbols);
    assert(module->functions.count == 0);  // We assume that the function table is empty.
    add_function(&module->functions, 0, 0, NULL, NULL);  // Main/script function.
    compiler.function = get_function(&module->functions, 0);
    compile_expr(&compiler);
    emit_simple(&compiler, T_OP_RET);  // Return from main function.
    free_compiler(&compiler);
}
