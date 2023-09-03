#ifndef COMPILER_H
#define COMPILER_H

#include "ir.h"


void compile(const char *restrict src, struct ir_block *block);

#endif

