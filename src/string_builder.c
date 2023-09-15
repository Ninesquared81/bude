#include "string_builder.c"

char *build_string(struct string_builder *builder) {
    int length = 0;
    for (struct string_builder *current = builder; current != NULL; current = current->next) {
        length += current->view.length;
    }

    char *string = malloc(length + 1);
    char *front = string;

    for (struct string_builder *current = builder; current != NULL; current = current->next) {
        struct string_view *view = &current->view;
        memcpy(front, view->start, view->length);
        front += view->length;
    }
    string[length] = '\0';
    return string;
}

