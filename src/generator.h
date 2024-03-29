#ifndef GENERATOR_H
#define GENERATOR_H

#include "asm.h"
#include "module.h"

enum generate_result {
    GENERATE_OK,
    GENERATE_ERROR,
};

enum generate_result generate(struct module *module, struct asm_block *assembly);

#endif
