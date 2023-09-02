#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ir.h"


struct ir_block *allocate_block(int size) {
    assert(size >= 0);
    struct ir_block *block = malloc(sizeof *block + size);
    block->capacity = size;
    block->count = 0;
    return block;
}

void free_block(struct ir_block *block) {
    free(block);
}

static struct ir_block *grow_block(struct ir_block *block) {
    int old_capacity = block->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : 8;
    if ((block = realloc(block, sizeof *block + new_capacity)) == NULL) {
        /* Technically, this could leak memory, but any OS worth its salt
           will free all our memory when we exit. */
        fprintf(stderr, "realloc failed!\n");
        exit(1);
    }
    block->capacity = new_capacity;
    return block;
}

void write_simple(struct ir_block *block, enum opcode instruction) {
    if (block->count + 1 > block->capacity) {
        block = grow_block(block);
    }
    block->code[block->count++] = instruction;
}

void write_immediate(struct ir_block *block, enum opcode instruction, uint8_t operand) {
    if (block->count + 2 > block->capacity) {
        block = grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
}
