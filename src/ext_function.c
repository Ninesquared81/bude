#include <assert.h>

#include "ext_function.h"
#include "memory.h"

#define EXTERNAL_TABLE_INIT_SIZE 64
#define EXT_LIB_TABLE_INIT_SIZE 8

void init_external_table(struct external_table *externals) {
    INIT_DARRAY(externals, EXTERNAL_TABLE_INIT_SIZE);
}

void free_external_table(struct external_table *externals) {
    FREE_DARRAY(externals);
}

void init_ext_lib_table(struct ext_lib_table *libraries) {
    INIT_DARRAY(libraries, EXT_LIB_TABLE_INIT_SIZE);
}

void free_ext_lib_table(struct ext_lib_table *libraries) {
    FREE_DARRAY(libraries);
}

int add_external(struct external_table *externals, struct ext_library *library,
                 struct ext_function *external) {
    int ext_index = externals->count;
    DARRAY_APPEND(externals, *external);
    DARRAY_APPEND(library, ext_index);
    return ext_index;
}

struct ext_function *get_external(struct external_table *externals, int index) {
    assert(0 <= index && index < externals->count);
    return &externals->items[index];
}
