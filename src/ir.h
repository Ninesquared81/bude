#ifndef IR_H
#define IR_H

#include <stdint.h>


enum opcode {
    OP_PUSH,
    OP_POP,
    OP_ADD,
    OP_MULT,
    OP_PRINT,
};

struct ir_block {
    int capacity;
    int count;
    uint8_t *code;
};

void init_block(struct ir_block *block);
void free_block(struct ir_block *block);

void write_simple(struct ir_block *block, enum opcode instruction);
void write_immediate(struct ir_block *block, enum opcode instruction, uint8_t operand);

#endif
