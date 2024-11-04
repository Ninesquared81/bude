#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "region.h"

struct string_view {
    const char *start;
    size_t length;
};

#define SV_END(sv) \
    ((sv).start + (sv).length)
#define SV_BETWEEN(start, end) \
    ((struct string_view) {(start), (end) - (start)})
#define SV_LIT_INIT(str_lit) \
    {.start = str_lit, .length = sizeof str_lit - 1}
#define SV_LIT(str_lit) ((struct string_view) SV_LIT_INIT(str_lit))
#define SV_FMT(sv) (((sv).length < (size_t)INT_MAX) ? (int)(sv).length : INT_MAX), (sv).start
#define PRI_SV ".*s"

char *view_to_string(struct string_view *view, struct region *region);
struct string_view copy_view(struct string_view *view, void *buf);
struct string_view copy_view_in_region(struct string_view *view, struct region *region);

#define SV_SLICE_END INT_MAX

bool sv_eq(const struct string_view *a, const struct string_view *b);
struct string_view sv_slice(const struct string_view *super, int start, int stop);

#endif
