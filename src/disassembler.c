#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "ir.h"


static int immediate_instruction(const char *name, struct ir_block *block, int offset) {
    int opcode = block->code[offset];
    int operand = block->code[offset + 1];
    int padding = strlen(name) - 16;  // Negative.
    assert(padding < 0);
    printf("%s:%*d 0x%02x\n", name, padding, opcode, operand);
    return offset + 2;
}

static int simple_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%s:%d\n", name, block->code[offset]);
    return offset + 1;
}

static int disassemble_instruction(struct ir_block *block, int offset) {
    printf("%04d ", offset);

    uint8_t instruction = block->code[offset];
    switch (instruction) {
    case OP_PUSH:
        return immediate_instruction("OP_PUSH", block, offset);
    case OP_POP:
        return simple_instruction("OP_POP", block, offset);
    case OP_ADD:
        return simple_instruction("OP_ADD", block, offset);
    case OP_MULT:
        return simple_instruction("OP_MULT", block, offset);
    case OP_PRINT:
        return simple_instruction("OP_PRINT", block, offset);
    default:
        printf("Unknown opcode: %d\n", instruction);
        return offset + 1;
    }
}

void disassemble_block(struct ir_block *block) {
    for (int offset = 0; offset < block->count; ) {
        offset = disassemble_instruction(block, offset);
    }
}
