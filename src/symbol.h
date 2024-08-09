#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "string_view.h"
#include "type.h"

#define SYMDICT_INIT_SIZE 128

enum symbol_type {
    SYM_LOOP_VAR,
    SYM_PACK,
    SYM_COMP,
    SYM_PACK_FIELD,
    SYM_COMP_FIELD,
    SYM_FUNCTION,
    SYM_VAR,
    SYM_EXT_FUNCTION,
    SYM_EXT_LIBRARY,
};

struct symbol {
    struct string_view name;
    enum symbol_type type;
    union {
        struct {
            size_t level;
        } loop_var;
        struct {
            type_index index;
        } pack;
        struct {
            type_index index;
        } comp;
        struct {
            type_index pack;
            int field_offset;
        } pack_field;
        struct {
            type_index comp;
            int field_offset;
        } comp_field;
        struct {
            int index;
        } function;
        struct {
            int var;  // Index of local variable.
            int function;  // Index of owning function.
        } var;
        struct {
            int index;
        } ext_function;
        struct {
            int index;
        } ext_library;
    };
};

struct symdict_slot {
    struct symbol symbol;
    uint32_t hash;
    bool is_occupied;
};

struct symbol_dictionary {
    size_t capacity;
    size_t count;
    struct symdict_slot *slots;
};

void init_symbol_dictionary(struct symbol_dictionary *dict);
void free_symbol_dictionary(struct symbol_dictionary *dict);
void insert_symbol(struct symbol_dictionary *dict, const struct symbol *symbol);
struct symbol *lookup_symbol(const struct symbol_dictionary *dict, const struct string_view *sv);

#endif
