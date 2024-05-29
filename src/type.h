#ifndef TYPE_H
#define TYPE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "string_view.h"
#include "region.h"

enum simple_type {
    // Sentinel types.
    TYPE_ERROR,

    // Basic integral types.
    TYPE_WORD,
    TYPE_BYTE,
    TYPE_PTR,
    TYPE_INT,

    // Specific-width integral types.
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_S8,
    TYPE_S16,
    TYPE_S32,

    // Floating-point types.
    TYPE_F32,
    TYPE_F64,

    // Character types.
    TYPE_CHAR,
    TYPE_CHAR16,
    TYPE_CHAR32,
};

#define SIMPLE_TYPE_COUNT 16
#define LAST_SIMPLE_TYPE TYPE_CHAR32
static_assert(SIMPLE_TYPE_COUNT == LAST_SIMPLE_TYPE + 1);
static_assert(TYPE_ERROR == 0);

enum builtin_type {
    TYPE_STRING = SIMPLE_TYPE_COUNT,
};

#define BUILTIN_TYPE_COUNT 1
#define LAST_BUILTIN_TYPE TYPE_STRING
static_assert(BUILTIN_TYPE_COUNT + SIMPLE_TYPE_COUNT == LAST_BUILTIN_TYPE + 1);

// Each type has a unique numeric identifier. For simple types,
// the identifier comes from the enum above. User-defined types
// such as packs and comps use subsequent identifiers.
typedef int type_index;

#define IS_SIMPLE_TYPE(type) \
    ((type) < SIMPLE_TYPE_COUNT && TYPE_ERROR <= (type))

bool is_signed(type_index type);
bool is_integral(type_index type);
bool is_float(type_index type);
bool is_numeric(type_index type);
bool is_character(type_index type);

struct type_info {
    enum type_kind {
        // This marks a type as uninitialised.
        KIND_UNINIT = -1,

        KIND_SIMPLE = 0,
        KIND_PACK,
        KIND_COMP,
    } kind;
    union {
        struct {
            int field_count;
            int size;
            type_index fields[8];
        } pack;
        struct {
            int field_count;
            int word_count;
            type_index *fields;
            int *offsets;
        } comp;
    };
    struct string_view name;
};

const char *kind_name(enum type_kind kind);

struct type_table {
    int capacity;
    int count;
    struct type_info *infos;
    struct region *extra_info;
};

struct string_view type_name(struct type_table *table, type_index type);
size_t type_size(struct type_table *table, type_index type);

bool is_pack(struct type_table *table, type_index type);
bool is_comp(struct type_table *table, type_index type);

void init_type_table(struct type_table *types);
void free_type_table(struct type_table *types);
type_index new_type(struct type_table *types, struct string_view *name);
void init_type(struct type_table *types, type_index type, const struct type_info *info);
const struct type_info *lookup_type(const struct type_table *types, type_index type);
void *alloc_extra(struct type_table *types, size_t size);

#endif
