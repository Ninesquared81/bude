#ifndef COMPILER_H
#define COMPILER_H

#include "ir.h"
#include "type.h"


void compile(const char *src, struct ir_block *block, const char *filename,
             struct type_table *types);

#endif

