#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include "string_view.h"

struct string_builder {
    struct string_view view;
    struct string_builder *next;
};

char *build_string(struct string_builder *builder);

#endif
