#include "interpreter.h"
#include "disassembler.h"
#include "ir.h"

enum interpret_result interpret(struct ir_block *block) {
    disassemble_block(block);
    return INTERPRET_OK;
}

