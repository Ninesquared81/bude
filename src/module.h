#ifndef MODULE_H
#define MODULE_H

#include <stddef.h>
#include <stdint.h>

#include "function.h"
#include "string_builder.h"
#include "string_view.h"
#include "type.h"

struct string_table {
    size_t capacity;
    size_t count;
    struct string_view *views;
};

struct module {
    struct function_table functions;
    struct string_table strings;
    struct type_table types;
    struct region *region;
    const char *filename;
    size_t max_for_loop_level;
};

void init_module(struct module *module, const char *filename);
void free_module(struct module *module);

int write_string(struct module *module, struct string_builder *builder);
struct string_view *read_string(struct module *module, int index);

#endif
