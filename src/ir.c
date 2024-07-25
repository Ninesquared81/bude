#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "location.h"
#include "memory.h"
#include "region.h"
#include "type_punning.h"

#ifndef BLOCK_INIT_SIZE
#define BLOCK_INIT_SIZE 128
#endif

#ifndef JUMP_INFO_TABLE_INIT_SIZE
#define JUMP_INFO_TABLE_INIT_SIZE 8
#endif

#define X(opcode) [opcode] = #opcode,
const char *t_opcode_names[] = {
    T_OPCODES
};

const char *w_opcode_names[]  = {
    W_OPCODES
};
#undef X

const char *get_t_opcode_name(enum t_opcode opcode) {
    assert(0 <= opcode && opcode < sizeof t_opcode_names / sizeof t_opcode_names[0]);
    return t_opcode_names[opcode];
}

const char *get_w_opcode_name(enum w_opcode opcode) {
    assert(0 <= opcode && opcode < sizeof w_opcode_names / sizeof w_opcode_names[0]);
    return w_opcode_names[opcode];
}

static const int w_instruction_sizes[] = {
    [W_OP_NOP]                       = 1,
    [W_OP_PUSH8]                     = 2,
    [W_OP_PUSH16]                    = 3,
    [W_OP_PUSH32]                    = 5,
    [W_OP_PUSH64]                    = 9,
    [W_OP_PUSH_INT8]                 = 2,
    [W_OP_PUSH_INT16]                = 3,
    [W_OP_PUSH_INT32]                = 5,
    [W_OP_PUSH_INT64]                = 9,
    [W_OP_PUSH_FLOAT32]              = 5,
    [W_OP_PUSH_FLOAT64]              = 9,
    [W_OP_PUSH_CHAR8]                = 2,
    [W_OP_PUSH_CHAR16]               = 3,
    [W_OP_PUSH_CHAR32]               = 5,
    [W_OP_LOAD_STRING8]              = 2,
    [W_OP_LOAD_STRING16]             = 3,
    [W_OP_LOAD_STRING32]             = 5,
    [W_OP_POP]                       = 1,
    [W_OP_POPN8]                     = 2,
    [W_OP_POPN16]                    = 3,
    [W_OP_POPN32]                    = 5,
    [W_OP_ADD]                       = 1,
    [W_OP_ADDF32]                    = 1,
    [W_OP_ADDF64]                    = 1,
    [W_OP_AND]                       = 1,
    [W_OP_DEREF]                     = 1,
    [W_OP_DIVF32]                    = 1,
    [W_OP_DIVF64]                    = 1,
    [W_OP_DIVMOD]                    = 1,
    [W_OP_IDIVMOD]                   = 1,
    [W_OP_EDIVMOD]                   = 1,
    [W_OP_DUPE]                      = 1,
    [W_OP_DUPEN8]                    = 2,
    [W_OP_DUPEN16]                   = 3,
    [W_OP_DUPEN32]                   = 5,
    [W_OP_EQUALS]                    = 1,
    [W_OP_EXIT]                      = 1,
    [W_OP_FOR_DEC_START]             = 3,
    [W_OP_FOR_DEC]                   = 3,
    [W_OP_FOR_INC_START]             = 3,
    [W_OP_FOR_INC]                   = 3,
    [W_OP_GET_LOOP_VAR]              = 3,
    [W_OP_GREATER_THAN]              = 1,
    [W_OP_HIGHER_THAN]               = 1,
    [W_OP_JUMP]                      = 3,
    [W_OP_JUMP_COND]                 = 3,
    [W_OP_JUMP_NCOND]                = 3,
    [W_OP_LESS_THAN]                 = 1,
    [W_OP_LOCAL_GET]                 = 3,
    [W_OP_LOCAL_SET]                 = 3,
    [W_OP_LOWER_THAN]                = 1,
    [W_OP_MULT]                      = 1,
    [W_OP_MULTF32]                   = 1,
    [W_OP_MULTF64]                   = 1,
    [W_OP_NOT]                       = 1,
    [W_OP_NOT_EQUALS]                = 1,
    [W_OP_OR]                        = 1,
    [W_OP_PRINT]                     = 1,
    [W_OP_PRINT_CHAR]                = 1,
    [W_OP_PRINT_FLOAT]               = 1,
    [W_OP_PRINT_INT]                 = 1,
    [W_OP_PRINT_STRING]              = 1,
    [W_OP_SUB]                       = 1,
    [W_OP_SUBF32]                    = 1,
    [W_OP_SUBF64]                    = 1,
    [W_OP_SWAP]                      = 1,
    [W_OP_SWAP_COMPS8]               = 3,
    [W_OP_SWAP_COMPS16]              = 5,
    [W_OP_SWAP_COMPS32]              = 9,
    [W_OP_SX8]                       = 1,
    [W_OP_SX8L]                      = 1,
    [W_OP_SX16]                      = 1,
    [W_OP_SX16L]                     = 1,
    [W_OP_SX32]                      = 1,
    [W_OP_SX32L]                     = 1,
    [W_OP_ZX8]                       = 1,
    [W_OP_ZX8L]                      = 1,
    [W_OP_ZX16]                      = 1,
    [W_OP_ZX16L]                     = 1,
    [W_OP_ZX32]                      = 1,
    [W_OP_ZX32L]                     = 1,
    [W_OP_FPROM]                     = 1,
    [W_OP_FPROML]                    = 1,
    [W_OP_FDEM]                      = 1,
    [W_OP_ICONVF32]                  = 1,
    [W_OP_ICONVF32L]                 = 1,
    [W_OP_ICONVF64]                  = 1,
    [W_OP_ICONVF64L]                 = 1,
    [W_OP_FCONVI64]                  = 1,
    [W_OP_ICONVC32]                  = 1,
    [W_OP_CHAR_8CONV32]              = 1,
    [W_OP_CHAR_32CONV8]              = 1,
    [W_OP_CHAR_16CONV32]             = 1,
    [W_OP_CHAR_32CONV16]             = 1,
    [W_OP_PACK1]                     = 2,
    [W_OP_PACK2]                     = 3,
    [W_OP_PACK3]                     = 4,
    [W_OP_PACK4]                     = 5,
    [W_OP_PACK5]                     = 6,
    [W_OP_PACK6]                     = 7,
    [W_OP_PACK7]                     = 8,
    [W_OP_PACK8]                     = 9,
    [W_OP_UNPACK1]                   = 2,
    [W_OP_UNPACK2]                   = 3,
    [W_OP_UNPACK3]                   = 4,
    [W_OP_UNPACK4]                   = 5,
    [W_OP_UNPACK5]                   = 6,
    [W_OP_UNPACK6]                   = 7,
    [W_OP_UNPACK7]                   = 8,
    [W_OP_UNPACK8]                   = 9,
    [W_OP_PACK_FIELD_GET]            = 3,
    [W_OP_COMP_FIELD_GET8]           = 2,
    [W_OP_COMP_FIELD_GET16]          = 3,
    [W_OP_COMP_FIELD_GET32]          = 5,
    [W_OP_PACK_FIELD_SET]            = 3,
    [W_OP_COMP_FIELD_SET8]           = 2,
    [W_OP_COMP_FIELD_SET16]          = 3,
    [W_OP_COMP_FIELD_SET32]          = 5,
    [W_OP_COMP_SUBCOMP_GET8]         = 3,
    [W_OP_COMP_SUBCOMP_GET16]        = 5,
    [W_OP_COMP_SUBCOMP_GET32]        = 9,
    [W_OP_COMP_SUBCOMP_SET8]         = 3,
    [W_OP_COMP_SUBCOMP_SET16]        = 5,
    [W_OP_COMP_SUBCOMP_SET32]        = 9,
    [W_OP_CALL8]                     = 2,
    [W_OP_CALL16]                    = 3,
    [W_OP_CALL32]                    = 5,
    [W_OP_EXTCALL8]                  = 2,
    [W_OP_EXTCALL16]                 = 3,
    [W_OP_EXTCALL32]                 = 5,
    [W_OP_RET]                       = 1,
};

