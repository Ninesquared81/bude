#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "ir.h"


static int immediate_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s 0x%02x\n", name, block->code[offset + 1]);
    return offset + 2;
}

static int jump_instruction(const char *name, struct ir_block *block, int offset) {
    int jump = read_s16(block, offset + 1);
    printf("%-16s %-6d (%d -> %d)\n", name, jump, offset, offset + jump);
    return offset + 3;
}

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int disassemble_instruction(struct ir_block *block, int offset) {
    uint8_t instruction = block->code[offset];
    printf("%04d [%03d] ", offset, instruction);

    switch (instruction) {
    case OP_NOP:
        return simple_instruction("OP_NOP", offset);
    case OP_PUSH:
        return immediate_instruction("OP_PUSH", block, offset);
    case OP_POP:
        return simple_instruction("OP_POP", offset);
    case OP_ADD:
        return simple_instruction("OP_ADD", offset);
    case OP_DIVMOD:
        return simple_instruction("OP_DIVMOD", offset);
    case OP_JUMP:
        return jump_instruction("OP_JUMP", block, offset);
    case OP_JUMP_COND:
        return jump_instruction("OP_JUMP_COND", block, offset);
    case OP_JUMP_NCOND:
        return jump_instruction("OP_JUMP_NCOND", block, offset);
    case OP_MULT:
        return simple_instruction("OP_MULT", offset);
    case OP_NOT:
        return simple_instruction("OP_NOT", offset);
    case OP_PRINT:
        return simple_instruction("OP_PRINT", offset);
    case OP_SUB:
        return simple_instruction("OP_SUB", offset);
    case OP_SWAP:
        return simple_instruction("OP_SWAP", offset);
    default:
        printf("<Unknown opcode>\n");
        return offset + 1;
    }
}

void disassemble_block(struct ir_block *block) {
    for (int offset = 0; offset < block->count; ) {
        offset = disassemble_instruction(block, offset);
    }
}
