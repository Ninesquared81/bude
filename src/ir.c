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

#ifndef CONST_TABLE_INIT_SIZE
#define CONST_TABLE_INIT_SIZE 64
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

struct mem_obj {
    void *data;
    size_t size;
    int index;
};


const char *opcode_names[] = {
    [OP_NOP]               = "OP_NOP",
    [OP_PUSH8]             = "OP_PUSH8",
    [OP_PUSH16]            = "OP_PUSH16",
    [OP_PUSH32]            = "OP_PUSH32",
    [OP_PUSH64]            = "OP_PUSH64",
    [OP_PUSH_INT8]         = "OP_PUSH_INT8",
    [OP_PUSH_INT16]        = "OP_PUSH_INT16",
    [OP_PUSH_INT32]        = "OP_PUSH_INT32",
    [OP_PUSH_INT64]        = "OP_PUSH_INT64",
    [OP_PUSH_CHAR8]        = "OP_PUSH_CHAR8",
    [OP_LOAD_STRING8]      = "OP_LOAD_STRING8",
    [OP_LOAD_STRING16]     = "OP_LOAD_STRING16",
    [OP_LOAD_STRING32]     = "OP_LOAD_STRING32",
    [OP_POP]               = "OP_POP",
    [OP_ADD]               = "OP_ADD",
    [OP_AND]               = "OP_AND",
    [OP_DEREF]             = "OP_DEREF",
    [OP_DIVMOD]            = "OP_DIVMOD",
    [OP_DUPE]              = "OP_DUPE",
    [OP_EXIT]              = "OP_EXIT",
    [OP_FOR_DEC_START]     = "OP_FOR_DEC_START",
    [OP_FOR_DEC]           = "OP_FOR_DEC",
    [OP_FOR_INC_START]     = "OP_FOR_INC_START",
    [OP_FOR_INC]           = "OP_FOR_INC",
    [OP_GET_LOOP_VAR]      = "OP_GET_LOOP_VAR",
    [OP_JUMP]              = "OP_JUMP",
    [OP_JUMP_COND]         = "OP_JUMP_COND",
    [OP_JUMP_NCOND]        = "OP_JUMP_NCOND",
    [OP_MULT]              = "OP_MULT",
    [OP_NOT]               = "OP_NOT",
    [OP_OR]                = "OP_OR",
    [OP_PRINT]             = "OP_PRINT",
    [OP_PRINT_CHAR]        = "OP_PRINT_CHAR",
    [OP_PRINT_INT]         = "OP_PRINT_INT",
    [OP_SUB]               = "OP_SUB",
    [OP_SWAP]              = "OP_SWAP",
    [OP_SX8]               = "OP_SX8",
    [OP_SX8L]              = "OP_SX8L",
    [OP_SX16]              = "OP_SX16",
    [OP_SX16L]             = "OP_SX16L",
    [OP_SX32]              = "OP_SX32",
    [OP_SX32L]             = "OP_SX32L",
    [OP_ZX8]               = "OP_ZX8",
    [OP_ZX8L]              = "OP_ZX8L",
    [OP_ZX16]              = "OP_ZX16",
    [OP_ZX16L]             = "OP_ZX16L",
    [OP_ZX32]              = "OP_ZX32",
    [OP_ZX32L]             = "OP_ZX32L",
    [OP_AS_BYTE]           = "OP_AS_BYTE",
    [OP_AS_U8]             = "OP_AS_U8",
    [OP_AS_U16]            = "OP_AS_U16",
    [OP_AS_U32]            = "OP_AS_U32",
    [OP_AS_S8]             = "OP_AS_S8",
    [OP_AS_S16]            = "OP_AS_S16",
    [OP_AS_S32]            = "OP_AS_S32",
};

const char *get_opcode_name(enum opcode opcode) {
    assert(0 <= opcode && opcode < sizeof opcode_names / sizeof opcode_names[0]);
    return opcode_names[opcode];
}

