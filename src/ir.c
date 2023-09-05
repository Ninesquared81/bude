#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ir.h"
#include "type_punning.h"

#ifndef BLOCK_INIT_SIZE
#define BLOCK_INIT_SIZE 128
#endif

void init_block(struct ir_block *block) {
    block->code = malloc(BLOCK_INIT_SIZE * sizeof *block->code);
    if (block->code == NULL) {
        fprintf(stderr, "malloc failed!\n");
        exit(1);
    }
    block->capacity = BLOCK_INIT_SIZE;
    block->count = 0;
}

void free_block(struct ir_block *block) {
    free(block->code);
    block->code = NULL;
    block->capacity = 0;
    block->count = 0;
}

static struct ir_block *grow_block(struct ir_block *block) {
    int old_capacity = block->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : 8;
    void *new_code = realloc(block->code, sizeof *block + new_capacity);
    if (new_code == NULL) {
        /* Technically, this could leak memory, but any OS worth its salt
           will free all our memory when we exit. */
        fprintf(stderr, "realloc failed!\n");
        exit(1);
    }
    block->code = new_code;
    block->capacity = new_capacity;
    return block;
}

void write_simple(struct ir_block *block, enum opcode instruction) {
    if (block->count + 1 > block->capacity) {
        block = grow_block(block);
    }
    block->code[block->count++] = instruction;
}

void write_immediate_u8(struct ir_block *block, enum opcode instruction, uint8_t operand) {
    if (block->count + 2 > block->capacity) {
        block = grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
}

void write_immediate_s8(struct ir_block *block, enum opcode instruction, int8_t operand) {
    write_immediate_u8(block, instruction, s8_to_u8(operand));
}

void write_immediate_u16(struct ir_block *block, enum opcode instruction, uint16_t operand) {
    if (block->count + 3 > block->capacity) {
        block = grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
}

void write_immediate_s16(struct ir_block *block, enum opcode instruction, int16_t operand) {
    write_immediate_u16(block, instruction, s16_to_u16(operand));
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

