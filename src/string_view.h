#ifndef STRING_VIEW_H
#define STRING_VIEW_H

struct string_view {
    const char *start;
    int length;
};

char *view_to_string(struct string_view *view);

#endif
