#ifndef REGION_H
#define REGION_H

#include <stddef.h>

struct region {
    struct region *next;
    size_t size;
    size_t alloc_count;
    char bytes[];
};

struct region *new_region(size_t size);
void kill_region(struct region *region);
struct region *copy_region(const struct region *region);

void *region_alloc(struct region *region, size_t size);

#endif
