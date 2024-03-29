#ifndef COMPILER_H
#define COMPILER_H

#include "ir.h"
#include "module.h"

void compile(const char *src, struct module *module);

#endif

