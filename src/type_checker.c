#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "function.h"
#include "location.h"
#include "memory.h"
#include "module.h"
#include "type_checker.h"
#include "type_punning.h"

#define TEMP_REGION_SIZE (512 * 1024)

struct arithm_conv {
    type_index result_type;
    enum w_opcode lhs_conv;
    enum w_opcode rhs_conv;
    enum w_opcode result_conv;
};

struct float_conv {
    type_index result_type;
    enum w_opcode lhs_conv;
    enum w_opcode rhs_conv;
};

static struct string_view type_array_to_sv(struct type_checker *checker,
                                           int count, type_index types[count]) {
    struct string_builder builder = {0};
    struct string_builder *bp = &builder;
    bp = store_char(bp, '[', checker->temp);
    if (count > 0) {
        struct string_view name = type_name(checker->types, types[0]);
        bp = store_view(bp, &name, checker->temp);
        for (int i = 1; i < count; ++i) {
            bp = store_char(bp, ' ', checker->temp);
            name = type_name(checker->types, types[i]);
            bp = store_view(bp, &name, checker->temp);
        }
    }
    bp = store_char(bp, ']', checker->temp);
    return build_string_in_region(&builder, checker->temp);
}

static void type_error(struct type_checker *checker, const char *restrict message, ...) {
    checker->had_error = true;
    ir_error(checker->module->filename, checker->in_block, checker->ip, "Type error: ");
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, ".\n");
}

enum jmpdir {JMP_DEST, JMP_SRC};  // Whether the CURRENT type stack is at the jump source or destination.

static void inconsistent_jump_error(struct type_checker *checker, int state_index, enum jmpdir direction) {
    struct tstack_state *state = checker->states.states[state_index];
    struct type_stack *tstack = checker->tstack;
    struct string_view src_sv = {0};
    struct string_view dest_sv = {0};
    switch (direction) {
    case JMP_DEST:
        src_sv = type_array_to_sv(checker, state->count, state->types);
        dest_sv = type_array_to_sv(checker, TSTACK_COUNT(tstack), tstack->types);
        break;
    case JMP_SRC:
        src_sv = type_array_to_sv(checker, TSTACK_COUNT(tstack), tstack->types);
        dest_sv = type_array_to_sv(checker, state->count, state->types);
        break;
    }
    type_error(checker, "inconsistent stack after jump instruction: %"PRI_SV" -> %"PRI_SV,
               SV_FMT(src_sv), SV_FMT(dest_sv));
}

static void expect_types_equal(struct type_checker *checker, type_index expected_type,
                          type_index actual_type) {
    if (actual_type != expected_type) {
        struct string_view expected_sv = type_name(checker->types, expected_type);
        struct string_view actual_sv = type_name(checker->types, actual_type);
        type_error(checker, "expected type %"PRI_SV" but got type %"PRI_SV,
                   SV_FMT(expected_sv), SV_FMT(actual_sv));
        exit(1);
    }
}

static const struct type_info *expect_keep_kind(struct type_checker *checker,
                                                enum type_kind kind) {
    type_index type = ts_peek(checker);
    const struct type_info *info = lookup_type(checker->types, type);
    if (info == NULL) {
        type_error(checker, "unknown type");
        assert(0 && "Invalid state");
    }
    if (info->kind != kind) {
        type_error(checker, "expected a '%s' type but got type '%"PRI_SV"' instead",
                   kind_name(kind), SV_FMT(type_name(checker->types, type)));
        exit(1);
    }
    return info;
}

static const struct type_info *expect_kind(struct type_checker *checker, enum type_kind kind) {
    const struct type_info *info = expect_keep_kind(checker, kind);
    ts_pop(checker);
    return info;
}

static void expect_keep_type(struct type_checker *checker, type_index expected_type) {
    expect_types_equal(checker, expected_type, ts_peek(checker));
}

static void expect_type(struct type_checker *checker, type_index expected_type) {
    expect_types_equal(checker, expected_type, ts_pop(checker));
}

static void expect_types(struct type_checker *checker,
                         int count, type_index expected_types[count]) {
    assert(count >= 0);
    if (TSTACK_COUNT(checker->tstack) < count) {
        struct string_view expected_sv = type_array_to_sv(checker, count, expected_types);
        struct string_view actual_sv = type_array_to_sv(checker, TSTACK_COUNT(checker->tstack),
                                                        checker->tstack->types);
        type_error(checker, "expected types %"PRI_SV", but got types %"PRI_SV,
                   SV_FMT(expected_sv),
                   SV_FMT(actual_sv));
        checker->tstack->top = checker->tstack->types;
    }
    else {
        if (!array_eq(count, &checker->tstack->top[-count],
                      count, expected_types, sizeof(type_index))) {
            struct string_view expected_sv = type_array_to_sv(checker, count, expected_types);
            struct string_view actual_sv = type_array_to_sv(checker, count,
                                                            &checker->tstack->top[-count]);
            type_error(checker, "expected types %"PRI_SV", but got types %"PRI_SV,
                       SV_FMT(expected_sv),
                       SV_FMT(actual_sv));
        }
        checker->tstack->top -= count;
    }
}

void reset_type_stack(struct type_stack *tstack) {
    tstack->top = tstack->types;
}

static void init_type_checker_states(struct type_checker_states *states) {
    states->region = new_region(TYPE_STACK_STATES_REGION_SIZE);
    CHECK_ALLOCATION(states->region);
}

static void reset_type_checker_states(struct type_checker_states *states,
                                      struct jump_info_table *jumps) {
    clear_region(states->region);
    states->size = jumps->count;
    if (states->size != 0) {
        states->states    = region_calloc(states->region, states->size, sizeof *states->states);
        CHECK_ALLOCATION(states->states);
        states->ips       = region_calloc(states->region, states->size, sizeof *states->ips);
        CHECK_ALLOCATION(states->ips);
        states->wir_dests = region_calloc(states->region, states->size, sizeof *states->wir_dests);
        CHECK_ALLOCATION(states->wir_dests);
        states->wir_srcs  = region_calloc(states->region, states->size, sizeof *states->wir_srcs);
        CHECK_ALLOCATION(states->wir_srcs);
        memcpy(states->ips, jumps->items, states->size * sizeof states->ips[0]);
    }
    else {
        states->states = NULL;
        states->ips = NULL;
        states->wir_dests = NULL;
        states->wir_srcs = NULL;
    }
}

static void free_type_checker_states(struct type_checker_states *states) {
    kill_region(states->region);
    // Zero out all fields.
    *states = (struct type_checker_states) {0};
}

void init_type_checker(struct type_checker *checker, struct module *module) {
    init_type_checker_states(&checker->states);
    // The ir_blocks will be set later.
    checker->in_block = NULL;
    checker->out_block = NULL;
    checker->module = module;
    checker->types = &module->types;  // The type table is used a lot so it gets its own field.
    checker->tstack = malloc(sizeof *checker->tstack);
    CHECK_ALLOCATION(checker->tstack);
    checker->ip = 0;
    checker->current_function = 0;
    checker->had_error = false;
    reset_type_stack(checker->tstack);
    checker->temp = new_region(TEMP_REGION_SIZE);
    CHECK_ALLOCATION(checker->temp);
}

void free_type_checker(struct type_checker *checker) {
    free_type_checker_states(&checker->states);
    free(checker->tstack);
    checker->tstack = NULL;
}


/* This table tells one how to convert the lhs and rhs of an integer arithmetic conversion.
 * For floating-point conversions, see the 'float_conversions' table below. All arithmetic
 * operations happen on the level of word-sized (64-bit) operations, so the lhs and rhs must
 * first be converted to signed/unsigned 64-bit integers. The overall result type depends on
 * Bude's conversion rules and may be narrower than word size, so the final column tells how
 * to convert back to the narrower type. For all conversion instructions, if no conversion is
 * to occur, a NOP instruction is listed instead.
 *   The conversion rules depend on the widths and signednesses of the two operands. If the
 * operands are of the same type (same width and signedness), then the result is also that
 * type. If the types are of different widths, then the result type is the wider of the two
 * operand types. If the types are of the same width but different signedness, then the
 * result is the unsigned type of that width.
 *   Some operations require an integral operand to be 'promoted'. This is where the type is
 * converted to the 64-bit type with the same signedness. The operation to accomplish this can
 * be found by taking that type as rhs with the type 'int' as the lhs.
 */
