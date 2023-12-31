#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>

struct string_view {
    const char *start;
    size_t length;
};

char *view_to_string(struct string_view *view);
bool sv_eq(const struct string_view *a, const struct string_view *b);

#endif
