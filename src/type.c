#include <stddef.h>

#include "memory.h"
#include "type.h"

#define TYPE_TABLE_INIT_SIZE 32
#define TYPE_TABLE_REGION_SIZE 4096


const char *kind_name(enum type_kind kind) {
    switch(kind) {
    case KIND_SIMPLE: return "simple";
    case KIND_PACK:   return "pack";
    case KIND_COMP:   return "comp";
    }
    assert(0 && "Invalid kind");
    return "<Invalid kind>";
}

const char *type_name(type_index type) {
    switch (type) {
    case TYPE_ERROR: return "<TYPE_ERROR>";
    case TYPE_WORD:  return "word";
    case TYPE_BYTE:  return "byte";
    case TYPE_PTR:   return "ptr";
    case TYPE_INT:   return "int";
    case TYPE_U8:    return "u8";
    case TYPE_U16:   return "u16";
    case TYPE_U32:   return "u32";
    case TYPE_S8:    return "s8";
    case TYPE_S16:   return "s16";
    case TYPE_S32:   return "s32";
    }
    // TODO: get custom type names.
    return "<User-defined type>";
}

size_t type_size(type_index type) {
    switch (type) {
    case TYPE_ERROR: return 0;
    case TYPE_WORD:  return 8;
    case TYPE_BYTE:  return 1;
    case TYPE_PTR:   return 8;
    case TYPE_U8:    return 1;
    case TYPE_U16:   return 2;
    case TYPE_U32:   return 4;
    case TYPE_S8:    return 1;
    case TYPE_S16:   return 2;
    case TYPE_S32:   return 4;
    }
    // TODO: get size of custom types.
    return 0;
}

void init_type_table(struct type_table *types) {
    types->capacity = 0;
    types->count = 0;
    types->infos = allocate_array(TYPE_TABLE_INIT_SIZE, sizeof types->infos[0]);
    types->extra_info = new_region(TYPE_TABLE_REGION_SIZE);
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
    // Offset to get valid index again (see below).
    return index + SIMPLE_TYPE_COUNT;
}

const struct type_info *lookup_type(const struct type_table *types, type_index type) {
    /* NOTE: if a simple type is requested, instead of filling up our table with simple types,
     * we keep a "basic" type info struct which we can return the address of instead (it's
     * static so returning the address is fine). This means we only have to keep user-defined
     * types in the table.
     */
    static const struct type_info basic = {0};
    if (IS_SIMPLE_TYPE(type)) return &basic;
    int index = type - SIMPLE_TYPE_COUNT;
    if (index < 0 || index >= types->count) return NULL;
    return &types->infos[index];
}

void *alloc_extra(struct type_table *types, size_t size) {
    return region_alloc(types->extra_info, size);
}
