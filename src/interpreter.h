#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <stdbool.h>

#include "function.h"
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
    struct stack *call_stack;
    struct function_table *functions;
    int current_function;
};

bool init_interpreter(struct interpreter *interpreter, struct function_table *functions);
void free_interpreter(struct interpreter *interpreter);

enum interpret_result interpret(struct interpreter *interpreter);

#endif
