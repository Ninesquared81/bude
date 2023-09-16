#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stdint.h>


#define IS_JUMP(instruction) (                   \
        instruction == OP_JUMP       ||          \
        instruction == OP_JUMP_COND  ||          \
        instruction == OP_JUMP_NCOND  )

enum opcode {
    OP_NOP,
    OP_PUSH8,
    OP_PUSH16,
    OP_PUSH32,
    OP_LOAD8,
    OP_LOAD16,
    OP_LOAD32,
    OP_POP,
    OP_ADD,
    OP_DEREF,
    OP_DIVMOD,
    OP_DUPE,
    OP_JUMP,
    OP_JUMP_COND,
    OP_JUMP_NCOND,
    OP_MULT,
    OP_NOT,
    OP_PRINT,
    OP_PRINT_CHAR,
    OP_SUB,
    OP_SWAP,
};

struct constant_table {
    int capacity;
    int count;
    uint64_t *data;
};

struct jump_info_table {
    int capacity;
    int count;
    int *dests;
};

struct memory_handler {
    int capacity;
    int count;
    struct mem_obj *objects;
};

struct ir_block {
    int capacity;
    int count;
    uint8_t *code;
    struct constant_table constants;
    struct jump_info_table jumps;
    struct region *static_memory;
};

void init_block(struct ir_block *block);
void free_block(struct ir_block *block);
void init_constant_table(struct constant_table *table);
void free_constant_table(struct constant_table *table);
void init_jump_info_table(struct jump_info_table *table);
void free_jump_info_table(struct jump_info_table *table);

void write_simple(struct ir_block *block, enum opcode instruction);

void write_immediate_u8(struct ir_block *block, enum opcode instruction, uint8_t operand);
void write_immediate_s8(struct ir_block *block, enum opcode instruction, int8_t operand);
void write_immediate_u16(struct ir_block *block, enum opcode instruction, uint16_t operand);
void write_immediate_s16(struct ir_block *block, enum opcode instruction, int16_t operand);
void write_immediate_u32(struct ir_block *block, enum opcode instruction, uint32_t operand);
void write_immediate_s32(struct ir_block *block, enum opcode instruction, int32_t operand);

void write_immediate_uv(struct ir_block *block, enum opcode instruction8, uint32_t operand);
void write_immediate_sv(struct ir_block *block, enum opcode instruction8, int32_t operand);

void overwrite_u8(struct ir_block *block, int start, uint8_t value);
void overwrite_s8(struct ir_block *block, int start, int8_t value);
void overwrite_u16(struct ir_block *block, int start, uint16_t value);
void overwrite_s16(struct ir_block *block, int start, int16_t value);
void overwrite_u32(struct ir_block *block, int start, uint32_t value);
void overwrite_s32(struct ir_block *block, int start, int32_t value);


void overwrite_instruction(struct ir_block *block, int index, enum opcode instruction);

uint8_t read_u8(struct ir_block *block, int index);
int8_t read_s8(struct ir_block *block, int index);
uint16_t read_u16(struct ir_block *block, int index);
int16_t read_s16(struct ir_block *block, int index);
uint32_t read_u32(struct ir_block *block, int index);
int32_t read_s32(struct ir_block *block, int index);

int write_constant(struct ir_block *block, uint64_t constant);
uint64_t read_constant(struct ir_block *block, int index);

int write_jump(struct ir_block *block, int dest);
int find_jump(struct ir_block *block, int dest);
bool is_jump_dest(struct ir_block *block, int dest);

#endif
