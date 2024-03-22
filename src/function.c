#include <stddef.h>

#include "function.h"

void init_function_table(struct function_table *functions) {
    functions.count = 0;
    functions.capacity = FUNCTION_TABLE_INIT_SIZE;
    functions.functions = allocate_array(FUNCTION_TABLE_INIT_SIZE, sizeof(struct function));
    functions.region = new_region(FUNCTION_TABLE_REGION_SIZE);
}

void free_function_table(struct function_table *functions) {
    free_array(functions.functions, functions.capacity, sizeof(struct function));
    functions.functions = NULL;
    functions.capacity = 0;
    functions.count = 0;
    kill_region(functions.region);
    functions.region = NULL;
}