static struct arithm_conv arithmetic_conversions[SIMPLE_TYPE_COUNT][SIMPLE_TYPE_COUNT] = {
    /* lhs_type  rhs_type    result_type lhs_conv    rhs_conv   result_conv */
    [TYPE_WORD][TYPE_WORD] = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_WORD][TYPE_BYTE] = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_WORD][TYPE_INT]  = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_BYTE][TYPE_WORD] = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_BYTE][TYPE_BYTE] = {TYPE_BYTE, W_OP_NOP,   W_OP_NOP,   W_OP_ZX8},
    [TYPE_BYTE][TYPE_INT]  = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_INT][TYPE_WORD]  = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_INT][TYPE_BYTE]  = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_INT][TYPE_INT]   = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_BOOL][TYPE_WORD] = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},

    /* Fixed unsigned types. */
    [TYPE_WORD][TYPE_U8]   = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_WORD][TYPE_U16]  = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_WORD][TYPE_U32]  = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U8][TYPE_WORD]   = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U16][TYPE_WORD]  = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U32][TYPE_WORD]  = {TYPE_WORD, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},

    [TYPE_BYTE][TYPE_U8]   = {TYPE_BYTE, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_BYTE][TYPE_U16]  = {TYPE_U16,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_BYTE][TYPE_U32]  = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U8][TYPE_BYTE]   = {TYPE_BYTE, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U16][TYPE_BYTE]  = {TYPE_U16,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U32][TYPE_BYTE]  = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},

    [TYPE_INT][TYPE_U8]    = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_INT][TYPE_U16]   = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_INT][TYPE_U32]   = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U8][TYPE_INT]    = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U16][TYPE_INT]   = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_U32][TYPE_INT]   = {TYPE_INT,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},

    [TYPE_U8][TYPE_U8]     = {TYPE_U8,   W_OP_NOP,   W_OP_NOP,   W_OP_ZX8},
    [TYPE_U8][TYPE_U16]    = {TYPE_U16,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX16},
    [TYPE_U8][TYPE_U32]    = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX32},
    [TYPE_U16][TYPE_U8]    = {TYPE_U16,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX16},
    [TYPE_U16][TYPE_U16]   = {TYPE_U16,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX16},
    [TYPE_U16][TYPE_U32]   = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX32},
    [TYPE_U32][TYPE_U8]    = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX32},
    [TYPE_U32][TYPE_U16]   = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX32},
    [TYPE_U32][TYPE_U32]   = {TYPE_U32,  W_OP_NOP,   W_OP_NOP,   W_OP_ZX32},

    [TYPE_U8][TYPE_S8]     = {TYPE_U8,   W_OP_NOP,   W_OP_SX8,   W_OP_ZX8},
    [TYPE_U8][TYPE_S16]    = {TYPE_S16,  W_OP_NOP,   W_OP_SX16,  W_OP_ZX16},
    [TYPE_U8][TYPE_S32]    = {TYPE_S32,  W_OP_NOP,   W_OP_SX32,  W_OP_ZX32},
    [TYPE_U16][TYPE_S8]    = {TYPE_U16,  W_OP_NOP,   W_OP_SX8,   W_OP_ZX16},
    [TYPE_U16][TYPE_S16]   = {TYPE_U16,  W_OP_NOP,   W_OP_SX16,  W_OP_ZX16},
    [TYPE_U16][TYPE_S32]   = {TYPE_S32,  W_OP_NOP,   W_OP_SX32,  W_OP_ZX32},
    [TYPE_U32][TYPE_S8]    = {TYPE_U32,  W_OP_NOP,   W_OP_SX8,   W_OP_ZX32},
    [TYPE_U32][TYPE_S16]   = {TYPE_U32,  W_OP_NOP,   W_OP_SX16,  W_OP_ZX32},
    [TYPE_U32][TYPE_S32]   = {TYPE_U32,  W_OP_NOP,   W_OP_SX32,  W_OP_ZX32},

    /* Fixed signed types. */
    [TYPE_WORD][TYPE_S8]   = {TYPE_WORD, W_OP_NOP,   W_OP_SX8,   W_OP_NOP},
    [TYPE_WORD][TYPE_S16]  = {TYPE_WORD, W_OP_NOP,   W_OP_SX16,  W_OP_NOP},
    [TYPE_WORD][TYPE_S32]  = {TYPE_WORD, W_OP_NOP,   W_OP_SX16,  W_OP_NOP},
    [TYPE_S8][TYPE_WORD]   = {TYPE_WORD, W_OP_SX8L,  W_OP_NOP,   W_OP_NOP},
    [TYPE_S16][TYPE_WORD]  = {TYPE_WORD, W_OP_SX16L, W_OP_NOP,   W_OP_NOP},
    [TYPE_S32][TYPE_WORD]  = {TYPE_WORD, W_OP_SX32L, W_OP_NOP,   W_OP_NOP},

    [TYPE_BYTE][TYPE_S8]   = {TYPE_BYTE, W_OP_NOP,   W_OP_NOP,   W_OP_ZX8},
    [TYPE_BYTE][TYPE_S16]  = {TYPE_S16,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_BYTE][TYPE_S32]  = {TYPE_S32,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_S8][TYPE_BYTE]   = {TYPE_BYTE, W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_S16][TYPE_BYTE]  = {TYPE_S16,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},
    [TYPE_S32][TYPE_BYTE]  = {TYPE_S32,  W_OP_NOP,   W_OP_NOP,   W_OP_NOP},

    [TYPE_INT][TYPE_S8]    = {TYPE_INT,  W_OP_NOP,   W_OP_SX8,   W_OP_NOP},
    [TYPE_INT][TYPE_S16]   = {TYPE_INT,  W_OP_NOP,   W_OP_SX16,  W_OP_NOP},
    [TYPE_INT][TYPE_S32]   = {TYPE_INT,  W_OP_NOP,   W_OP_SX32,  W_OP_NOP},
    [TYPE_S8][TYPE_INT]    = {TYPE_INT,  W_OP_SX8L,  W_OP_NOP,   W_OP_NOP},
    [TYPE_S16][TYPE_INT]   = {TYPE_INT,  W_OP_SX16L, W_OP_NOP,   W_OP_NOP},
    [TYPE_S32][TYPE_INT]   = {TYPE_INT,  W_OP_SX32L, W_OP_NOP,   W_OP_NOP},

    [TYPE_S8][TYPE_S8]     = {TYPE_S8,   W_OP_SX8L,  W_OP_SX8,   W_OP_ZX8},
    [TYPE_S8][TYPE_S16]    = {TYPE_S16,  W_OP_SX8L,  W_OP_SX16,  W_OP_ZX16},
    [TYPE_S8][TYPE_S32]    = {TYPE_S32,  W_OP_SX8L,  W_OP_SX32,  W_OP_ZX32},
    [TYPE_S16][TYPE_S8]    = {TYPE_S16,  W_OP_SX16L, W_OP_SX8,   W_OP_ZX16},
    [TYPE_S16][TYPE_S16]   = {TYPE_S16,  W_OP_SX16L, W_OP_SX16,  W_OP_ZX16},
    [TYPE_S16][TYPE_S32]   = {TYPE_S32,  W_OP_SX16L, W_OP_SX32,  W_OP_ZX32},
    [TYPE_S32][TYPE_S8]    = {TYPE_S32,  W_OP_SX32L, W_OP_SX8,   W_OP_ZX32},
    [TYPE_S32][TYPE_S16]   = {TYPE_S32,  W_OP_SX32L, W_OP_SX16,  W_OP_ZX32},
    [TYPE_S32][TYPE_S32]   = {TYPE_S32,  W_OP_SX32L, W_OP_SX32,  W_OP_ZX32},

    [TYPE_S8][TYPE_U8]     = {TYPE_U8,   W_OP_SX8L,  W_OP_NOP,   W_OP_ZX8},
    [TYPE_S8][TYPE_U16]    = {TYPE_U16,  W_OP_SX8L,  W_OP_NOP,   W_OP_ZX16},
    [TYPE_S8][TYPE_U32]    = {TYPE_U32,  W_OP_SX8L,  W_OP_NOP,   W_OP_ZX32},
    [TYPE_S16][TYPE_U8]    = {TYPE_S16,  W_OP_SX16L, W_OP_NOP,   W_OP_ZX16},
    [TYPE_S16][TYPE_U16]   = {TYPE_U16,  W_OP_SX16L, W_OP_NOP,   W_OP_ZX16},
    [TYPE_S16][TYPE_U32]   = {TYPE_U32,  W_OP_SX16L, W_OP_NOP,   W_OP_ZX32},
    [TYPE_S32][TYPE_U8]    = {TYPE_S32,  W_OP_SX32L, W_OP_NOP,   W_OP_ZX32},
    [TYPE_S32][TYPE_U16]   = {TYPE_S32,  W_OP_SX32L, W_OP_NOP,   W_OP_ZX32},
    [TYPE_S32][TYPE_U32]   = {TYPE_U32,  W_OP_SX32L, W_OP_NOP,   W_OP_ZX32},
};

/* This table tells one how to convert the lhs and rhs for floating-point
 * arithmetic operations. For mixed integer--floating-point operations, the table
 * assumes the integer operand has already been promoted to stack-word width (see
 * the above 'arithemtic conversions' table for this operation.
 *   Values of type 'word' are reinterpreted as signed integers, which may cause
 * counter-intuitive behaviour when the MSB is set. The reason for this compromise
 * is that x86-64 SSE has no facility for converting 64-bit unsigned integers to
 * floating-point. It's not a problem for narrower unsigned types since they can be
 * safely stored in a 64-bit 'int' without loss of meaning.
 */
static struct float_conv float_conversions[SIMPLE_TYPE_COUNT][SIMPLE_TYPE_COUNT] = {
    /* Integer--floating-point conversions. */
    [TYPE_INT][TYPE_F32] = {TYPE_F32, W_OP_ICONVF32L, W_OP_NOP},
    [TYPE_INT][TYPE_F64] = {TYPE_F64, W_OP_ICONVF64L, W_OP_NOP},
    [TYPE_F32][TYPE_INT] = {TYPE_F32, W_OP_NOP,       W_OP_ICONVF32},
    [TYPE_F64][TYPE_INT] = {TYPE_F64, W_OP_NOP,       W_OP_ICONVF64},
    /* Floating-point--floating-point conversions. */
    [TYPE_F32][TYPE_F32] = {TYPE_F32, W_OP_NOP,       W_OP_NOP},
    [TYPE_F32][TYPE_F64] = {TYPE_F64, W_OP_FPROML,    W_OP_NOP},
    [TYPE_F64][TYPE_F32] = {TYPE_F64, W_OP_NOP,       W_OP_FPROM},
    [TYPE_F64][TYPE_F64] = {TYPE_F64, W_OP_NOP,       W_OP_NOP},
};

static struct arithm_conv convert(type_index lhs, type_index rhs) {
    if (IS_SIMPLE_TYPE(lhs) && IS_SIMPLE_TYPE(rhs)) {
        return arithmetic_conversions[lhs][rhs];
    }
    // Custom types are always non-arithmetic.
    return (struct arithm_conv) {0};
}

static struct float_conv convert_float(type_index lhs, type_index rhs) {
    if (IS_SIMPLE_TYPE(lhs) && IS_SIMPLE_TYPE(rhs)) {
        return float_conversions[lhs][rhs];
    }
    return (struct float_conv) {0};
}

static enum w_opcode promote(type_index type) {
    return convert(TYPE_INT, type).rhs_conv;
}

static enum w_opcode promotel(type_index type) {
    return convert(type, TYPE_INT).lhs_conv;
}

static enum w_opcode demote(type_index type) {
    return convert(type, type).result_conv;
}

static enum w_opcode promote_float(type_index type) {
    if (type == TYPE_F32) return W_OP_FPROM;
    return W_OP_NOP;
}

static enum w_opcode float_to_int(type_index type) {
    assert(is_float(type));
    return (type == TYPE_F64) ? W_OP_FCONVI64 : W_OP_FCONVI32;
}

static enum w_opcode int_to_float(type_index type) {
    assert(is_float(type));
    return convert_float(type, TYPE_INT).rhs_conv;
}

static enum w_opcode convert_float_args(type_index *lhs_type, type_index *rhs_type) {
    assert(is_float(*lhs_type) || is_float(*rhs_type));
    enum w_opcode conv_instruction = W_OP_NOP;
    if (is_integral(*lhs_type)) {
        conv_instruction = promotel(*lhs_type);
        *lhs_type = TYPE_INT;
    }
    else if (is_integral(*rhs_type)) {
        conv_instruction = promote(*rhs_type);
        *rhs_type = TYPE_INT;
    }
    return conv_instruction;
}

static enum w_opcode sign_extend(type_index type) {
    switch (type) {
    case TYPE_BYTE:
    case TYPE_U8:
    case TYPE_S8:
        return W_OP_SX8;
    case TYPE_U16:
    case TYPE_S16:
        return W_OP_SX16;
    case TYPE_U32:
    case TYPE_S32:
        return W_OP_SX32;
    default:
        return W_OP_NOP;
    }
}

