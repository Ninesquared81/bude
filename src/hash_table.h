#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>

struct hash_table {
    size_t capacity;
    size_t count;
    struct table_element *elements;
};

struct table_element {
    struct table_element *next;
    struct string_view *sv;
};

uint32_t hash_sv(struct string_view *key);

#endif
