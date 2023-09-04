#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ir.h"

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

void write_immediate(struct ir_block *block, enum opcode instruction, uint8_t operand) {
    if (block->count + 2 > block->capacity) {
        block = grow_block(block);
    }
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
}
