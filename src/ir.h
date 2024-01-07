#ifndef IR_H
#define IR_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "string_builder.h"
#include "string_view.h"

/* Documenation:
 * Below is a mini-grammar for the language used to informally document each IR instruction:
 *
 * instruction  = <mnemonic> [operand-type]
 *   mnemonic   = NOP, PUSHn (n: size variable), POP, ...
 * operand-type = <operand-kind>_<operand-sign><operand-size>
 * operand-kind = Imm (immediate), Idx (array index), Off (relative offset)
 * operand-sign = u, s
 * operand-size = 8, 16, 32, 64, n (n: size variable)
 */

enum t_opcode {
    /* NOP -- no operation. */
    T_OP_NOP,
    /* PUSHn Imm_un -- Push a word to the stack. */
    T_OP_PUSH8,
    T_OP_PUSH16,
    T_OP_PUSH32,
    T_OP_PUSH64,
    /* PUSH_INTn Imm_sn -- Push a word-sized signed integer to the stack. */
    T_OP_PUSH_INT8,
    T_OP_PUSH_INT16,
    T_OP_PUSH_INT32,
    T_OP_PUSH_INT64,
    /* PUSH_CHAR8 Imm_u8 -- Push an ASCII character to the stack. */
    T_OP_PUSH_CHAR8,
    /* LOAD_STRINGn Idx_un -- Load a string (ptr word) onto the stack from the strings table. */
    T_OP_LOAD_STRING8,
    T_OP_LOAD_STRING16,
    T_OP_LOAD_STRING32,
    /* POP -- Pop and discard the top stack element. */
    T_OP_POP,
    /* ADD -- Add top two stack elements. */
    T_OP_ADD,
    /* AND -- Logical (value-preserving) and operation of top two elements. */
    T_OP_AND,
    /* DEREF -- Dereference (byte) ptr at top of stack. */
    T_OP_DEREF,
    /* DIVMOD -- Unsigned division and reaminder. */
    T_OP_DIVMOD,
    /* IDIVMOD -- Signed (truncated) division and remainder. */
    T_OP_IDIVMOD,
    /* EDIVMOD -- Signed (Euclidean) divion and remainder (remaninder non-negative). */
    T_OP_EDIVMOD,
    /* DUPE -- Duplicate top stack element. */
    T_OP_DUPE,
    /* EXIT -- Exit the program, using the top of the stack as the exit code. */
    T_OP_EXIT,
    /* FOR_DEC_START Off_s16 -- Initialise a for loop counter to the top element. */
    T_OP_FOR_DEC_START,
    /* FOR_DEC Off_s16 -- Decrement a for loop counter by 1 and loop while it's not zero. */
    T_OP_FOR_DEC,
    /* FOR_INC_START Off_s16 -- Initialise a for loop counter to zero and the target to the
       top element. */
    T_OP_FOR_INC_START,
    /* FOR_INC Off_s16 -- Increment a for loop counter by 1 and jump a given distance if
       the counter is not equal to the target. */
    T_OP_FOR_INC,
    /* GET_LOOP_VAR Idx_u16 -- Get the loop variable a given distance from the current loop. */
    T_OP_GET_LOOP_VAR,
    /* JUMP Off_s16 -- Jump a given distance. */
    T_OP_JUMP,
    /* JUMP_COND Off_s16 -- Jump the given distance if the top element is non-zero (true). */
    T_OP_JUMP_COND,
    /* JUMP_NCOND Off_s16 -- Jump the given distance if the top element is zero (false). */
    T_OP_JUMP_NCOND,
    /* MULT -- Multiply the top two stack elements. */
    T_OP_MULT,
    /* NOT -- Logical not operation of the top two stack elements. */
    T_OP_NOT,
    /* OR -- Logical (value-preserving) or operation of top two stack elements. */
    T_OP_OR,
    /* (T)PRINT -- Print the top element of the stack in a format surmised from its type. */
    T_OP_PRINT,
    /* PRINT_CHAR -- Print the top element of the stack as a character. */
    T_OP_PRINT_CHAR,
    /* PRINT_INT -- Print the top element of the stack as a signed integer. */
    T_OP_PRINT_INT,
    /* SUB -- Subtract the top stack element from the next element. */
    T_OP_SUB,
    /* SWAP -- Swap the top two stack elements. */
    T_OP_SWAP,
    /* AS_BYTE, AS_Un, AS_Sn -- Clear any excess bits and treat as an integer of that type. */
    T_OP_AS_BYTE,
    T_OP_AS_U8,
    T_OP_AS_U16,
    T_OP_AS_U32,
    T_OP_AS_S8,
    T_OP_AS_S16,
    T_OP_AS_S32,
    /* (T)PACKn Idx_un -- Construct a pack with the given type index. */
    T_OP_PACK8,
    T_OP_PACK16,
    T_OP_PACK32,
    /* (T)COMPn Idx_un -- Construct a comp with the given type index. */
    T_OP_COMP8,
    T_OP_COMP16,
    T_OP_COMP32,
};

