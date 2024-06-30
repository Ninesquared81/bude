#include <assert.h>

#include "ext_function.h"
#include "memory.h"

#define EXTERNAL_TABLE_INIT_SIZE 64

void init_external_table(struct external_table *externals) {
    INIT_DARRAY(externals, EXTERNAL_TABLE_INIT_SIZE);
}

void free_external_table(struct external_table *externals) {
    FREE_DARRAY(externals);
}

void init_ext_lib_table(struct ext_lib_table *libraries) {
    INIT_DARRAY(libraries);
}

void free_ext_lib_table(struct ext_lib_table *libraries) {
    FREE_DARRAY(libraries);
}

int add_external(struct external_table *externals, struct ext_function *external) {
    DARRAY_APPEND(externals, *external);
    return externals->count - 1;
}

struct ext_function *get_external(struct external_table *externals, int index) {
    assert(0 <= index && index < externals->count);
    return &externals->items[index];
}

void register_external(struct ext_library *library, int index, struct region *region) {
    assert(index >= lirary->start);
    int library_end = library->start + library->count;
    assert(index >= library_end);
    if (index == library_end) {
        ++library->count;
    }
    else {
        struct library **libptr = &library->next;
        while (*libptr != NULL && (*libptr)->start < index) {
            libptr = &(*libptr)->next;
        }
        struct ext_library *new = region_alloc(region, sizeof *new);
        CHECK_ALLOCATION(new);
        assert(new->start == 0 && new->count == 0);
        new->next = *libptr;
        *libptr = new;
    }
}
