#ifndef COMPILER_H
#define COMPILER_H

#include "ir.h"
#include "module.h"
#include "symbol.h"

void compile(const char *src, struct module *module, struct symbol_dictionary *symbols);

#endif
