#include <stdlib.h>

#include "region.h"

#define REGION_END(region) (&(region)->bytes[(region)->size])

struct region *new_region(size_t size) {
    struct region *r = calloc(1, sizeof *r + size);
    if (r == NULL) return NULL;
    r->next = NULL;
    r->size = size;
    r->alloc_head = r->bytes;
    return r;
}

void kill_region(struct region *region) {
    while (region != NULL) {
        struct region *prev = region;
        region = region->next;
        free(prev);
    }
}

void *region_alloc(struct region *region, size_t size) {
    if (size > region->size) return NULL;
    if (REGION_END(region) - region->alloc_head < (long long)size) {
        if (region->next == NULL) {
            region->next = new_region(region->size);
            if (region->next == NULL) return NULL;
        }
        return region_alloc(region->next, size);
    }
    void *start = region->alloc_head;
    region->alloc_head += size;
    return start;
}
