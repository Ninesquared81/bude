#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "location.h"
#include "region.h"
#include "type_punning.h"

#ifndef BLOCK_INIT_SIZE
#define BLOCK_INIT_SIZE 128
#endif

#ifndef JUMP_INFO_TABLE_INIT_SIZE
#define JUMP_INFO_TABLE_INIT_SIZE 8
#endif

#ifndef STRING_TABLE_INIT_SIZE
#define STRING_TABLE_INIT_SIZE 64
#endif

#ifndef MEMORY_INIT_SIZE
#define MEMORY_INIT_SIZE 1024
#endif


const char *t_opcode_names[] = {
    [T_OP_NOP]               = "T_OP_NOP",
    [T_OP_PUSH8]             = "T_OP_PUSH8",
    [T_OP_PUSH16]            = "T_OP_PUSH16",
    [T_OP_PUSH32]            = "T_OP_PUSH32",
    [T_OP_PUSH64]            = "T_OP_PUSH64",
    [T_OP_PUSH_INT8]         = "T_OP_PUSH_INT8",
    [T_OP_PUSH_INT16]        = "T_OP_PUSH_INT16",
    [T_OP_PUSH_INT32]        = "T_OP_PUSH_INT32",
    [T_OP_PUSH_INT64]        = "T_OP_PUSH_INT64",
    [T_OP_PUSH_CHAR8]        = "T_OP_PUSH_CHAR8",
    [T_OP_LOAD_STRING8]      = "T_OP_LOAD_STRING8",
    [T_OP_LOAD_STRING16]     = "T_OP_LOAD_STRING16",
    [T_OP_LOAD_STRING32]     = "T_OP_LOAD_STRING32",
    [T_OP_POP]               = "T_OP_POP",
    [T_OP_ADD]               = "T_OP_ADD",
    [T_OP_AND]               = "T_OP_AND",
    [T_OP_DEREF]             = "T_OP_DEREF",
    [T_OP_DIVMOD]            = "T_OP_DIVMOD",
    [T_OP_DUPE]              = "T_OP_DUPE",
    [T_OP_EXIT]              = "T_OP_EXIT",
    [T_OP_FOR_DEC_START]     = "T_OP_FOR_DEC_START",
    [T_OP_FOR_DEC]           = "T_OP_FOR_DEC",
    [T_OP_FOR_INC_START]     = "T_OP_FOR_INC_START",
    [T_OP_FOR_INC]           = "T_OP_FOR_INC",
    [T_OP_GET_LOOP_VAR]      = "T_OP_GET_LOOP_VAR",
    [T_OP_JUMP]              = "T_OP_JUMP",
    [T_OP_JUMP_COND]         = "T_OP_JUMP_COND",
    [T_OP_JUMP_NCOND]        = "T_OP_JUMP_NCOND",
    [T_OP_MULT]              = "T_OP_MULT",
    [T_OP_NOT]               = "T_OP_NOT",
    [T_OP_OR]                = "T_OP_OR",
    [T_OP_PRINT]             = "T_OP_PRINT",
    [T_OP_PRINT_CHAR]        = "T_OP_PRINT_CHAR",
    [T_OP_PRINT_INT]         = "T_OP_PRINT_INT",
    [T_OP_SUB]               = "T_OP_SUB",
    [T_OP_SWAP]              = "T_OP_SWAP",
    [T_OP_AS_BYTE]           = "T_OP_AS_BYTE",
    [T_OP_AS_U8]             = "T_OP_AS_U8",
    [T_OP_AS_U16]            = "T_OP_AS_U16",
    [T_OP_AS_U32]            = "T_OP_AS_U32",
    [T_OP_AS_S8]             = "T_OP_AS_S8",
    [T_OP_AS_S16]            = "T_OP_AS_S16",
    [T_OP_AS_S32]            = "T_OP_AS_S32",

};