enum w_opcode {
    /* NOP -- no operation. */
    W_OP_NOP,
    /* PUSHn Imm_un -- Push a word to the stack. */
    W_OP_PUSH8,
    W_OP_PUSH16,
    W_OP_PUSH32,
    W_OP_PUSH64,
    /* PUSH_INTn Imm_sn -- Push a word-sized signed integer to the stack. */
    W_OP_PUSH_INT8,
    W_OP_PUSH_INT16,
    W_OP_PUSH_INT32,
    W_OP_PUSH_INT64,
    /* PUSH_CHAR8 Imm_u8 -- Push an ASCII character to the stack. */
    W_OP_PUSH_CHAR8,
    /* LOAD_STRINGn Imm_un -- Load a string (ptr word) onto the stack from the strings table. */
    W_OP_LOAD_STRING8,
    W_OP_LOAD_STRING16,
    W_OP_LOAD_STRING32,
    /* POP -- Pop and discard the top stack element. */
    W_OP_POP,
    /* ADD -- Add top two stack elements. */
    W_OP_ADD,
    /* AND -- Logical (value-preserving) and operation of top two elements. */
    W_OP_AND,
    /* DEREF -- Dereference (byte) ptr at top of stack. */
    W_OP_DEREF,
    /* DIVMOD -- Unsigned division and reaminder. */
    W_OP_DIVMOD,
    /* IDIVMOD -- Signed (truncated) division and remainder. */
    W_OP_IDIVMOD,
    /* EDIVMOD -- Signed (Euclidean) divion and remainder (remaninder non-negative). */
    W_OP_EDIVMOD,
    /* DUPE -- Duplicate top stack element. */
    W_OP_DUPE,
    /* EXIT -- Exit the program, using the top of the stack as the exit code. */
    W_OP_EXIT,
    /* FOR_DEC_START Off_s16 -- Initialise a for loop counter to the top element. */
    W_OP_FOR_DEC_START,
    /* FOR_DEC Off_s16 -- Decrement a for loop counter by 1 and loop while it's not zero. */
    W_OP_FOR_DEC,
    /* FOR_INC_START Off_s16 -- Initialise a for loop counter to zero and the target to the
       top element. */
    W_OP_FOR_INC_START,
    /* FOR_INC Off_s16 -- Increment a for loop counter by 1 and jump a given distance if
       the counter is not equal to the target. */
    W_OP_FOR_INC,
    /* GET_LOOP_VAR Idx_u16 -- Get the loop variable a given distance from the current loop. */
    W_OP_GET_LOOP_VAR,
    /* JUMP Off_s16 -- Jump a given distance. */
    W_OP_JUMP,
    /* JUMP_COND Off_s16 -- Jump the given distance if the top element is non-zero (true). */
    W_OP_JUMP_COND,
    /* JUMP_NCOND Off_s16 -- Jump the given distance if the top element is zero (false). */
    W_OP_JUMP_NCOND,
    /* MULT -- Multiply the top two stack elements. */
    W_OP_MULT,
    /* NOT -- Logical not operation of the top two stack elements. */
    W_OP_NOT,
    /* OR -- Logical (value-preserving) or operation of top two stack elements. */
    W_OP_OR,
    /* (W)PRINT -- Print the top element of the stack as a word. */
    W_OP_PRINT,
    /* PRINT_CHAR -- Print the top element of the stack as a character. */
    W_OP_PRINT_CHAR,
    /* PRINT_INT -- Print the top element of the stack as a signed integer. */
    W_OP_PRINT_INT,
    /* SUB -- Subtract the top stack element from the next element. */
    W_OP_SUB,
    /* SWAP -- Swap the top two stack elements. */
    W_OP_SWAP,
    /* SXn, SXnL -- sign extend an n-bit integer. The -L versions operate on the
       element under the top (i.e. the left-hand side of a binary operation). */
    W_OP_SX8,
    W_OP_SX8L,
    W_OP_SX16,
    W_OP_SX16L,
    W_OP_SX32,
    W_OP_SX32L,
    /* ZXn, ZXnL -- zero extend an n-bit integer. The -L versions operate on the
       element under the top (i.e. the left-hand side of a binary operation). */
    W_OP_ZX8,
    W_OP_ZX8L,
    W_OP_ZX16,
    W_OP_ZX16L,
    W_OP_ZX32,
    W_OP_ZX32L,
    /* (W)PACKn Imm_u8... -- Construct a pack with n fields of the provided sizes. */
    W_OP_PACK1,
    W_OP_PACK2,
    W_OP_PACK3,
    W_OP_PACK4,
    W_OP_PACK5,
    W_OP_PACK6,
    W_OP_PACK7,
    W_OP_PACK8,
};

