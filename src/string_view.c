#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string_view.h"

char *view_to_string(struct string_view *view, struct region *region) {
    char *string = region_alloc(region, view->length + 1);
    if (string == NULL) return NULL;
    memcpy(string, view->start, view->length);
    string[view->length] = '\0';
    return string;
}

struct string_view copy_view(struct string_view *view, void *buf) {
    // NOTE: buf must be large enough to store contents of view.
    memcpy(buf, view->start, view->length);
    return (struct string_view) {.start = buf, .length = view->length};
}

struct string_view copy_view_in_region(struct string_view *view, struct region *region) {
    void *chars = region_alloc(region, view->length + 1);  // Null-terminated.
    if (chars == NULL) return (struct string_view) {0};
    return copy_view(view, chars);
}

bool sv_eq(const struct string_view *a, const struct string_view *b) {
    return a->length == b->length && strncmp(a->start, b->start, a->length) == 0;
}

struct string_view sv_slice(const struct string_view *super, int start, int stop) {
    if (start < 0) {
        start += super->length;
        if (start < 0) start = 0;
    }
    if (stop < 0) {
        stop += super->length;
        if (stop < 0) stop = 0;
    }
    assert(start >= 0 && stop >= 0);
    if ((size_t)start > super->length) start = super->length;
    if ((size_t)stop > super->length) stop = super->length;
    if (stop < start) stop = start;
    return SV_BETWEEN(&super->start[start], &super->start[stop]);
}
