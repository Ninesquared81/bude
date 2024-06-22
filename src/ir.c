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

void ir_error(const char *restrict filename, struct ir_block *block,
              size_t index, const char *restrict message) {
    report_location(filename, &block->locations[index]);
    fprintf(stderr, message);
}
