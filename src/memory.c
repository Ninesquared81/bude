#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"


void *allocate_array(size_t count, size_t size) {
    void *array = calloc(count, size);
    CHECK_ALLOCATION(array);
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
    CHECK_ALLOCATION(new);
    if (array != NULL) {
        memcpy(new, array, size * old_count);
        free_array(array, old_count, size);
    }
    return new;
}

bool array_eq(size_t a_count, const void *a, size_t b_count, const void *b, size_t size) {
    if (a_count != b_count) return false;
    if (a_count == 0) return true;
    assert(a != NULL);
    assert(b != NULL);
    assert(size > 0);
    return memcmp(a, b, a_count * size) == 0;
}
