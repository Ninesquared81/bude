#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "module.h"

#ifndef MODULE_REGION_SIZE
#define MODULE_REGION_SIZE 1024
#endif

#ifndef STRING_TABLE_INIT_SIZE
#define STRING_TABLE_INIT_SIZE 64
#endif


static void init_string_table(struct string_table *table) {
    table->views = allocate_array(STRING_TABLE_INIT_SIZE, sizeof *table->views);
    table->capacity = STRING_TABLE_INIT_SIZE;
    table->count = 0;
}

static void free_string_table(struct string_table *table) {
    free_array(table->views, table->capacity, sizeof *table->views);
    table->views = NULL;
    table->capacity = 0;
    table->count = 0;
}

void init_module(struct module *module, const char *filename) {
    module->filename = filename;
    module->max_for_loop_level = 0;
    module->region = new_region(MODULE_REGION_SIZE);
    if (module->region == NULL) {
        // TODO: CHECK_ALLOCATION() macro.
        fprintf(stderr, "Failed to allocate region for module.\n");
        exit(1);
    }
    init_function_table(&module->functions);
    init_string_table(&module->strings);
    init_type_table(&module->types);
}

void free_module(struct module *module) {
    free_function_table(&module->functions);
    free_string_table(&module->strings);
    free_type_table(&module->types);
    kill_region(module->region);
    module->region = NULL;
}

static void grow_string_table(struct string_table *table) {
    size_t old_capacity = table->capacity;
    size_t new_capacity = old_capacity + old_capacity/2;
    assert(new_capacity > 0);
    table->views = reallocate_array(table->views, old_capacity, new_capacity,
                                      sizeof table->views[0]);
    table->capacity = new_capacity;
}

int write_string(struct module *module, struct string_builder *builder) {
    struct string_view view = build_string_in_region(builder, module->region);
    if (view.start == NULL) {
        fprintf(stderr, "Failed to allocate string.\n");
        exit(1);
    }
    struct string_table *table = &module->strings;
    if (table->count + 1 > table->capacity) {
        grow_string_table(table);
    }
    table->views[table->count++] = view;
    return table->count - 1;
}

struct string_view *read_string(struct module *module, int index) {
    assert(index >= 0);
    assert((size_t)index < module->strings.count);
    return &module->strings.views[index];
}

