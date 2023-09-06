#ifndef IR_H
#define IR_H

#include <stdint.h>


#define IS_JUMP(instruction) (                   \
        instruction == OP_JUMP       ||          \
        instruction == OP_JUMP_COND  ||          \
        instruction == OP_JUMP_NCOND  )

enum opcode {
    OP_NOP,
    OP_PUSH,
    OP_POP,
    OP_ADD,
    OP_DIVMOD,
    OP_DUPE,
    OP_JUMP,
    OP_JUMP_COND,
    OP_JUMP_NCOND,
    OP_MULT,
    OP_NOT,
    OP_PRINT,
    OP_SUB,
    OP_SWAP,
};

struct ir_block {
    int capacity;
    int count;
    uint8_t *code;
};

void init_block(struct ir_block *block);
void free_block(struct ir_block *block);

void write_simple(struct ir_block *block, enum opcode instruction);

void write_immediate_u8(struct ir_block *block, enum opcode instruction, uint8_t operand);
void write_immediate_s8(struct ir_block *block, enum opcode instruction, int8_t operand);
void write_immediate_u16(struct ir_block *block, enum opcode instruction, uint16_t operand);
void write_immediate_s16(struct ir_block *block, enum opcode instruction, int16_t operand);

void overwrite_u8(struct ir_block *block, int index, uint8_t value);
void overwrite_s8(struct ir_block *block, int index, int8_t value);
void overwrite_u16(struct ir_block *block, int index, uint16_t value);
void overwrite_s16(struct ir_block *block, int index, int16_t value);

uint8_t read_u8(struct ir_block *block, int index);
int8_t read_s8(struct ir_block *block, int index);
uint16_t read_u16(struct ir_block *block, int index);
int16_t read_s16(struct ir_block *block, int index);


#endif