static enum w_opcode decode_character(type_index type) {
    switch (type) {
    case TYPE_CHAR:   return W_OP_CHAR_8CONV32;
    case TYPE_CHAR16: return W_OP_CHAR_16CONV32;
    case TYPE_CHAR32: return W_OP_NOP;
    default: return W_OP_NOP;
    }
}

static enum w_opcode encode_character(type_index type) {
    switch (type) {
    case TYPE_CHAR:   return W_OP_CHAR_32CONV8;
    case TYPE_CHAR16: return W_OP_CHAR_32CONV16;
    case TYPE_CHAR32: return W_OP_NOP;
    default: return W_OP_NOP;
    }
}

static size_t find_state(struct type_checker_states *states, int ip) {
    int hi = states->size - 1;
    int lo = 0;
    while (hi > lo) {
        size_t mid = lo + (hi - lo) / 2;
        int state = states->ips[mid];
        if (state == ip) {
            return mid;
        }
        if (state < ip) {
            lo = mid + 1;
        }
        else {
            hi = mid - 1;
        }
    }
    return lo;
}

static bool save_state_with_index(struct type_checker *checker, size_t index) {
    struct type_checker_states *states = &checker->states;
    assert(index < (size_t)checker->in_block->jumps.count);
    if (states->states[index] != NULL) {
        // There was already a state saved there.
        return false;
    }
    size_t count = TSTACK_COUNT(checker->tstack);
    size_t tstack_size = count * sizeof(type_index);
    struct tstack_state *state = region_alloc(states->region, sizeof *state + tstack_size);
    CHECK_ALLOCATION(state);
    state->count = count;
    memcpy(state->types, checker->tstack->types, tstack_size);
    states->states[index] = state;
    return true;
}

static bool save_state_at(struct type_checker *checker, int ip) {
    size_t index = find_state(&checker->states, ip);
    assert(index < checker->states.size);
    return save_state_with_index(checker, index);
}

static bool load_state_at(struct type_checker *checker, int ip) {
    struct type_checker_states *states = &checker->states;
    size_t index = find_state(states, ip);
    struct tstack_state *state = states->states[index];
    if (state == NULL || states->ips[index] != ip) {
        return false;
    }
    memcpy(checker->tstack->types, state->types, sizeof state->types[0] * state->count);
    checker->tstack->top = &checker->tstack->types[state->count];
    return true;
}

static bool check_type_array(struct type_checker *checker, size_t count,
                             type_index types[count]) {
    if ((ptrdiff_t)count != TSTACK_COUNT(checker->tstack)) {
        return false;
    }
    if (count == 0) {
        // We allow `types` to be NULL for no types, so we cannot rely on memcmp for this case.
        return true;
    }
    assert(count > 0);
    assert(types != NULL);
    return memcmp(types, checker->tstack->types, count * sizeof *types) == 0;
}

static bool check_state_with_index(struct type_checker *checker, size_t index) {
    struct type_checker_states *states = &checker->states;
    struct tstack_state *state = states->states[index];
    if (state == NULL) {
        // Did not find state.
        return false;
    }
    return check_type_array(checker, state->count, state->types);
}

static bool check_state_at(struct type_checker *checker, int ip) {
    size_t index = find_state(&checker->states, ip);
    assert(index < checker->states.size);
    return check_state_with_index(checker, index);
}

static bool save_jump(struct type_checker *checker, int dest_offset) {
    int dest = checker->ip + dest_offset + 1;
    int wir_src = checker->out_block->count;
    struct type_checker_states *states = &checker->states;
    size_t index = find_state(states, dest);
    assert(index < (size_t)checker->in_block->jumps.count);
    struct src_list *src_node = region_alloc(states->region, sizeof *src_node);
    CHECK_ALLOCATION(src_node);
    src_node->next = states->wir_srcs[index];
    src_node->src = wir_src;
    states->wir_srcs[index] = src_node;
    if (!save_state_at(checker, dest)) {
        // If jump was already saved.
        return check_state_at(checker, dest);
    }
    return true;
}

static bool is_state_saved(struct type_checker *checker, int ip) {
    struct type_checker_states *states = &checker->states;
    size_t index = find_state(states, ip);
    // This ought to be true, since we shouldn't need to insert destinations.
    assert(index < states->size && states->ips[index] == ip);
    return states->states[index] != NULL;
}

static bool is_forward_jump_dest(struct type_checker *checker, int ip) {
    return is_jump_dest(checker->in_block, ip) && is_state_saved(checker, ip);
}

static void emit_simple(struct type_checker *checker, enum w_opcode instruction) {
    write_simple(checker->out_block, instruction, &checker->in_block->locations[checker->ip]);
}

static void emit_simple_nnop(struct type_checker *checker, enum w_opcode instruction) {
    if (instruction != W_OP_NOP) emit_simple(checker, instruction);
}

static void emit_immediate_uv(struct type_checker *checker, enum w_opcode instruction8,
                              uint64_t arg) {
    enum w_opcode instruction16 = instruction8 + 1;
    enum w_opcode instruction32 = instruction8 + 2;
    enum w_opcode instruction64 = instruction8 + 3;
    if (arg <= UINT8_MAX) {
        write_immediate_u8(checker->out_block, instruction8, arg,
                           &checker->in_block->locations[checker->ip]);
    }
    else if (arg <= UINT16_MAX) {
        write_immediate_u16(checker->out_block, instruction16, arg,
                           &checker->in_block->locations[checker->ip]);
    }
    else if (arg <= UINT32_MAX) {
        write_immediate_u32(checker->out_block, instruction32, arg,
                           &checker->in_block->locations[checker->ip]);
    }
    else if (arg <= UINT64_MAX) {
        write_immediate_u64(checker->out_block, instruction64, arg,
                           &checker->in_block->locations[checker->ip]);
    }
}

static void emit_immediate_sv(struct type_checker *checker, enum w_opcode instruction8,
                              int64_t arg) {
    emit_immediate_uv(checker, instruction8, s64_to_u64(arg));
}

static uint8_t copy_immediate_u8(struct type_checker *checker, enum w_opcode instruction) {
    uint8_t value = read_u8(checker->in_block, checker->ip + 1);
    write_immediate_u8(checker->out_block, instruction, value,
                       &checker->in_block->locations[checker->ip]);
    checker->ip += 1;
    return value;
}

static uint16_t copy_immediate_u16(struct type_checker *checker, enum w_opcode instruction) {
    uint16_t value = read_u16(checker->in_block, checker->ip + 1);
    write_immediate_u16(checker->out_block, instruction, value,
                        &checker->in_block->locations[checker->ip]);
    checker->ip += 2;
    return value;
}

static uint32_t copy_immediate_u32(struct type_checker *checker, enum w_opcode instruction) {
    uint32_t value = read_u32(checker->in_block, checker->ip + 1);
    write_immediate_u32(checker->out_block, instruction, value,
                        &checker->in_block->locations[checker->ip]);
    checker->ip += 4;
    return value;
}

static uint64_t copy_immediate_u64(struct type_checker *checker, enum w_opcode instruction) {
    uint64_t value = read_u64(checker->in_block, checker->ip + 1);
    write_immediate_u64(checker->out_block, instruction, value,
                        &checker->in_block->locations[checker->ip]);
    checker->ip += 8;
    return value;
}

static void copy_jump_instruction(struct type_checker *checker, enum w_opcode instruction) {
    int16_t jump = read_s16(checker->in_block, checker->ip + 1);
    checker->ip += 2;
    int dest = checker->ip + jump - 1;  // -1 since jumps work in a dumb way (should fix).
    size_t index = find_state(&checker->states, dest);
    assert(index < checker->states.size);
    assert(checker->states.ips[index] == dest);
    int wir_jump = 0;
    if (jump < 0) {
        // Backwards jump: destination already known.
        int wir_dest = checker->states.wir_dests[index];
        int wir_src = checker->out_block->count;
        wir_jump = wir_dest - wir_src - 1;
    }
    write_immediate_s16(checker->out_block, instruction, wir_jump,
                        &checker->in_block->locations[checker->ip]);
}

static void patch_jump(struct type_checker *checker, int ip, int jump) {
    overwrite_s16(checker->out_block, ip + 1, jump);
}

static void emit_immediate_u8(struct type_checker *checker, enum w_opcode instruction,
                              uint8_t operand) {
    write_immediate_u8(checker->out_block, instruction, operand,
                       &checker->in_block->locations[checker->ip]);
}

static void emit_immediate_u16(struct type_checker *checker, enum w_opcode instruction,
                              uint16_t operand) {
    write_immediate_u16(checker->out_block, instruction, operand,
                        &checker->in_block->locations[checker->ip]);
}

static void emit_immediate_u32(struct type_checker *checker, enum w_opcode instruction,
                              uint32_t operand) {
    write_immediate_u32(checker->out_block, instruction, operand,
                        &checker->in_block->locations[checker->ip]);
}

static void emit_immediate_s8(struct type_checker *checker, enum w_opcode instruction,
                              int8_t operand) {
    emit_immediate_u8(checker, instruction, s8_to_u8(operand));
}

static void emit_immediate_s16(struct type_checker *checker, enum w_opcode instruction,
                              int16_t operand) {
    emit_immediate_u16(checker, instruction, s16_to_u16(operand));
}

static void emit_immediate_s32(struct type_checker *checker, enum w_opcode instruction,
                              int32_t operand) {
    emit_immediate_u32(checker, instruction, s32_to_u32(operand));
}

static void emit_u8(struct type_checker *checker, uint8_t value) {
    write_u8(checker->out_block, value, &checker->in_block->locations[checker->ip]);
}

static void emit_u16(struct type_checker *checker, uint16_t value) {
    write_u16(checker->out_block, value, &checker->in_block->locations[checker->ip]);
}

static void emit_u32(struct type_checker *checker, uint32_t value) {
    write_u32(checker->out_block, value, &checker->in_block->locations[checker->ip]);
}

static void emit_s8(struct type_checker *checker, int8_t value) {
    emit_u8(checker, s8_to_u8(value));
}

static void emit_s16(struct type_checker *checker, int16_t value) {
    emit_u16(checker, s16_to_u16(value));
}

static void emit_s32(struct type_checker *checker, int32_t value) {
    emit_u32(checker, s32_to_u32(value));
}

