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
};

#define TYPE_COUNT 5
static_assert(TYPE_COUNT == TYPE_INT + 1);
static_assert(TYPE_ERROR == 0);

struct type_stack {
    enum type *top;
    enum type types[TYPE_STACK_SIZE];
};

struct type_checker {
    struct ir_block *block;
    struct type_stack *tstack;
    int ip;
    bool had_error;
};

enum type_check_result {
    TYPE_CHECK_OK,
    TYPE_CHECK_ERR,
};

void init_type_checker(struct type_checker *checker, struct ir_block *block);
void free_type_checker(struct type_checker *checker);

void ts_push(struct type_checker *checker, enum type type);
enum type ts_pop(struct type_checker *checker);
enum type ts_peek(struct type_checker *checker);

enum type_check_result type_check(struct type_checker *checker);

#endif
