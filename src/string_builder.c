#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string_builder.h"

static struct string_builder *append_builder(struct string_builder *builder) {
    assert(builder->next == NULL);
    struct string_builder *new  = calloc(1, sizeof *builder->next);
    if (new == NULL) {
        return NULL;
    }
    // Note: rely on calloc for zero-initialisation.
    builder->next = new;
    return new;
}

struct string_builder *start_view(struct string_builder *builder, const char *start) {
    if (builder->owned_count > 0) {
        builder = append_builder(builder);
        if (builder == NULL) return NULL;
    }
    builder->owned_count = -1;
    builder->view.start = start;
    builder->view.length = 0;
    return builder;
}

struct string_builder *store_char(struct string_builder *builder, char ch) {
    if (SB_IS_VIEW(builder) || builder->owned_count >= (int)SB_OWNED_SIZE) {
        builder = append_builder(builder);
        if (builder == NULL) return NULL;
    }
    builder->owned[builder->owned_count++] = ch;
    return builder;
}

char *build_string(struct string_builder *builder) {
    int length = 0;
    for (struct string_builder *current = builder; current != NULL; current = current->next) {
        length += (SB_IS_VIEW(current)) ? current->view.length : current->owned_count;
    }

    char *string = malloc(length + 1);
    if (string == NULL) return NULL;
    
    char *front = string;
    for (struct string_builder *current = builder; current != NULL; current = current->next) {
        struct string_view view = SB_NODE_AS_VIEW(current);
        memcpy(front, view.start, view.length);
        front += view.length;
    }
    string[length] = '\0';
    return string;
}