static_assert(T_OP_NOP == 0 && W_OP_NOP == 0);

enum ir_instruction_set {
    IR_TYPED,
    IR_WORD_ORIENTED,
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
    struct location *locations;
    enum ir_instruction_set instruction_set;
    const char *filename;
    size_t max_for_loop_level;
    struct jump_info_table jumps;
    struct string_table strings;
    struct string_table symbols;
    struct region *static_memory;
};

typedef int opcode;

const char *get_t_opcode_name(enum t_opcode opcode);
const char *get_w_opcode_name(enum w_opcode opcode);

#define get_opcode_name(opcode)                 \
    _Generic((opcode),                          \
             enum t_opcode: get_t_opcode_name,  \
             enum w_opcode: get_w_opcode_name   \
        )(opcode)

bool is_t_jump(enum t_opcode instruction);
bool is_w_jump(enum w_opcode instruction);

#define is_jump(instruction)\
    _Generic((instruction),\
             enum t_opcode: is_t_jump,  \
             enum w_opcode: is_w_jump   \
        )(instruction)
        

void init_block(struct ir_block *block, enum ir_instruction_set instruction_set,
                const char *filename);
void free_block(struct ir_block *block);
void init_jump_info_table(struct jump_info_table *table);
void free_jump_info_table(struct jump_info_table *table);
void init_string_table(struct string_table *table);
void free_string_table(struct string_table *table);

void copy_jump_info_table(struct jump_info_table *restrict dest,
                          struct jump_info_table *restrict src);
void copy_string_table(struct string_table *restrict dest,
                       struct string_table *restrict src);

void copy_metadata(struct ir_block *restrict dest, struct ir_block *restrict src);

void write_simple(struct ir_block *block, opcode instruction, struct location *location);

void write_immediate_u8(struct ir_block *block, opcode instruction, uint8_t operand,
                        struct location *location);
void write_immediate_s8(struct ir_block *block, opcode instruction, int8_t operand,
                        struct location *location);
void write_immediate_u16(struct ir_block *block, opcode instruction, uint16_t operand,
                         struct location *location);
void write_immediate_s16(struct ir_block *block, opcode instruction, int16_t operand,
                         struct location *location);
void write_immediate_u32(struct ir_block *block, opcode instruction, uint32_t operand,
                         struct location *location);
void write_immediate_s32(struct ir_block *block, opcode instruction, int32_t operand,
                         struct location *location);
void write_immediate_u64(struct ir_block *block, opcode instruction, uint64_t operand,
                         struct location *location);
void write_immediate_s64(struct ir_block *block, opcode instruction, int64_t operand,
                         struct location *location);

void write_u8(struct ir_block *block, uint8_t value, struct location *location);
void write_s8(struct ir_block *block, int8_t value, struct location *location);
void write_u16(struct ir_block *block, uint16_t value, struct location *location);
void write_s16(struct ir_block *block, int16_t value, struct location *location);
void write_u32(struct ir_block *block, uint32_t value, struct location *location);
void write_s32(struct ir_block *block, int32_t value, struct location *location);
void write_u64(struct ir_block *block, uint64_t value, struct location *location);
void write_s64(struct ir_block *block, int64_t value, struct location *location);

void overwrite_u8(struct ir_block *block, int start, uint8_t value);
void overwrite_s8(struct ir_block *block, int start, int8_t value);
void overwrite_u16(struct ir_block *block, int start, uint16_t value);
void overwrite_s16(struct ir_block *block, int start, int16_t value);
void overwrite_u32(struct ir_block *block, int start, uint32_t value);
void overwrite_s32(struct ir_block *block, int start, int32_t value);
void overwrite_u64(struct ir_block *block, int start, uint64_t value);
void overwrite_s64(struct ir_block *block, int start, int64_t value);


void overwrite_instruction(struct ir_block *block, int index, opcode instruction);

uint8_t read_u8(struct ir_block *block, int index);
int8_t read_s8(struct ir_block *block, int index);
uint16_t read_u16(struct ir_block *block, int index);
int16_t read_s16(struct ir_block *block, int index);
uint32_t read_u32(struct ir_block *block, int index);
int32_t read_s32(struct ir_block *block, int index);
uint64_t read_u64(struct ir_block *block, int index);
int64_t read_s64(struct ir_block *block, int index);

uint32_t write_string(struct ir_block *block, struct string_builder *builder);
struct string_view *read_string(struct ir_block *block, uint32_t index);

int add_jump(struct ir_block *block, int dest);
int find_jump(struct ir_block *block, int dest);
bool is_jump_dest(struct ir_block *block, int dest);

void ir_error(struct ir_block *block, size_t index, const char *message);

#endif
