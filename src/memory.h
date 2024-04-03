#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_ALLOCATION(ptr)                                    \
    do {if (ptr == NULL) {                                       \
        fprintf(stderr, "Allocation failed in %s on line %d.\n", \
                __FILE__, __LINE__ - 1);                         \
        exit(1);                                                 \
        }} while (0)

void *allocate_array(size_t count, size_t size);
void free_array(void *array, [[maybe_unused]] size_t count, [[maybe_unused]] size_t size);
void *reallocate_array(void *array, size_t old_count, size_t new_count, size_t size);

#endif
