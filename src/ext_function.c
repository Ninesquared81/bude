#include "ext_function.h"
#include "memory.h"

#define EXTERNAL_TABLE_INIT_SIZE 64

void init_external_table(struct external_table *externals) {
    INIT_DARRAY(externals, EXTERNAL_TABLE_INIT_SIZE);
}

void free_external_table(struct external_table *externals) {
    FREE_DARRAY(externals);
}

int add_external(struct external_table *externals, struct ext_function *external) {
    DARRAY_APPEND(externals, *external);
    return externals->count - 1;
}

struct ext_function *get_external(struct external_table *externals, int index) {
    assert(0 <= index && index < externals->count);
    return &externals->items[index];
}
