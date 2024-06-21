#include <stdarg.h>
#include <stddef.h>

#include "function.h"
#include "memory.h"

#define LOCALS_INIT_SIZE 32

static void init_local_table(struct local_table *table) {
    INIT_DARRAY(table, LOCALS_INIT_SIZE);
}

static void free_local_table(struct local_table *table) {
    INIT_DARRAY(table, LOCALS_INIT_SIZE);
}

void init_function_table(struct function_table *functions) {
    INIT_DARRAY(functions, FUNCTION_TABLE_INIT_SIZE);
    functions->region = new_region(FUNCTION_TABLE_REGION_SIZE);
    CHECK_ALLOCATION(functions->region);
}

void free_function_table(struct function_table *functions) {
    for (int i = 0; i < functions->count; ++i) {
        struct function function = functions->items[i];
        free_block(&function.t_code);
        free_block(&function.w_code);
        free_local_table(&function.locals);
    }
    FREE_DARRAY(functions);
    kill_region(functions->region);
    functions->region = NULL;
}

int add_local(struct function *function, type_index type) {
    struct local_table *locals = &function->locals;
    // Other fields will be set later:
    DARRAY_APPEND(locals, (struct local){.type = type});
    return locals->count - 1;
}

static int insert_function(struct function_table *table, struct function *function) {
    DARRAY_APPEND(table, *function);
    return table->count - 1;
}

int add_function(struct function_table *table, int param_count, int ret_count,
                 type_index params[param_count], type_index rets[ret_count]) {
    assert(param_count >= 0);
    assert(ret_count >= 0);
    struct function function = {.sig = {param_count, ret_count, params, rets}};
    init_block(&function.t_code, IR_TYPED);
    init_block(&function.w_code, IR_WORD_ORIENTED);
    init_local_table(&function.locals);
    return insert_function(table, &function);
}

struct function *get_function(struct function_table *table, int index) {
    return &table->items[index];
}
