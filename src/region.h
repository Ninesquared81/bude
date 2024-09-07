#ifndef REGION_H
#define REGION_H

#include <stddef.h>

typedef const void *REGION_RESTORE;

struct region {
    struct region *next;
    size_t size;
    size_t alloc_count;
    char bytes[];
};

struct region *new_region(size_t size);
void kill_region(struct region *region);
struct region *copy_region(const struct region *region);
void clear_region(struct region *region);

REGION_RESTORE record_region(const struct region *region);
void restore_region(struct region *region, REGION_RESTORE restore_point);

void *region_alloc(struct region *region, size_t size);
void *region_calloc(struct region *region, size_t count, size_t size);

#endif
