#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "location.h"
#include "type_checker.h"

struct arithm_conv {
    type_index result_type;
    enum w_opcode lhs_conv;
    enum w_opcode rhs_conv;
    enum w_opcode result_conv;
};

static void type_error(struct type_checker *checker, const char *restrict message, ...) {
    ir_error(checker->in_block, checker->ip, "Type error: ");
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, ".\n");
}

void reset_type_stack(struct type_stack *tstack) {
    tstack->top = tstack->types;
}

static void init_type_checker_states(struct type_checker_states *states,
                                     struct jump_info_table *jumps) {
    states->size = jumps->count;
    states->states = calloc(states->size, sizeof *states->states);
    states->ips = calloc(states->size, sizeof *states->ips);
    states->jump_srcs = calloc(states->size, sizeof *states->jump_srcs);
    memcpy(states->ips, jumps->dests, states->size * sizeof states->ips[0]);
}

static void free_type_checker_states(struct type_checker_states *states) {
    for (size_t i = 0; i < states->size; ++i) {
        // NOTE: it's safe to pass NULL to free, so no need to check for it.
        free(states->states[i]);
    }
    states->size = 0;
    free(states->states);
    free(states->ips);
    free(states->jump_srcs);
}

void init_type_checker(struct type_checker *checker, struct ir_block *in_block,
                       struct ir_block *out_block) {
    init_type_checker_states(&checker->states, &in_block->jumps);
    checker->in_block = in_block;
    checker->out_block = out_block;
    copy_metadata(checker->out_block, checker->in_block);
    checker->tstack = malloc(sizeof *checker->tstack);
    checker->ip = 0;
    checker->had_error = false;
    reset_type_stack(checker->tstack);
}

void free_type_checker(struct type_checker *checker) {
    free_type_checker_states(&checker->states);
    free(checker->tstack);
    checker->tstack = NULL;
}

static void emit_simple(struct type_checker *checker, enum w_opcode instruction) {
    write_simple(checker->out_block, instruction, &checker->in_block->locations[checker->ip]);
}

static void emit_simple_nnop(struct type_checker *checker, enum w_opcode instruction) {
    if (instruction != W_OP_NOP) emit_simple(checker, instruction);
}

static void copy_immediate_u8(struct type_checker *checker, enum w_opcode instruction) {
    uint8_t value = read_u8(checker->in_block, checker->ip + 1);
    write_immediate_u8(checker->out_block, instruction, value,
                       &checker->in_block->locations[checker->ip]);
    checker->ip += 1;
}

static void copy_immediate_u16(struct type_checker *checker, enum w_opcode instruction) {
    uint16_t value = read_u16(checker->in_block, checker->ip + 1);
    write_immediate_u16(checker->out_block, instruction, value,
                        &checker->in_block->locations[checker->ip]);
    checker->ip += 2;
}

static void copy_immediate_u32(struct type_checker *checker, enum w_opcode instruction) {
    uint32_t value = read_u32(checker->in_block, checker->ip + 1);
    write_immediate_u32(checker->out_block, instruction, value,
                        &checker->in_block->locations[checker->ip]);
    checker->ip += 4;
}

static void copy_immediate_u64(struct type_checker *checker, enum w_opcode instruction) {
    uint64_t value = read_u64(checker->in_block, checker->ip + 1);
    write_immediate_u64(checker->out_block, instruction, value,
                        &checker->in_block->locations[checker->ip]);
    checker->ip += 8;
}

static void copy_jump_instruction(struct type_checker *checker, enum w_opcode instruction) {
    int16_t jump = read_s16(checker->in_block, checker->ip + 1);
    write_immediate_s16(checker->out_block, instruction, jump,
                        &checker->in_block->locations[checker->ip]);
    int dest = checker->out_block->count - 2 + jump;  // -2 because of operand.
    add_jump(checker->out_block, dest);
    checker->ip += 2;
}

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

static bool is_integral(type_index type) {
    switch (type) {
    case TYPE_WORD:
    case TYPE_BYTE:
    case TYPE_INT:
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_S8:
    case TYPE_S16:
    case TYPE_S32:
        return true;
    default:
        return false;
    }
}

static bool is_signed(type_index type) {
    switch (type) {
    case TYPE_INT:
    case TYPE_S8:
    case TYPE_S16:
    case TYPE_S32:
        return true;
    default:
        return false;
    }
}