const char *w_opcode_names[] = {
    [W_OP_NOP]               = "W_OP_NOP",
    [W_OP_PUSH8]             = "W_OP_PUSH8",
    [W_OP_PUSH16]            = "W_OP_PUSH16",
    [W_OP_PUSH32]            = "W_OP_PUSH32",
    [W_OP_PUSH64]            = "W_OP_PUSH64",
    [W_OP_PUSH_INT8]         = "W_OP_PUSH_INT8",
    [W_OP_PUSH_INT16]        = "W_OP_PUSH_INT16",
    [W_OP_PUSH_INT32]        = "W_OP_PUSH_INT32",
    [W_OP_PUSH_INT64]        = "W_OP_PUSH_INT64",
    [W_OP_PUSH_CHAR8]        = "W_OP_PUSH_CHAR8",
    [W_OP_LOAD_STRING8]      = "W_OP_LOAD_STRING8",
    [W_OP_LOAD_STRING16]     = "W_OP_LOAD_STRING16",
    [W_OP_LOAD_STRING32]     = "W_OP_LOAD_STRING32",
    [W_OP_POP]               = "W_OP_POP",
    [W_OP_ADD]               = "W_OP_ADD",
    [W_OP_AND]               = "W_OP_AND",
    [W_OP_DEREF]             = "W_OP_DEREF",
    [W_OP_DIVMOD]            = "W_OP_DIVMOD",
    [W_OP_IDIVMOD]           = "W_OP_IDIVMOD",
    [W_OP_EDIVMOD]           = "W_OP_EDIVMOD",
    [W_OP_DUPE]              = "W_OP_DUPE",
    [W_OP_EXIT]              = "W_OP_EXIT",
    [W_OP_FOR_DEC_START]     = "W_OP_FOR_DEC_START",
    [W_OP_FOR_DEC]           = "W_OP_FOR_DEC",
    [W_OP_FOR_INC_START]     = "W_OP_FOR_INC_START",
    [W_OP_FOR_INC]           = "W_OP_FOR_INC",
    [W_OP_GET_LOOP_VAR]      = "W_OP_GET_LOOP_VAR",
    [W_OP_JUMP]              = "W_OP_JUMP",
    [W_OP_JUMP_COND]         = "W_OP_JUMP_COND",
    [W_OP_JUMP_NCOND]        = "W_OP_JUMP_NCOND",
    [W_OP_MULT]              = "W_OP_MULT",
    [W_OP_NOT]               = "W_OP_NOT",
    [W_OP_OR]                = "W_OP_OR",
    [W_OP_PRINT]             = "W_OP_PRINT",
    [W_OP_PRINT_CHAR]        = "W_OP_PRINT_CHAR",
    [W_OP_PRINT_INT]         = "W_OP_PRINT_INT",
    [W_OP_SUB]               = "W_OP_SUB",
    [W_OP_SWAP]              = "W_OP_SWAP",
    [W_OP_SX8]               = "W_OP_SX8",
    [W_OP_SX8L]              = "W_OP_SX8L",
    [W_OP_SX16]              = "W_OP_SX16",
    [W_OP_SX16L]             = "W_OP_SX16L",
    [W_OP_SX32]              = "W_OP_SX32",
    [W_OP_SX32L]             = "W_OP_SX32L",
    [W_OP_ZX8]               = "W_OP_ZX8",
    [W_OP_ZX8L]              = "W_OP_ZX8L",
    [W_OP_ZX16]              = "W_OP_ZX16",
    [W_OP_ZX16L]             = "W_OP_ZX16L",
    [W_OP_ZX32]              = "W_OP_ZX32",
    [W_OP_ZX32L]             = "W_OP_ZX32L",
};

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

void init_tir_block(struct tir_block *block, const char *restrict filename) {
    block->code = allocate_array(BLOCK_INIT_SIZE, sizeof *block->code);
    block->locations = allocate_array(BLOCK_INIT_SIZE, sizeof *block->locations);
    block->capacity = BLOCK_INIT_SIZE;
    block->count = 0;
    block->filename = filename;
    block->max_for_loop_level = 0;
    init_jump_info_table(&block->jumps);
    init_string_table(&block->strings);
    block->static_memory = new_region(MEMORY_INIT_SIZE);
    //CHECK_ALLOCATION(block->static_memory);
    if (block->static_memory == NULL) {
        fprintf(stderr, "Failed to allocate region!\n");
        exit(1);
    }
}

void free_tir_block(struct tir_block *block) {
    free_array(block->code, block->capacity, sizeof *block->code);
    free_array(block->locations, block->capacity, sizeof *block->locations);
    block->code = NULL;
    block->locations = NULL;
    block->capacity = 0;
    block->count = 0;
    free_jump_info_table(&block->jumps);
    kill_region(block->static_memory);
}

void init_jump_info_table(struct jump_info_table *table) {
    table->dests = NULL;
    table->capacity = 0;
    table->count = 0;
}