bool is_jump(enum opcode instruction) {
    switch (instruction) {
    case OP_JUMP:
    case OP_JUMP_COND:
    case OP_JUMP_NCOND:
    case OP_FOR_DEC_START:
    case OP_FOR_DEC:
    case OP_FOR_INC_START:
    case OP_FOR_INC:
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

void init_block(struct ir_block *block, const char *restrict filename) {
    block->code = allocate_array(BLOCK_INIT_SIZE, sizeof *block->code);
    block->locations = allocate_array(BLOCK_INIT_SIZE, sizeof *block->locations);
    block->capacity = BLOCK_INIT_SIZE;
    block->count = 0;
    block->filename = filename;
    block->max_for_loop_level = 0;
    init_constant_table(&block->constants);
    init_jump_info_table(&block->jumps);
    init_string_table(&block->strings);
    block->static_memory = new_region(MEMORY_INIT_SIZE);
    //CHECK_ALLOCATION(block->static_memory);
    if (block->static_memory == NULL) {
        fprintf(stderr, "Failed to allocate region!\n");
        exit(1);
    }
}

void free_block(struct ir_block *block) {
    free_array(block->code, block->capacity, sizeof *block->code);
    free_array(block->locations, block->capacity, sizeof *block->locations);
    block->code = NULL;
    block->locations = NULL;
    block->capacity = 0;
    block->count = 0;
    free_constant_table(&block->constants);
    free_jump_info_table(&block->jumps);
    kill_region(block->static_memory);
}

void init_constant_table(struct constant_table *table) {
    table->data = allocate_array(CONST_TABLE_INIT_SIZE, sizeof *table->data);
    table->capacity = CONST_TABLE_INIT_SIZE;
    table->count = 0;
}

void free_constant_table(struct constant_table *table) {
    free_array(table->data, table->capacity, sizeof table->data[0]);
    table->data = 0;
    table->capacity = 0;
    table->count = 0;
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

static void grow_block(struct ir_block *block) {
    int old_capacity = block->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : BLOCK_INIT_SIZE;
    block->code = reallocate_array(block->code, old_capacity, new_capacity,
                                   sizeof block->code[0]);
    block->capacity = new_capacity;
}

static void grow_constant_table(struct constant_table *table) {
    int old_capacity = table->capacity;
    int new_capacity = (old_capacity > 0) ? old_capacity + old_capacity/2 : CONST_TABLE_INIT_SIZE;
    table->data = reallocate_array(table->data, old_capacity, new_capacity,
                                   sizeof table->data[0]);
    table->capacity = new_capacity;
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

void write_immediate_u64(struct ir_block *block, enum opcode instruction, uint64_t operand) {
    if (block->count + 9 > block->capacity) {
        grow_block(block);
    }
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

void write_immediate_s64(struct ir_block *block, enum opcode instruction, int64_t operand) {
    write_immediate_u64(block, instruction, s64_to_u64(operand));
}

void write_immediate_uv(struct ir_block *block, enum opcode instruction8, uint64_t operand) {
    enum opcode instruction16 = instruction8 + 1;
    enum opcode instruction32 = instruction8 + 2;
    enum opcode instruction64 = instruction8 + 3;
    if (operand <= UINT8_MAX) {
        write_immediate_u8(block, instruction8, operand);
    }
    else if (operand <= UINT16_MAX) {
        write_immediate_u16(block, instruction16, operand);
    }
    else if (operand <= UINT32_MAX) {
        write_immediate_u32(block, instruction32, operand);
    }
    else {
        write_immediate_u64(block, instruction64, operand);
    }
}

void write_immediate_sv(struct ir_block *block, enum opcode instruction8, int64_t operand) {
    enum opcode instruction16 = instruction8 + 1;
    enum opcode instruction32 = instruction8 + 2;
    enum opcode instruction64 = instruction8 + 3;
    if (INT8_MIN < operand && operand <= INT8_MAX) {
        write_immediate_s8(block, instruction8, operand);
    }
    else if (INT16_MIN < operand && operand <= INT16_MAX) {
        write_immediate_s16(block, instruction16, operand);
    }
    else if (INT32_MIN < operand && operand <= INT32_MAX) {
        write_immediate_s32(block, instruction32, operand);
    }
    else {
        write_immediate_s64(block, instruction64, operand);
    }
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

void overwrite_instruction(struct ir_block *block, int index, enum opcode instruction) {
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

uint32_t write_string(struct ir_block *block, struct string_builder *builder) {
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

struct string_view *read_string(struct ir_block *block, uint32_t index) {
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

int add_jump(struct ir_block *block, int dest) {
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

int find_jump(struct ir_block *block, int dest) {
    // Binary search.
    struct jump_info_table *jumps = &block->jumps;
    int index = binary_search(jumps, dest);
    return (index < jumps->count && jumps->dests[index] == dest) ? index : -1;  // -1: search failed.
}

bool is_jump_dest(struct ir_block *block, int dest) {
    return find_jump(block, dest) != -1;
}
