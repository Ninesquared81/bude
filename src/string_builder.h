#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stddef.h>

#include "string_view.h"
#include "region.h"

#define SB_OWNED_SIZE (int)sizeof(struct string_view)
#define SB_IS_VIEW(builder) ((builder)->owned_count == -1)
#define SB_IS_OWNED(builder) (1 <= (builder)->owned_count && \
                              (builder)->owned_count <= SB_OWNED_SIZE)
#define SB_IS_FREE(builder) (!SB_IS_VIEW(builder) && !SB_IS_OWNED(builder))

#define SB_NODE_AS_VIEW(builder) ((SB_IS_VIEW(builder))                 \
                                   ? (builder)->view                    \
                                   : (struct string_view) {             \
                                       .start = &(builder)->owned[0],   \
                                       .length = (builder)->owned_count})

#define SB_STORE_STR_LIT(builder, str_lit, region) \
    store_view(builder, &SV_LIT(str_lit), region)

#define SB_FROM_SV(sv) ((struct string_builder) {.view = (sv), .owned_count = -1})

struct string_builder {
    union {
        struct string_view view;
        char owned[SB_OWNED_SIZE];
    };
    int owned_count;  // Note: 0 => free; -1 => view; 1--SB_OWNED_SIZE => owned.
    struct string_builder *next;
};

struct string_builder *start_view(struct string_builder *builder, const char *start,
                                  struct region *region);
struct string_builder *store_char(struct string_builder *builder, char ch, struct region *region);
struct string_builder *store_view(struct string_builder *builder, const struct string_view *view,
                                  struct region *region);
void build_string(struct string_builder *builder, char *buffer);

struct string_view build_string_in_region(struct string_builder *builder, struct region *region);

size_t sb_length(struct string_builder *builder);

void kill_string_builder(struct string_builder *builder);

#endif