void free_jump_info_table(struct jump_info_table *table) {
    free(table->dests);
    table->dests = NULL;
    table->capacity = 0;
    table->count = 0;
}

void init_string_table(struct string_table *table) {
    table->views = allocate_array(STRING_TABLE_INIT_SIZE, sizeof *table->views);
    table->capacity = STRING_TABLE_INIT_SIZE;
    table->count = 0;
}

void free_string_table(struct string_table *table) {
    free_array(table->views, table->capacity, sizeof *table->views);
    table->views = NULL;
    table->capacity = 0;
    table->count = 0;
}

static void grow_block(struct tir_block *block) {
    int old_capacity = block->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : BLOCK_INIT_SIZE;
    block->code = reallocate_array(block->code, old_capacity, new_capacity,
                                   sizeof block->code[0]);
    block->capacity = new_capacity;
}

static void grow_jump_info_table(struct jump_info_table *table) {
    int old_capacity = table->capacity;
    int new_capacity = (old_capacity > 0)
        ? old_capacity + old_capacity/2
        : JUMP_INFO_TABLE_INIT_SIZE;

    table->dests = reallocate_array(table->dests, old_capacity, new_capacity,
                                    sizeof table->dests[0]);
    table->capacity = new_capacity;    
}

static void grow_string_table(struct string_table *table) {
    size_t old_capacity = table->capacity;
    size_t new_capacity = old_capacity + old_capacity/2;
    assert(new_capacity > 0);
    table->views = reallocate_array(table->views, old_capacity, new_capacity,
                                      sizeof table->views[0]);
    table->capacity = new_capacity;
}

