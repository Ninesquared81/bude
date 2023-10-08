#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stdint.h>

struct hash_table {
    size_t capacity;
    size_t count;
    struct table_bucket *buckets;
    struct region *element_region;
};

struct table_element {
    // We use separate chaining.
    struct table_element *next;
    struct string_view sv;
};

struct table_bucket {
    struct table_element head;
    uint32_t hash;
    bool is_occupied;
}

uint32_t hash_sv(struct string_view *key);

#endif