static type_index emit_divmod_instruction(struct type_checker *checker,
                                    type_index lhs_type, type_index rhs_type) {
    struct arithm_conv conversion = convert(lhs_type, rhs_type);
    enum w_opcode divmod_instruction = W_OP_DIVMOD;  // Unsigned division.
    if (is_signed(conversion.result_type)) {
        // In bude, division is Euclidean (remainder always non-negative) by default.
        // Use W_OP_IDIVMOD (truncated) if possible.
        divmod_instruction = (is_signed(lhs_type)) ? W_OP_EDIVMOD : W_OP_IDIVMOD;
    }
    emit_simple_nnop(checker, conversion.lhs_conv);
    emit_simple_nnop(checker, conversion.rhs_conv);
    emit_simple(checker, divmod_instruction);
    emit_simple_nnop(checker, conversion.result_conv);
    emit_simple_nnop(checker, conversion.result_conv);
    return conversion.result_type;
}

static void emit_pack_instruction(struct type_checker *checker, type_index index) {
    const struct type_info *info = lookup_type(checker->types, index);
    enum w_opcode instruction = W_OP_PACK1 + info->pack.field_count - 1;
    emit_simple(checker, instruction);
    for (int i = 0; i < info->pack.field_count; ++i) {
        type_index field_type = info->pack.fields[i];
        size_t size = type_size(checker->types, field_type);
        emit_u8(checker, size);
    }
}

static void emit_unpack_instruction(struct type_checker *checker, const struct type_info *info) {
    enum w_opcode instruction = W_OP_UNPACK1 + info->pack.field_count - 1;
    emit_simple(checker, instruction);
    for (int i = 0; i < info->pack.field_count; ++i) {
        type_index field_type = info->pack.fields[i];
        size_t size = type_size(checker->types, field_type);
        emit_u8(checker, size);
    }
}

static void emit_pack_field(struct type_checker *checker, enum w_opcode instruction,
                            type_index index, int offset) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL && info->kind == KIND_PACK);
    assert(offset < info->pack.field_count);
    int byte_offset = 0;
    for (int i = 0; i < offset; ++i) {
        type_index field = info->pack.fields[i];
        byte_offset += type_size(checker->types, field);
    }
    type_index field_type = info->pack.fields[offset];
    emit_immediate_s8(checker, instruction, byte_offset);
    emit_s8(checker, type_size(checker->types, field_type));
}

static void emit_comp_field(struct type_checker *checker, enum w_opcode instruction8,
                            int offset) {
    emit_immediate_sv(checker, instruction8, offset);
}

static void emit_comp_subcomp(struct type_checker *checker, enum w_opcode instruction8,
                              int offset, int field_word_count) {
    assert(offset >= 0);
    assert(field_word_count >= 0);
    enum w_opcode instruction16 = instruction8 + 1;
    enum w_opcode instruction32 = instruction8 + 2;
    if (offset <= INT8_MAX && field_word_count <= INT8_MAX) {
        emit_immediate_s8(checker, instruction8, offset);
        emit_s8(checker, field_word_count);
    }
    else if (offset <= INT16_MAX && field_word_count <= INT16_MAX) {
        emit_immediate_s16(checker, instruction16, offset);
        emit_s16(checker, field_word_count);
    }
    else {
        emit_immediate_s32(checker, instruction32, offset);
        emit_s32(checker, field_word_count);
    }
}


static void emit_pack_field_get(struct type_checker *checker, type_index index, int offset) {
    emit_pack_field(checker, W_OP_PACK_FIELD_GET, index, offset);
}

static void emit_comp_field_get(struct type_checker *checker, type_index index, int offset) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL && info->kind == KIND_COMP);
    assert(offset < info->comp.field_count);
    type_index field_type = info->comp.fields[offset];
    int offset_from_end = info->comp.offsets[offset];
    const struct type_info *field_info = lookup_type(checker->types, field_type);
    assert(field_info != NULL);
    if (field_info->kind != KIND_COMP) {
        emit_comp_field(checker, W_OP_COMP_FIELD_GET8, offset_from_end);
    }
    else {
        int word_count = field_info->comp.word_count;
        emit_comp_subcomp(checker, W_OP_COMP_SUBCOMP_GET8, offset_from_end, word_count);
    }
}

static void emit_pack_field_set(struct type_checker *checker, type_index index, int offset) {
    emit_pack_field(checker, W_OP_PACK_FIELD_SET, index, offset);
}

static void emit_comp_field_set(struct type_checker *checker, type_index index, int offset) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL);
    assert(info->kind == KIND_COMP);
    assert(0 <= offset && offset < info->comp.field_count);
    type_index field_type = info->comp.fields[offset];
    int offset_from_end = info->comp.offsets[offset];
    const struct type_info *field_info = lookup_type(checker->types, field_type);
    assert(field_info != NULL);
    if (field_info->kind != KIND_COMP) {
        emit_comp_field(checker, W_OP_COMP_FIELD_SET8, offset_from_end);
    }
    else {
        int word_count = field_info->comp.word_count;
        emit_comp_subcomp(checker, W_OP_COMP_SUBCOMP_SET8, offset_from_end, word_count);
    }
}

static void emit_array_instruction(struct type_checker *checker, enum w_opcode instruction8,
                                   type_index index_type, const struct type_info *info) {
    emit_simple_nnop(checker, promote(index_type));
    // Re-use this function, even though we aren't using subcomps.
    emit_comp_subcomp(checker, instruction8, info->array.element_count,
                      type_word_count(checker->types, info->array.element_type));
}

static void emit_print_instruction(struct type_checker *checker, type_index type) {
    const struct type_info *info = lookup_type(checker->types, type);
    assert(info);
    if (type == TYPE_STRING) {
        emit_simple(checker, W_OP_PRINT_STRING);
    }
    else if (info->kind == KIND_COMP) {
        for (int i = info->comp.field_count - 1; i >= 0; --i) {
            emit_print_instruction(checker, info->comp.fields[i]);
        }
    }
    else if (info->kind == KIND_ARRAY) {
        for (int i = 0; i < info->array.element_count; ++i) {
            // Print elements in reverse order.
            emit_print_instruction(checker, info->array.element_type);
        }
    }
    else if (is_signed(type)) {
        // Promote signed type to int.
        enum w_opcode conv_instruction = promote(type);
        emit_simple_nnop(checker, conv_instruction);
        emit_simple(checker, W_OP_PRINT_INT);
    }
    else if (is_float(type)) {
        // Promote single-precision to double-precision.
        enum w_opcode conv_instruction = promote_float(type);
        emit_simple_nnop(checker, conv_instruction);
        emit_simple(checker, W_OP_PRINT_FLOAT);
    }
    else if (type == TYPE_CHAR) {
        emit_simple(checker, W_OP_PRINT_CHAR);
    }
    else if (type == TYPE_CHAR16) {
        emit_simple(checker, W_OP_CHAR_16CONV32);
        emit_simple(checker, W_OP_CHAR_32CONV8);
        emit_simple(checker, W_OP_PRINT_CHAR);
    }
    else if (type == TYPE_CHAR32) {
        emit_simple(checker, W_OP_CHAR_32CONV8);
        emit_simple(checker, W_OP_PRINT_CHAR);
    }
    else if (type == TYPE_BOOL) {
        emit_simple(checker, W_OP_PRINT_BOOL);
    }
    else if (type_word_count(checker->types, type) == 1) {
        emit_simple(checker, W_OP_PRINT);
    }
    else {
        type_error(checker, "Cannot print type '%"PRI_SV"'",
                   SV_FMT(type_name(checker->types, type)));
    }
}

static void emit_swap_comps(struct type_checker *checker, int lhs_size, int rhs_size) {
    // Note: this instruction is encoded in the same way as COMP_SUBCOMPn, so we reuse that.
    emit_comp_subcomp(checker, W_OP_SWAP_COMPS8, lhs_size, rhs_size);
}

static struct string_view type_stack_to_sv(struct type_checker *checker) {
    return type_array_to_sv(checker, TSTACK_COUNT(checker->tstack), checker->tstack->types);
}

static bool check_pointer_addition(struct type_checker *checker,
                                   type_index lhs_type, type_index rhs_type) {
    enum w_opcode conversion = W_OP_NOP;
    if (lhs_type == TYPE_PTR) {
        if (rhs_type == TYPE_PTR) {
            type_error(checker, "cannot add two pointers");
        }
        conversion = promote(rhs_type);
    }
    else if (rhs_type == TYPE_PTR) {
        conversion = promote(lhs_type);
    }
    else {
        return false;
    }
    emit_simple_nnop(checker, conversion);
    return true;
}

static void check_unreachable(struct type_checker *checker) {
    bool is_ret = checker->in_block->code[checker->ip] == T_OP_RET;
    while (checker->in_block->code[checker->ip + 1] == T_OP_NOP
           && !is_jump_dest(checker->in_block, checker->ip + 1)) {
        ++checker->ip;
        if (checker->ip >= checker->in_block->count) return;
    }
    if (!is_forward_jump_dest(checker, checker->ip + 1)) {
        ++checker->ip;
        if (!is_ret
            && checker->in_block->code[checker->ip] == T_OP_RET
            && checker->ip + 1 >= checker->in_block->count) {
            // Final RET, okay to be unreachable.
            return;
        }
        int start_ip = checker->ip;
        while (checker->ip + 1 < checker->in_block->count
               && !is_forward_jump_dest(checker, checker->ip + 1)) {
            ++checker->ip;
        }
        if (checker->ip + 1 >= checker->in_block->count) {
            type_error(checker, "code from index %d to end of func_%d is unreachable",
                       start_ip, checker->current_function);
            return;
        }
        type_error(checker, "code from index %d to %d in func_%d is unreachable",
                   start_ip, checker->ip + 1, checker->current_function);
    }
    // Load previous state.
    bool success = load_state_at(checker, checker->ip + 1);
    assert(success && "could not load previous state");
}

static void check_jump_instruction(struct type_checker *checker) {
    int offset = read_s16(checker->in_block, checker->ip + 1);
    if (!save_jump(checker, offset)) {
        int dest = checker->ip + offset + 1;
        size_t index = find_state(&checker->states, dest);
        inconsistent_jump_error(checker, index, JMP_SRC);
    }
}

static void check_as_simple(struct type_checker *checker, type_index as_type) {
    assert(IS_SIMPLE_TYPE(as_type));
    type_index from_type = ts_pop(checker);
    if (!IS_SIMPLE_TYPE(from_type) && !is_pack(checker->types, from_type)) {
        struct string_view from_name = type_name(checker->types, from_type);
        struct string_view as_name = type_name(checker->types, as_type);
        type_error(checker, "Cannot coerce type '%"PRI_SV"' to simple type '%"PRI_SV"'",
                   SV_FMT(from_name), SV_FMT(as_name));
    }
    ts_push(checker, as_type);
}

