#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "location.h"
#include "type_checker.h"
#include "type_punning.h"

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

static const struct type_info *expect_kind(struct type_checker *checker, enum type_kind kind) {
    type_index type = ts_pop(checker);
    const struct type_info *info = lookup_type(checker->types, type);
    if (info == NULL) {
        type_error(checker, "unknown type");
        assert(0 && "Invalid state");
    }
    if (info->kind != kind) {
        type_error(checker, "expected a '%s' type but got type '%s' instead",
                   kind_name(kind), type_name(checker->types, type));
        exit(1);
    }
    return info;
}

static void expect_type(struct type_checker *checker, type_index expected_type) {
    type_index actual_type = ts_pop(checker);
    if (actual_type != expected_type) {
        type_error(checker, "expected type '%s' but got type '%s'",
                   type_name(checker->types, expected_type),
                   type_name(checker->types, actual_type));
        exit(1);
    }
}

static void expect_keep_type(struct type_checker *checker, type_index expected_type) {
    type_index actual_type = ts_peek(checker);
    if (actual_type != expected_type) {
        type_error(checker, "expected type '%s' but got type '%s'",
                   type_name(checker->types, expected_type),
                   type_name(checker->types, actual_type));
        exit(1);
    }
}

void reset_type_stack(struct type_stack *tstack) {
    tstack->top = tstack->types;
}

static void init_type_checker_states(struct type_checker_states *states) {
    states->region = new_region(TYPE_STACK_STATES_REGION_SIZE);
    if (states->region == NULL) {
        fprintf(stderr, "Failed to allocate region for type checker states");
        exit(1);
    }
}

static void reset_type_checker_states(struct type_checker_states *states,
                                      struct jump_info_table *jumps) {
    clear_region(states->region);
    states->size = jumps->count;
    states->states    = region_calloc(states->region, states->size, sizeof *states->states);
    states->ips       = region_calloc(states->region, states->size, sizeof *states->ips);
    states->wir_dests = region_calloc(states->region, states->size, sizeof *states->wir_dests);
    states->wir_srcs  = region_calloc(states->region, states->size, sizeof *states->wir_srcs);
    memcpy(states->ips, jumps->dests, states->size * sizeof states->ips[0]);
}

static void free_type_checker_states(struct type_checker_states *states) {
    kill_region(states->region);
    // Zero out all fields.
    *states = (struct type_checker_states) {0};
}

