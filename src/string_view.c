#include "string_view.h"

char *view_to_string(struct string_view *view) {
    char *string = malloc(view->length + 1);
    memcpy(string, view->start, view->length);
    string[view->length] = '\0';
    return string;
}
