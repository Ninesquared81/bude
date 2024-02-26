#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "region.h"

struct string_view {
    const char *start;
    size_t length;
};

#define SV_LIT_INIT(str_lit) \
    {.start = str_lit, .length = sizeof str_lit - 1}
#define SV_LIT(str_lit) ((struct string_view) SV_LIT_INIT(str_lit))

char *view_to_string(struct string_view *view, struct region *region);
bool sv_eq(const struct string_view *a, const struct string_view *b);

#endif
