#include <stddef.h>

#include "memory.h"
#include "type.h"

#define TYPE_TABLE_INIT_SIZE 32

void init_type_table(struct type_table *types) {
    types->capacity = 0;
    types->count = 0;
    types->infos = allocate_array(TYPE_TABLE_INIT_SIZE, sizeof types->infos[0]);
}

void free_type_table(struct type_table *types) {
    free_array(types->infos, types->capacity, sizeof types->infos[0]);
    types->infos = NULL;
    types->capacity = 0;
    types->count = 0;
}

static void grow_type_table(struct type_table *types) {
    int old_capacity = types->capacity;
    int new_capacity = (old_capacity > 0)
        ? old_capacity + old_capacity/2
        : TYPE_TABLE_INIT_SIZE;
    types->infos = reallocate_array(types->infos, old_capacity, new_capacity,
                                    sizeof *types->infos);
}

type_index new_type(struct type_table *types, const struct type_info *info) {
    if (types->count + 1 > types->capacity) {
        grow_type_table(types);
    }
    int index = types->count++;
    if (info != NULL) {
        types->infos[index] = *info;
    }
    return index + SIMPLE_TYPE_COUNT;
}

const struct type_info *lookup_type(const struct type_table *types, type_index type) {
    static const struct type_info basic = {0};
    if (IS_BASIC_TYPE(type)) return &basic;
    int index = type - SIMPLE_TYPE_COUNT;
    if (index < 0 || index >= types->count) return NULL;
    return &types->infos[index];
}

