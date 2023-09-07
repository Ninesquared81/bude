#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "type_punning.h"

#ifndef BLOCK_INIT_SIZE
#define BLOCK_INIT_SIZE 128
#endif

#ifndef CONST_TABLE_INIT_SIZE
#define CONST_TABLE_INIT_SIZE 64
#endif

static void *allocate_array(size_t count, size_t size) {
    void *array = calloc(count, size);
    if (array == NULL) {
        fprintf(stderr, "Could not allocate array.\n");
        exit(1);
    }
    return array;
}

static void free_array(void *array, [[maybe_unused]] size_t count, [[maybe_unused]] size_t size) {
    free(array);
}

static void *reallocate_array(void *array, size_t old_count, size_t new_count, size_t size) {
    if (new_count < old_count) {
        return array;
    }
    void *new = allocate_array(new_count, size);
    if (new == NULL) {
        fprintf(stderr, "Could not reallocate array.\n");
        exit(1);
    }
    if (array != NULL) {
        memcpy(new, array, size * old_count);
        free_array(array, old_count, size);
    }
    return new;
}

void init_block(struct ir_block *block) {
    block->code = allocate_array(BLOCK_INIT_SIZE, sizeof *block->code);
    block->capacity = BLOCK_INIT_SIZE;
    block->count = 0;
    init_constant_table(&block->constants);
}

void free_block(struct ir_block *block) {
    free_array(block->code, block->capacity, sizeof *block->code);
    block->code = NULL;
    block->capacity = 0;
    block->count = 0;
    free_constant_table(&block->constants);
}

void init_constant_table(struct constant_table *table) {
    table->data = allocate_array(CONST_TABLE_INIT_SIZE, sizeof *table->data);
    table->capacity = CONST_TABLE_INIT_SIZE;
    table->count = 0;
}

void free_constant_table(struct constant_table *table) {
    free_array(table->data, table->capacity, sizeof table->data[0]);
    table->capacity = 0;
    table->count = 0;
}

static void grow_block(struct ir_block *block) {
    int old_capacity = block->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : BLOCK_INIT_SIZE;
    block->code = reallocate_array(block->code, old_capacity, new_capacity, sizeof block->code[0]);
    block->capacity = new_capacity;
}

static void grow_constant_table(struct constant_table *table) {
    int old_capacity = table->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : CONST_TABLE_INIT_SIZE;
    table->data = reallocate_array(table->data, old_capacity, new_capacity, sizeof table->data[0]);
    table->capacity = new_capacity;
}

void write_simple(struct ir_block *block, enum opcode instruction) {
    if (block->count + 1 > block->capacity) {
        grow_block(block);
    }
    block->code[block->count++] = instruction;
}

void write_immediate_u8(struct ir_block *block, enum opcode instruction, uint8_t operand) {
    if (block->count + 2 > block->capacity) {
        grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
}

void write_immediate_s8(struct ir_block *block, enum opcode instruction, int8_t operand) {
    write_immediate_u8(block, instruction, s8_to_u8(operand));
}

void write_immediate_u16(struct ir_block *block, enum opcode instruction, uint16_t operand) {
    if (block->count + 3 > block->capacity) {
        grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
}

void write_immediate_s16(struct ir_block *block, enum opcode instruction, int16_t operand) {
    write_immediate_u16(block, instruction, s16_to_u16(operand));
}

void write_immediate_u32(struct ir_block *block, enum opcode instruction, uint32_t operand) {
    if (block->count + 5 > block->capacity) {
        grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
    block->code[block->count++] = operand >> 16;
    block->code[block->count++] = operand >> 24;
}

void write_immediate_s32(struct ir_block *block, enum opcode instruction, int32_t operand) {
    write_immediate_u32(block, instruction, s32_to_u32(operand));
}

void write_immediate_uv(struct ir_block *block, enum opcode instruction8, uint32_t operand) {
    enum opcode instruction16 = instruction8 + 1;
    enum opcode instruction32 = instruction8 + 2;
    if (operand <= UINT8_MAX) {
        write_immediate_u8(block, instruction8, operand);
    }
    else if (operand <= UINT16_MAX) {
        write_immediate_u16(block, instruction16, operand);
    }
    else {
        write_immediate_u32(block, instruction32, operand);
    }
}

void write_immediate_sv(struct ir_block *block, enum opcode instruction8, int32_t operand) {
    enum opcode instruction16 = instruction8 + 1;
    enum opcode instruction32 = instruction8 + 2;
    if (INT8_MIN < operand && operand <= INT8_MAX) {
        write_immediate_s8(block, instruction8, operand);
    }
    else if (INT16_MIN < operand && operand <= INT16_MAX) {
        write_immediate_s16(block, instruction16, operand);
    }
    else {
        write_immediate_s32(block, instruction32, operand);
    }    
}

void overwrite_u8(struct ir_block *block, int start, uint8_t value) {
    assert(start + 1 < block->count);
    block->code[start] = value;
}

void overwrite_s8(struct ir_block *block, int start, int8_t value) {
    overwrite_u8(block, start, s8_to_u8(value));
}

void overwrite_u16(struct ir_block *block, int start, uint16_t value) {
    assert(start + 2 < block->count);  // Make sure there's space.
    // Note: The IR instruction set is little-endian.
    block->code[start] = value;  // LSB.
    block->code[start + 1] = value >> 8;  // MSB. 
}

void overwrite_s16(struct ir_block *block, int start, int16_t value) {
    overwrite_u16(block, start, s16_to_u16(value));
}

uint8_t read_u8(struct ir_block *block, int index) {
    assert(index < block->count);
    return block->code[index];
}

int8_t read_s8(struct ir_block *block, int index) {
    return u8_to_s8(read_u8(block, index));
}

uint16_t read_u16(struct ir_block *block, int index) {
    assert(index + 1 < block->count);
    return block->code[index] ^ (block->code[index + 1] << 8);
}

int16_t read_s16(struct ir_block *block, int index) {
    return u16_to_s16(read_u16(block, index));
}

uint32_t read_u32(struct ir_block *block, int index) {
    assert(index + 3 < block->count);

    uint32_t result = block->code[index];
    result ^= block->code[index + 1] << 8;
    result ^= block->code[index + 2] << 16;
    result ^= block->code[index + 3] << 24;
    return result;
}

int32_t read_s32(struct ir_block *block, int index) {
    return u32_to_s32(read_u32(block, index));
}


int write_constant(struct ir_block *block, uint64_t constant) {
    struct constant_table *table = &block->constants;
    if (table->count + 1 > table->capacity) {
        grow_constant_table(table);
    }
    table->data[table->count++] = constant;
    return table->count - 1;
}

uint64_t read_constant(struct ir_block *block, int index) {
    assert(0 <= index && index < block->constants.count);
    return block->constants.data[index];
}
