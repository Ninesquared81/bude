#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string_builder.h"

static struct string_builder *append_builder(struct string_builder *builder,
                                             struct region *region) {
    assert(builder->next == NULL);
    struct string_builder *new = region_alloc(region, sizeof *new);
    if (new == NULL) {
        return NULL;
    }
    memset(new, 0, sizeof *new);
    builder->next = new;
    return new;
}

struct string_builder *start_view(struct string_builder *builder, const char *start,
                                  struct region *region) {
    if (builder->owned_count > 0) {
        builder = append_builder(builder, region);
        if (builder == NULL) return NULL;
    }
    builder->owned_count = -1;
    builder->view.start = start;
    builder->view.length = 0;
    return builder;
}

struct string_builder *store_char(struct string_builder *builder, char ch, struct region *region) {
    if (SB_IS_VIEW(builder) || builder->owned_count >= (int)SB_OWNED_SIZE) {
        builder = append_builder(builder, region);
        if (builder == NULL) return NULL;
    }
    builder->owned[builder->owned_count++] = ch;
    return builder;
}

struct string_builder *store_view(struct string_builder *builder, const struct string_view *view,
                                  struct region *region) {
    if (!SB_IS_FREE(builder)) {
        builder = append_builder(builder, region);
        if (builder == NULL) return NULL;
    }
    builder->owned_count = -1;
    builder->view = *view;
    return builder;
}

void build_string(struct string_builder *builder, char *buffer) {
    // This function assumes the buffer is large enough.
    for (; builder != NULL; builder = builder->next) {
        struct string_view view = SB_NODE_AS_VIEW(builder);
        memcpy(buffer, view.start, view.length);
        buffer += view.length;
    }
    // buffer now points to the end of the string.
    *buffer = '\0';
}

struct string_view build_string_in_region(struct string_builder *builder, struct region *region) {
    size_t length = sb_length(builder);
    char *buffer = region_alloc(region, length + 1);
    if (buffer == NULL) return (struct string_view) {0};
    build_string(builder, buffer);
    return (struct string_view) {.start = buffer, .length = length};
}

size_t sb_length(struct string_builder *builder) {
    size_t length = 0;
    for (; builder != NULL; builder = builder->next) {
        length += SB_NODE_AS_VIEW(builder).length;
    }
    return length;
}
