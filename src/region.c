#include <stdlib.h>

#include "region.h"

#define ALIGN_TO(alignment, count)                      \
    do {                                                \
        if ((count) % alignment != 0) {                 \
            count += alignment - (count) % alignment;   \
        }                                               \
    } while (0)

#define ALIGN(count, size)                                      \
    do {                                                        \
        if (size % 16 == 0) {                                   \
            ALIGN_TO(16, count);                                \
        }                                                       \
        else if (size % 8 == 0) {                               \
            ALIGN_TO(8, count);                                 \
        }                                                       \
        else if (size % 4 == 0) {                               \
            ALIGN_TO(4, count);                                 \
        }                                                       \
        else if (size % 2 == 0) {                               \
            ALIGN_TO(2, count);                                 \
        }                                                       \
    } while (0)

struct region *new_region(size_t size) {
    struct region *r = calloc(1, sizeof *r + size);
    if (r == NULL) return NULL;
    r->next = NULL;
    r->size = size;
    r->alloc_count = 0;
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
    ALIGN(region->alloc_count, size);
    if (size > region->size) return NULL;
    if (region->alloc_count + size > region->size) {
        if (region->next == NULL) {
            region->next = new_region(region->size);
            if (region->next == NULL) return NULL;
        }
        return region_alloc(region->next, size);
    }
    void *start = &region->bytes[region->alloc_count];
    region->alloc_count += size;
    return start;
}
