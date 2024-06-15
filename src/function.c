#include <stdarg.h>
#include <stddef.h>

#include "function.h"
#include "memory.h"

#define LOCALS_INIT_SIZE 32

static void init_local_table(struct local_table *table) {
    table->locals = allocate_array(LOCALS_INIT_SIZE, sizeof *table->locals);
    table->capacity = LOCALS_INIT_SIZE;
    table->count = 0;
}

static void free_local_table(struct local_table *table) {
    free_array(table->locals, table->capacity, sizeof *table->locals);
    table->capacity = 0;
    table->count = 0;
}

static void grow_local_table(struct local_table *table) {
    int old_capacity = table->capacity;
    int new_capacity = old_capacity + old_capacity/2;
    assert(new_capacity > 0);
    table->locals = reallocate_array(table->locals, old_capacity, new_capacity,
                                     sizeof table->locals[0]);
    table->capacity = new_capacity;
}

void init_function_table(struct function_table *functions) {
    functions->count = 0;
    functions->capacity = FUNCTION_TABLE_INIT_SIZE;
    functions->functions = allocate_array(FUNCTION_TABLE_INIT_SIZE, sizeof(struct function));
    functions->region = new_region(FUNCTION_TABLE_REGION_SIZE);
    CHECK_ALLOCATION(functions->region);
}

void free_function_table(struct function_table *functions) {
    for (int i = 0; i < functions->count; ++i) {
        struct function function = functions->functions[i];
        free_block(&function.t_code);
        free_block(&function.w_code);
        free_local_table(&function.locals);
    }
    free_array(functions->functions, functions->capacity, sizeof(struct function));
    functions->functions = NULL;
    functions->capacity = 0;
    functions->count = 0;
    kill_region(functions->region);
    functions->region = NULL;
}

int add_local(struct function *function, type_index type) {
    struct local_table *locals = &function->locals;
    if (locals->count + 1 > locals->capacity) {
        grow_local_table(locals);
    }
    // Other fields will be set later:
    locals->locals[locals->count++] = (struct local){.type = type};
    return locals->count - 1;
}

static int insert_function(struct function_table *table, struct function *function) {
    if (table->count + 1 > table->capacity) {
        int old_capacity = table->capacity;
        int new_capacity = old_capacity + old_capacity/2;
        if (new_capacity == 0) {
            new_capacity = FUNCTION_TABLE_INIT_SIZE;
        }
        table->functions = reallocate_array(table->functions, old_capacity,
                                            new_capacity, sizeof(struct function));
        table->capacity = new_capacity;
    }
    int index = table->count++;
    table->functions[index] = *function;
    return index;
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
    return &table->functions[index];
}