static struct arithm_conv convert(type_index rhs, type_index lhs) {
    if (IS_SIMPLE_TYPE(rhs) && IS_SIMPLE_TYPE(lhs)) {
        return arithmetic_conversions[lhs][rhs];
    }
    // Custom types are always non-arithmetic.
    return (struct arithm_conv) {0};
}

static enum w_opcode promote(type_index type) {
    return convert(TYPE_INT, type).rhs_conv;
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

static bool check_pointer_addition(struct type_checker *checker,
                                   type_index lhs_type, type_index rhs_type) {
    enum w_opcode conversion = W_OP_NOP;
    if (lhs_type == TYPE_PTR) {
        if (rhs_type == TYPE_PTR) {
            checker->had_error = true;
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

static bool save_state_at(struct type_checker *checker, int ip) {
    struct type_checker_states *states = &checker->states;
    size_t index = find_state(states, ip);
    assert(index < (size_t)checker->in_block->jumps.count);
    if (states->states[index] != NULL) {
        // There was already a state saved there.
        return false;
    }
    size_t count = TSTACK_COUNT(checker->tstack);
    size_t tstack_size = count * sizeof(type_index);
    struct tstack_state *state = malloc(sizeof *state + tstack_size);
    state->count = count;
    memcpy(state->types, checker->tstack->types, tstack_size);
    states->states[index] = state;
    return true;
}

static bool save_state(struct type_checker *checker) {
    return save_state_at(checker, checker->ip);
}

static bool load_state(struct type_checker *checker, int ip) {
    struct type_checker_states *states =&checker->states;
    size_t index = find_state(states, ip);
    struct tstack_state *state = states->states[index];
    if (state == NULL) {
        return false;
    }
    memcpy(checker->tstack->types, state->types, state->count);
    checker->tstack->top = &checker->tstack->types[state->count];
    return true;
}

static bool check_state_at(struct type_checker *checker, int ip) {
    struct type_checker_states *states = &checker->states;
    size_t index = find_state(states, ip);
    struct tstack_state *state = states->states[index];
    if (state == NULL) {
        // Did not find state.
        return false;
    }
    if ((ptrdiff_t)state->count != TSTACK_COUNT(checker->tstack)) {
        return false;
    }
    return memcmp(state->types, checker->tstack->types, state->count) == 0;
}

static bool check_state(struct type_checker *checker) {
    return check_state_at(checker, checker->ip);
}

static int find_jump_src(struct type_checker *checker) {
    struct type_checker_states *states = &checker->states;
    if (states->size == 0) {
        return -1;
    }
    size_t index = find_state(states, checker->ip);
    assert(index < (size_t)checker->in_block->jumps.count);
    if (states->states[index] == NULL) {
        // State not found.
        return -1;
    }
    return states->jump_srcs[index];
}

static bool save_jump(struct type_checker *checker, int dest_offset) {
    int dest = checker->ip + dest_offset + 1;
    struct type_checker_states *states = &checker->states;
    size_t index = find_state(states, dest);
    assert(index < (size_t)checker->in_block->jumps.count);
    states->jump_srcs[index] = checker->ip;
    if (!save_state_at(checker, dest)) {
        // If jump was already saved.
        return check_state_at(checker, dest);
    }
    return true;
}

static void check_unreachable(struct type_checker *checker) {
    while (checker->in_block->code[checker->ip + 1] == W_OP_NOP
           && !is_jump_dest(checker->in_block, checker->ip + 1)) {
        ++checker->ip;
        if (checker->ip >= checker->in_block->count) return;
    }
    if (!is_jump_dest(checker->in_block, checker->ip + 1)) {
        checker->had_error = true;
        int start_ip = checker->ip + 1;
        int ip = start_ip;
        while (ip + 1 < checker->in_block->count && !is_jump_dest(checker->in_block, ip + 1)) {
            ++ip;
        }
        if (ip + 1 < checker->in_block->count) {
            type_error(checker, "code from index %d to %d is unreachable",
                       start_ip, ip);
        }
        else {
            type_error(checker, "code from index %d to end is unreachable",
                       start_ip);
            return;
        }
        checker->ip = ip;
    }
    int src = find_jump_src(checker);
    if (src == -1) {
        type_error(checker, "could not find source of jump");
        return;
    }
    load_state(checker, src);
}

static void check_jump_instruction(struct type_checker *checker) {
    int offset = read_s16(checker->in_block, checker->ip + 1);
    if (!save_jump(checker, offset)) {
        checker->had_error = true;
        type_error(checker, "inconsistent stack after jump instruction",
                   checker->ip);
    }
}

enum type_check_result type_check(struct type_checker *checker) {
    for (; checker->ip < checker->in_block->count; ++checker->ip) {
        if (is_jump_dest(checker->in_block, checker->ip)) {
            if (!save_state(checker)) {
                // Previous state was saved here.
                if (!check_state(checker)) {
                    checker->had_error = true;
                    type_error(checker, "inconsistent stack after jump instruction", checker->ip);
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
        case T_OP_PUSH_CHAR8:
            ts_push(checker, TYPE_BYTE);
            copy_immediate_u8(checker, W_OP_PUSH_CHAR8);
            break;
        case T_OP_LOAD_STRING8:
            ts_push(checker, TYPE_PTR);
            ts_push(checker, TYPE_WORD);
            copy_immediate_u8(checker, W_OP_LOAD_STRING8);
            break;
        case T_OP_LOAD_STRING16:
            ts_push(checker, TYPE_PTR);
            ts_push(checker, TYPE_WORD);
            copy_immediate_u16(checker, W_OP_LOAD_STRING16);
            break;
        case T_OP_LOAD_STRING32:
            ts_push(checker, TYPE_PTR);
            ts_push(checker, TYPE_WORD);
            copy_immediate_u32(checker, W_OP_LOAD_STRING32);
            break;
        case T_OP_POP:
            // Don't care about type.
            ts_pop(checker);
            emit_simple(checker, W_OP_POP);
            break;
        case T_OP_ADD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            type_index result_type;
            if (!check_pointer_addition(checker, lhs_type, rhs_type)) {
                struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
                result_type = conversion.result_type;
                if (result_type == TYPE_ERROR) {
                    checker->had_error = true;
                    type_error(checker, "invalid types for `+`");
                    result_type = TYPE_WORD;  // Continue with a word.
                }
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
                emit_simple_nnop(checker, conversion.result_conv);
            }
            else {
                result_type = TYPE_PTR;
            }
            ts_push(checker, result_type);
            emit_simple(checker, W_OP_ADD);
            break;
        }
        case T_OP_AND: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            if (lhs_type != rhs_type) {
                checker->had_error = true;
                type_error(checker, "mismatched types for `and`");
                lhs_type = TYPE_WORD;
            }
            ts_push(checker, lhs_type);
            emit_simple(checker, W_OP_AND);
            break;
        }
        case T_OP_DEREF:
            if (ts_pop(checker) != TYPE_PTR) {
                checker->had_error = true;
                type_error(checker, "expected pointer");
            }
            ts_push(checker, TYPE_BYTE);
            emit_simple(checker, W_OP_DEREF);
            break;
        case T_OP_DIVMOD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
                type_error(checker, "invalid types for `divmod`");
                conversion.result_type = TYPE_WORD;
            }
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
            ts_push(checker, conversion.result_type);  // Quotient.
            ts_push(checker, conversion.result_type);  // Remainder.
            break;
        }
        case T_OP_IDIVMOD: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
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
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
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
            emit_simple(checker, W_OP_DUPE);
            break;
        }
        case T_OP_GET_LOOP_VAR:
            ts_push(checker, TYPE_INT);  // Loop variable is always an integer.
            copy_immediate_u16(checker, W_OP_GET_LOOP_VAR);
            break;
        case T_OP_MULT: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
                type_error(checker, "invalid types for `*`");
                conversion.result_type = TYPE_WORD;
            }
            emit_simple_nnop(checker, conversion.lhs_conv);
            emit_simple_nnop(checker, conversion.rhs_conv);
            emit_simple(checker, W_OP_MULT);
            emit_simple_nnop(checker, conversion.result_conv);
            ts_push(checker, conversion.result_type);
            break;
        }
        case T_OP_NOT: {
            ts_peek(checker);  // Emits error if the stack is empty.
            emit_simple(checker, W_OP_NOT);
            break;
        }
        case T_OP_OR: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            if (lhs_type != rhs_type) {
                checker->had_error = true;
                type_error(checker, "mismatched tyes for `or`:");
                lhs_type = TYPE_WORD;  // In case of an error, recover by using a word.
            }
            ts_push(checker, lhs_type);
            emit_simple(checker, W_OP_OR);
            break;
        }
        case T_OP_PRINT: {
            type_index type = ts_pop(checker);
            enum w_opcode print_instruction = W_OP_PRINT;
            if (is_signed(type)) {
                // Promote signed type to int.
                enum w_opcode conv_instruction = promote(type);
                emit_simple_nnop(checker, conv_instruction);
                print_instruction = W_OP_PRINT_INT;
            }
            emit_simple(checker, print_instruction);
            break;
        }
        case T_OP_PRINT_CHAR: {
            if (ts_pop(checker) != TYPE_BYTE) {
                checker->had_error = true;
                type_error(checker, "expected byte for `print-char`");
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
                checker->had_error = true;
                type_error(checker, "invalid type for `OP_PRINT_INT`");
            }
            emit_simple(checker, W_OP_PRINT_INT);
            break;
        }
        case T_OP_SUB: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type != TYPE_ERROR) {
                emit_simple_nnop(checker, conversion.lhs_conv);
                emit_simple_nnop(checker, conversion.rhs_conv);
            }
            else if (lhs_type == TYPE_PTR) {
                if (rhs_type == TYPE_PTR) {
                    conversion.result_type = TYPE_INT;
                }
                else if (is_integral(rhs_type)) {
                    emit_simple_nnop(checker, promote(rhs_type));
                }
                else {
                    checker->had_error = true;
                    type_error(checker, "invalid types for `-`");
                    conversion.result_type = TYPE_WORD;
                }
            }
            else {
                checker->had_error = true;
                type_error(checker, "invalid types for `-`");
                conversion.result_type = TYPE_WORD;
            }
            ts_push(checker, conversion.result_type);
            emit_simple(checker, W_OP_SUB);
            emit_simple_nnop(checker, conversion.result_conv);
            break;
        }
        case T_OP_SWAP: {
            type_index rhs_type = ts_pop(checker);
            type_index lhs_type = ts_pop(checker);
            ts_push(checker, rhs_type);
            ts_push(checker, lhs_type);
            emit_simple(checker, W_OP_SWAP);
            break;
        }
        case T_OP_AS_BYTE: {
            ts_pop(checker);
            ts_push(checker, TYPE_BYTE);
            emit_simple(checker, W_OP_ZX8);
            break;
        }
        case T_OP_AS_U8: {
            ts_pop(checker);
            ts_push(checker, TYPE_U8);
            emit_simple(checker, W_OP_ZX8);
            break;
        }
        case T_OP_AS_U16: {
            ts_pop(checker);
            ts_push(checker, TYPE_U16);
            emit_simple(checker, W_OP_ZX16);
            break;
        }
        case T_OP_AS_U32: {
            ts_pop(checker);
            ts_push(checker, TYPE_U32);
            emit_simple(checker, W_OP_ZX32);
            break;
        }
        case T_OP_AS_S8: {
            ts_pop(checker);
            ts_push(checker, TYPE_S8);
            emit_simple(checker, W_OP_ZX8);
            break;
        }
        case T_OP_AS_S16: {
            ts_pop(checker);
            ts_push(checker, TYPE_S16);
            emit_simple(checker, W_OP_ZX16);
            break;
        }
        case T_OP_AS_S32: {
            ts_pop(checker);
            ts_push(checker, TYPE_S32);
            emit_simple(checker, W_OP_ZX32);
            break;
        }
        case T_OP_EXIT: {
            type_index type = ts_pop(checker);
            if (!is_integral(type)) {
                checker->had_error = true;
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
        }
    }
    emit_simple(checker, W_OP_NOP);  // Emit final NOP.
    return (!checker->had_error) ? TYPE_CHECK_OK : TYPE_CHECK_ERROR;
}

void ts_push(struct type_checker *checker, type_index type) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top >= &tstack->types[TYPE_STACK_SIZE]) {
        checker->had_error = true;
        type_error(checker, "insufficient stack space");
        return;
    }
    *tstack->top++ = type;
}

type_index ts_pop(struct type_checker *checker) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top == tstack->types) {
        checker->had_error = true;
        type_error(checker, "insufficient stack space");
        return TYPE_ERROR;
    }
    return *--tstack->top;
}

type_index ts_peek(struct type_checker *checker) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top == tstack->types) {
        checker->had_error = true;
        type_error(checker, "insufficient stack space");
        return TYPE_ERROR;
    }
    return tstack->top[-1];
}
