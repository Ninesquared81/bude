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
    void *chars = region_alloc(region, view->length);
    if (chars == NULL) return (struct string_view) {0};
    return copy_view(view, chars);
}

bool sv_eq(const struct string_view *a, const struct string_view *b) {
    return a->length == b->length && strncmp(a->start, b->start, a->length) == 0;
}

