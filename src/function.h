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

struct function {
    struct ir_block t_code;
    struct ir_block w_code;
    struct signature sig;
};

struct function_table {
    int count;
    int capacity;
    struct function *functions;
    struct region *region;
    const char *filename;
};

void init_function_table(struct function_table *functions, const char *filename);
void free_function_table(struct function_table *functions);

int insert_function(struct function_table *table, struct function *function);
int add_function(struct function_table *table, int param_count, int ret_count, ...);
struct function *get_function(struct function_table *table, int index);

#endif
