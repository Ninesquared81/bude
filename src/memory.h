#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_ALLOCATION(ptr)                                    \
    do {if (ptr == NULL) {                                       \
        fprintf(stderr, "Allocation failed in %s on line %d.\n", \
                __FILE__, __LINE__ - 1);                         \
        exit(1);                                                 \
        }} while (0)

#define CHECK_ARRAY_ALLOCATION(ptr, count)                              \
    do {if (ptr == NULL && count != 0) {                                \
            fprintf(stderr, "Failed to allocate %zu objects in %s on line %d.\n", \
                    (size_t)count, __FILE__, __LINE__ - 1);             \
            exit(1);                                                    \
        }} while (0)                                                    \

#define DARRAY_INIT_SIZE 8

#define INIT_DARRAY(da, init_size)                                      \
    do {                                                                \
        (da)->capacity = init_size;                                     \
        (da)->count = 0;                                                \
        (da)->items = allocate_array(init_size, sizeof (da)->items[0]); \
    } while (0)

#define GROW_DARRAY(da)                                                 \
    do {                                                                \
        int old_capacity = (da)->capacity;                              \
        int new_capacity = old_capacity + old_capacity/2;               \
        if (new_capacity == 0) new_capacity = DARRAY_INIT_SIZE;         \
        size_t size = sizeof (da)->items[0];                            \
        void *new_items =                                               \
            reallocate_array((da)->items, old_capacity * size,          \
                             new_capacity * size, size);                \
        (da)->items = new_items;                                        \
    } while (0)

#define DARRAY_APPEND(da, item)                 \
    do {                                        \
        if ((da)->count + 1 > (da)->capacity) { \
            GROW_DARRAY(da);                    \
        }                                       \
        (da)->items[(da)->count++] = item;      \
    } while (0)

#define FREE_DARRAY(da)                                                 \
    do {                                                                \
        free_array((da)->items, (da)->capacity, sizeof (da)->items[0]); \
        (da)->items = NULL;                                             \
        (da)->count = 0;                                                \
        (da)->capacity = 0;                                             \
    } while (0)

void *allocate_array(size_t count, size_t size);
void free_array(void *array, [[maybe_unused]] size_t count, [[maybe_unused]] size_t size);
void *reallocate_array(void *array, size_t old_count, size_t new_count, size_t size);

bool array_eq(size_t a_count, const void *a, size_t b_count, const void *b, size_t size);

#endif
