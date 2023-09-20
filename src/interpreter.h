#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ir.h"
#include "stack.h"

enum interpret_result {
    INTERPRET_OK,
    INTERPRET_ERROR,
};

enum interpret_result interpret(struct ir_block *block, struct stack *stack);

#endif
