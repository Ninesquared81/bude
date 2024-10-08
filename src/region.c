#include <stdlib.h>
#include <string.h>

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

struct region *copy_region(const struct region *region) {
    struct region *new = new_region(region->size);
    memcpy(new->bytes, region->bytes, region->alloc_count);
    new->alloc_count = region->alloc_count;
    if (region->next != NULL) {
        new->next = copy_region(region->next);
    }
    return new;
}

void clear_region(struct region *region) {
    // Need to maintain that the memory is zeored.
    memset(region->bytes, 0, region->size);
    region->alloc_count = 0;
    if (region->next != NULL) {
        clear_region(region->next);
    }
}

REGION_RESTORE record_region(const struct region *region) {
    return &region->bytes[region->alloc_count];
}

void restore_region(struct region *region, REGION_RESTORE restore_point) {
    ptrdiff_t diff = &region->bytes[region->alloc_count] - (const char *)restore_point;
    region->alloc_count -= diff;
    memset(&region->bytes[region->alloc_count], 0, diff);
}

void *region_alloc(struct region *region, size_t size) {
    if (size == 0) return NULL;
    if (size > region->size) return NULL;
    ALIGN(region->alloc_count, size);
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

void *region_calloc(struct region *region, size_t count, size_t size) {
    if (size == 0) return NULL;
    if (SIZE_MAX / size < count) return NULL;  // Multiplication overflow.
    return region_alloc(region, count * size);  // Region zeroed on initial allocation.
}
