#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ir.h"

enum interpret_result {
    INTERPRET_OK,
    INTERPRET_ERROR,
};

enum interpret_result interpret(struct ir_block *block);

#endif
