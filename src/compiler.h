#ifndef COMPILER_H
#define COMPILER_H

#include "ir.h"


void compile(const char *src, struct ir_block *block, const char *filename);

#endif