int get_w_instruction_size(enum w_opcode opcode) {
    assert(0 <= opcode && opcode < sizeof w_instruction_sizes);
    return w_instruction_sizes[opcode];
}

bool is_t_jump(enum t_opcode instruction) {
    switch (instruction) {
    case T_OP_JUMP:
    case T_OP_JUMP_COND:
    case T_OP_JUMP_NCOND:
    case T_OP_FOR_DEC_START:
    case T_OP_FOR_DEC:
    case T_OP_FOR_INC_START:
    case T_OP_FOR_INC:
        return true;
    default:
        return false;
    }
}

bool is_w_jump(enum w_opcode instruction) {
    switch (instruction) {
    case W_OP_JUMP:
    case W_OP_JUMP_COND:
    case W_OP_JUMP_NCOND:
    case W_OP_FOR_DEC_START:
    case W_OP_FOR_DEC:
    case W_OP_FOR_INC_START:
    case W_OP_FOR_INC:
        return true;
    default:
        return false;
    }
}

void init_block(struct ir_block *block, enum ir_instruction_set instruction_set) {
    block->code = allocate_array(BLOCK_INIT_SIZE, sizeof *block->code);
    block->locations = allocate_array(BLOCK_INIT_SIZE, sizeof *block->locations);
    block->capacity = BLOCK_INIT_SIZE;
    block->count = 0;
    block->instruction_set = instruction_set;
    init_jump_info_table(&block->jumps);
}

