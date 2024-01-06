#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include <assert.h>

#include "ir.h"
#include "stack.h"
#include "type.h"

#define TYPE_STACK_SIZE STACK_SIZE

#define TSTACK_COUNT(tstack) (tstack->top - tstack->types)

struct type_stack {
    type_index *top;
    type_index types[TYPE_STACK_SIZE];
};

struct tstack_state {
    size_t count;
    type_index types[];
};

struct type_checker_states {
    size_t size;
    struct tstack_state **states;
    int *ips;
    int *jump_srcs;
};

struct type_checker {
    struct type_checker_states states;
    struct ir_block *in_block;
    struct ir_block *out_block;
    struct type_stack *tstack;
    struct type_table *types;
    int ip;
    bool had_error;
};

enum type_check_result {
    TYPE_CHECK_OK,
    TYPE_CHECK_ERROR,
};

void reset_type_stack(struct type_stack *tstack);

void init_type_checker(struct type_checker *checker, struct ir_block *in_block,
                       struct ir_block *out_block);
void free_type_checker(struct type_checker *checker);

void ts_push(struct type_checker *checker, type_index type);
type_index ts_pop(struct type_checker *checker);
type_index ts_peek(struct type_checker *checker);

enum type_check_result type_check(struct type_checker *checker);

#endif
