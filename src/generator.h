#ifndef GENERATOR_H
#define GENERATOR_H

#include "asm.h"
#include "ir.h"

enum generate_result {
    GENERATE_OK,
    GENERATE_ERROR,
};

enum generate_result generate(struct ir_block *block, struct asm_block *assembly);

#endif
