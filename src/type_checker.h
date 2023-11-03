#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include <assert.h>

#include "ir.h"
#include "stack.h"

#define TYPE_STACK_SIZE STACK_SIZE

enum type {
    TYPE_ERROR,

    TYPE_WORD,
    TYPE_BYTE,
    TYPE_PTR,
    TYPE_INT,

    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_S8,
    TYPE_S16,
    TYPE_S32,
};

#define TYPE_COUNT 11
static_assert(TYPE_COUNT == TYPE_S32 + 1);
static_assert(TYPE_ERROR == 0);

#define TSTACK_COUNT(tstack) (tstack->top - tstack->types)

struct type_stack {
    enum type *top;
    enum type types[TYPE_STACK_SIZE];
};

struct tstack_state {
    size_t count;
    enum type types[];
};

struct type_checker_states {
    size_t size;
    struct tstack_state **states;
    int *ips;
    int *jump_srcs;
};

struct type_checker {
    struct type_checker_states states;
    struct ir_block *block;
    struct type_stack *tstack;
    int ip;
    bool had_error;
};

enum type_check_result {
    TYPE_CHECK_OK,
    TYPE_CHECK_ERROR,
};

void reset_type_stack(struct type_stack *tstack);

void init_type_checker(struct type_checker *checker, struct ir_block *block);
void free_type_checker(struct type_checker *checker);

void ts_push(struct type_checker *checker, enum type type);
enum type ts_pop(struct type_checker *checker);
enum type ts_peek(struct type_checker *checker);

enum type_check_result type_check(struct type_checker *checker);

#endif
