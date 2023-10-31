#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker.h"

struct arithm_conv {
    enum type result_type;
    enum opcode lhs_conv;
    enum opcode rhs_conv;
    enum opcode result_conv;
};

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

void init_type_checker(struct type_checker *checker, struct ir_block *block) {
    init_type_checker_states(&checker->states, &block->jumps);
    checker->block = block;
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

static struct arithm_conv arithmetic_conversions[TYPE_COUNT][TYPE_COUNT] = {
    [TYPE_WORD][TYPE_WORD] = {TYPE_WORD, OP_NOP, OP_NOP, OP_NOP},
    [TYPE_WORD][TYPE_BYTE] = {TYPE_WORD, OP_NOP, OP_NOP, OP_NOP},
    [TYPE_WORD][TYPE_INT]  = {TYPE_WORD, OP_NOP, OP_NOP, OP_NOP},
    [TYPE_BYTE][TYPE_WORD] = {TYPE_WORD, OP_NOP, OP_NOP, OP_NOP},
    [TYPE_BYTE][TYPE_BYTE] = {TYPE_BYTE, OP_NOP, OP_NOP, OP_ZX8},
    [TYPE_BYTE][TYPE_INT]  = {TYPE_INT,  OP_NOP, OP_NOP, OP_NOP},
    [TYPE_INT][TYPE_WORD]  = {TYPE_WORD, OP_NOP, OP_NOP, OP_NOP},
    [TYPE_INT][TYPE_BYTE]  = {TYPE_INT,  OP_NOP, OP_NOP, OP_NOP},
    [TYPE_INT][TYPE_INT]   = {TYPE_INT,  OP_NOP, OP_NOP, OP_NOP},
};

static bool is_integral(enum type type) {
    switch (type) {
    case TYPE_WORD:
    case TYPE_BYTE:
    case TYPE_INT:
        return true;
    default:
        return false;
    }
}

static bool is_signed(enum type type) {
    switch (type) {
    case TYPE_INT:
        return true;
    default:
        return false;
    }
}

static enum opcode promote(enum type type) {
    return arithmetic_conversions[TYPE_INT][type].rhs_conv;
}

static enum opcode sign_extend(enum type type) {
    switch (type) {
    case TYPE_BYTE:
        return OP_SX8;
    default:
        return OP_NOP;
    }
}

static bool check_pointer_addition(struct type_checker *checker,
                                   enum type lhs_type, enum type rhs_type) {
    enum opcode conversion;
    if (lhs_type == TYPE_PTR) {
        if (rhs_type == TYPE_PTR) {
            checker->had_error = true;
            fprintf(stderr, "Type error: cannot add two pointers.\n");
        }
        conversion = promote(rhs_type);
    }
    else if (rhs_type == TYPE_PTR) {
        conversion = promote(lhs_type);
    }
    else {
        return false;
    }
    overwrite_instruction(checker->block, checker->ip - 1, conversion);
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
    assert(index < (size_t)checker->block->jumps.count);
    if (states->states[index] != NULL) {
        // There was already a state saved there.
        return false;
    }
    size_t count = TSTACK_COUNT(checker->tstack);
    size_t tstack_size = count * sizeof(enum type);
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
    assert(index < (size_t)checker->block->jumps.count);
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
    assert(index < (size_t)checker->block->jumps.count);
    states->jump_srcs[index] = checker->ip;
    if (!save_state_at(checker, dest)) {
        // If jump was already saved.
        return check_state_at(checker, dest);
    }
    return true;
}

static void check_unreachable(struct type_checker *checker) {
    while (checker->block->code[checker->ip + 1] == OP_NOP
           && !is_jump_dest(checker->block, checker->ip + 1)) {
        ++checker->ip;
        if (checker->ip >= checker->block->count) return;
    }
    if (!is_jump_dest(checker->block, checker->ip + 1)) {
        checker->had_error = true;
        int start_ip = checker->ip + 1;
        int ip = start_ip;
        while (ip + 1 < checker->block->count && !is_jump_dest(checker->block, ip + 1)) {
            ++ip;
        }
        if (ip + 1 < checker->block->count) {
            fprintf(stderr, "Type error: code from index %d to %d is unreachable.\n",
                    start_ip, ip);
        }
        else {
            fprintf(stderr, "Type error: code from index %d to end is unreachable.\n",
                    start_ip);
            return;
        }
        checker->ip = ip;
    }
    int src = find_jump_src(checker);
    if (src == -1) {
        fprintf(stderr, "Type error: could not find source of jump.\n");
        return;
    }
    load_state(checker, src);
}

static void check_jump_instruction(struct type_checker *checker) {
    int offset = read_s16(checker->block, checker->ip + 1);
    if (!save_jump(checker, offset)) {
        checker->had_error = true;
        fprintf(stderr, "Type error at %d: inconsistent stack after jump instruction.\n",
                checker->ip);
    }
    checker->ip += 2;
}

enum type_check_result type_check(struct type_checker *checker) {
    for (; checker->ip < checker->block->count; ++checker->ip) {
        if (is_jump_dest(checker->block, checker->ip)) {
            if (!save_state(checker)) {
                // Previous state was saved here.
                if (!check_state(checker)) {
                    checker->had_error = true;
                    fprintf(stderr,
                            "Type error at %d: inconsistent stack after jump instruction.\n",
                            checker->ip);
                }
            }
        }
        enum opcode instruction = checker->block->code[checker->ip];
        switch (instruction) {
        case OP_NOP:
            /* Do nothing. */
            break;
        case OP_PUSH8:
            ++checker->ip;
            ts_push(checker, TYPE_INT);
            break;
        case OP_PUSH16:
            checker->ip += 2;
            ts_push(checker, TYPE_INT);
            break;
        case OP_PUSH32:
            checker->ip += 4;
            ts_push(checker, TYPE_INT);
            break;
        case OP_LOAD8:
            ++checker->ip;
            ts_push(checker, TYPE_WORD);
            break;
        case OP_LOAD16:
            checker->ip += 2;
            ts_push(checker, TYPE_WORD);
            break;
        case OP_LOAD32:
            checker->ip += 4;
            ts_push(checker, TYPE_WORD);
            break;
        case OP_LOAD_STRING8:
            ++checker->ip;
            ts_push(checker, TYPE_PTR);
            ts_push(checker, TYPE_WORD);
            break;
        case OP_LOAD_STRING16:
            checker->ip += 2;
            ts_push(checker, TYPE_PTR);
            ts_push(checker, TYPE_WORD);
            break;
        case OP_LOAD_STRING32:
            checker->ip += 4;
            ts_push(checker, TYPE_PTR);
            ts_push(checker, TYPE_WORD);
            break;
        case OP_POP:
            // Don't care about type.
            ts_pop(checker);
            break;
        case OP_ADD: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            enum type result_type;
            if (!check_pointer_addition(checker, lhs_type, rhs_type)) {
                struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
                result_type = conversion.result_type;
                if (result_type == TYPE_ERROR) {
                    checker->had_error = true;
                    fprintf(stderr, "Type error in `+`.\n");
                    result_type = TYPE_WORD;  // Continue with a word.
                }
                overwrite_instruction(checker->block, checker->ip - 2, conversion.lhs_conv);
                overwrite_instruction(checker->block, checker->ip - 1, conversion.rhs_conv);
                overwrite_instruction(checker->block, checker->ip + 1, conversion.result_conv);
            }
            else {
                result_type = TYPE_PTR;
            }
            ts_push(checker, result_type);
            ++checker->ip;  // Skip result conversion.
            break;
        }
        case OP_AND: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            if (lhs_type != rhs_type) {
                checker->had_error = true;
                fprintf(stderr, "Mismatched types for `and`.\n");
                lhs_type = TYPE_WORD;
            }
            ts_push(checker, lhs_type);
            break;
        }
        case OP_DEREF:
            if (ts_pop(checker) != TYPE_PTR) {
                checker->had_error = true;
                fprintf(stderr, "Type error: expected pointer.\n");
            }
            ts_push(checker, TYPE_BYTE);
            break;
        case OP_DIVMOD: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
                fprintf(stderr, "Type error: invalid types for `divmod`.\n");
                conversion.result_type = TYPE_WORD;
            }
            if (is_signed(conversion.result_type)) {
                // In bude, division is Euclidean (remainder always non-negative) by default.
                // Use OP_IDIVMOD (truncated) if possible.
                enum opcode signed_instruction = (is_signed(lhs_type)) ? OP_EDIVMOD : OP_IDIVMOD;
                overwrite_instruction(checker->block, checker->ip, signed_instruction);
            }
            overwrite_instruction(checker->block, checker->ip - 2, conversion.lhs_conv);
            overwrite_instruction(checker->block, checker->ip - 1, conversion.rhs_conv);
            overwrite_instruction(checker->block, checker->ip + 1, conversion.result_conv);
            ts_push(checker, conversion.result_type);  // Quotient.
            ts_push(checker, conversion.result_type);  // Remainder.
            ++checker->ip;  // Skip result conversion.
            break;
        }
        case OP_IDIVMOD:
        case OP_EDIVMOD: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
                fprintf(stderr, "Type error: invalid types for `idivmod`.\n");
                conversion.result_type = TYPE_WORD;
            }
            overwrite_instruction(checker->block, checker->ip - 2, conversion.lhs_conv);
            overwrite_instruction(checker->block, checker->ip - 1, conversion.rhs_conv);
            overwrite_instruction(checker->block, checker->ip + 1, conversion.result_conv);
            ts_push(checker, conversion.result_type);
            ts_push(checker, conversion.result_type);
            ++checker->ip;  // Skip result conversion.
            break;
        }
        case OP_DUPE: {
            enum type type = ts_pop(checker);
            ts_push(checker, type);
            ts_push(checker, type);
            break;
        }
        case OP_GET_LOOP_VAR:
            checker->ip += 2;  // Loop variable.
            ts_push(checker, TYPE_INT);  // Loop variable is always an integer.
            break;
        case OP_MULT: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type == TYPE_ERROR) {
                checker->had_error = true;
                fprintf(stderr, "Type error: invalid types for `*`.\n");
                conversion.result_type = TYPE_WORD;
            }
            overwrite_instruction(checker->block, checker->ip - 2, conversion.lhs_conv);
            overwrite_instruction(checker->block, checker->ip - 1, conversion.rhs_conv);
            overwrite_instruction(checker->block, checker->ip + 1, conversion.result_conv);
            ++checker->ip;  // Skip result conversion.
            ts_push(checker, conversion.result_type);
            break;
        }
        case OP_NOT: {
            ts_peek(checker);  // Emits error if the stack is empty.
            break;
        }
        case OP_OR: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            if (lhs_type != rhs_type) {
                checker->had_error = true;
                fprintf(stderr, "Type error: mismatched tyes for `or`:.\n");
                lhs_type = TYPE_WORD;  // In case of an error, recover by using a word.
            }
            ts_push(checker, lhs_type);
            break;
        }
        case OP_PRINT: {
            enum type type = ts_pop(checker);
            if (is_signed(type)) {
                // Promote signed type to int.
                enum opcode conv_instruction = promote(type);
                overwrite_instruction(checker->block, checker->ip - 1, conv_instruction);
                overwrite_instruction(checker->block, checker->ip, OP_PRINT_INT);
            }
            break;
        }
        case OP_PRINT_CHAR: {
            if (ts_pop(checker) != TYPE_BYTE) {
                checker->had_error = true;
                fprintf(stderr, "Type error: expected byte for `print-char`.\n");
            }
            break;
        }
        case OP_PRINT_INT: {
            enum type type = ts_pop(checker);
            if (is_integral(type)) {
                enum opcode conv_instruction = sign_extend(type);
                overwrite_instruction(checker->block, checker->ip - 1, conv_instruction);
            }
            else {
                checker->had_error = true;
                fprintf(stderr, "Type error: invalid type for `OP_PRINT_CHAR`.\n");
            }
            break;
        }
        case OP_SUB: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            struct arithm_conv conversion = arithmetic_conversions[lhs_type][rhs_type];
            if (conversion.result_type != TYPE_ERROR) {
                overwrite_instruction(checker->block, checker->ip - 2, conversion.lhs_conv);
                overwrite_instruction(checker->block, checker->ip - 1, conversion.rhs_conv);
                overwrite_instruction(checker->block, checker->ip + 1, conversion.result_conv);
            }
            else if (lhs_type == TYPE_PTR) {
                if (rhs_type == TYPE_PTR) {
                    conversion.result_type = TYPE_INT;
                }
                else if (is_integral(rhs_type)) {
                    overwrite_instruction(checker->block, checker->ip, promote(rhs_type));
                }
                else {
                    checker->had_error = true;
                    fprintf(stderr, "Type error: invalid types for `-`.\n");
                    conversion.result_type = TYPE_WORD;
                }
            }
            else {
                checker->had_error = true;
                fprintf(stderr, "Type error: invalid types for `-`.\n");
                conversion.result_type = TYPE_WORD;
            }
            ts_push(checker, conversion.result_type);
            ++checker->ip;  // Skip result conversion.
            break;
        }
        case OP_SWAP: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            ts_push(checker, rhs_type);
            ts_push(checker, lhs_type);
            break;
        }
        case OP_SX8:
        case OP_SX8L:
        case OP_SX16:
        case OP_SX16L:
        case OP_SX32:
        case OP_SX32L:
        case OP_ZX8:
        case OP_ZX8L:
        case OP_ZX16:
        case OP_ZX16L:
        case OP_ZX32:
        case OP_ZX32L:
            // No type checking.
            break;
        case OP_EXIT: {
            enum type type = ts_pop(checker);
            if (!is_integral(type)) {
                checker->had_error = true;
                fprintf(stderr, "Type error: expected integral type for `exit`.\n");
            }
            check_unreachable(checker);
            break;
        }
        case OP_JUMP_COND:
        case OP_JUMP_NCOND:
            ts_pop(checker);
            check_jump_instruction(checker);
            break;
        case OP_JUMP:
            check_jump_instruction(checker);
            check_unreachable(checker);
            break;
        case OP_FOR_DEC_START:
        case OP_FOR_INC_START:
            ts_pop(checker);
            /* Fallthrough */
        case OP_FOR_DEC:
        case OP_FOR_INC:
            check_jump_instruction(checker);
            break;
        }
    }
    return (!checker->had_error) ? TYPE_CHECK_OK : TYPE_CHECK_ERROR;
}

void ts_push(struct type_checker *checker, enum type type) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top >= &tstack->types[TYPE_STACK_SIZE]) {
        checker->had_error = true;
        fprintf(stderr, "Insufficient stack space.\n");
        return;
    }
    *tstack->top++ = type;
}

enum type ts_pop(struct type_checker *checker) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top == tstack->types) {
        checker->had_error = true;
        fprintf(stderr, "Insufficient stack space.\n");
        return TYPE_ERROR;
    }
    return *--tstack->top;
}

enum type ts_peek(struct type_checker *checker) {
    struct type_stack *tstack = checker->tstack;
    if (tstack->top == tstack->types) {
        checker->had_error = true;
        fprintf(stderr, "Insufficient stack space.\n");
        return TYPE_ERROR;
    }
    return tstack->top[-1];
}
