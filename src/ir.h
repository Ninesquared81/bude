#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stdint.h>

#include "string_builder.h"
#include "string_view.h"

enum opcode {
    /* NOP -- no operation. */
    OP_NOP,
    /* PUSHn Imm_sn -- Push a 64-bit integer to the stack. */
    OP_PUSH8,
    OP_PUSH16,
    OP_PUSH32,
    OP_PUSH64,
    /* LOAD_STRINGn Imm_un -- Push a string (ptr word) to the stack. */
    OP_LOAD_STRING8,
    OP_LOAD_STRING16,
    OP_LOAD_STRING32,
    /* POP -- Pop and discard the top stack element. */
    OP_POP,
    /* ADD -- Add top two stack elements. */
    OP_ADD,
    /* AND -- Logical (value-preserving) and operation of top two elements. */
    OP_AND,
    /* DEREF -- Dereference (byte) ptr at top of stack. */
    OP_DEREF,
    /* DIVMOD -- Unsigned division and reaminder. */
    OP_DIVMOD,
    /* IDIVMOD -- Signed (truncated) division and remainder. */
    OP_IDIVMOD,
    /* EDIVMOD -- Signed (Euclidean) divion and remainder (remaninder non-negative). */
    OP_EDIVMOD,
    /* DUPE -- Duplicate top stack element. */
    OP_DUPE,
    /* EXIT -- Exit the program, using the top of the stack as the exit code. */
    OP_EXIT,
    /* FOR_DEC_START -- Initialise a for loop counter to the top element. */
    OP_FOR_DEC_START,
    /* FOR_DEC -- Decrement a for loop counter by 1 and loop while it's not zero. */
    OP_FOR_DEC,
    /* FOR_INC_START -- Initialise a for loop counter to zero and the target to the top element. */
    OP_FOR_INC_START,
    /* FOR_INC Imm_s16 -- Increment a for loop counter by 1 and jump a given distance if
       the counter is not equal to the target. */
    OP_FOR_INC,
    /* GET_LOOP_VAR Imm_u16 -- Get the loop variable a given distance from the current loop. */
    OP_GET_LOOP_VAR,
    /* JUMP Imm_s16 -- Jump a given distance. */
    OP_JUMP,
    /* JUMP_COND Imm_s16 -- Jump the given distance if the top element is non-zero (true). */
    OP_JUMP_COND,
    /* JUMP_NCOND Imm_s16 -- Jump the given distance if the top element is zero (false). */
    OP_JUMP_NCOND,
    /* MULT -- Multiply the top two stack elements. */
    OP_MULT,
    /* NOT -- Logical not operation of the top two stack elements. */
    OP_NOT,
    /* OR -- Logical (value-preserving) or operation of top two stack elements. */
    OP_OR,
    /* PRINT -- Print the top element of the stack as a word. */
    OP_PRINT,
    /* PRINT_CHAR -- Print the top element of the stack as a character. */
    OP_PRINT_CHAR,
    /* PRINT_INT -- Print the top element of the stack as a signed integer. */
    OP_PRINT_INT,
    /* SUB -- Subtract the top stack element from the next element. */
    OP_SUB,
    /* SWAP -- Swap the top two stack elements. */
    OP_SWAP,
    /* SXn, SXnL -- sign extend an n-bit integer. The -L versions operate on the
       element under the top (i.e. the left-hand side of a binary operation). */
    OP_SX8,
    OP_SX8L,
    OP_SX16,
    OP_SX16L,
    OP_SX32,
    OP_SX32L,
    /* ZXn, ZXnL -- zero extend an n-bit integer. The -L versions operate on the
       element under the top (i.e. the left-hand side of a binary operation). */
    OP_ZX8,
    OP_ZX8L,
    OP_ZX16,
    OP_ZX16L,
    OP_ZX32,
    OP_ZX32L,
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

struct string_table {
    size_t capacity;
    size_t count;
    struct string_view *views;
};

struct ir_block {
    int capacity;
    int count;
    uint8_t *code;
    size_t max_for_loop_level;
    struct constant_table constants;
    struct jump_info_table jumps;
    struct string_table strings;
    struct region *static_memory;
};

const char *get_opcode_name(enum opcode opcode);
bool is_jump(enum opcode instruction);

void init_block(struct ir_block *block);
void free_block(struct ir_block *block);
void init_constant_table(struct constant_table *table);
void free_constant_table(struct constant_table *table);
void init_jump_info_table(struct jump_info_table *table);
void free_jump_info_table(struct jump_info_table *table);
void init_string_table(struct string_table *table);
void free_string_table(struct string_table *table);

void write_simple(struct ir_block *block, enum opcode instruction);

void write_immediate_u8(struct ir_block *block, enum opcode instruction, uint8_t operand);
void write_immediate_s8(struct ir_block *block, enum opcode instruction, int8_t operand);
void write_immediate_u16(struct ir_block *block, enum opcode instruction, uint16_t operand);
void write_immediate_s16(struct ir_block *block, enum opcode instruction, int16_t operand);
void write_immediate_u32(struct ir_block *block, enum opcode instruction, uint32_t operand);
void write_immediate_s32(struct ir_block *block, enum opcode instruction, int32_t operand);

void write_immediate_uv(struct ir_block *block, enum opcode instruction8, uint64_t operand);
void write_immediate_sv(struct ir_block *block, enum opcode instruction8, int64_t operand);

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
uint64_t read_u64(struct ir_block *block, int index);
int64_t read_s64(struct ir_block *block, int index);

int write_constant(struct ir_block *block, uint64_t constant);
uint64_t read_constant(struct ir_block *block, int index);

uint32_t write_string(struct ir_block *block, struct string_builder *builder);
struct string_view *read_string(struct ir_block *block, uint32_t index);

int add_jump(struct ir_block *block, int dest);
int find_jump(struct ir_block *block, int dest);
bool is_jump_dest(struct ir_block *block, int dest);

#endif
