#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "module.h"
#include "string_view.h"

#ifndef MODULE_REGION_SIZE
#define MODULE_REGION_SIZE 1024
#endif

#ifndef STRING_TABLE_INIT_SIZE
#define STRING_TABLE_INIT_SIZE 64
#endif


static void init_string_table(struct string_table *strings) {
    INIT_DARRAY(strings, STRING_TABLE_INIT_SIZE);
}

static void free_string_table(struct string_table *strings) {
    FREE_DARRAY(strings);
}

void init_module(struct module *module, const char *filename) {
    module->filename = filename;
    module->region = new_region(MODULE_REGION_SIZE);
    CHECK_ALLOCATION(module->region);
    init_external_table(&module->externals);
    init_ext_lib_table(&module->ext_libraries);
    init_function_table(&module->functions);
    init_string_table(&module->strings);
    init_type_table(&module->types);
}

void free_module(struct module *module) {
    free_external_table(&module->externals);
    free_ext_lib_table(&module->ext_libraries);
    free_function_table(&module->functions);
    free_string_table(&module->strings);
    free_type_table(&module->types);
    kill_region(module->region);
    module->region = NULL;
}

int write_string(struct module *module, struct string_builder *builder) {
    struct string_view view = build_string_in_region(builder, module->region);
    CHECK_ALLOCATION(view.start);
    struct string_table *strings = &module->strings;
    DARRAY_APPEND(strings, view);
    return strings->count - 1;
}

struct string_view *read_string(struct module *module, int index) {
    assert(index >= 0);
    assert(index < module->strings.count);
    return &module->strings.items[index];
}

int find_string(struct module *module, const struct string_view *view) {
    for (int i = 0; i < module->strings.count; ++i) {
        if (sv_eq(&module->strings.items[i], view)) return i;
    }
    return -1;
}
