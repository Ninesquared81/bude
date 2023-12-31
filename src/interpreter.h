#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <stdbool.h>

#include "ir.h"
#include "stack.h"

enum interpret_result {
    INTERPRET_OK,
    INTERPRET_ERROR,
};

struct interpreter {
    struct ir_block *block;
    struct stack *main_stack;
    struct stack *auxiliary_stack;
    struct stack *loop_stack;
};

bool init_interpreter(struct interpreter *interpreter, struct ir_block *block);
void free_interpreter(struct interpreter *interpreter);

enum interpret_result interpret(struct interpreter *interpreter);

#endif
