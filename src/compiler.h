#ifndef COMPILER_H
#define COMPILER_H

#include "ir.h"
#include "function.h"
#include "type.h"


void compile(const char *src, const char *filename, struct type_table *types,
             struct function_table *functions);

#endif

