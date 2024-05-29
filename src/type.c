#include <stddef.h>

#include "memory.h"
#include "stack.h"
#include "type.h"

#define TYPE_TABLE_INIT_SIZE 32
#define TYPE_TABLE_REGION_SIZE 4096


const char *kind_name(enum type_kind kind) {
    switch(kind) {
    case KIND_UNINIT: return "<Uninitialised type>";
    case KIND_SIMPLE: return "simple";
    case KIND_PACK:   return "pack";
    case KIND_COMP:   return "comp";
    }
    assert(0 && "Invalid kind");
    return "<Invalid kind>";
}

struct string_view type_name(struct type_table *table, type_index type) {
    if (IS_SIMPLE_TYPE(type)) {
        switch ((enum simple_type)type) {
        case TYPE_ERROR:  return SV_LIT("<TYPE_ERROR>");
        case TYPE_WORD:   return SV_LIT("word");
        case TYPE_BYTE:   return SV_LIT("byte");
        case TYPE_PTR:    return SV_LIT("ptr");
        case TYPE_INT:    return SV_LIT("int");
        case TYPE_U8:     return SV_LIT("u8");
        case TYPE_U16:    return SV_LIT("u16");
        case TYPE_U32:    return SV_LIT("u32");
        case TYPE_S8:     return SV_LIT("s8");
        case TYPE_S16:    return SV_LIT("s16");
        case TYPE_S32:    return SV_LIT("s32");
        case TYPE_F32:    return SV_LIT("f32");
        case TYPE_F64:    return SV_LIT("f64");
        case TYPE_CHAR:   return SV_LIT("char");
        case TYPE_CHAR32: return SV_LIT("char32");
        }
        assert(0 && "unreachable");
    }
    const struct type_info *info = lookup_type(table, type);
    if (info == NULL) return SV_LIT("<Undefined type>");
    return info->name;
}

size_t type_size(struct type_table *table, type_index type) {
    if (IS_SIMPLE_TYPE(type)) {
        switch ((enum simple_type)type) {
        case TYPE_ERROR:  return 0;
        case TYPE_WORD:   return 8;
        case TYPE_BYTE:   return 1;
        case TYPE_PTR:    return 8;
        case TYPE_INT:    return 8;
        case TYPE_U8:     return 1;
        case TYPE_U16:    return 2;
        case TYPE_U32:    return 4;
        case TYPE_S8:     return 1;
        case TYPE_S16:    return 2;
        case TYPE_S32:    return 4;
        case TYPE_F32:    return 4;
        case TYPE_F64:    return 8;
        case TYPE_CHAR:   return 4;  // Assume max size for UTF-8, which is 4 bytes.
        case TYPE_CHAR32: return 4;
        }
        assert(0 && "unreachable");
    }
    const struct type_info *info = lookup_type(table, type);
    if (info == NULL) return 0;
    switch (info->kind) {
    case KIND_PACK:
        return info->pack.size;
    case KIND_COMP:
        return info->comp.word_count * sizeof(stack_word);
    default:
        return 0;
    }
}

bool is_integral(type_index type) {
    switch (type) {
    case TYPE_WORD:
    case TYPE_BYTE:
    case TYPE_INT:
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_S8:
    case TYPE_S16:
    case TYPE_S32:
        return true;
    default:
        return false;
    }
}

bool is_signed(type_index type) {
    switch (type) {
    case TYPE_INT:
    case TYPE_S8:
    case TYPE_S16:
    case TYPE_S32:
        return true;
    default:
        return false;
    }
}

bool is_float(type_index type) {
    switch (type) {
    case TYPE_F32:
    case TYPE_F64:
        return true;
    default:
        return false;
    }
}

bool is_numeric(type_index type) {
    return is_integral(type) || is_float(type);
}

bool is_character(type_index type) {
    switch (type) {
    case TYPE_CHAR:
    case TYPE_CHAR32:
        return true;
    default:
        return false;
    }
}

bool is_pack(struct type_table *table, type_index type) {
    if (IS_SIMPLE_TYPE(type)) return false;
    const struct type_info *info = lookup_type(table, type);
    assert(info != NULL);
    return info->kind == KIND_PACK;
}

bool is_comp(struct type_table *table, type_index type) {
    if (IS_SIMPLE_TYPE(type)) return false;
    const struct type_info *info = lookup_type(table, type);
    assert(info != NULL);
    return info->kind == KIND_COMP;
}

static void init_builtin_types(struct type_table *types) {
    static_assert(TYPE_TABLE_INIT_SIZE >= BUILTIN_TYPE_COUNT);
    assert(types->count == 0);
    static type_index string_fields[] = {
        TYPE_PTR,
        TYPE_WORD,
    };
    static int string_offsets[] = {2, 1};
    types->infos[types->count++] = (struct type_info) {
        .kind = KIND_COMP,
        .comp = {
            .field_count = 2,
            .word_count = 2,
            .fields = string_fields,
            .offsets = string_offsets,
        },
        .name = SV_LIT("string"),
    };
    assert(types->count == BUILTIN_TYPE_COUNT);
}

void init_type_table(struct type_table *types) {
    types->capacity = TYPE_TABLE_INIT_SIZE;
    types->count = 0;
    types->infos = allocate_array(TYPE_TABLE_INIT_SIZE, sizeof types->infos[0]);
    types->extra_info = new_region(TYPE_TABLE_REGION_SIZE);
    init_builtin_types(types);
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
    types->capacity = new_capacity;
}

type_index new_type(struct type_table *types, struct string_view *name) {
    if (types->count + 1 > types->capacity) {
        grow_type_table(types);
    }
    int index = types->count++;
    struct type_info *info = &types->infos[index];
    info->kind = KIND_UNINIT;
    info->name = copy_view_in_region(name, types->extra_info);
    // Offset to get valid index again (see below).
    return index + SIMPLE_TYPE_COUNT;
}

void init_type(struct type_table *types, type_index type, const struct type_info *info) {
    assert(info != NULL);
    assert(!IS_SIMPLE_TYPE(type));
    int index = type - SIMPLE_TYPE_COUNT;
    struct string_view name = types->infos[index].name;
    types->infos[index] = *info;
    types->infos[index].name = name;
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