void init_type_checker(struct type_checker *checker, struct function_table *functions,
                       struct type_table *types) {
    init_type_checker_states(&checker->states);
    // The ir_blocks will be set later.
    checker->in_block = NULL;
    checker->out_block = NULL;
    checker->functions = functions;
    checker->types = types;
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

static struct arithm_conv convert(type_index lhs, type_index rhs) {
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
    memcpy(checker->tstack->types, state->types, state->count);
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
    assert(src_node);
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
    else if (!is_signed(type)) {
        emit_simple(checker, W_OP_PRINT);
    }
    else {
        // Promote signed type to int.
        enum w_opcode conv_instruction = promote(type);
        emit_simple_nnop(checker, conv_instruction);
        emit_simple(checker, W_OP_PRINT_INT);
    }
}

static void emit_swap_comps(struct type_checker *checker, int lhs_size, int rhs_size) {
    // Note: this instruction is encoded in the same way as COMP_SUBCOMPn, so we reuse that.
    emit_comp_subcomp(checker, W_OP_SWAP_COMPS8, lhs_size, rhs_size);
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

static void check_unreachable(struct type_checker *checker) {
    while (checker->in_block->code[checker->ip + 1] == W_OP_NOP
           && !is_jump_dest(checker->in_block, checker->ip + 1)) {
        ++checker->ip;
        if (checker->ip >= checker->in_block->count) return;
    }
    if (!is_forward_jump_dest(checker, checker->ip + 1)) {
        checker->had_error = true;
        int start_ip = checker->ip + 1;
        int ip = start_ip;
        while (ip + 1 < checker->in_block->count && !is_forward_jump_dest(checker, ip + 1)) {
            ++ip;
        }
        if (ip + 1 >= checker->in_block->count) {
            type_error(checker, "code from index %d to end is unreachable", start_ip);
            return;
        }
        type_error(checker, "code from index %d to %d is unreachable", start_ip, ip);
        checker->ip = ip;
    }
    // Load previous state.
    bool success = load_state_at(checker, checker->ip + 1);
    assert(success && "could not load previous state");
}

static void check_jump_instruction(struct type_checker *checker) {
    int offset = read_s16(checker->in_block, checker->ip + 1);
    if (!save_jump(checker, offset)) {
        checker->had_error = true;
        type_error(checker, "inconsistent stack after jump instruction",
                   checker->ip);
    }
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
    for (int i = info->pack.field_count - 1; i >= 0 && info->pack.fields[i] != TYPE_ERROR; ) {
        type_index field_type = info->pack.fields[i];
        type_index arg_type = ts_pop(checker);
        if (arg_type != field_type) {
            type_error(checker, "invalid type for '%s'[%d]: expected '%s' but got '%s'.",
                       type_name(checker->types, index), i,
                       type_name(checker->types, field_type),
                       type_name(checker->types, arg_type));
        }
        int size = type_size(checker->types, field_type);
        i += (size > 0) ? size : 0;
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
    for (int i = info->comp.field_count - 1; i >= 0; --i) {
        expect_type(checker, info->comp.fields[i]);
    }
    ts_push(checker, index);
}

static void check_decomp_instruction(struct type_checker *checker) {
    const struct type_info *info = expect_kind(checker, KIND_COMP);
    for (int i = 0; i < info->comp.field_count; ++i) {
        ts_push(checker, info->comp.fields[i]);
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

enum type_check_result type_check(struct type_checker *checker) {
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
                    checker->had_error = true;
                    type_error(checker, "inconsistent stack after jump instruction");
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
            const struct type_info *info = lookup_type(checker->types, type);
            assert(info != NULL && "Unknown type");
            if (type == TYPE_STRING) {
                emit_immediate_s8(checker, W_OP_POPN8, 2);
            }
            if (info->kind != KIND_COMP) {
                emit_simple(checker, W_OP_POP);
            }
            else {
                emit_immediate_sv(checker, W_OP_POPN8, info->comp.field_count);
            }
            break;
        }
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
            const struct type_info *info = lookup_type(checker->types, type);
            assert(info != NULL && "Unknown type");
            if (type == TYPE_STRING) {
                // Strings are a special case: they count as simple types, but behave like a comp.
                // However, since they are built in, we aready know their field types (ptr, int).
                emit_immediate_s8(checker, W_OP_DUPEN8, 2);
            }
            else if (info->kind != KIND_COMP) {
                emit_simple(checker, W_OP_DUPE);
            }
            else {
                emit_immediate_sv(checker, W_OP_DUPEN8, info->comp.word_count);
            }
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
            emit_print_instruction(checker, type);
            break;
        }
        case T_OP_PRINT_CHAR: {
            type_index type = ts_pop(checker);
            if (type != TYPE_CHAR && type != TYPE_BYTE) {
                checker->had_error = true;
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
            if (!is_comp(checker->types, lhs_type) && !is_comp(checker->types, rhs_type)) {
                emit_simple(checker, W_OP_SWAP);
            }
            else {
                const struct type_info *lhs_info = lookup_type(checker->types, lhs_type);
                const struct type_info *rhs_info = lookup_type(checker->types, rhs_type);
                assert(lhs_info != NULL);
                assert(rhs_info != NULL);
                int lhs_size = (lhs_info->kind == KIND_COMP) ? lhs_info->comp.word_count : 1;
                int rhs_size = (rhs_info->kind == KIND_COMP) ? rhs_info->comp.word_count : 1;
                emit_swap_comps(checker, lhs_size, rhs_size);
            }
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
