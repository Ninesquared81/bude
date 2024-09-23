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

void sb_append(struct string_builder *start, struct string_builder rest, struct region *region) {
    while (start->next != NULL) {
        start = start->next;
    }
    start->next = region_alloc(region, sizeof *start->next);
    *start->next = rest;
}

void build_string(struct string_builder *builder, char *buffer) {
    join_string(builder, "", buffer);
}

void join_string(struct string_builder *builder, const char *fill, char *buffer) {
    assert(fill != NULL);  // Pass "" for no fill value.
    assert(buffer != NULL);
    assert(builder != NULL);
    int fill_length = strlen(fill);
    // This function assumes the buffer is large enough.
    struct string_view view = SB_NODE_AS_VIEW(builder);
    memcpy(buffer, view.start, view.length);
    buffer += view.length;
    for (builder = builder->next; builder != NULL; builder = builder->next) {
        view = SB_NODE_AS_VIEW(builder);
        memcpy(buffer, fill, fill_length);
        memcpy(buffer, view.start, view.length);
        buffer += fill_length + view.length;
    }
    // buffer now points to the end of the string.
    *buffer = '\0';
}

struct string_view build_string_in_region(struct string_builder *builder, struct region *region) {
    return join_string_in_region(builder, "", region);
}

struct string_view join_string_in_region(struct string_builder *builder, const char *fill,
                                         struct region *region) {
    int fill_length = strlen(fill);
    size_t length = sb_length(builder);
    for (struct string_builder *sb = builder->next; sb != NULL; sb = sb->next) {
        length += fill_length;
    }
    char *buffer = region_alloc(region, length + 1);
    if (buffer == NULL) return (struct string_view) {0};
    join_string(builder, fill, buffer);
    return (struct string_view) {.start = buffer, .length = length};
}

size_t sb_length(struct string_builder *builder) {
    size_t length = 0;
    for (; builder != NULL; builder = builder->next) {
        length += SB_NODE_AS_VIEW(builder).length;
    }
    return length;
}
