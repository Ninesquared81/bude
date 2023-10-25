#include <stdlib.h>
#include <string.h>

#include "string_view.h"

char *view_to_string(struct string_view *view) {
    char *string = malloc(view->length + 1);
    if (string == NULL) return NULL;
    memcpy(string, view->start, view->length);
    string[view->length] = '\0';
    return string;
}

bool sv_eq(const struct string_view *a, const struct string_view *b) {
    return a->length == b->length && strcmp(a->start, b->start) == 0;
}
