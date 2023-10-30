#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "ir.h"


static int immediate_u8_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s %u\n", name, read_u8(block, offset + 1));
    return offset + 2;
}

static int immediate_u16_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s %u\n", name, read_u16(block, offset + 1));
    return offset + 3;
}

static int immediate_u32_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s %u\n", name, read_u32(block, offset + 1));
    return offset + 5;
}

static int immediate_s8_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s %d\n", name, read_s8(block, offset + 1));
    return offset + 2;
}

static int immediate_s16_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s %d\n", name, read_s16(block, offset + 1));
    return offset + 3;
}

static int immediate_s32_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%-16s %d\n", name, read_s32(block, offset + 1));
    return offset + 5;
}

static int jump_instruction(const char *name, struct ir_block *block, int offset) {
    int jump = read_s16(block, offset + 1);
    printf("%-16s %-6d (%d -> %d)\n", name, jump, offset, offset + jump + 1);
    return offset + 3;
}

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int disassemble_instruction(struct ir_block *block, int offset) {
    enum opcode instruction = block->code[offset];
    printf("%c %04d [%03d] ",
           (is_jump_dest(block, offset)) ? '*': ' ', offset, instruction);

    switch (instruction) {
    case OP_NOP:
        return simple_instruction("OP_NOP", offset);
    case OP_PUSH8:
        return immediate_s8_instruction("OP_PUSH8", block, offset);
    case OP_PUSH16:
        return immediate_s16_instruction("OP_PUSH16", block, offset);
    case OP_PUSH32:
        return immediate_s32_instruction("OP_PUSH32", block, offset);
    case OP_LOAD8:
        return immediate_u8_instruction("OP_LOAD8", block, offset);
    case OP_LOAD16:
        return immediate_u16_instruction("OP_LOAD16", block, offset);
    case OP_LOAD32:
        return immediate_u32_instruction("OP_LOAD32", block, offset);
    case OP_LOAD_STRING8:
        return immediate_u8_instruction("OP_LOAD_STRING8", block, offset);
    case OP_LOAD_STRING16:
        return immediate_u16_instruction("OP_LOAD_STRING16", block, offset);
    case OP_LOAD_STRING32:
        return immediate_u32_instruction("OP_LOAD_STRING32", block, offset);
    case OP_POP:
        return simple_instruction("OP_POP", offset);
    case OP_ADD:
        return simple_instruction("OP_ADD", offset);
    case OP_AND:
        return simple_instruction("OP_AND", offset);
    case OP_DEREF:
        return simple_instruction("OP_DEREF", offset);
    case OP_DIVMOD:
        return simple_instruction("OP_DIVMOD", offset);
    case OP_IDIVMOD:
        return simple_instruction("OP_IDIVMOD", offset);
    case OP_EDIVMOD:
        return simple_instruction("OP_EDIVMOD", offset);
    case OP_DUPE:
        return simple_instruction("OP_DUPE", offset);
    case OP_EXIT:
        return simple_instruction("OP_EXIT", offset);
    case OP_FOR_DEC_START:
        return jump_instruction("OP_FOR_DEC_START", block, offset);
    case OP_FOR_DEC:
        return jump_instruction("OP_FOR_DEC", block, offset);
    case OP_FOR_INC_START:
        return jump_instruction("OP_FOR_INC_START", block, offset);
    case OP_FOR_INC:
        return jump_instruction("OP_FOR_INC", block, offset);
    case OP_GET_LOOP_VAR:
        return immediate_u16_instruction("OP_GET_LOOP_VAR", block, offset);
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
    case OP_OR:
        return simple_instruction("OP_OR", offset);
    case OP_PRINT:
        return simple_instruction("OP_PRINT", offset);
    case OP_PRINT_CHAR:
        return simple_instruction("OP_PRINT_CHAR", offset);
    case OP_PRINT_INT:
        return simple_instruction("OP_PRINT_INT", offset);
    case OP_SUB:
        return simple_instruction("OP_SUB", offset);
    case OP_SWAP:
        return simple_instruction("OP_SWAP", offset);
    }
    // Not in switch so that the compiler can ensure all cases are handled.
    printf("<Unknown opcode>\n");
    return offset + 1;
}

void disassemble_block(struct ir_block *block) {
    for (int offset = 0; offset < block->count; ) {
        offset = disassemble_instruction(block, offset);
    }
}
