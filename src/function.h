#ifndef FUNCTION_H
#define FUNCTION_H

#include "ir.h"
#include "region.h"
#include "type.h"

#define FUNCTION_TABLE_INIT_SIZE 64
#define FUNCTION_TABLE_REGION_SIZE 1024 * 1024

struct signature {
    int param_count;
    int ret_count;
    type_index *params;
    type_index *rets;
};

struct local {
    type_index type;
    int offset;
    int size;
};

struct local_table {
    int capacity;
    int count;
    struct local *items;
};

struct function {
    struct ir_block t_code;
    struct ir_block w_code;
    struct signature sig;
    struct local_table locals;
    int max_for_loop_level;
    int locals_size;
};

struct function_table {
    int count;
    int capacity;
    struct function *items;
    struct region *region;
};

int add_local(struct function *function, type_index type);

void init_function_table(struct function_table *functions);
void free_function_table(struct function_table *functions);

int add_function(struct function_table *functions, struct signature sig);
struct function *get_function(struct function_table *functions, int index);

#endif
