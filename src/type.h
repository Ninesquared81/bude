#ifndef TYPE_H
#define TYPE_H

enum simple_type {
    TYPE_ERROR,

    TYPE_WORD,
    TYPE_BYTE,
    TYPE_PTR,
    TYPE_INT,

    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_S8,
    TYPE_S16,
    TYPE_S32,
};

// Each type has a unique numeric identifier. For simple types,
// the identifier comes from the enum above. User-defined types
// such as packs and comps use subsequent identifiers.
typedef int type_index;

#define SIMPLE_TYPE_COUNT 11
static_assert(SIMPLE_TYPE_COUNT == TYPE_S32 + 1);
static_assert(TYPE_ERROR == 0);

#define IS_BASIC_TYPE(type) \
    ((type) < SIMPLE_TYPE_COUNT && TYPE_ERROR <= (type))

#endif
