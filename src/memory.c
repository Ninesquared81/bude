#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"


void *allocate_array(size_t count, size_t size) {
    void *array = calloc(count, size);
    if (array == NULL) {
        fprintf(stderr, "Could not allocate array.\n");
        exit(1);
    }
    return array;
}

void free_array(void *array, [[maybe_unused]] size_t count, [[maybe_unused]] size_t size) {
    free(array);
}

void *reallocate_array(void *array, size_t old_count, size_t new_count, size_t size) {
    if (new_count < old_count) {
        return array;
    }
    void *new = allocate_array(new_count, size);
    if (new == NULL) {
        fprintf(stderr, "Could not reallocate array.\n");
        exit(1);
    }
    if (array != NULL) {
        memcpy(new, array, size * old_count);
        free_array(array, old_count, size);
    }
    return new;
}

