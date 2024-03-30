#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "ir.h"
#include "module.h"

void disassemble_block(struct ir_block *block);
void disassemble_tir(struct module *module);
void disassemble_wir(struct module *module);

#endif