static void check_to_integral(struct type_checker *checker, type_index to_type) {
    assert(is_integral(to_type));
    type_index from_type = ts_pop(checker);
    if (is_integral(from_type)) {
        emit_simple_nnop(checker, promote(from_type));
    }
    else if (is_float(from_type)) {
        emit_simple(checker, float_to_int(from_type));
    }
    else if (is_character(from_type)) {
        // Convert character to UTF-32, then reinterpret as u32/int.
        emit_simple_nnop(checker, decode_character(from_type));
    }
    else {
        struct string_view from_name = type_name(checker->types, from_type);
        struct string_view to_name = type_name(checker->types, to_type);
        type_error(checker, "Cannot convert type '%"PRI_SV"' to '%"PRI_SV"'",
                   SV_FMT(from_name), SV_FMT(to_name));
    }
    ts_push(checker, to_type);
}

static void check_to_bool(struct type_checker *checker) {
    type_index from_type = ts_pop(checker);
    if (is_integral(from_type)) {
        emit_simple(checker, W_OP_ICONVB);  // NOTE: no need to sign/zero-extend value.
    }
    else if (from_type == TYPE_F32) {
        emit_simple(checker, W_OP_FCONVB32);
    }
    else if (from_type == TYPE_F64) {
        emit_simple(checker, W_OP_FCONVB64);
    }
    else if (from_type == TYPE_BOOL) {
        // Do nothing.
    }
    else {
        struct string_view from_name = type_name(checker->types, from_type);
        type_error(checker, "Cannot convert type '%"PRI_SV"' to 'bool'",
                   SV_FMT(from_name));
    }
    ts_push(checker, TYPE_BOOL);
}

static void check_to_float(struct type_checker *checker, type_index to_type) {
    assert(is_float(to_type));
    type_index from_type = ts_pop(checker);
    if (is_integral(from_type)) {
        emit_simple_nnop(checker, promote(from_type));
        emit_simple(checker, int_to_float(to_type));
    }
    else if (is_float(from_type)) {
        // NOTE: This makes use of the relative ordering of TYPE_F32 and TYPE_F64.
        // This is unlikely to change, but if it does, it will break this code.
        if (to_type > from_type) {
            // f32 to f64.
            emit_simple(checker, W_OP_FPROM);
        }
        else if (to_type < from_type) {
            // f64 to f32.
            emit_simple(checker, W_OP_FDEM);
        }
        // Do nothing if the types are the same.
    }
    else if (is_character(from_type)) {
        // Convert to UTF-32/u32 (implicitly also word/int), then to f32/64.
        emit_simple_nnop(checker, decode_character(from_type));
        emit_simple(checker, int_to_float(to_type));
    }
    else {
        struct string_view from_name = type_name(checker->types, from_type);
        struct string_view to_name = type_name(checker->types, to_type);
        type_error(checker, "Cannot convert type '%"PRI_SV"' to '%"PRI_SV"'",
                   SV_FMT(from_name), SV_FMT(to_name));
    }
    ts_push(checker, to_type);
}

static void check_to_character(struct type_checker *checker, type_index to_type) {
    assert(is_character(to_type));
    type_index from_type = ts_pop(checker);
    if (is_integral(from_type)) {
        emit_simple_nnop(checker, promote(from_type));
        emit_simple(checker, W_OP_ICONVC32);
        emit_simple_nnop(checker, encode_character(to_type));
    }
    else if (is_float(from_type)) {
        emit_simple(checker, float_to_int(from_type));
        emit_simple(checker, W_OP_ICONVC32);
        emit_simple_nnop(checker, encode_character(to_type));
    }
    else if (from_type == to_type) {
        /* Do nothing. */
    }
    else if (is_character(from_type)) {
        emit_simple_nnop(checker, decode_character(from_type));
        emit_simple_nnop(checker, encode_character(to_type));
    }
    else {
        struct string_view from_name = type_name(checker->types, from_type);
        struct string_view to_name = type_name(checker->types, to_type);
        type_error(checker, "Cannot convert type '%"PRI_SV"' to '%"PRI_SV"'",
                   SV_FMT(from_name), SV_FMT(to_name));
    }
    ts_push(checker, to_type);
}

static void check_pack_instruction(struct type_checker *checker, type_index index) {
    const struct type_info *info = lookup_type(checker->types, index);
    if (info == NULL) {
        type_error(checker, "unknown type index %d", index);
        assert(0 && "Invalid IR code generated");
    }
    if (info->kind != KIND_PACK) {
        type_error(checker, "type index %d is not of kind 'KIND_PACK'", index);
        assert(0 && "Invalid IR code generated");
    }
    for (int i = info->pack.field_count - 1; i >= 0; --i) {
        type_index field_type = info->pack.fields[i];
        type_index arg_type = ts_pop(checker);
        struct string_view pack_name = type_name(checker->types, index);
        if (arg_type != field_type) {
            struct string_view field_name = type_name(checker->types, field_type);
            struct string_view arg_name = type_name(checker->types, arg_type);
            type_error(checker, "invalid type for field %d of '%"PRI_SV"':"
                       " expected %"PRI_SV" but got %"PRI_SV,
                       i, SV_FMT(pack_name), SV_FMT(field_name), SV_FMT(arg_name));
        }
    }
    ts_push(checker, index);
}

static const struct type_info *check_unpack_instruction(struct type_checker *checker) {
    const struct type_info *info = expect_kind(checker, KIND_PACK);
    for (int i = 0; i < info->pack.field_count; ++i) {
        ts_push(checker, info->pack.fields[i]);
    }
    return info;
}

static void check_comp_instruction(struct type_checker *checker, type_index index) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL && "Invalid state");
    assert(info->kind == KIND_COMP && "Invalid IR code generated");
    expect_types(checker, info->comp.field_count, info->comp.fields);
    ts_push(checker, index);
}

static void check_decomp_instruction(struct type_checker *checker) {
    type_index type = ts_pop(checker);
    const struct type_info *info = lookup_type(checker->types, type);
    assert(info != NULL);
    if (info->kind == KIND_COMP) {
        for (int i = 0; i < info->comp.field_count; ++i) {
            ts_push(checker, info->comp.fields[i]);
        }
    }
    else if (info->kind == KIND_ARRAY) {
        for (int i = 0; i < info->array.element_count; ++i) {
            ts_push(checker, info->array.element_type);
        }
    }
    else {
        type_error(checker, "Invaid type for `decomp`: '%"PRI_SV"'.",
                   SV_FMT(type_name(checker->types, type)));
        ts_push(checker, type);
    }
}

static void check_pack_field_get(struct type_checker *checker, type_index index, uint8_t offset) {
    expect_keep_type(checker, index);
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL && info->kind == KIND_PACK);
    assert(offset < info->pack.field_count);
    ts_push(checker, info->pack.fields[offset]);
}

static void check_comp_field_get(struct type_checker *checker, type_index index,
                                 uint32_t offset) {
    expect_keep_type(checker, index);
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL && info->kind == KIND_COMP);
    assert((int)offset < info->comp.field_count);
    ts_push(checker, info->comp.fields[offset]);
}

static void check_pack_field_set(struct type_checker *checker, type_index index, int offset) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL);
    assert(info->kind == KIND_PACK);
    assert(0 <= offset && offset < info->pack.field_count);
    expect_type(checker, info->pack.fields[offset]);
}

static void check_comp_field_set(struct type_checker *checker, type_index index, int offset) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL);
    assert(info->kind == KIND_COMP);
    assert(0 <= offset && offset < info->comp.field_count);
    expect_type(checker, info->comp.fields[offset]);
}

static void check_array_create(struct type_checker *checker, type_index index) {
    const struct type_info *info = lookup_type(checker->types, index);
    assert(info != NULL && info->kind == KIND_ARRAY);
    int element_count = info->array.element_count;
    assert(element_count > 0);
    for (int i = 0; i < element_count; ++i) {
        expect_type(checker, info->array.element_type);
    }
    ts_push(checker, index);
}

static void check_signature(struct type_checker *checker, struct signature sig) {
    expect_types(checker, sig.param_count, sig.params);
    for (int i = 0; i < sig.ret_count; ++i) {
        ts_push(checker, sig.rets[i]);
    }
}

static void check_function_call(struct type_checker *checker, int index) {
    struct function *function = get_function(&checker->module->functions, index);
    check_signature(checker, function->sig);
}

static void check_ext_function_call(struct type_checker *checker, int index) {
    struct ext_function *external = get_external(&checker->module->externals, index);
    check_signature(checker, external->sig);
}

static void check_function_return(struct type_checker *checker, struct function *function) {
    int ret_count = function->sig.ret_count;
    type_index *rets = function->sig.rets;
    if (!check_type_array(checker, ret_count, rets)) {
        struct string_view expected_sv = type_array_to_sv(checker, ret_count, rets);
        struct string_view actual_sv = type_stack_to_sv(checker);
        type_error(checker, "expected return types %"PRI_SV", but got %"PRI_SV,
                   SV_FMT(expected_sv),
                   SV_FMT(actual_sv));
        clear_region(checker->temp);
    }
}

static int get_locals_size(struct type_checker *checker, struct local_table *locals) {
    int size = 0;
    for (int i = 0; i < locals->count; ++i) {
        struct local *local = &locals->items[i];
        int local_size = type_word_count(checker->types, local->type);
        local->size = local_size;
        local->offset = size;
        size += local_size;
    }
    return size;
}

static struct function *start_function(struct type_checker *checker, int func_index) {
    /* NOTE: there is no corresponding `end_function()` function since all the cleanup
       happens at the start of the next function (or at the end of all functions). */
    struct function *function = get_function(&checker->module->functions, func_index);
    function->locals_size = get_locals_size(checker, &function->locals);
    checker->in_block = &function->t_code;
    checker->out_block = &function->w_code;
    reset_type_checker_states(&checker->states, &checker->in_block->jumps);
    type_index *params = function->sig.params;
    int param_count = function->sig.param_count;
    assert(param_count >= 0);
    if (param_count > 0) {
        assert(params != NULL);
        memcpy(&checker->tstack->types, params, param_count * sizeof *params);
    }
    checker->tstack->top = checker->tstack->types + param_count;
    checker->ip = 0;
    checker->current_function = func_index;
    return function;
}

