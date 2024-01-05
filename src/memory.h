#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

void *allocate_array(size_t count, size_t size);
void free_array(void *array, [[maybe_unused]] size_t count, [[maybe_unused]] size_t size);
void *reallocate_array(void *array, size_t old_count, size_t new_count, size_t size);

#endif
