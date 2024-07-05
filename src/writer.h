#ifndef WRITER_H
#define WRITER_H

#include <stdio.h>

#include "module.h"

void display_bytecode(struct module *module, FILE *f);
int write_bytecode(struct module *module, FILE *f);
int write_bytecode_ex(struct module *module, FILE *f, int version_number);

#endif