static void type_check_function(struct type_checker *checker, int func_index) {
    struct function *function = start_function(checker, func_index);
    for (; checker->ip < checker->in_block->count; ++checker->ip) {
        if (is_jump_dest(checker->in_block, checker->ip)) {
            size_t index = find_state(&checker->states, checker->ip);
            int wir_dest = checker->out_block->count;
            // Save jump destination.
            checker->states.wir_dests[index] = wir_dest;
            add_jump(checker->out_block, wir_dest);
            if (!save_state_with_index(checker, index)) {
                // Previous state was saved here.
                if (!check_state_with_index(checker, index)) {
                    inconsistent_jump_error(checker, index, JMP_DEST);
                }
                struct src_list *wir_src_node = checker->states.wir_srcs[index];
                assert(wir_src_node != NULL && "There must be at least one src saved.");
                // Patch every jump that ends here.
                for (; wir_src_node != NULL; wir_src_node = wir_src_node->next) {
                    int wir_jump = wir_dest - wir_src_node->src - 1;
                    // NOTE: To get to this point, the jump SHOULD be a forwards jump.
                    // If not, there's some logic error in the program, so we assert.
                    assert(wir_jump > 0 && "Invalid state");
                    patch_jump(checker, wir_src_node->src, wir_jump);
                }
            }
        }
        enum t_opcode instruction = checker->in_block->code[checker->ip];
        switch (instruction) {
        case T_OP_NOP:
            /* Do nothing. */
            break;
        case T_OP_PUSH8:
            ts_push(checker, TYPE_WORD);
            copy_immediate_u8(checker, W_OP_PUSH8);
            break;
        case T_OP_PUSH16:
            ts_push(checker, TYPE_WORD);
            copy_immediate_u16(checker, W_OP_PUSH16);
            break;
        case T_OP_PUSH32:
            ts_push(checker, TYPE_WORD);
            copy_immediate_u32(checker, W_OP_PUSH32);
            break;
        case T_OP_PUSH64:
            ts_push(checker, TYPE_WORD);
            copy_immediate_u64(checker, W_OP_PUSH64);
            break;
        case T_OP_PUSH_INT8:
            ts_push(checker, TYPE_INT);
            copy_immediate_u8(checker, W_OP_PUSH_INT8);
            break;
        case T_OP_PUSH_INT16:
            ts_push(checker, TYPE_INT);
            copy_immediate_u16(checker, W_OP_PUSH_INT16);
            break;
        case T_OP_PUSH_INT32:
            ts_push(checker, TYPE_INT);
            copy_immediate_u32(checker, W_OP_PUSH_INT32);
            break;
        case T_OP_PUSH_INT64:
            ts_push(checker, TYPE_INT);
            copy_immediate_u64(checker, W_OP_PUSH_INT64);
            break;
        case T_OP_PUSH_FLOAT32:
            ts_push(checker, TYPE_F32);
            copy_immediate_u32(checker, W_OP_PUSH_FLOAT32);
            break;
        case T_OP_PUSH_FLOAT64:
            ts_push(checker, TYPE_F64);
            copy_immediate_u64(checker, W_OP_PUSH_FLOAT64);
            break;
        case T_OP_PUSH_CHAR8:
            ts_push(checker, TYPE_CHAR);
            copy_immediate_u8(checker, W_OP_PUSH_CHAR8);
            break;
        case T_OP_PUSH_CHAR16:
            ts_push(checker, TYPE_CHAR);
            copy_immediate_u16(checker, W_OP_PUSH_CHAR16);
            break;
        case T_OP_PUSH_CHAR32:
            ts_push(checker, TYPE_CHAR);
            copy_immediate_u32(checker, W_OP_PUSH_CHAR32);
            break;
        case T_OP_LOAD_STRING8:
            ts_push(checker, TYPE_STRING);
            copy_immediate_u8(checker, W_OP_LOAD_STRING8);
            break;
        case T_OP_LOAD_STRING16:
            ts_push(checker, TYPE_STRING);
            copy_immediate_u16(checker, W_OP_LOAD_STRING16);
            break;
        case T_OP_LOAD_STRING32:
            ts_push(checker, TYPE_STRING);
            copy_immediate_u32(checker, W_OP_LOAD_STRING32);
            break;
        case T_OP_POP: {
            type_index type = ts_pop(checker);
            size_t word_count = type_word_count(checker->types, type);
            assert(word_count > 0 && "Invalid type");
            if (word_count == 1) {
                emit_simple(checker, W_OP_POP);
            }
            else {
                // NOTE: This includes the builtin type `string`.
                emit_immediate_sv(checker, W_OP_POPN8, word_count);
            }
            break;
        }
        case T_OP_ADD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            type_index result_type = TYPE_ERROR;
            enum w_opcode add_instruction = W_OP_ADD;
            if (check_pointer_addition(checker, lhs_type, rhs_type)) {
                // Pointer-offset addition.
                result_type = TYPE_PTR;
            }
            else if (is_integral(lhs_type) && is_integral(rhs_type)) {
                // Integer addition.
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                result_type = conversion.result_type;
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                emit_simple_nnop(checker, conversion.result_conv);
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                // Float additon.
                if (is_integral(lhs_type)) {
                    emit_simple_nnop(checker, promotel(lhs_type));
                    lhs_type = TYPE_INT;
                }
                else if (is_integral(rhs_type)) {
                    emit_simple_nnop(checker, promote(rhs_type));
                    rhs_type = TYPE_INT;
                }
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                result_type = conversion.result_type;
                add_instruction = (result_type == TYPE_F32) ? W_OP_ADDF32 : W_OP_ADDF64;
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
            }
            if (result_type == TYPE_ERROR) {
                type_error(checker, "invalid types for `+`");
                result_type = TYPE_WORD;  // Continue with a word.
            }
            ts_push(checker, result_type);
            emit_simple(checker, add_instruction);
            break;
        }
        case T_OP_AND: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            if (lhs_type != rhs_type) {
                type_error(checker, "mismatched types for `and`");
                lhs_type = TYPE_WORD;
            }
            ts_push(checker, lhs_type);
            emit_simple(checker, W_OP_AND);
            break;
        }
        case T_OP_DEREF:
            if (ts_pop(checker) != TYPE_PTR) {
                type_error(checker, "expected pointer");
            }
            ts_push(checker, TYPE_BYTE);
            emit_simple(checker, W_OP_DEREF);
            break;
        case T_OP_DIV: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            type_index result_type = TYPE_ERROR;
            if (is_float(lhs_type) || is_float(rhs_type)) {
                // Float division.
                if (is_integral(lhs_type)) {
                    emit_simple_nnop(checker, promotel(lhs_type));
                    lhs_type = TYPE_INT;
                }
                else if (is_integral(rhs_type)) {
                    emit_simple_nnop(checker, promote(rhs_type));
                    rhs_type = TYPE_INT;
                }
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                result_type = conversion.result_type;
                emit_simple(checker, (result_type == TYPE_F64) ? W_OP_DIVF64 : W_OP_DIVF32);
            }
            else if (is_integral(lhs_type) && is_integral(rhs_type)) {
                result_type = emit_divmod_instruction(checker, lhs_type, rhs_type);
                emit_simple(checker, W_OP_POP);
            }
            if (result_type == TYPE_ERROR) {
                type_error(checker, "invalid types for `/`");
                result_type = TYPE_WORD;
            }
            ts_push(checker, result_type);
            break;
        }
        case T_OP_DIVMOD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            type_index result_type = TYPE_WORD;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                result_type = emit_divmod_instruction(checker, lhs_type, rhs_type);
            }
            else {
                type_error(checker, "invalid types for `divmod`");
            }
            ts_push(checker, result_type);  // Quotient.
            ts_push(checker, result_type);  // Remainder.
            break;
        }
        case T_OP_IDIVMOD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            struct arithm_conv conversion = convert(lhs_type, rhs_type);
            if (conversion.result_type == TYPE_ERROR) {
                type_error(checker, "invalid types for `idivmod`");
                conversion.result_type = TYPE_WORD;
            }
            emit_simple_nnop(checker, conversion.lhs_conv);
            emit_simple_nnop(checker, conversion.rhs_conv);
            emit_simple(checker, W_OP_IDIVMOD);
            emit_simple_nnop(checker, conversion.result_conv);
            emit_simple_nnop(checker, conversion.result_conv);
            ts_push(checker, conversion.result_type);
            ts_push(checker, conversion.result_type);
            break;
        }
        case T_OP_EDIVMOD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            struct arithm_conv conversion = convert(lhs_type, rhs_type);
            if (conversion.result_type == TYPE_ERROR) {
                type_error(checker, "invalid types for `edivmod`");
                conversion.result_type = TYPE_WORD;
            }
            emit_simple_nnop(checker, conversion.lhs_conv);
            emit_simple_nnop(checker, conversion.rhs_conv);
            emit_simple(checker, W_OP_EDIVMOD);
            emit_simple_nnop(checker, conversion.result_conv);
            emit_simple_nnop(checker, conversion.result_conv);
            ts_push(checker, conversion.result_type);
            ts_push(checker, conversion.result_type);
            break;
        }
        case T_OP_DUPE: {
            type_index type = ts_pop(checker);
            ts_push(checker, type);
            ts_push(checker, type);
            int word_count = type_word_count(checker->types, type);
            assert(word_count > 0);
            if (word_count == 1) {
                emit_simple(checker, W_OP_DUPE);
            }
            else {
                emit_immediate_sv(checker, W_OP_DUPEN8, word_count);
            }
            break;
        }
        case T_OP_EQUALS: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode comparison = W_OP_EQUALS;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                comparison = (conversion.result_type == TYPE_F32)
                    ? W_OP_EQUALS_F32
                    : W_OP_EQUALS_F64;
            }
            else {
                struct string_view lhs_name = type_name(&checker->module->types, lhs_type);
                struct string_view rhs_name = type_name(&checker->module->types, rhs_type);
                type_error(checker, "invalid types for `=`: %"PRI_SV" and %"PRI_SV,
                           SV_FMT(lhs_name), SV_FMT(rhs_name));
            }
            emit_simple(checker, comparison);
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_GET_LOOP_VAR:
            ts_push(checker, TYPE_INT);  // Loop variable is always an integer.
            copy_immediate_u16(checker, W_OP_GET_LOOP_VAR);
            break;
        case T_OP_GREATER_EQUALS: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode comparison = W_OP_GREATER_EQUALS;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                if (!is_signed(conversion.result_type)) {
                    comparison =  W_OP_HIGHER_SAME;
                }
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                comparison = (conversion.result_type == TYPE_F32)
                    ? W_OP_GREATER_EQUALS_F32
                    : W_OP_GREATER_EQUALS_F64;
            }
            else {
                type_error(checker, "invalid types for `>=`");
            }
            emit_simple(checker, comparison);
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_GREATER_THAN: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode comparison = W_OP_GREATER_THAN;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                if (!is_signed(conversion.result_type)) {
                    comparison =  W_OP_HIGHER_THAN;
                }
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                comparison = (conversion.result_type == TYPE_F32)
                    ? W_OP_GREATER_THAN_F32
                    : W_OP_GREATER_THAN_F64;
            }
            else {
                type_error(checker, "invalid types for `>`");
            }
            emit_simple(checker, comparison);
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_LESS_EQUALS: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode comparison = W_OP_LESS_EQUALS;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                if (!is_signed(conversion.result_type)) {
                    comparison =  W_OP_LOWER_SAME;
                }
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                comparison = (conversion.result_type == TYPE_F32)
                    ? W_OP_LESS_EQUALS_F32
                    : W_OP_LESS_EQUALS_F64;
            }
            else {
                type_error(checker, "invalid types for `<=`");
            }
            emit_simple(checker, comparison);
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_LESS_THAN: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode comparison = W_OP_LESS_THAN;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                if (!is_signed(conversion.result_type)) {
                    comparison =  W_OP_LOWER_SAME;
                }
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                comparison = (conversion.result_type == TYPE_F32)
                    ? W_OP_LESS_THAN_F32
                    : W_OP_LESS_THAN_F64;
            }
            else {
                type_error(checker, "invalid types for `<`");
            }
            emit_simple(checker, comparison);
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_LOCAL_GET: {
            int index = copy_immediate_u16(checker, W_OP_LOCAL_GET);
            ts_push(checker, function->locals.items[index].type);
            break;
        }
        case T_OP_LOCAL_SET: {
            int index = copy_immediate_u16(checker, W_OP_LOCAL_SET);
            expect_type(checker, function->locals.items[index].type);
            break;
        }
        case T_OP_MULT: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode mult_instruction = W_OP_MULT;
            enum w_opcode result_conv = W_OP_NOP;
            type_index result_type = TYPE_ERROR;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                result_conv = conversion.result_conv;
                result_type = conversion.result_type;
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                result_type = conversion.result_type;
                mult_instruction = (result_type == TYPE_F64) ? W_OP_MULTF64 : W_OP_MULTF32;
            }
            if (result_type == TYPE_ERROR) {
                type_error(checker, "invalid types for `*`");
                result_type = TYPE_WORD;
            }
            emit_simple(checker, mult_instruction);
            emit_simple_nnop(checker, result_conv);
            ts_push(checker, result_type);
            break;
        }
        case T_OP_NEG: {
            type_index type = ts_peek(checker);  // Type preserved by instruction.
            if (is_integral(type)) {
                emit_simple_nnop(checker, promote(type));
                emit_simple(checker, W_OP_NEG);
                emit_simple_nnop(checker, demote(type));
            }
            else if (type == TYPE_F32) {
                emit_simple(checker, W_OP_NEGF32);
            }
            else if (type == TYPE_F64) {
                emit_simple(checker, W_OP_NEGF64);
            }
            else {
                struct string_view name = type_name(checker->types, type);
                type_error(checker, "Invalid type for `~`: '%"PRI_SV"'", SV_FMT(name));
            }
            break;
        }
        case T_OP_NOT: {
            type_index type = ts_pop(checker);
            if (is_integral(type) || type == TYPE_BOOL) {
                emit_simple(checker, W_OP_NOT);
            }
            else if (is_float(type)) {
                ts_push(checker, type);  // Needed for `check_to_bool()`.
                check_to_bool(checker);  // Convert to bool first, then invert.
                emit_simple(checker, W_OP_NOT);
            }
            else {
                struct string_view name = type_name(checker->types, type);
                type_error(checker, "Invalid type for `not`: '%"PRI_SV"'.", SV_FMT(name));
            }
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_NOT_EQUALS: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode comparison = W_OP_EQUALS;
            if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                emit_simple_nnop(checker, convert_float_args(&lhs_type, &rhs_type));
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                comparison = (conversion.result_type == TYPE_F32)
                    ? W_OP_EQUALS_F32
                    : W_OP_EQUALS_F64;
            }
            else {
                type_error(checker, "invalid types for `/=`");
            }
            emit_simple(checker, comparison);
            ts_push(checker, TYPE_BOOL);
            break;
        }
        case T_OP_OR: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            if (lhs_type != rhs_type) {
                type_error(checker, "mismatched tyes for `or`:");
                lhs_type = TYPE_WORD;  // In case of an error, recover by using a word.
            }
            ts_push(checker, lhs_type);
            emit_simple(checker, W_OP_OR);
            break;
        }
        case T_OP_OVER: {
            type_index b_type = ts_pop(checker);
            type_index a_type = ts_pop(checker);
            ts_push(checker, a_type);
            ts_push(checker, b_type);
            ts_push(checker, a_type);
            // Treat (a b) as a comp containing a and b as subcomps.
            int a_size = type_word_count(checker->types, a_type);
            int b_size = type_word_count(checker->types, b_type);
            emit_comp_subcomp(checker, W_OP_COMP_SUBCOMP_GET8, a_size + b_size, a_size);
            break;
        }
        case T_OP_PRINT: {
            type_index type = ts_pop(checker);
            emit_print_instruction(checker, type);
            break;
        }
        case T_OP_PRINT_CHAR: {
            type_index type = ts_pop(checker);
            if (type != TYPE_CHAR && type != TYPE_BYTE) {
                type_error(checker, "expected char or byte for `print-char`");
            }
            emit_simple(checker, W_OP_PRINT_CHAR);
            break;
        }
        case T_OP_PRINT_INT: {
            type_index type = ts_pop(checker);
            if (is_integral(type)) {
                enum w_opcode conv_instruction = sign_extend(type);
                emit_simple_nnop(checker, conv_instruction);
            }
            else {
                type_error(checker, "invalid type for `OP_PRINT_INT`");
            }
            emit_simple(checker, W_OP_PRINT_INT);
            break;
        }
        case T_OP_ROT: {
            type_index c_type = ts_pop(checker);
            type_index b_type = ts_pop(checker);
            type_index a_type = ts_pop(checker);
            ts_push(checker, b_type);
            ts_push(checker, c_type);
            ts_push(checker, a_type);
            int a_size = type_word_count(checker->types, a_type);
            int b_size = type_word_count(checker->types, b_type);
            int c_size = type_word_count(checker->types, c_type);
            // Treat (a b c) as two comps: {a} and {b c}.
            emit_swap_comps(checker, a_size, b_size + c_size);
            break;
        }
        case T_OP_SUB: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            enum w_opcode sub_instruction = W_OP_SUB;
            type_index result_type = TYPE_ERROR;
            enum w_opcode result_conv = W_OP_NOP;
            if (lhs_type == TYPE_PTR) {
                if (rhs_type == TYPE_PTR) {
                    result_type = TYPE_INT;
                }
                else if (is_integral(rhs_type)) {
                    emit_simple_nnop(checker, promote(rhs_type));
                    result_type = TYPE_PTR;
                }
                else {
                    type_error(checker, "invalid types for `-`");
                }
            }
            else if (is_float(lhs_type) || is_float(rhs_type)) {
                // Floating-point arithmetic.
                if (is_integral(lhs_type)) {
                    emit_simple_nnop(checker, promotel(lhs_type));
                    lhs_type = TYPE_INT;
                }
                else if (is_integral(rhs_type)) {
                    emit_simple_nnop(checker, promote(rhs_type));
                    rhs_type = TYPE_INT;
                }
                struct float_conv conversion = convert_float(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                result_type = conversion.result_type;
                sub_instruction = (result_type == TYPE_F64) ? W_OP_SUBF64 : W_OP_SUBF32;
            }
            else if (is_integral(lhs_type) && is_integral(rhs_type)) {
                struct arithm_conv conversion = convert(lhs_type, rhs_type);
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                result_type = conversion.result_type;
                result_conv = conversion.result_conv;
            }
            if (result_type == TYPE_ERROR) {
                type_error(checker, "invalid types for `-`");
                result_type = TYPE_WORD;
            }
            ts_push(checker, result_type);
            emit_simple(checker, sub_instruction);
            emit_simple_nnop(checker, result_conv);
            break;
        }
        case T_OP_SWAP: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            ts_push(checker, rhs_type);
            ts_push(checker, lhs_type);
            int lhs_size = type_word_count(checker->types, lhs_type);
            int rhs_size = type_word_count(checker->types, rhs_type);
            assert(lhs_size > 0 && rhs_size > 0);
            if (lhs_size == 1 && rhs_size == 1) {
                emit_simple(checker, W_OP_SWAP);
            }
            else {
                emit_swap_comps(checker, lhs_size, rhs_size);
            }
            break;
        }
        case T_OP_AS_WORD:
            check_as_simple(checker, TYPE_WORD);
            break;
        case T_OP_AS_BYTE:
            check_as_simple(checker, TYPE_BYTE);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_AS_PTR:
            check_as_simple(checker, TYPE_PTR);
            break;
        case T_OP_AS_INT:
            check_as_simple(checker, TYPE_INT);
            break;
        case T_OP_AS_BOOL:
            check_as_simple(checker, TYPE_BOOL);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_AS_U8:
            check_as_simple(checker, TYPE_U8);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_AS_U16:
            check_as_simple(checker, TYPE_U16);
            emit_simple(checker, W_OP_ZX16);
            break;
        case T_OP_AS_U32:
            check_as_simple(checker, TYPE_U32);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_AS_S8:
            check_as_simple(checker, TYPE_S8);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_AS_S16:
            check_as_simple(checker, TYPE_S16);
            emit_simple(checker, W_OP_ZX16);
            break;
        case T_OP_AS_S32:
            check_as_simple(checker, TYPE_S32);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_AS_F32:
            check_as_simple(checker, TYPE_F32);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_AS_F64:
            check_as_simple(checker, TYPE_F64);
            break;
        case T_OP_AS_CHAR:
            // NOTE: We don't check that the value is actually a valid UTF-8 encoding.
            check_as_simple(checker, TYPE_CHAR);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_AS_CHAR16:
            check_as_simple(checker, TYPE_CHAR16);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_AS_CHAR32:
            check_as_simple(checker, TYPE_CHAR32);
            emit_simple(checker, W_OP_ICONVC32);
            break;
        case T_OP_TO_WORD:
            check_to_integral(checker, TYPE_WORD);
            break;
        case T_OP_TO_BYTE:
            check_to_integral(checker, TYPE_BYTE);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_TO_PTR:
            check_to_integral(checker, TYPE_PTR);
            break;
        case T_OP_TO_INT:
            check_to_integral(checker, TYPE_INT);
            break;
        case T_OP_TO_BOOL:
            check_to_bool(checker);
            break;
        case T_OP_TO_U8:
            check_to_integral(checker, TYPE_U8);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_TO_U16:
            check_to_integral(checker, TYPE_U16);
            emit_simple(checker, W_OP_ZX16);
            break;
        case T_OP_TO_U32:
            check_to_integral(checker, TYPE_U32);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_TO_S8:
            check_to_integral(checker, TYPE_S8);
            emit_simple(checker, W_OP_ZX8);
            break;
        case T_OP_TO_S16:
            check_to_integral(checker, TYPE_S16);
            emit_simple(checker, W_OP_ZX16);
            break;
        case T_OP_TO_S32:
            check_to_integral(checker, TYPE_S32);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_TO_F32:
            check_to_float(checker, TYPE_F32);
            emit_simple(checker, W_OP_ZX32);
            break;
        case T_OP_TO_F64:
            check_to_float(checker, TYPE_F64);
            break;
        case T_OP_TO_CHAR:
            check_to_character(checker, TYPE_CHAR);
            break;
        case T_OP_TO_CHAR16:
            check_to_character(checker, TYPE_CHAR16);
            break;
        case T_OP_TO_CHAR32:
            check_to_character(checker, TYPE_CHAR32);
            break;
        case T_OP_EXIT: {
            type_index type = ts_pop(checker);
            if (is_integral(type)) {
                emit_simple_nnop(checker, promote(type));
            }
            else {
                type_error(checker, "expected integral type for `exit`");
            }
            check_unreachable(checker);
            emit_simple(checker, W_OP_EXIT);
            break;
        }
        case T_OP_JUMP_COND:
            ts_pop(checker);
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_JUMP_COND);
            break;
        case T_OP_JUMP_NCOND:
            ts_pop(checker);
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_JUMP_NCOND);
            break;
        case T_OP_JUMP:
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_JUMP);
            check_unreachable(checker);
            break;
        case T_OP_FOR_DEC_START:
            ts_pop(checker);
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_FOR_DEC_START);
            break;
        case T_OP_FOR_INC_START:
            ts_pop(checker);
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_FOR_INC_START);
            break;
        case T_OP_FOR_DEC:
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_FOR_DEC);
            break;
        case T_OP_FOR_INC:
            check_jump_instruction(checker);
            copy_jump_instruction(checker, W_OP_FOR_INC);
            break;
        case T_OP_PACK8: {
            int8_t index = read_s8(checker->in_block, checker->ip + 1);
            check_pack_instruction(checker, index);
            emit_pack_instruction(checker, index);
            checker->ip += 1;
            break;
        }
        case T_OP_PACK16: {
            int16_t index = read_s16(checker->in_block, checker->ip + 1);
            check_pack_instruction(checker, index);
            emit_pack_instruction(checker, index);
            checker->ip += 1;
            break;
        }
        case T_OP_PACK32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            check_pack_instruction(checker, index);
            emit_pack_instruction(checker, index);
            checker->ip += 1;
            break;
        }
        case T_OP_UNPACK: {
            const struct type_info *info = check_unpack_instruction(checker);
            emit_unpack_instruction(checker, info);
            break;
        }
        case T_OP_COMP8: {
            int8_t index = read_s8(checker->in_block, checker->ip + 1);
            checker->ip += 1;
            check_comp_instruction(checker, index);
            // No instructions are emitted in the word-oriented dialect, just
            // treat all the words in the comp as a singe unit.
            break;
        }
        case T_OP_COMP16: {
            int16_t index = read_s16(checker->in_block, checker->ip + 1);
            checker->ip += 2;
            check_comp_instruction(checker, index);
            break;
        }
        case T_OP_COMP32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            checker->ip += 4;
            check_comp_instruction(checker, index);
            break;
        }
        case T_OP_DECOMP: {
            check_decomp_instruction(checker);
            break;
        }
        case T_OP_PACK_FIELD_GET8: {
            int8_t index = read_s8(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 2);
            checker->ip += 2;
            check_pack_field_get(checker, index, offset);
            emit_pack_field_get(checker, index, offset);
            break;
        }
        case T_OP_PACK_FIELD_GET16: {
            int16_t index = read_s16(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 3);
            checker->ip += 3;
            check_pack_field_get(checker, index, offset);
            emit_pack_field_get(checker, index, offset);
            break;
        }
        case T_OP_PACK_FIELD_GET32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 5);
            checker->ip += 5;
            check_pack_field_get(checker, index, offset);
            emit_pack_field_get(checker, index, offset);
            break;
        }
        case T_OP_COMP_FIELD_GET8: {
            int8_t index = read_s8(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 2);
            checker->ip += 2;
            check_comp_field_get(checker, index, offset);
            emit_comp_field_get(checker, index, offset);
            break;
        }
        case T_OP_COMP_FIELD_GET16: {
            int16_t index = read_s16(checker->in_block, checker->ip + 1);
            int16_t offset = read_s16(checker->in_block, checker->ip + 3);
            checker->ip += 4;
            check_comp_field_get(checker, index, offset);
            emit_comp_field_get(checker, index, offset);
            break;
        }
        case T_OP_COMP_FIELD_GET32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            int32_t offset = read_s32(checker->in_block, checker->ip + 5);
            checker->ip += 8;
            check_comp_field_get(checker, index, offset);
            emit_comp_field_get(checker, index, offset);
            break;
        }
        case T_OP_PACK_FIELD_SET8: {
            int8_t index = read_s8(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 2);
            checker->ip += 2;
            check_pack_field_set(checker, index, offset);
            emit_pack_field_set(checker, index, offset);
            break;
        }
        case T_OP_PACK_FIELD_SET16: {
            int16_t index = read_s16(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 3);
            checker->ip += 3;
            check_pack_field_set(checker, index, offset);
            emit_pack_field_set(checker, index, offset);
            break;
        }
        case T_OP_PACK_FIELD_SET32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 5);
            checker->ip += 5;
            check_pack_field_set(checker, index, offset);
            emit_pack_field_set(checker, index, offset);
            break;
        }
        case T_OP_COMP_FIELD_SET8: {
            int8_t index = read_s8(checker->in_block, checker->ip + 1);
            int8_t offset = read_s8(checker->in_block, checker->ip + 2);
            checker->ip += 2;
            check_comp_field_set(checker, index, offset);
            emit_comp_field_set(checker, index, offset);
            break;
        }
        case T_OP_COMP_FIELD_SET16: {
            int16_t index = read_s16(checker->in_block, checker->ip + 1);
            int16_t offset = read_s16(checker->in_block, checker->ip + 3);
            checker->ip += 4;
            check_comp_field_set(checker, index, offset);
            emit_comp_field_set(checker, index, offset);
            break;
        }
        case T_OP_COMP_FIELD_SET32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            int32_t offset = read_s32(checker->in_block, checker->ip + 5);
            checker->ip += 8;
            check_comp_field_set(checker, index, offset);
            emit_comp_field_set(checker, index, offset);
            break;
        }
        case T_OP_ARRAY_CREATE8: {
            int32_t index = read_s8(checker->in_block, checker->ip + 1);
            checker->ip += 1;
            check_array_create(checker, index);
            break;
        }
        case T_OP_ARRAY_CREATE16: {
            int32_t index = read_s16(checker->in_block, checker->ip + 1);
            checker->ip += 2;
            check_array_create(checker, index);
            break;
        }
        case T_OP_ARRAY_CREATE32: {
            int32_t index = read_s32(checker->in_block, checker->ip + 1);
            checker->ip += 5;
            check_array_create(checker, index);
            break;
        }
        case T_OP_ARRAY_GET: {
            type_index index_type = ts_pop(checker);
            if (!is_integral(index_type)) {
                struct string_view index_name = type_name(&checker->module->types, index_type);
                type_error(checker, "array index must be an integer, not %"PRI_SV, SV_FMT(index_name));
            }
            const struct type_info *info = expect_keep_kind(checker, KIND_ARRAY);
            assert(info && info->kind == KIND_ARRAY);
            ts_push(checker, info->array.element_type);
            emit_array_instruction(checker, W_OP_ARRAY_GET8, index_type, info);
            break;
        }
        case T_OP_ARRAY_SET: {
            type_index index_type = ts_pop(checker);
            type_index element_type = ts_pop(checker);
            if (!is_integral(index_type)) {
                struct string_view index_name = type_name(&checker->module->types, index_type);
                type_error(checker, "array index must be an integer, not %"PRI_SV, SV_FMT(index_name));
            }
            const struct type_info *info = expect_keep_kind(checker, KIND_ARRAY);
            assert(info && info->kind == KIND_ARRAY);
            expect_types_equal(checker, info->array.element_type, element_type);
            emit_array_instruction(checker, W_OP_ARRAY_SET8, index_type, info);
            break;
        }
        case T_OP_CALL8: {
            uint8_t index = read_u8(checker->in_block, checker->ip + 1);
            checker->ip += 1;
            check_function_call(checker, index);
            emit_immediate_u8(checker, W_OP_CALL8, index);
            break;
        }
        case T_OP_CALL16: {
            uint16_t index = read_u16(checker->in_block, checker->ip + 1);
            checker->ip += 2;
            check_function_call(checker, index);
            emit_immediate_u16(checker, W_OP_CALL16, index);
            break;
        }
        case T_OP_CALL32: {
            uint32_t index = read_u32(checker->in_block, checker->ip + 1);
            checker->ip += 4;
            check_function_call(checker, index);
            emit_immediate_u32(checker, W_OP_CALL32, index);
            break;
        }
        case T_OP_EXTCALL8: {
            uint8_t index = read_u8(checker->in_block, checker->ip + 1);
            checker->ip += 1;
            check_ext_function_call(checker, index);
            emit_immediate_u8(checker, W_OP_EXTCALL8, index);
            break;
        }
        case T_OP_EXTCALL16: {
            uint16_t index = read_u16(checker->in_block, checker->ip + 1);
            checker->ip += 2;
            check_ext_function_call(checker, index);
            emit_immediate_u16(checker, W_OP_EXTCALL16, index);
            break;
        }
        case T_OP_EXTCALL32: {
            uint32_t index = read_u32(checker->in_block, checker->ip + 1);
            checker->ip += 4;
            check_ext_function_call(checker, index);
            emit_immediate_u32(checker, W_OP_EXTCALL32, index);
            break;
        }
        case T_OP_RET:
            check_function_return(checker, function);
            check_unreachable(checker);
            emit_simple(checker, W_OP_RET);
            break;
        }
    }
}

enum type_check_result type_check(struct type_checker *checker) {
    for (int i = 0; i < checker->module->functions.count; ++i) {
        type_check_function(checker, i);
    }
    return (!checker->had_error) ? TYPE_CHECK_OK : TYPE_CHECK_ERROR;
}

void ts_push(struct type_checker *checker, type_index type) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top >= &tstack->types[TYPE_STACK_SIZE]) {
        type_error(checker, "insufficient stack space");
        return;
    }
    *tstack->top++ = type;
}

type_index ts_pop(struct type_checker *checker) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top == tstack->types) {
        type_error(checker, "insufficient stack space");
        return TYPE_ERROR;
    }
    return *--tstack->top;
}

type_index ts_peek(struct type_checker *checker) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top == tstack->types) {
        type_error(checker, "insufficient stack space");
        return TYPE_ERROR;
    }
    return tstack->top[-1];
}