void free_block(struct ir_block *block) {
    free_array(block->code, block->capacity, sizeof *block->code);
    free_array(block->locations, block->capacity, sizeof *block->locations);
    block->code = NULL;
    block->locations = NULL;
    block->capacity = 0;
    block->count = 0;
    free_jump_info_table(&block->jumps);
}

void init_jump_info_table(struct jump_info_table *jumps) {
    INIT_DARRAY(jumps, JUMP_INFO_TABLE_INIT_SIZE);
}

void free_jump_info_table(struct jump_info_table *jumps) {
    FREE_DARRAY(jumps);
}

static void grow_block(struct ir_block *block) {
    int old_capacity = block->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : BLOCK_INIT_SIZE;
    block->code = reallocate_array(block->code, old_capacity, new_capacity,
                                   sizeof block->code[0]);
    block->locations = reallocate_array(block->locations, old_capacity, new_capacity,
                                        sizeof block->locations[0]);
    block->capacity = new_capacity;
}

void write_simple(struct ir_block *block, opcode instruction, struct location *location) {
    if (block->count + 1 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
}

void write_u8(struct ir_block *block, uint8_t operand, struct location *location) {
    if (block->count + 1 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = operand;
}

void write_s8(struct ir_block *block, int8_t operand, struct location *location) {
    write_u8(block, s8_to_u8(operand), location);
}

void write_u16(struct ir_block *block, uint16_t operand, struct location *location) {
    if (block->count + 2 > block->capacity) {
        grow_block(block);
    }
    for (int i = 0; i < 2; ++i) {
        block->locations[block->count + i] = *location;
    }
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
}

void write_s16(struct ir_block *block, int16_t operand, struct location *location) {
    write_u16(block, u16_to_s16(operand), location);
}

void write_u32(struct ir_block *block, uint32_t operand, struct location *location) {
    if (block->count + 4 > block->capacity) {
        grow_block(block);
    }
    for (int i = 0; i < 4; ++i) {
        block->locations[block->count + i] = *location;
    }
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
    block->code[block->count++] = operand >> 16;
    block->code[block->count++] = operand >> 24;
}

void write_s32(struct ir_block *block, int32_t operand, struct location *location) {
    write_u32(block, s32_to_u32(operand), location);
}

void write_u64(struct ir_block *block, uint64_t operand, struct location *location) {
    if (block->count + 8 > block->capacity) {
        grow_block(block);
    }
    for (int i = 0; i < 8; ++i) {
        block->locations[block->count + i] = *location;
    }
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
    block->code[block->count++] = operand >> 16;
    block->code[block->count++] = operand >> 24;
    block->code[block->count++] = operand >> 32;
    block->code[block->count++] = operand >> 40;
    block->code[block->count++] = operand >> 48;
    block->code[block->count++] = operand >> 56;
}

void write_s64(struct ir_block *block, int64_t operand, struct location *location) {
    write_u64(block, s64_to_u64(operand), location);
}

void write_immediate_u8(struct ir_block *block, opcode instruction, uint8_t operand,
                        struct location *location) {
    if (block->count + 2 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->locations[block->count + 1] = *location;
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
}

void write_immediate_s8(struct ir_block *block, opcode instruction, int8_t operand,
                        struct location *location) {
    write_immediate_u8(block, instruction, s8_to_u8(operand), location);
}

void write_immediate_u16(struct ir_block *block, opcode instruction, uint16_t operand,
                        struct location *location) {
    if (block->count + 3 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    write_u16(block, operand, location);
}

void write_immediate_s16(struct ir_block *block, opcode instruction, int16_t operand,
                        struct location *location) {
    write_immediate_u16(block, instruction, s16_to_u16(operand), location);
}

void write_immediate_u32(struct ir_block *block, opcode instruction, uint32_t operand,
                        struct location *location) {
    if (block->count + 5 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    write_u32(block, operand, location);
}

void write_immediate_s32(struct ir_block *block, opcode instruction, int32_t operand,
                        struct location *location) {
    write_immediate_u32(block, instruction, s32_to_u32(operand), location);
}

void write_immediate_u64(struct ir_block *block, opcode instruction, uint64_t operand,
                        struct location *location) {
    if (block->count + 9 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    write_u64(block, operand, location);
}

void write_immediate_s64(struct ir_block *block, opcode instruction, int64_t operand,
                        struct location *location) {
    write_immediate_u64(block, instruction, s64_to_u64(operand), location);
}

void overwrite_u8(struct ir_block *block, int start, uint8_t value) {
    assert(0 <= start && start < block->count);
    block->code[start] = value;
}

void overwrite_s8(struct ir_block *block, int start, int8_t value) {
    overwrite_u8(block, start, s8_to_u8(value));
}

void overwrite_u16(struct ir_block *block, int start, uint16_t value) {
    assert(0 <= start && start + 1 < block->count);  // Make sure there's space.
    // Note: The IR instruction set is little-endian.
    block->code[start] = value;  // LSB.
    block->code[start + 1] = value >> 8;  // MSB.
}

void overwrite_s16(struct ir_block *block, int start, int16_t value) {
    overwrite_u16(block, start, s16_to_u16(value));
}

void overwrite_u32(struct ir_block *block, int start, uint32_t value) {
    assert(0 <= start && start + 3 < block->count);
    block->code[start] = value;
    block->code[start + 1] = value >> 8;
    block->code[start + 2] = value >> 16;
    block->code[start + 3] = value >> 24;
}

void overwrite_s32(struct ir_block *block, int start, int32_t value) {
    overwrite_u32(block, start, s32_to_u32(value));
}

void overwrite_u64(struct ir_block *block, int start, uint64_t value) {
    assert(0 <= start && start + 7 < block->count);
    block->code[start] = value;
    block->code[start + 1] = value >> 8;
    block->code[start + 2] = value >> 16;
    block->code[start + 3] = value >> 24;
    block->code[start + 4] = value >> 32;
    block->code[start + 5] = value >> 40;
    block->code[start + 6] = value >> 48;
    block->code[start + 7] = value >> 56;
}

void overwrite_s64(struct ir_block *block, int start, int64_t value) {
    overwrite_u64(block, start, s64_to_u64(value));
}

void overwrite_instruction(struct ir_block *block, int index, opcode instruction) {
    // An instruction opcode is basically just a u8.
    overwrite_u8(block, index, instruction);
}

uint8_t read_u8(struct ir_block *block, int index) {
    assert(0 <= index && index < block->count);
    return block->code[index];
}

int8_t read_s8(struct ir_block *block, int index) {
    return u8_to_s8(read_u8(block, index));
}

uint16_t read_u16(struct ir_block *block, int index) {
    assert(0 <= index && index + 1 < block->count);
    return block->code[index] ^ (block->code[index + 1] << 8);
}

int16_t read_s16(struct ir_block *block, int index) {
    return u16_to_s16(read_u16(block, index));
}

uint32_t read_u32(struct ir_block *block, int index) {
    assert(0 <= index && index + 3 < block->count);

    uint32_t result = block->code[index];
    result ^= block->code[index + 1] << 8;
    result ^= block->code[index + 2] << 16;
    result ^= block->code[index + 3] << 24;
    return result;
}

int32_t read_s32(struct ir_block *block, int index) {
    return u32_to_s32(read_u32(block, index));
}

uint64_t read_u64(struct ir_block *block, int index) {
    assert(0 <= index && index + 3 < block->count);

    uint64_t result = block->code[index];
    result ^= (uint64_t)block->code[index + 1] << 8;
    result ^= (uint64_t)block->code[index + 2] << 16;
    result ^= (uint64_t)block->code[index + 3] << 24;
    result ^= (uint64_t)block->code[index + 4] << 32;
    result ^= (uint64_t)block->code[index + 5] << 40;
    result ^= (uint64_t)block->code[index + 6] << 48;
    result ^= (uint64_t)block->code[index + 7] << 56;
    return result;
}

int64_t read_s64(struct ir_block *block, int index) {
    return u64_to_s64(read_u64(block, index));
}

static int binary_search(struct jump_info_table *jumps, int dest) {
    int hi = jumps->count - 1;
    int lo = 0;
    while (hi > lo) {
        int mid = lo + (hi - lo) / 2;
        if (jumps->items[mid] == dest) {
            return mid;
        }
        if (jumps->items[mid] < dest) {
            lo = mid + 1;
        }
        else {
            hi = mid - 1;
        }
    }
    return lo;
}

int add_jump(struct ir_block *block, int dest) {
    struct jump_info_table *jumps = &block->jumps;
    if (jumps->count + 1 > jumps->capacity) {
        GROW_DARRAY(jumps);
    }
    // Usually, we add elements in order, so first see if we can just add it on the end.
    // Also, we handle the empty case here as well.
    if (jumps->count == 0 || jumps->items[jumps->count - 1] < dest) {
        jumps->items[jumps->count++] = dest;
        return jumps->count - 1;
    }
    // Binary insert.
    int index = binary_search(jumps, dest);
    size_t shift = jumps->count - index;
    memmove(&jumps->items[index + 1], &jumps->items[index], shift * sizeof jumps->items[0]);
    ++jumps->count;
    jumps->items[index] = dest;
    return index;
}

int find_jump(struct ir_block *block, int dest) {
    // Binary search.
    struct jump_info_table *jumps = &block->jumps;
    int index = binary_search(jumps, dest);
    // -1: search failed.
    return (index < jumps->count && jumps->items[index] == dest) ? index : -1;
}

bool is_jump_dest(struct ir_block *block, int dest) {
    return find_jump(block, dest) != -1;
}

void recompute_jump_dests(struct ir_block *block) {
    assert(block->instruction_set == IR_WORD_ORIENTED);
    block->jumps.count = 0;  // Clear jump info table.
    for (int ip = 0; ip < block->count; ) {
        enum w_opcode instruction = block->code[ip];
        int size = get_w_instruction_size(instruction);
        if (is_w_jump(instruction)) {
            int jump = read_s16(block, ip + 1);
            int dest = ip + 1 + jump;  // Jump measured from after opcode (stupid design!)
            add_jump(block, dest);
        }
        ip += size;
    }
}

void ir_error(const char *restrict filename, struct ir_block *block,
              size_t index, const char *restrict message) {
    report_location(filename, &block->locations[index]);
    fprintf(stderr, message);
}
