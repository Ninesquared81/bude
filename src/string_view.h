#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>

struct string_view {
    const char *start;
    size_t length;
};

char *view_to_string(struct string_view *view);

#endif