void write_simple(struct tir_block *block, enum t_opcode instruction, struct location *location) {
    if (block->count + 1 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
}

void write_immediate_u8(struct tir_block *block, enum t_opcode instruction, uint8_t operand,
                        struct location *location) {
    if (block->count + 2 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
}

void write_immediate_s8(struct tir_block *block, enum t_opcode instruction, int8_t operand,
                        struct location *location) {
    write_immediate_u8(block, instruction, s8_to_u8(operand), location);
}

void write_immediate_u16(struct tir_block *block, enum t_opcode instruction, uint16_t operand,
                        struct location *location) {
    if (block->count + 3 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
}

void write_immediate_s16(struct tir_block *block, enum t_opcode instruction, int16_t operand,
                        struct location *location) {
    write_immediate_u16(block, instruction, s16_to_u16(operand), location);
}

void write_immediate_u32(struct tir_block *block, enum t_opcode instruction, uint32_t operand,
                        struct location *location) {
    if (block->count + 5 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
    block->code[block->count++] = operand >> 16;
    block->code[block->count++] = operand >> 24;
}

void write_immediate_s32(struct tir_block *block, enum t_opcode instruction, int32_t operand,
                        struct location *location) {
    write_immediate_u32(block, instruction, s32_to_u32(operand), location);
}

void write_immediate_u64(struct tir_block *block, enum t_opcode instruction, uint64_t operand,
                        struct location *location) {
    if (block->count + 9 > block->capacity) {
        grow_block(block);
    }
    block->locations[block->count] = *location;
    block->code[block->count++] = instruction;
    block->code[block->count++] = operand;
    block->code[block->count++] = operand >> 8;
    block->code[block->count++] = operand >> 16;
    block->code[block->count++] = operand >> 24;
    block->code[block->count++] = operand >> 32;
    block->code[block->count++] = operand >> 40;
    block->code[block->count++] = operand >> 48;
    block->code[block->count++] = operand >> 56;
}

void write_immediate_s64(struct tir_block *block, enum t_opcode instruction, int64_t operand,
                        struct location *location) {
    write_immediate_u64(block, instruction, s64_to_u64(operand), location);
}

void overwrite_u8(struct tir_block *block, int start, uint8_t value) {
    assert(0 <= start && start < block->count);
    block->code[start] = value;
}

void overwrite_s8(struct tir_block *block, int start, int8_t value) {
    overwrite_u8(block, start, s8_to_u8(value));
}

void overwrite_u16(struct tir_block *block, int start, uint16_t value) {
    assert(0 <= start && start + 1 < block->count);  // Make sure there's space.
    // Note: The IR instruction set is little-endian.
    block->code[start] = value;  // LSB.
    block->code[start + 1] = value >> 8;  // MSB. 
}

void overwrite_s16(struct tir_block *block, int start, int16_t value) {
    overwrite_u16(block, start, s16_to_u16(value));
}

void overwrite_u32(struct tir_block *block, int start, uint32_t value) {
    assert(0 <= start && start + 3 < block->count);
    block->code[start] = value;
    block->code[start + 1] = value >> 8;
    block->code[start + 2] = value >> 16;
    block->code[start + 3] = value >> 24;
}

void overwrite_s32(struct tir_block *block, int start, int32_t value) {
    overwrite_u32(block, start, s32_to_u32(value));
}

void overwrite_u64(struct tir_block *block, int start, uint64_t value) {
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

void overwrite_s64(struct tir_block *block, int start, int64_t value) {
    overwrite_u64(block, start, s64_to_u64(value));
}

void overwrite_instruction(struct tir_block *block, int index, enum t_opcode instruction) {
    // An instruction opcode is basically just a u8.
    overwrite_u8(block, index, instruction);
}

uint8_t read_u8(struct tir_block *block, int index) {
    assert(0 <= index && index < block->count);
    return block->code[index];
}

int8_t read_s8(struct tir_block *block, int index) {
    return u8_to_s8(read_u8(block, index));
}

uint16_t read_u16(struct tir_block *block, int index) {
    assert(0 <= index && index + 1 < block->count);
    return block->code[index] ^ (block->code[index + 1] << 8);
}

int16_t read_s16(struct tir_block *block, int index) {
    return u16_to_s16(read_u16(block, index));
}

uint32_t read_u32(struct tir_block *block, int index) {
    assert(0 <= index && index + 3 < block->count);

    uint32_t result = block->code[index];
    result ^= block->code[index + 1] << 8;
    result ^= block->code[index + 2] << 16;
    result ^= block->code[index + 3] << 24;
    return result;
}

int32_t read_s32(struct tir_block *block, int index) {
    return u32_to_s32(read_u32(block, index));
}

uint64_t read_u64(struct tir_block *block, int index) {
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

int64_t read_s64(struct tir_block *block, int index) {
    return u64_to_s64(read_u64(block, index));
}

uint32_t write_string(struct tir_block *block, struct string_builder *builder) {
    struct string_view view = build_string_in_region(builder, block->static_memory);
    if (view.start == NULL) {
        fprintf(stderr, "Failed to allocate string.\n");
        exit(1);
    }
    struct string_table *table = &block->strings;
    if (table->count + 1 > table->capacity) {
        grow_string_table(table);
    }
    table->views[table->count++] = view;
    return table->count - 1;
}

struct string_view *read_string(struct tir_block *block, uint32_t index) {
    assert(index < block->strings.count);
    return &block->strings.views[index];
}

static int binary_search(struct jump_info_table *jumps, int dest) {
    int hi = jumps->count - 1;
    int lo = 0;
    while (hi > lo) {
        int mid = lo + (hi - lo) / 2;
        if (jumps->dests[mid] == dest) {
            return mid;
        }
        if (jumps->dests[mid] < dest) {
            lo = mid + 1;
        }
        else {
            hi = mid - 1;
        }
    }
    return lo;
}

int add_jump(struct tir_block *block, int dest) {
    struct jump_info_table *jumps = &block->jumps;
    if (jumps->count + 1 > jumps->capacity) {
        grow_jump_info_table(jumps);
    }
    // Usually, we add elements in order, so first see if we can just add it on the end.
    // Also, we handle the empty case here as well.
    if (jumps->count == 0 || jumps->dests[jumps->count - 1] < dest) {
        jumps->dests[jumps->count++] = dest;
        return jumps->count - 1;
    }
    // Binary insert.
    int index = binary_search(jumps, dest);
    size_t shift = jumps->count - index;
    memmove(&jumps->dests[index + 1], &jumps->dests[index], shift * sizeof jumps->dests[0]);
    ++jumps->count;
    jumps->dests[index] = dest;
    return index;
}

int find_jump(struct tir_block *block, int dest) {
    // Binary search.
    struct jump_info_table *jumps = &block->jumps;
    int index = binary_search(jumps, dest);
    return (index < jumps->count && jumps->dests[index] == dest) ? index : -1;  // -1: search failed.
}

bool is_jump_dest(struct tir_block *block, int dest) {
    return find_jump(block, dest) != -1;
}
