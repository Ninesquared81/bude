#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker.h"

struct oper_type {
    enum type type;
    enum opcode lhs_conversion;
    enum opcode rhs_conversion;
    enum opcode oper;
};

void init_type_checker(struct type_checker *checker, struct ir_block *block) {
    checker->block = block;
    checker->tstack = malloc(sizeof *checker->tstack);
    checker->ip = 0;
    checker->had_error = false;
}

void free_type_checker(struct type_checker *checker) {
    free(checker->tstack);
    checker->tstack = NULL;
}

static bool is_integral_type(enum type type) {
    switch (type) {
    case TYPE_WORD:
    case TYPE_BYTE:
    case TYPE_INT:
        return true;
    default:
        return false;
    }
}

static void rewrite_types(struct type_checker *checker, struct oper_type *oper_type) {
    struct ir_block *block = checker->block;
    int ip = checker->ip;
    overwrite_instruction(block, ip - 2, oper_type->lhs_conversion);
    overwrite_instruction(block, ip - 1, oper_type->rhs_conversion);
    overwrite_instruction(block, ip, oper_type->oper);
}

enum type_check_result type_check(struct type_checker *checker) {
    for (; checker->ip < checker->block->count; ++checker->ip) {
        switch (checker->block->code[checker->ip]) {
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
            struct oper_type result_table[TYPE_COUNT][TYPE_COUNT] = {
                [TYPE_WORD][TYPE_WORD] = {TYPE_WORD, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_WORD][TYPE_BYTE] = {TYPE_WORD, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_WORD][TYPE_PTR]  = {TYPE_PTR,  OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_WORD][TYPE_INT]  = {TYPE_WORD, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_BYTE][TYPE_WORD] = {TYPE_WORD, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_BYTE][TYPE_BYTE] = {TYPE_BYTE, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_BYTE][TYPE_PTR]  = {TYPE_PTR,  OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_BYTE][TYPE_INT]  = {TYPE_INT,  OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_PTR][TYPE_WORD]  = {TYPE_PTR,  OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_PTR][TYPE_BYTE]  = {TYPE_PTR,  OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_PTR][TYPE_INT]   = {TYPE_PTR,  OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_INT][TYPE_WORD]  = {TYPE_WORD, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_INT][TYPE_BYTE]  = {TYPE_WORD, OP_NOP,  OP_NOP, OP_ADD},
                [TYPE_INT][TYPE_PTR]   = {TYPE_PTR,  OP_SWAP, OP_NOP, OP_ADD},
                [TYPE_INT][TYPE_INT]   = {TYPE_INT,  OP_NOP,  OP_NOP, OP_ADD},
            };
            struct oper_type result = result_table[lhs_type][rhs_type];
            if (result.type == TYPE_ERROR) {
                checker->had_error = true;
                fprintf(stderr, "Type error in `+`.\n");
                result.type = TYPE_WORD;  // Continue with a word.
            }
            ts_push(checker, result.type);
            rewrite_types(checker, &result);
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
            struct oper_type result_table[TYPE_COUNT][TYPE_COUNT] = {
                [TYPE_WORD][TYPE_WORD] = {TYPE_WORD, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_WORD][TYPE_BYTE] = {TYPE_WORD, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_WORD][TYPE_INT]  = {TYPE_WORD, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_BYTE][TYPE_WORD] = {TYPE_WORD, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_BYTE][TYPE_BYTE] = {TYPE_BYTE, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_BYTE][TYPE_INT]  = {TYPE_INT,  OP_NOP, OP_NOP, OP_IDIVMOD},
                [TYPE_INT][TYPE_WORD]  = {TYPE_WORD, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_INT][TYPE_BYTE]  = {TYPE_WORD, OP_NOP, OP_NOP, OP_DIVMOD},
                [TYPE_INT][TYPE_INT]   = {TYPE_INT,  OP_NOP, OP_NOP, OP_EDIVMOD},
            };
            struct oper_type result = result_table[lhs_type][rhs_type];
            if (result.type == TYPE_ERROR) {
                checker->had_error = true;
                fprintf(stderr, "Type error: invalid type for `divmod`");
                result.type = TYPE_WORD;
            }
            ts_push(checker, result.type);
            rewrite_types(checker, &result);
            break;
        }
        case OP_DUPE: {
            enum type type = ts_pop(checker);
            ts_push(checker, type);
            ts_push(checker, type);
            break;
        }
        case OP_GET_LOOP_VAR:
            ts_push(checker, TYPE_INT);  // Loop variable is always an integer.
            break;
        case OP_MULT: {
            // integral types.
            break;
        }
        case OP_NOT: {
            ts_peek(checker);  // Emits error if the stack is empty.
            break;
        }
        case OP_OR: {
            break;
        }
        case OP_PRINT: {
            enum type type = ts_pop(checker);
            (void)type;
            break;
        }
        case OP_PRINT_CHAR: {
            if (ts_pop(checker) != TYPE_BYTE) {
                checker->had_error = true;
                fprintf(stderr, "Type error: expected byte for `print-char`.\n");
            }
            break;
        }
        case OP_SUB: {
            // integral types; ptr and integral; ptr and ptr.
            break;
        }
        case OP_SWAP: {
            enum type rhs_type = ts_pop(checker);
            enum type lhs_type = ts_pop(checker);
            ts_push(checker, rhs_type);
            ts_push(checker, lhs_type);
            break;
        }
        case OP_EXIT: {
            enum type type = ts_pop(checker);
            if (!is_integral_type(type)) {
                checker->had_error = true;
                fprintf(stderr, "Type error: expected integral type for `exit`.\n");
            }
            // TODO: work out how to handle end of control flow here.
        }
        }
    }
    return (!checker->had_error) ? TYPE_CHECK_OK : TYPE_CHECK_ERR;
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
